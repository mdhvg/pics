#include "ModelHandler.h"
#include "Application.h"
#include "Debug.h"
#include "ImageUtils.h"
#include "clip.h"
#include "ggml.h"
#include "spdlog/spdlog.h"
#include <cstdio>

void modelHandler() {
	Application &app = Application::getInstance();
	if (!app.clip) {
		app.clip = clip_model_load(
			ROOT_DIR "/CLIP-ViT-B-32-laion2B-s34B-b79K_ggml-model-f16.gguf",
			10);
	}
	clip_ctx *clip = app.clip;
	int		  image_size = clip_get_vision_hparams(clip)->image_size;

	std::vector<std::pair<fs::path, int>> pendingEmbeddings;

	app.db.executeCommand(
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
		int batch_size = std::min(( int )pendingEmbeddings.size() - i, 4);
		// int batch_size = 1;
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
			ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE +
			ggml_graph_overhead();
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

		app.db.executeCommand("BEGIN TRANSACTION;");
		// Save embeddings to the vector index
		for (int b = 0; b < batch_size; b++) {
			app.index.try_reserve(4);
			app.index.add(pendingEmbeddings[i + b].second,
						  output + b * hparams->projection_dim);

			// Update the fields in the sqlite database
			app.db.executeCommand(
				fmt::format("UPDATE Images SET embedding = 1 WHERE id = {}",
							pendingEmbeddings[i + b].second));
		}

		// Save the vector index to disk
		app.index.save(INDEX_PATH);
		app.db.executeCommand("COMMIT");

		// Cleanup
		ggml_free(vision_ctx);
	}
}
