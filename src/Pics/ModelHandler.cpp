#include "ModelHandler.h"
#include "Application.h"
#include "Debug.h"
#include "ImageUtils.h"
#include "SignalBus.h"
#include "clip.h"
#include "spdlog/spdlog.h"

#include "ggml.h"

#include <cstddef>
#include <cstdio>

void finishTextRequests() {
	Application &app = Application::get_instance();
	SignalBus	&bus = SignalBus::getInstance();

	ggml_context *text_ctx;
	ggml_cgraph	 *text_graph;

	// while (!bus.textEncodeQ.empty()) {
	// 	std::string request = *(bus.textEncodeQ.pop());
	// 	float *embedding = embedText(request, app.clip, text_ctx, text_graph);
	// }
}

float *embedText(std::string &text,
	clip_ctx				 *clip,
	ggml_context			 *text_ctx,
	ggml_cgraph				 *text_graph) {
	const size_t	   graph_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
	clip_text_hparams *hparams = clip_get_text_hparams(clip);
	ggml_init_params   graph_params = { graph_size, nullptr, true };
	text_ctx = ggml_init(graph_params);
	clip_tokens tokens;
	clip_tokenize(clip, text, &tokens);
	text_graph = build_text_encode_graph(text_ctx, clip, &tokens);
	float *output = new float[hparams->projection_dim];
	clip_get_text_embedding(clip, text_ctx, text_graph, output);
	return output;
}

void modelHandler() {
	Application &app = Application::get_instance();
	SignalBus	&bus = SignalBus::getInstance();
	if (!app.clip) {
		app.clip = clip_model_load(
			ROOT_DIR "/CLIP-ViT-B-32-laion2B-s34B-b79K_ggml-model-f16.gguf",
			10);
	}
	clip_ctx *clip = app.clip;
	int		  image_size = clip_get_vision_hparams(clip)->image_size;

	std::vector<std::pair<fs::path, int>> pendingEmbeddings;

	app.db.execute_command(
		"SELECT id, path FROM Images WHERE embedding = 0;",
		[](void *data, int, char **argv, char **) -> int {
			std::vector<std::pair<fs::path, int>> *pendingEmbeddings =
				( std::vector<std::pair<fs::path, int>> * )data;
			pendingEmbeddings->push_back({ argv[1], std::stoi(argv[0]) });
			return 0;
		},
		&pendingEmbeddings);

	ggml_context *vision_ctx;
	ggml_cgraph	 *vision_graph;

	// TODO: Change batch size from 4
	for (int i = 0; i < pendingEmbeddings.size(); i += 4) {
		// Check if there is any text embedding task in text embedding queue and
		// fullfill that first
		finishTextRequests();

		int batch_size = std::min(( int )pendingEmbeddings.size() - i, 4);
		// Load image
		// Resize image
		// Change from interleaved to planar format
		// Batch images (use 4 right now, but later decide a more efficient
		// number)
		float *processedImages =
			preprocessNImages(pendingEmbeddings, 0, batch_size, image_size);

		clip_vision_hparams *hparams = clip_get_vision_hparams(clip);
		SPDLOG_INFO("Projection size: {}", hparams->projection_dim);
		float *output = new float[hparams->projection_dim * batch_size];
		// Pass to model and store the output embeddings

		// TODO: Write a assert system for clip.cpp (independent of pics)
		// Use clip-assert to check if clip model has vision encoder
		const size_t graph_size =
			ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
		ggml_init_params graph_params = { graph_size, nullptr, true };
		vision_ctx = ggml_init(graph_params);
		vision_graph = build_image_encode_graph(vision_ctx, clip, batch_size);

		for (int b = 0; b < batch_size; b++) {
			SPDLOG_INFO("Doing images: {}",
				pendingEmbeddings[i + b].first.string());
		}

		clip_get_image_embedding(clip,
			vision_ctx,
			vision_graph,
			processedImages,
			batch_size,
			output);

		SPDLOG_INFO("Made Embeddings !!!");

		app.db.execute_command("BEGIN TRANSACTION;");
		// Save embeddings to the vector index
		for (int b = 0; b < batch_size; b++) {
			app.index.try_reserve(4);
			app.index.add(pendingEmbeddings[i + b].second,
				output + b * hparams->projection_dim);

			// Update the fields in the sqlite database
			app.db.execute_command(
				fmt::format("UPDATE Images SET embedding = 1 WHERE id = {}",
					pendingEmbeddings[i + b].second));
		}

		// Save the vector index to disk
		app.index.save(INDEX_PATH);
		app.db.execute_command("COMMIT");

		// Cleanup
		ggml_free(vision_ctx);
	}
}
