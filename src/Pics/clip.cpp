#include <X11/Xdefs.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <regex>
#include <stdexcept>
#include <vector>

#include "clip.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml.h"
#include "gguf.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

#define NOMINMAX
#include <windows.h>

typedef volatile LONG atomic_int;
typedef atomic_int	  atomic_bool;

static void atomic_store(atomic_int *ptr, LONG val) {
	InterlockedExchange(ptr, val);
}
static LONG atomic_load(atomic_int *ptr) {
	return InterlockedCompareExchange(ptr, 0, 0);
}
static LONG atomic_fetch_add(atomic_int *ptr, LONG inc) {
	return InterlockedExchangeAdd(ptr, inc);
}
static LONG atomic_fetch_sub(atomic_int *ptr, LONG dec) {
	return atomic_fetch_add(ptr, -(dec));
}

typedef HANDLE pthread_t;

typedef DWORD thread_ret_t;
static int	  pthread_create(pthread_t *out,
							 void	   *unused,
							 thread_ret_t (*func)(void *),
							 void *arg) {
	   ( void )unused;
	   HANDLE handle =
		   CreateThread(NULL, 0, ( LPTHREAD_START_ROUTINE )func, arg, 0, NULL);
	   if (handle == NULL) {
		   return EAGAIN;
	   }

	   *out = handle;
	   return 0;
}

static int pthread_join(pthread_t thread, void *unused) {
	( void )unused;
	return ( int )WaitForSingleObject(thread, INFINITE);
}

static int sched_yield(void) {
	Sleep(0);
	return 0;
}

#define pthread_exit(stat) return stat;
#else
#include <pthread.h>

typedef void *thread_ret_t;

#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

// #define CLIP_DEBUG

static std::string format(const char *fmt, ...) {
	va_list ap;
	va_list ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	int size = vsnprintf(NULL, 0, fmt, ap);
	GGML_ASSERT(size >= 0 && size < INT_MAX); // NOLINT
	std::vector<char> buf(size + 1);
	int				  size2 = vsnprintf(buf.data(), size + 1, fmt, ap2);
	GGML_ASSERT(size2 == size);
	va_end(ap2);
	va_end(ap);
	return std::string(buf.data(), buf.size());
}

//
// key constants
//

#define KEY_FTYPE		   "general.file_type"
#define KEY_NAME		   "general.name"
#define KEY_DESCRIPTION	   "general.description"
#define KEY_HAS_TEXT_ENC   "clip.has_text_encoder"
#define KEY_HAS_VIS_ENC	   "clip.has_vision_encoder"
#define KEY_USE_GELU	   "clip.use_gelu"
#define KEY_N_EMBD		   "clip.%s.embedding_length"
#define KEY_N_FF		   "clip.%s.feed_forward_length"
#define KEY_N_BLOCK		   "clip.%s.block_count"
#define KEY_N_HEAD		   "clip.%s.attention.head_count"
#define KEY_LAYER_NORM_EPS "clip.%s.attention.layer_norm_epsilon"
#define KEY_PROJ_DIM	   "clip.%s.projection_dim"
#define KEY_TOKENS		   "tokenizer.ggml.tokens"
#define KEY_N_POSITIONS	   "clip.text.context_length"
#define KEY_IMAGE_SIZE	   "clip.vision.image_size"
#define KEY_PATCH_SIZE	   "clip.vision.patch_size"
#define KEY_IMAGE_MEAN	   "clip.vision.image_mean"
#define KEY_IMAGE_STD	   "clip.vision.image_std"

//
// tensor name constants
//

#define TN_TOKEN_EMBD  "%s.token_embd.weight"
#define TN_POS_EMBD	   "%s.position_embd.weight"
#define TN_CLASS_EMBD  "v.class_embd"
#define TN_PATCH_EMBD  "v.patch_embd.weight"
#define TN_ATTN_K	   "%s.blk.%d.attn_k.%s"
#define TN_ATTN_Q	   "%s.blk.%d.attn_q.%s"
#define TN_ATTN_V	   "%s.blk.%d.attn_v.%s"
#define TN_ATTN_OUTPUT "%s.blk.%d.attn_out.%s"
#define TN_FFN_DOWN	   "%s.blk.%d.ffn_down.%s"
#define TN_FFN_UP	   "%s.blk.%d.ffn_up.%s"
#define TN_LN_1		   "%s.blk.%d.ln1.%s"
#define TN_LN_2		   "%s.blk.%d.ln2.%s"
#define TN_LN_PRE	   "%s.pre_ln.%s"
#define TN_LN_POST	   "%s.post_ln.%s"
#define TN_TEXT_PROJ   "text_projection.weight"
#define TN_VIS_PROJ	   "visual_projection.weight"

//
// utilities to get data from a gguf file
//

int get_key_idx(const gguf_context *ctx, const char *key) {
	int i = gguf_find_key(ctx, key);
	if (i == -1) {
		fprintf(stderr, "key %s not found in file\n", key);
		throw std::runtime_error(format("Missing required key: %s", key));
	}

	return i;
}

const uint32_t get_u32(const gguf_context *ctx, std::string key) {
	const int i = get_key_idx(ctx, key.c_str());

	return gguf_get_val_u32(ctx, i);
}

const float get_f32(const gguf_context *ctx, std::string key) {
	const int i = get_key_idx(ctx, key.c_str());

	return gguf_get_val_f32(ctx, i);
}

ggml_tensor *get_tensor(struct ggml_context *ctx, std::string name) {
	ggml_tensor *cur = ggml_get_tensor(ctx, name.c_str());
	if (!cur) {
		printf("unable to find tensor %s\n", name.c_str());
		throw std::runtime_error(
			format("unable to find tensor %s\n", name.c_str()));
	}

	return cur;
}

std::string get_ftype(int ftype) {
	switch (ftype) {
	case 0:
		return "f32";
		break;
	case 1:
		return "f16";
		break;
	case 2:
		return "q4_0";
		break;
	case 3:
		return "q4_1";
		break;
	case 6:
		return "q5_0";
		break;
	case 7:
		return "q5_1";
		break;
	case 8:
		return "q8_0";
		break;
	default:
		throw std::runtime_error(format("Unrecognized file type: %d\n", ftype));
	}
}

//
// Vocab utils
//

struct clip_vocab {
	using id = clip_vocab_id;
	using token = std::string;

	std::map<token, id>		 token_to_id;
	std::map<id, token>		 id_to_token;
	std::vector<std::string> special_tokens;

	//    void add_special_token(const std::string & token);
};

//
// clip layers
//

struct clip_layer {
	// attention
	ggml_tensor *k_w;
	ggml_tensor *k_b;
	ggml_tensor *q_w;
	ggml_tensor *q_b;
	ggml_tensor *v_w;
	ggml_tensor *v_b;

	ggml_tensor *o_w;
	ggml_tensor *o_b;

	// layernorm 1
	ggml_tensor *ln_1_w;
	ggml_tensor *ln_1_b;

	// ff
	ggml_tensor *ff_i_w;
	ggml_tensor *ff_i_b;

	ggml_tensor *ff_o_w;
	ggml_tensor *ff_o_b;

	// layernorm 2
	ggml_tensor *ln_2_w;
	ggml_tensor *ln_2_b;
};

struct clip_text_model {
	struct clip_text_hparams hparams;

	// embeddings
	ggml_tensor *token_embeddings;
	ggml_tensor *position_embeddings;

	std::vector<clip_layer> layers;

	ggml_tensor *post_ln_w;
	ggml_tensor *post_ln_b;

	ggml_tensor *projection;
};

struct clip_vision_model {
	struct clip_vision_hparams hparams;

	// embeddings
	ggml_tensor *class_embedding;
	ggml_tensor *patch_embeddings;
	ggml_tensor *position_embeddings;

	ggml_tensor *pre_ln_w;
	ggml_tensor *pre_ln_b;

	std::vector<clip_layer> layers;

	ggml_tensor *post_ln_w;
	ggml_tensor *post_ln_b;

	ggml_tensor *projection;
};

struct clip_ctx {
	bool					 has_text_encoder = false;
	bool					 has_vision_encoder = false;
	struct clip_text_model	 text_model;
	struct clip_vision_model vision_model;
	struct clip_vocab		 vocab;
	float					 image_mean[3];
	float					 image_std[3];
	bool					 use_gelu = false;
	int32_t					 ftype = 1;
	ggml_context			*ctx_ggml;
	gguf_context			*ctx_gguf;
	ggml_backend_t			 backend;
	ggml_backend_buffer_t	 backend_buf;
};

// read and create ggml_context containing the tensors and their data
struct clip_ctx *clip_model_load(const char *fname, const int verbosity = 1) {

	ggml_context		   *tmp_ctx = nullptr;
	struct gguf_init_params params = {
		/*.no_alloc = */ false,
		/*.ctx      = */ &tmp_ctx,
	};

	struct gguf_context *gguf_ctx = gguf_init_from_file(fname, params);
	if (!gguf_ctx) {
		throw std::runtime_error("ctx is null");
	}

	const int n_tensors = gguf_get_n_tensors(gguf_ctx);

	if (verbosity >= 1) {
		const int		  n_kv = gguf_get_n_kv(gguf_ctx);
		const int		  ftype = get_u32(gguf_ctx, KEY_FTYPE);
		const std::string ftype_str = get_ftype(ftype);
		const int		  idx_desc = get_key_idx(gguf_ctx, KEY_DESCRIPTION);
		const std::string description = gguf_get_val_str(gguf_ctx, idx_desc);
		const int		  idx_name = gguf_find_key(gguf_ctx, KEY_NAME);
		if (idx_name != -1) { // make name optional temporarily as some of the
							  // uploaded models missing it due to a bug
			const std::string name = gguf_get_val_str(gguf_ctx, idx_name);
			printf("%s: model name:   %s\n", __func__, name.c_str());
		}
		printf("%s: description:  %s\n", __func__, description.c_str());
		printf("%s: GGUF version: %d\n", __func__, gguf_get_version(gguf_ctx));
		printf(
			"%s: alignment:    %zu\n", __func__, gguf_get_alignment(gguf_ctx));
		printf("%s: n_tensors:    %d\n", __func__, n_tensors);
		printf("%s: n_kv:         %d\n", __func__, n_kv);
		printf("%s: ftype:        %s\n", __func__, ftype_str.c_str());
		printf("\n");
	}

	// kv
	if (verbosity >= 3) {
		const int n_kv = gguf_get_n_kv(gguf_ctx);

		for (int i = 0; i < n_kv; ++i) {
			const char *key = gguf_get_key(gguf_ctx, i);

			printf("%s: kv[%d]: key = %s\n", __func__, i, key);
		}
		printf("\n");
	}

	// data
	{
		for (int i = 0; i < n_tensors; ++i) {
			const char	*name = gguf_get_tensor_name(gguf_ctx, i);
			const size_t offset = gguf_get_tensor_offset(gguf_ctx, i);

			if (verbosity >= 3) {
				printf("%s: tensor[%d]: name = %s, offset=%zu\n",
					   __func__,
					   i,
					   name,
					   offset);
			}
		}
	}

	ggml_init_params model_ctx_params = { ggml_tensor_overhead() * n_tensors,
										  nullptr,
										  true };
	ggml_context	*model_ctx = ggml_init(model_ctx_params);

	clip_ctx *clip = new clip_ctx;

	clip->backend = ggml_backend_cpu_init();
	if (!clip->backend)
		__builtin_trap();

	// model size and capabilities
	{
		int idx = get_key_idx(gguf_ctx, KEY_HAS_TEXT_ENC);
		clip->has_text_encoder = gguf_get_val_bool(gguf_ctx, idx);

		idx = get_key_idx(gguf_ctx, KEY_HAS_VIS_ENC);
		clip->has_vision_encoder = gguf_get_val_bool(gguf_ctx, idx);

		idx = get_key_idx(gguf_ctx, KEY_USE_GELU);
		clip->use_gelu = gguf_get_val_bool(gguf_ctx, idx);

		if (verbosity >= 1) {
			printf(
				"%s: text_encoder:   %d\n", __func__, clip->has_text_encoder);
			printf(
				"%s: vision_encoder: %d\n", __func__, clip->has_vision_encoder);
			printf("%s: metadata size:  %.2f MB\n",
				   __func__,
				   ggml_get_mem_size(model_ctx) / 1024.0 / 1024.0);
			printf("%s: model size:     %.2f MB\n",
				   __func__,
				   (gguf_get_meta_size(gguf_ctx) / 1024.0 / 1024.0));
		}
	}

	// load tensors
	{
		if (!model_ctx) {
			fprintf(stderr, "%s: ggml_init() failed\n", __func__);
			clip_free(clip);
			return nullptr;
		}

		for (int i = 0; i < n_tensors; ++i) {
			const char	*name = gguf_get_tensor_name(gguf_ctx, i);
			ggml_tensor *src = ggml_get_tensor(tmp_ctx, name);
			ggml_tensor *dst = ggml_dup_tensor(model_ctx, src);
			ggml_set_name(dst, name);
		}
		clip->backend_buf =
			ggml_backend_alloc_ctx_tensors(model_ctx, clip->backend);
		for (ggml_tensor *cur = ggml_get_first_tensor(model_ctx);
			 cur != nullptr;
			 cur = ggml_get_next_tensor(model_ctx, cur)) {
			ggml_tensor *src = ggml_get_tensor(tmp_ctx, ggml_get_name(cur));
			size_t		 n_size = ggml_nbytes(src);
			ggml_backend_tensor_set(cur, ggml_get_data(src), 0, n_size);
		}
	}
	printf("Backend Size: %ld\n",
		   ggml_backend_buffer_get_size(clip->backend_buf));

	// text model
	if (clip->has_text_encoder) {
		// load text model
		auto &text_model = clip->text_model;
		auto &hparams = text_model.hparams;
		hparams.hidden_size = get_u32(gguf_ctx, format(KEY_N_EMBD, "text"));
		hparams.n_head = get_u32(gguf_ctx, format(KEY_N_HEAD, "text"));
		hparams.n_intermediate = get_u32(gguf_ctx, format(KEY_N_FF, "text"));
		hparams.n_layer = get_u32(gguf_ctx, format(KEY_N_BLOCK, "text"));
		hparams.num_positions = get_u32(gguf_ctx, KEY_N_POSITIONS);
		hparams.projection_dim =
			get_u32(gguf_ctx, format(KEY_PROJ_DIM, "text"));
		hparams.eps = get_f32(gguf_ctx, format(KEY_LAYER_NORM_EPS, "text"));

		const int idx_tokens = get_key_idx(gguf_ctx, KEY_TOKENS);
		hparams.n_vocab = gguf_get_arr_n(gguf_ctx, idx_tokens);
		auto &vocab = clip->vocab;
		for (int id = 0; id < hparams.n_vocab; ++id) {
			const std::string token =
				gguf_get_arr_str(gguf_ctx, idx_tokens, id);
			vocab.id_to_token[id] = token;
			vocab.token_to_id[token] = id;
		}

		if (verbosity >= 2) {
			printf("\n%s: text model hparams\n", __func__);
			printf("n_vocab            %d\n", hparams.n_vocab);
			printf("num_positions      %d\n", hparams.num_positions);
			printf("t_hidden_size      %d\n", hparams.hidden_size);
			printf("t_n_intermediate   %d\n", hparams.n_intermediate);
			printf("t_projection_dim   %d\n", hparams.projection_dim);
			printf("t_n_head           %d\n", hparams.n_head);
			printf("t_n_layer          %d\n", hparams.n_layer);
		}

		text_model.token_embeddings =
			get_tensor(model_ctx, format(TN_TOKEN_EMBD, "t"));
		text_model.position_embeddings =
			get_tensor(model_ctx, format(TN_POS_EMBD, "t"));
		text_model.post_ln_w =
			get_tensor(model_ctx, format(TN_LN_POST, "t", "weight"));
		text_model.post_ln_b =
			get_tensor(model_ctx, format(TN_LN_POST, "t", "bias"));
		text_model.projection = get_tensor(model_ctx, TN_TEXT_PROJ);
		text_model.layers.resize(hparams.n_layer);
		for (int il = 0; il < hparams.n_layer; ++il) {
			auto &layer = text_model.layers[il];
			layer.k_w =
				get_tensor(model_ctx, format(TN_ATTN_K, "t", il, "weight"));
			layer.q_w =
				get_tensor(model_ctx, format(TN_ATTN_Q, "t", il, "weight"));
			layer.v_w =
				get_tensor(model_ctx, format(TN_ATTN_V, "t", il, "weight"));
			layer.o_w = get_tensor(model_ctx,
								   format(TN_ATTN_OUTPUT, "t", il, "weight"));
			layer.ln_1_w =
				get_tensor(model_ctx, format(TN_LN_1, "t", il, "weight"));
			layer.ln_2_w =
				get_tensor(model_ctx, format(TN_LN_2, "t", il, "weight"));
			layer.ff_i_w =
				get_tensor(model_ctx, format(TN_FFN_DOWN, "t", il, "weight"));
			layer.ff_o_w =
				get_tensor(model_ctx, format(TN_FFN_UP, "t", il, "weight"));
			layer.k_b =
				get_tensor(model_ctx, format(TN_ATTN_K, "t", il, "bias"));
			layer.q_b =
				get_tensor(model_ctx, format(TN_ATTN_Q, "t", il, "bias"));
			layer.v_b =
				get_tensor(model_ctx, format(TN_ATTN_V, "t", il, "bias"));
			layer.o_b =
				get_tensor(model_ctx, format(TN_ATTN_OUTPUT, "t", il, "bias"));
			layer.ln_1_b =
				get_tensor(model_ctx, format(TN_LN_1, "t", il, "bias"));
			layer.ln_2_b =
				get_tensor(model_ctx, format(TN_LN_2, "t", il, "bias"));
			layer.ff_i_b =
				get_tensor(model_ctx, format(TN_FFN_DOWN, "t", il, "bias"));
			layer.ff_o_b =
				get_tensor(model_ctx, format(TN_FFN_UP, "t", il, "bias"));
		}
	}

	// vision model
	if (clip->has_vision_encoder) {
		// load vision model
		auto &vision_model = clip->vision_model;
		auto &hparams = vision_model.hparams;
		hparams.hidden_size = get_u32(gguf_ctx, format(KEY_N_EMBD, "vision"));
		hparams.n_head = get_u32(gguf_ctx, format(KEY_N_HEAD, "vision"));
		hparams.n_intermediate = get_u32(gguf_ctx, format(KEY_N_FF, "vision"));
		hparams.n_layer = get_u32(gguf_ctx, format(KEY_N_BLOCK, "vision"));
		hparams.image_size = get_u32(gguf_ctx, KEY_IMAGE_SIZE);
		hparams.patch_size = get_u32(gguf_ctx, KEY_PATCH_SIZE);
		hparams.projection_dim =
			get_u32(gguf_ctx, format(KEY_PROJ_DIM, "vision"));
		hparams.eps = get_f32(gguf_ctx, format(KEY_LAYER_NORM_EPS, "vision"));

		int idx_mean = get_key_idx(gguf_ctx, KEY_IMAGE_MEAN);
		int idx_std = get_key_idx(gguf_ctx, KEY_IMAGE_STD);
		for (int i = 0; i < 3; ++i) {
			clip->image_mean[i] =
				*(( float * )gguf_get_arr_data(gguf_ctx, idx_mean));
			clip->image_std[i] =
				*(( float * )gguf_get_arr_data(gguf_ctx, idx_std));
		}

		if (verbosity >= 2) {
			printf("\n%s: vision model hparams\n", __func__);
			printf("image_size         %d\n", hparams.image_size);
			printf("patch_size         %d\n", hparams.patch_size);
			printf("v_hidden_size      %d\n", hparams.hidden_size);
			printf("v_n_intermediate   %d\n", hparams.n_intermediate);
			printf("v_projection_dim   %d\n", hparams.projection_dim);
			printf("v_n_head           %d\n", hparams.n_head);
			printf("v_n_layer          %d\n", hparams.n_layer);
		}

		vision_model.patch_embeddings = get_tensor(model_ctx, TN_PATCH_EMBD);
		vision_model.class_embedding = get_tensor(model_ctx, TN_CLASS_EMBD);
		vision_model.position_embeddings =
			get_tensor(model_ctx, format(TN_POS_EMBD, "v"));
		vision_model.pre_ln_w =
			get_tensor(model_ctx, format(TN_LN_PRE, "v", "weight"));
		vision_model.pre_ln_b =
			get_tensor(model_ctx, format(TN_LN_PRE, "v", "bias"));
		vision_model.post_ln_w =
			get_tensor(model_ctx, format(TN_LN_POST, "v", "weight"));
		vision_model.post_ln_b =
			get_tensor(model_ctx, format(TN_LN_POST, "v", "bias"));
		vision_model.projection = get_tensor(model_ctx, TN_VIS_PROJ);
		vision_model.layers.resize(hparams.n_layer);
		for (int il = 0; il < hparams.n_layer; ++il) {
			auto &layer = vision_model.layers[il];
			layer.k_w =
				get_tensor(model_ctx, format(TN_ATTN_K, "v", il, "weight"));
			layer.q_w =
				get_tensor(model_ctx, format(TN_ATTN_Q, "v", il, "weight"));
			layer.v_w =
				get_tensor(model_ctx, format(TN_ATTN_V, "v", il, "weight"));
			layer.o_w = get_tensor(model_ctx,
								   format(TN_ATTN_OUTPUT, "v", il, "weight"));
			layer.ln_1_w =
				get_tensor(model_ctx, format(TN_LN_1, "v", il, "weight"));
			layer.ln_2_w =
				get_tensor(model_ctx, format(TN_LN_2, "v", il, "weight"));
			layer.ff_i_w =
				get_tensor(model_ctx, format(TN_FFN_DOWN, "v", il, "weight"));
			layer.ff_o_w =
				get_tensor(model_ctx, format(TN_FFN_UP, "v", il, "weight"));
			layer.k_b =
				get_tensor(model_ctx, format(TN_ATTN_K, "v", il, "bias"));
			layer.q_b =
				get_tensor(model_ctx, format(TN_ATTN_Q, "v", il, "bias"));
			layer.v_b =
				get_tensor(model_ctx, format(TN_ATTN_V, "v", il, "bias"));
			layer.o_b =
				get_tensor(model_ctx, format(TN_ATTN_OUTPUT, "v", il, "bias"));
			layer.ln_1_b =
				get_tensor(model_ctx, format(TN_LN_1, "v", il, "bias"));
			layer.ln_2_b =
				get_tensor(model_ctx, format(TN_LN_2, "v", il, "bias"));
			layer.ff_i_b =
				get_tensor(model_ctx, format(TN_FFN_DOWN, "v", il, "bias"));
			layer.ff_o_b =
				get_tensor(model_ctx, format(TN_FFN_UP, "v", il, "bias"));
		}
	}

	clip->ctx_gguf = gguf_ctx;
	clip->ctx_ggml = model_ctx;
	return clip;
}

bool clip_tokenize(clip_ctx			  *ctx,
				   const char		  *text,
				   struct clip_tokens *tokens) {
	if (!ctx->has_text_encoder) {
		printf("This GGUF file seems to have no text encoder\n");
		return false;
	}

	std::vector<std::string> words;

	// first split the text into words
	{
		std::string str = text;
		std::string pat =
			R"('s|'t|'re|'ve|'m|'ll|'d| ?[[:alpha:]]+| ?[[:digit:]]+| ?[^\s[:alpha:][:digit:]]+|\s+(?!\S)|\s+)";

		// Generate the subpattern from the special_tokens vector if it's not
		// empty
		if (!ctx->vocab.special_tokens.empty()) {
			std::string special_tokens_subpattern;
			for (const auto &token : ctx->vocab.special_tokens) {
				if (!special_tokens_subpattern.empty()) {
					special_tokens_subpattern += "|";
				}
				special_tokens_subpattern += token;
			}

			// Modify the regex pattern with the generated special tokens
			// subpattern
			pat = special_tokens_subpattern + "|" + pat;
		}

		std::regex	re(pat);
		std::smatch m;

		while (std::regex_search(str, m, re)) {
			for (auto x : m) {
				words.push_back(x);
			}
			str = m.suffix();
		}
	}

	std::vector<clip_vocab::id> v_tokens;
	v_tokens.push_back(49406); // startoftext

	for (const auto &word : words) {
		// feel lucky? let's try if it's a full word
		std::string full_word = "";
		if (word.find(" ") == 0) // starts_with for C++11
		{
			full_word += word.substr(1);
		} else {
			full_word += word;
		}
		full_word += "</w>";
		auto wit = ctx->vocab.token_to_id.find(full_word);
		if (wit != ctx->vocab.token_to_id.end()) {
			v_tokens.push_back(wit->second);
			continue;
		}

		for (int i = 0; i < word.size();) {
			for (int j = word.size() - 1; j >= i; j--) {
				auto cand = word.substr(i, j - i + 1);
				auto it = ctx->vocab.token_to_id.find(cand);
				if (it != ctx->vocab.token_to_id
							  .end()) { // word.substr(i, j-i+1) in vocab
					v_tokens.push_back(it->second);
					i = j + 1;
					break;
				} else if (j == i) { // word.substr(i, 1) has no matching
					fprintf(stderr,
							"%s: unknown token '%s'\n",
							__func__,
							word.substr(i, 1).data());
					i++;
				}
			}
		}
	}

	v_tokens.push_back(49407); // endoftext

	tokens->size = v_tokens.size();

	tokens->data = new int[v_tokens.size()];
	std::copy(v_tokens.begin(), v_tokens.end(), tokens->data);

	return true;
}

void clip_free(clip_ctx *ctx) {
	ggml_free(ctx->ctx_ggml);
	delete ctx;
}

bool clip_text_encode(clip_ctx			*clip,
					  const int			 n_threads,
					  const clip_tokens *tokens,
					  float				*vec,
					  const bool		 normalize) {
	if (!clip->has_text_encoder) {
		printf("This GGUF file seems to have no text encoder\n");
		return false;
	}

	const auto	&model = clip->text_model;
	const auto	&hparams = model.hparams;
	const size_t N = tokens->size;

	const int	n_vocab = hparams.n_vocab;
	const int	num_positions = hparams.num_positions;
	const int	hidden_size = hidden_size;
	const int	n_head = hparams.n_head;
	const int	d_head = hidden_size / n_head;
	const int	n_layer = hparams.n_layer;
	const int	n_intermediate = hparams.n_intermediate;
	const int	projection_dim = hparams.projection_dim;
	const float eps = hparams.eps;

	ggml_context	   *ggml_ctx = clip->ctx_ggml;
	struct ggml_cgraph *gf = ggml_new_graph(ggml_ctx);

	ggml_tensor *input_ids = ggml_new_tensor_1d(ggml_ctx, GGML_TYPE_I32, N);
	memcpy(input_ids->data, tokens->data, N * ggml_element_size(input_ids));

	ggml_tensor *positions = ggml_new_tensor_1d(ggml_ctx, GGML_TYPE_I32, N);
	for (int i = 0; i < N; i++) {
		ggml_set_i32_1d(positions, i, i);
	}

	ggml_tensor *embeddings =
		ggml_get_rows(ggml_ctx, model.token_embeddings, input_ids);

	embeddings =
		ggml_add(ggml_ctx,
				 ggml_get_rows(ggml_ctx, model.position_embeddings, positions),
				 embeddings);

	// loop over layers
	for (int il = 0; il < n_layer; il++) {
		ggml_tensor *cur =
			embeddings; // embeddings = residual, cur = hidden_states

		// layernorm1
		{
			cur = ggml_norm(ggml_ctx, cur, eps);

			cur = ggml_add(
				ggml_ctx,
				ggml_mul(ggml_ctx,
						 ggml_repeat(ggml_ctx, model.layers[il].ln_1_w, cur),
						 cur),
				ggml_repeat(ggml_ctx, model.layers[il].ln_1_b, cur));
		}

		// self-attention
		{
			ggml_tensor *Q =
				ggml_add(ggml_ctx,
						 ggml_repeat(ggml_ctx, model.layers[il].q_b, cur),
						 ggml_mul_mat(ggml_ctx, model.layers[il].q_w, cur));

			Q = ggml_scale_inplace(ggml_ctx, Q, 1.0f / sqrt(float(d_head)));
			Q = ggml_reshape_4d(ggml_ctx, Q, d_head, n_head, N, 1);
			Q = ggml_cont(ggml_ctx, ggml_permute(ggml_ctx, Q, 0, 2, 1, 3));
			Q = ggml_reshape_3d(ggml_ctx, Q, d_head, N, n_head);

			ggml_tensor *K =
				ggml_add(ggml_ctx,
						 ggml_repeat(ggml_ctx, model.layers[il].k_b, cur),
						 ggml_mul_mat(ggml_ctx, model.layers[il].k_w, cur));

			K = ggml_reshape_4d(ggml_ctx, K, d_head, n_head, N, 1);
			K = ggml_cont(ggml_ctx, ggml_permute(ggml_ctx, K, 0, 2, 1, 3));
			K = ggml_reshape_3d(ggml_ctx, K, d_head, N, n_head);

			ggml_tensor *V =
				ggml_add(ggml_ctx,
						 ggml_repeat(ggml_ctx, model.layers[il].v_b, cur),
						 ggml_mul_mat(ggml_ctx, model.layers[il].v_w, cur));
			V = ggml_reshape_4d(ggml_ctx, V, d_head, n_head, N, 1);
			V = ggml_cont(ggml_ctx, ggml_permute(ggml_ctx, V, 1, 2, 0, 3));
			V = ggml_reshape_3d(ggml_ctx, V, N, d_head, n_head);

			ggml_tensor *KQ = ggml_mul_mat(ggml_ctx, K, Q);
			KQ = ggml_diag_mask_inf_inplace(ggml_ctx, KQ, 0); // causal masking
			KQ = ggml_soft_max_inplace(ggml_ctx, KQ);

			ggml_tensor *KQV = ggml_mul_mat(ggml_ctx, V, KQ);
			KQV = ggml_reshape_4d(ggml_ctx, KQV, d_head, N, n_head, 1);
			KQV = ggml_cont(ggml_ctx, ggml_permute(ggml_ctx, KQV, 0, 2, 1, 3));

			cur = ggml_cpy(
				ggml_ctx,
				KQV,
				ggml_new_tensor_2d(ggml_ctx, GGML_TYPE_F32, hidden_size, N));
		}

		// attention output
		cur = ggml_add(ggml_ctx,
					   ggml_repeat(ggml_ctx, model.layers[il].o_b, cur),
					   ggml_mul_mat(ggml_ctx, model.layers[il].o_w, cur));

		// re-add the layer input, e.g., residual
		cur = ggml_add(ggml_ctx, cur, embeddings);

		embeddings = cur; // embeddings = residual, cur = hidden_states

		// layernorm2
		{
			cur = ggml_norm(ggml_ctx, cur, eps);

			cur = ggml_add(
				ggml_ctx,
				ggml_mul(ggml_ctx,
						 ggml_repeat(ggml_ctx, model.layers[il].ln_2_w, cur),
						 cur),
				ggml_repeat(ggml_ctx, model.layers[il].ln_2_b, cur));
		}

		cur = ggml_mul_mat(ggml_ctx, model.layers[il].ff_i_w, cur);
		cur = ggml_add(
			ggml_ctx, ggml_repeat(ggml_ctx, model.layers[il].ff_i_b, cur), cur);

		if (clip->use_gelu) {
			cur = ggml_gelu_inplace(ggml_ctx, cur);
		} else {
			cur = ggml_gelu_quick_inplace(ggml_ctx, cur);
		}

		cur = ggml_mul_mat(ggml_ctx, model.layers[il].ff_o_w, cur);
		cur = ggml_add(
			ggml_ctx, ggml_repeat(ggml_ctx, model.layers[il].ff_o_b, cur), cur);

		// residual 2
		cur = ggml_add(ggml_ctx, embeddings, cur);

		embeddings = cur;
	}

	// final -layer_norm
	{
		embeddings = ggml_norm(ggml_ctx, embeddings, eps);

		embeddings = ggml_add(
			ggml_ctx,
			ggml_mul(ggml_ctx,
					 ggml_repeat(ggml_ctx, model.post_ln_w, embeddings),
					 embeddings),
			ggml_repeat(ggml_ctx, model.post_ln_b, embeddings));
	}

	// get the output of eot token, e.g., last index
	ggml_tensor *eot = ggml_new_i32(ggml_ctx, N - 1);
	embeddings = ggml_get_rows(ggml_ctx, embeddings, eot);

	// text projection
	embeddings = ggml_mul_mat(ggml_ctx, model.projection, embeddings);

	// normalize output embeddings
	if (normalize) {
		ggml_tensor *length = ggml_sqrt(
			ggml_ctx, ggml_sum(ggml_ctx, ggml_sqr(ggml_ctx, embeddings)));
		assert(ggml_nbytes(length) == 0 && "Wrong function call here maybe");
		float lengthF = ggml_get_data_f32(length)[0];
		embeddings = ggml_scale_inplace(ggml_ctx, embeddings, 1.0f / lengthF);
	}

	ggml_set_name(embeddings, "check");

	// run the computation

	ggml_build_forward_expand(gf, embeddings);
	ggml_threadpool_params threadpool_params =
		ggml_threadpool_params_default(n_threads);
	ggml_threadpool *threadPool = ggml_threadpool_new(&threadpool_params);
	ggml_cplan		 cplan = ggml_graph_plan(gf, n_threads, threadPool);
	if (cplan.work_size != 0) {
		cplan.work_data = ( uint8_t * )malloc(cplan.work_size);
	}
	ggml_graph_compute(gf, &cplan);
	ggml_threadpool_free(threadPool);

// print
#ifdef CLIP_DEBUG
	{
		auto print_t_f32 = [&](ggml_tensor *t) {
			float *data = ( float * )t->data;
			printf("dtype: f32, dims: %jd %jd %jd %jd, nb: %jd %jd %jd %jd\n",
				   t->ne[0],
				   t->ne[1],
				   t->ne[2],
				   t->ne[3],
				   t->nb[0],
				   t->nb[1],
				   t->nb[2],
				   t->nb[3]);
			printf("data: ");
			for (int i = 0; i < std::min(( int )t->ne[0], 20); i++) {
				printf("%f ", data[i]);
			}

			// printf("\n\n");
			double sum = 0.0;
			for (int i = 0; i < ggml_nelements(t); i++) {
				sum += data[i];
			}
			printf("sum:  %f\n", sum);
		};

		auto print_t_f16 = [&](ggml_tensor *t) {
			ggml_fp16_t *data = ( ggml_fp16_t * )t->data;
			printf("dtype: f16, dims: %jd %jd %jd %jd\n",
				   t->ne[0],
				   t->ne[1],
				   t->ne[2],
				   t->ne[3]);
			printf("data: ");
			for (int i = 0; i < std::min(( int )t->ne[0], 10); i++) {
				printf("%f ", ggml_fp16_to_fp32(data[i]));
			}
			printf("\n\n");
			double sum = 0.0;
			for (int i = 0; i < ggml_nelements(t); i++) {
				sum += ggml_fp16_to_fp32(data[i]);
			}
			printf("sum:  %f\n", sum);
		};

		auto *t = ggml_get_tensor(graph_ctx, "check");
		if (t->type == GGML_TYPE_F32) {
			print_t_f32(t);
		} else {
			print_t_f16(t);
		}
	}

	printf("used_mem = %zu\n", ggml_used_mem(graph_ctx));
#endif
	memcpy(vec, ggml_get_data_f32(embeddings), sizeof(float) * projection_dim);

	if (cplan.work_size != 0) {
		free(cplan.work_data);
	}

	ggml_free(ggml_ctx);

	return true;
}

ggml_cgraph *build_image_encode_graph(ggml_context *graph_ctx,
									  clip_ctx	   *clip,
									  int			batch_size) {
	const auto		   &model = clip->vision_model;
	clip_vision_hparams hparams = model.hparams;
	int					image_size = hparams.image_size;
	int					patch_size = hparams.patch_size;
	int num_patches = ((image_size / patch_size) * (image_size / patch_size));
	int num_positions = num_patches + 1;
	int hidden_size = hparams.hidden_size;
	int n_head = hparams.n_head;
	int d_head = hidden_size / n_head;
	int eps = hparams.eps;
	int n_layer = hparams.n_layer;

	ggml_cgraph *graph = ggml_new_graph(graph_ctx);

	ggml_tensor *input_raw = ggml_new_tensor_4d(
		graph_ctx, GGML_TYPE_F32, image_size, image_size, 3, batch_size);
	ggml_set_name(input_raw, "input");
	ggml_set_input(input_raw);

	ggml_tensor *input = ggml_conv_2d(graph_ctx,
									  model.patch_embeddings,
									  input_raw,
									  patch_size,
									  patch_size,
									  0,
									  0,
									  1,
									  1);

	input =
		ggml_reshape_3d(graph_ctx, input, num_patches, hidden_size, batch_size);
	input = ggml_cont(graph_ctx, ggml_permute(graph_ctx, input, 1, 0, 2, 3));

	ggml_tensor *embeddings = ggml_new_tensor_3d(
		graph_ctx, GGML_TYPE_F32, hidden_size, num_positions, batch_size);
	ggml_set_name(embeddings, "embeddings");
	ggml_tensor *temp = ggml_new_tensor_3d(
		graph_ctx, GGML_TYPE_F32, hidden_size, 1, batch_size);

	embeddings = ggml_acc(graph_ctx,
						  embeddings,
						  ggml_repeat(graph_ctx, model.class_embedding, temp),
						  embeddings->nb[1],
						  embeddings->nb[2],
						  embeddings->nb[3],
						  0);
	embeddings = ggml_acc(graph_ctx,
						  embeddings,
						  input,
						  embeddings->nb[1],
						  embeddings->nb[2],
						  embeddings->nb[3],
						  model.class_embedding->nb[1]);

	ggml_tensor *positions =
		ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, num_positions);
	ggml_set_name(positions, "positions");

	embeddings = ggml_add(
		graph_ctx,
		embeddings,
		ggml_repeat(
			graph_ctx,
			ggml_get_rows(graph_ctx, model.position_embeddings, positions),
			embeddings));

	// Pre-layernorm
	embeddings = ggml_norm(graph_ctx, embeddings, eps);
	embeddings =
		ggml_add(graph_ctx,
				 ggml_mul(graph_ctx,
						  ggml_repeat(graph_ctx, model.pre_ln_w, embeddings),
						  embeddings),
				 ggml_repeat(graph_ctx, model.pre_ln_b, embeddings));

	// loop over layers
	for (int layer = 0; layer < n_layer; layer++) {
		ggml_tensor *cur = embeddings;

		// Layernorm 1
		cur = ggml_norm(graph_ctx, cur, eps);
		cur = ggml_add(
			graph_ctx,
			ggml_mul(graph_ctx,
					 ggml_repeat(graph_ctx, model.layers[layer].ln_1_w, cur),
					 cur),
			ggml_repeat(graph_ctx, model.layers[layer].ln_1_b, cur));

		// self-attention
		ggml_tensor *Q =
			ggml_add(graph_ctx,
					 ggml_repeat(graph_ctx, model.layers[layer].q_b, cur),
					 ggml_mul_mat(graph_ctx, model.layers[layer].q_w, cur));
		Q = ggml_scale_inplace(graph_ctx, Q, 1 / sqrt(( float )d_head));
		Q = ggml_reshape_4d(
			graph_ctx, Q, d_head, n_head, num_positions, batch_size);
		Q = ggml_cont(graph_ctx, ggml_permute(graph_ctx, Q, 0, 2, 1, 3));
		Q = ggml_reshape_3d(
			graph_ctx, Q, d_head, num_positions, n_head * batch_size);

		ggml_tensor *K =
			ggml_add(graph_ctx,
					 ggml_repeat(graph_ctx, model.layers[layer].k_b, cur),
					 ggml_mul_mat(graph_ctx, model.layers[layer].k_w, cur));
		K = ggml_reshape_4d(
			graph_ctx, K, d_head, n_head, num_positions, batch_size);
		K = ggml_cont(graph_ctx, ggml_permute(graph_ctx, K, 0, 2, 1, 3));
		K = ggml_reshape_3d(
			graph_ctx, K, d_head, num_positions, n_head * batch_size);

		ggml_tensor *V =
			ggml_add(graph_ctx,
					 ggml_repeat(graph_ctx, model.layers[layer].v_b, cur),
					 ggml_mul_mat(graph_ctx, model.layers[layer].v_w, cur));

		V = ggml_reshape_4d(
			graph_ctx, V, d_head, n_head, num_positions, batch_size);
		V = ggml_cont(graph_ctx, ggml_permute(graph_ctx, V, 1, 2, 0, 3));
		V = ggml_reshape_3d(
			graph_ctx, V, num_positions, d_head, n_head * batch_size);

		ggml_tensor *KQ = ggml_mul_mat(graph_ctx, K, Q);
		KQ = ggml_soft_max(graph_ctx, KQ);

		ggml_tensor *KQV = ggml_mul_mat(graph_ctx, V, KQ);
		KQV = ggml_reshape_4d(
			graph_ctx, KQV, d_head, num_positions, n_head, batch_size);
		KQV = ggml_cont(graph_ctx, ggml_permute(graph_ctx, KQV, 0, 2, 1, 3));

		cur = ggml_cpy(graph_ctx,
					   KQV,
					   ggml_new_tensor_3d(graph_ctx,
										  GGML_TYPE_F32,
										  hidden_size,
										  num_positions,
										  batch_size));

		// Attention output
		cur = ggml_add(graph_ctx,
					   ggml_repeat(graph_ctx, model.layers[layer].o_b, cur),
					   ggml_mul_mat(graph_ctx, model.layers[layer].o_w, cur));

		cur = ggml_add(graph_ctx, cur, embeddings);

		embeddings = cur;

		// Layernorm 2
		cur = ggml_norm(graph_ctx, cur, eps);
		cur = ggml_add(
			graph_ctx,
			ggml_mul(graph_ctx,
					 ggml_repeat(graph_ctx, model.layers[layer].ln_2_w, cur),
					 cur),
			ggml_repeat(graph_ctx, model.layers[layer].ln_2_b, cur));

		cur = ggml_mul_mat(graph_ctx, model.layers[layer].ff_i_w, cur);
		cur = ggml_add(graph_ctx,
					   ggml_repeat(graph_ctx, model.layers[layer].ff_i_b, cur),
					   cur);

		if (clip->use_gelu) {
			cur = ggml_gelu_inplace(graph_ctx, cur);
		} else {
			cur = ggml_gelu_quick_inplace(graph_ctx, cur);
		}

		cur = ggml_mul_mat(graph_ctx, model.layers[layer].ff_o_w, cur);
		cur = ggml_add(graph_ctx,
					   ggml_repeat(graph_ctx, model.layers[layer].ff_o_b, cur),
					   cur);

		// Residual 2
		cur = ggml_add(graph_ctx, embeddings, cur);

		embeddings = cur;
	}

	ggml_tensor *cls = ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, batch_size);
	ggml_set_name(cls, "cls");
	embeddings = ggml_get_rows(
		graph_ctx,
		ggml_reshape_2d(
			graph_ctx, embeddings, hidden_size, num_positions * batch_size),
		cls);

	// post-layernorm
	embeddings = ggml_norm(graph_ctx, embeddings, eps);
	embeddings =
		ggml_add(graph_ctx,
				 ggml_mul(graph_ctx,
						  ggml_repeat(graph_ctx, model.post_ln_w, embeddings),
						  embeddings),
				 ggml_repeat(graph_ctx, model.post_ln_b, embeddings));

	// final visual projection
	embeddings = ggml_mul_mat(graph_ctx, model.projection, embeddings);
	ggml_set_name(embeddings, "output");

	// TODO: Skipping normalization of output for now
	// normalise output embeddings
	//   ggml_tensor *output = ggml_new_tensor_2d(graph_ctx, GGML_TYPE_F32,
	//                                            hparams.projection_dim,
	//                                            batch_size);

	//   for (int b = 0; b < batch_size; b++) {
	//     ggml_tensor *row =
	// ggml_get_rows(graph_ctx, embeddings, ggml_new_i32(graph_ctx, b));
	//     output =
	//         ggml_acc_inplace(graph_ctx, output, row, output->nb[1],
	//         output->nb[2],
	//                          output->nb[3], b * ggml_nbytes(row));
	//   }

	ggml_build_forward_expand(graph, embeddings);
	// ggml_graph_dump_dot(graph, nullptr, "graph.dot");
	return graph;
}

void clip_get_image_embedding(struct clip_ctx *clip,
							  ggml_context	  *vision_ctx,
							  ggml_cgraph	  *vision_graph,
							  float			  *imageData,
							  int			   batch_size,
							  float			  *vec) {
	if (!clip->has_vision_encoder) {
		printf("This gguf file seems to have no vision encoder\n");
		return;
	}

	ggml_gallocr_t allocr =
		ggml_gallocr_new(ggml_backend_get_default_buffer_type(clip->backend));
	ggml_gallocr_alloc_graph(allocr, vision_graph);

	// unsigned char *dataUint =
	//     new unsigned char[image_size * image_size * 3 * batch_size];
	// for (int i = 0; i < image_size * image_size * 3 * batch_size; i++) {
	//   dataUint[i] = imageData[i] * 255.0f;
	// }
	// for (int i = 0; i < batch_size; i++) {
	//   stbi_write_png(format("out%d.png", i).c_str(), image_size, image_size *
	//   3,
	//                  1, dataUint + (i * (image_size * image_size * 3)), 0);
	// }

	int					image_size = clip->vision_model.hparams.image_size;
	clip_vision_hparams hparams = clip->vision_model.hparams;
	int					patch_size = hparams.patch_size;
	int num_patches = ((image_size / patch_size) * (image_size / patch_size));
	int num_positions = num_patches + 1;

	ggml_tensor *input = ggml_graph_get_tensor(vision_graph, "input");
	ggml_backend_tensor_set(input, imageData, 0, ggml_nbytes(input));

	ggml_tensor *embeddings = ggml_graph_get_tensor(vision_graph, "embeddings");
	size_t		 embeddingsSize = ggml_nbytes(embeddings);
	float		*embeddingsData = new float[embeddingsSize / sizeof(float)];
	memset(embeddingsData, 0, embeddingsSize);
	ggml_backend_tensor_set(embeddings, embeddingsData, 0, embeddingsSize);

	int *positionsData = new int[num_positions];
	for (int i = 0; i < num_positions; i++) {
		positionsData[i] = i;
	}
	ggml_tensor *positions = ggml_graph_get_tensor(vision_graph, "positions");
	ggml_backend_tensor_set(
		positions, positionsData, 0, ggml_nbytes(positions));

	ggml_tensor *cls = ggml_graph_get_tensor(vision_graph, "cls");
	// Re-using positionsData array since it's just setting i * num_positions
	// where 'i' is numbers from 0->n
	for (int i = 0; i < batch_size; i++) {
		positionsData[i] = i * num_positions;
	}
	ggml_backend_tensor_set(cls, positionsData, 0, ggml_nbytes(cls));

	ggml_tensor *output = ggml_graph_get_tensor(vision_graph, "output");

	const int64_t start_ms = ggml_time_ms();
	ggml_backend_graph_compute(clip->backend, vision_graph);
	const int64_t end_ms = ggml_time_ms() - start_ms;

	// int numberSize = 16;
	// int totalNumbers = 512;
	// char *outputString = new char[numberSize * totalNumbers];
	// for (int b = 0; b < batch_size; b++) {
	//   memset(outputString, 0, numberSize * totalNumbers);
	//   for (int i = 0; i < totalNumbers; i++) {
	//     snprintf(outputString + numberSize * i, numberSize, "%.6f, ",
	//              ((float *)output->data)[i + 512 * b]);
	//   }
	//   std::ofstream outFile(format("output_%d.txt", b), std::ios::binary);
	//   outFile.write(outputString, numberSize * totalNumbers);
	// }
	printf("Created image embeddings in time: %f sec.\n", end_ms / 1000.0f);

	memcpy(vec,
		   ggml_get_data_f32(output),
		   sizeof(float) * hparams.projection_dim * batch_size);
}

// float clip_similarity_score(const float *vec1, const float *vec2,
//                             const int vec_dim) {
//   float dot_product = 0.0;
//   for (int i = 0; i < vec_dim; i++) {
//     dot_product += vec1[i] * vec2[i];
//   }

//   return dot_product;
// }

// bool clip_compare_text_and_image(clip_ctx *ctx, const int n_threads,
//                                  const char *text, const clip_image_u8
//                                  *image, float *score) {
//   if (!(ctx->has_text_encoder && ctx->has_vision_encoder)) {
//     printf("clip_compare_text_and_image function can only be used with "
//            "two-tower models\n");
//     return false;
//   }

//   // prepare image and text vectors
//   const int projection_dim = ctx->vision_model.hparams.projection_dim;
//   float *img_vec = new float[projection_dim];
//   float *txt_vec = new float[projection_dim];

//   // tokenize and encode text
//   clip_tokens tokens;
//   if (!clip_tokenize(ctx, text, &tokens)) {
//     return false;
//   }

//   if (!clip_text_encode(ctx, n_threads, &tokens, txt_vec, true)) {
//     return false;
//   }

//   // preprocess and encode image
//   clip_image_f32 img_res;

//   if (!clip_image_preprocess(ctx, image, &img_res)) {
//     return false;
//   }

//   if (!clip_image_encode(ctx, n_threads, &img_res, img_vec, true)) {
//     return false;
//   }

//   // compute similarity
//   *score = clip_similarity_score(img_vec, txt_vec, projection_dim);

//   delete[] img_vec;
//   delete[] txt_vec;
//   return true;
// }

typedef struct {
	float score;
	int	  index;
} ScoreIndexPair;

int compare_scores(const void *a, const void *b) {
	const ScoreIndexPair *pair1 = ( const ScoreIndexPair * )a;
	const ScoreIndexPair *pair2 = ( const ScoreIndexPair * )b;

	if (pair1->score < pair2->score) {
		return 1;
	} else if (pair1->score > pair2->score) {
		return -1;
	} else {
		return 0;
	}
}

bool softmax_with_sorting(float	   *arr,
						  const int length,
						  float	   *sorted_scores,
						  int	   *indices) {
	ScoreIndexPair *score_index_pairs =
		( ScoreIndexPair * )malloc(length * sizeof(ScoreIndexPair));
	if (!score_index_pairs) {
		return false;
	}

	// Calculate softmax probabilities

	double sum = 0.0;
	for (int i = 0; i < length; i++) {
		arr[i] = exp(arr[i]) + 1e-9;
		sum += arr[i];
	}

	for (int i = 0; i < length; i++) {
		arr[i] /= sum;
		score_index_pairs[i].score = arr[i];
		score_index_pairs[i].index = i;
	}

	// Sort scores in descending order
	qsort(score_index_pairs, length, sizeof(ScoreIndexPair), compare_scores);

	// Copy sorted scores and indices to the respective arrays
	for (int i = 0; i < length; i++) {
		sorted_scores[i] = score_index_pairs[i].score;
		indices[i] = score_index_pairs[i].index;
	}

	free(score_index_pairs);
	return true;
}

// bool clip_zero_shot_label_image(struct clip_ctx *ctx, const int n_threads,
//                                 const struct clip_image_u8 *input_img,
//                                 const char **labels, const size_t n_labels,
//                                 float *scores, int *indices) {
//   if (!(ctx->has_text_encoder && ctx->has_vision_encoder)) {
//     printf("clip_zero_shot_label_image function can only be used with "
//            "two-tower models\n");
//     return false;
//   }

//   // load the image
//   clip_image_f32 img_res;

//   const int vec_dim = clip_get_vision_hparams(ctx)->projection_dim;

//   clip_image_preprocess(ctx, input_img, &img_res);

//   float *img_vec = new float[vec_dim];
//   if (!clip_image_encode(ctx, n_threads, &img_res, img_vec, false)) {
//     return false;
//   }

//   // encode texts and compute similarities
//   float *txt_vec = new float[vec_dim];
//   float *similarities = new float[n_labels];

//   for (int i = 0; i < n_labels; i++) {
//     const auto &text = labels[i];
//     clip_tokens tokens;
//     clip_tokenize(ctx, text, &tokens);
//     clip_text_encode(ctx, n_threads, &tokens, txt_vec, false);
//     similarities[i] = clip_similarity_score(img_vec, txt_vec, vec_dim);
//   }

//   // apply softmax and sort scores
//   softmax_with_sorting(similarities, n_labels, scores, indices);

//   delete[] img_vec;
//   delete[] txt_vec;
//   delete[] similarities;

//   return true;
// }

bool clip_model_quantize(const char *fname_inp,
						 const char *fname_out,
						 const int	 itype) {

	ggml_type type = GGML_TYPE_Q4_1;

	switch (itype) {
	case 2:
		type = GGML_TYPE_Q4_0;
		break;
	case 3:
		type = GGML_TYPE_Q4_1;
		break;
	case 6:
		type = GGML_TYPE_Q5_0;
		break;
	case 7:
		type = GGML_TYPE_Q5_1;
		break;
	case 8:
		type = GGML_TYPE_Q8_0;
		break;
	default:
		fprintf(stderr, "%s: invalid quantization type %d\n", __func__, itype);
		return false;
	};

	auto		ctx_clip = clip_model_load(fname_inp, 2);
	const auto &ctx_src = ctx_clip->ctx_gguf;
	const auto &ctx_data = ctx_clip->ctx_ggml;

	auto ctx_out = gguf_init_empty();
	gguf_set_kv(ctx_out, ctx_src);
	gguf_set_val_u32(ctx_out, "general.quantization_version", GGML_QNT_VERSION);
	gguf_set_val_u32(ctx_out, "general.file_type", itype);

	auto fout = std::ofstream(fname_out, std::ios::binary);

	const int n_tensors = gguf_get_n_tensors(ctx_src);

	for (int i = 0; i < n_tensors; ++i) {
		const char	*name = gguf_get_tensor_name(ctx_src, i);
		ggml_tensor *cur = ggml_get_tensor(ctx_data, name);
		gguf_add_tensor(ctx_out, cur);
	}

	const size_t meta_size = gguf_get_meta_size(ctx_out);
	for (size_t i = 0; i < meta_size; ++i) {
		fout.put(0);
	}

	// regexes of tensor names to be quantized
	const std::vector<std::string> k_names = {
		".*weight",
	};

	std::vector<uint8_t> read_data(512);
	std::vector<uint8_t> work(512);
	std::vector<float>	 conv_buf(512);
	std::vector<int64_t> hist_all(1 << 4, 0);
	size_t				 total_size_org = 0;
	size_t				 total_size_new = 0;

	for (int i = 0; i < n_tensors; ++i) {
		const std::string name = gguf_get_tensor_name(ctx_src, i);
		ggml_tensor		 *cur = ggml_get_tensor(ctx_data, name.c_str());

		enum ggml_type new_type;
		void		  *new_data;
		size_t		   new_size;

		bool quantize = false;
		for (const auto &s : k_names) {
			if (std::regex_match(name, std::regex(s))) {
				quantize = true;
				break;
			}
		}

		// quantize only 2D tensors
		quantize &= (ggml_n_dims(cur) == 2);

		if (quantize) {
			new_type = type;
			const size_t n_elms = ggml_nelements(cur);
			float		*f32_data;

			switch (cur->type) {
			case GGML_TYPE_F32:
				f32_data = ( float * )cur->data;
				break;
			case GGML_TYPE_F16:
				if (conv_buf.size() < n_elms) {
					conv_buf.resize(n_elms);
				}
				for (int j = 0; j < n_elms; ++j) {
					conv_buf[j] =
						ggml_fp16_to_fp32((( ggml_fp16_t * )cur->data)[j]);
				}
				f32_data = ( float * )conv_buf.data();
				break;
			default:
				printf("Please use an input file in f32 or f16\n");
				return false;
			}

			if (work.size() < n_elms * 4) {
				work.resize(n_elms * 4);
			}
			new_data = work.data();

			std::vector<int64_t> hist_cur(1 << 4, 0);

			// switch (new_type) {
			// case GGML_TYPE_Q4_0: {
			//   new_size = ggml_quantize_q4_0(f32_data, new_data, n_elms,
			//   cur->ne[0],
			//                                 hist_cur.data());
			// } break;
			// case GGML_TYPE_Q4_1: {
			//   new_size = ggml_quantize_q4_1(f32_data, new_data, n_elms,
			//   cur->ne[0],
			//                                 hist_cur.data());
			// } break;
			// case GGML_TYPE_Q5_0: {
			//   new_size = ggml_quantize_q5_0(f32_data, new_data, n_elms,
			//   cur->ne[0],
			//                                 hist_cur.data());
			// } break;
			// case GGML_TYPE_Q5_1: {
			//   new_size = ggml_quantize_q5_1(f32_data, new_data, n_elms,
			//   cur->ne[0],
			//                                 hist_cur.data());
			// } break;
			// case GGML_TYPE_Q8_0: {
			//   new_size = ggml_quantize_q8_0(f32_data, new_data, n_elms,
			//   cur->ne[0],
			//                                 hist_cur.data());
			// } break;
			// default: {
			//   fprintf(stderr, "%s: unsupported quantization type %d\n",
			//   __func__,
			//           new_type);
			//   return false;
			// }
			// }

			for (int j = 0; j < hist_cur.size(); ++j) {
				hist_all[j] += hist_cur[j];
			}
		} else {
			new_type = cur->type;
			new_data = cur->data;
			new_size = ggml_nbytes(cur);
		}
		const size_t orig_size = ggml_nbytes(cur);
		total_size_org += orig_size;
		total_size_new += new_size;
		gguf_set_tensor_type(ctx_out, name.c_str(), new_type);
		gguf_set_tensor_data(ctx_out, name.c_str(), new_data);
		fout.write(( const char * )new_data, new_size);
		size_t pad = GGML_PAD(new_size, gguf_get_alignment(ctx_out)) - new_size;
		for (int j = 0; j < pad; ++j) {
			fout.put(0);
		}

		printf("%s: n_dims = %d | quantize=%d | size = %f MB -> %f MB\n",
			   name.c_str(),
			   ggml_n_dims(cur),
			   quantize,
			   orig_size / 1024.0 / 1024.0,
			   new_size / 1024.0 / 1024.0);
	}

	// go back to beginning of file and write the updated metadata
	fout.seekp(0, std::ios::beg);
	std::vector<uint8_t> meta(meta_size);
	gguf_get_meta_data(ctx_out, meta.data());
	fout.write(( const char * )meta.data(), meta_size);

	fout.close();

	clip_free(ctx_clip);
	gguf_free(ctx_out);

	{
		printf("%s: original size  = %8.2f MB\n",
			   __func__,
			   total_size_org / 1024.0 / 1024.0);
		printf("%s: quantized size  = %8.2f MB\n",
			   __func__,
			   total_size_new / 1024.0 / 1024.0);

		int64_t sum_all = 0;
		for (size_t i = 0; i < hist_all.size(); ++i) {
			sum_all += hist_all[i];
		}

		printf("%s: hist: ", __func__);
		for (size_t i = 0; i < hist_all.size(); ++i) {
			printf("%5.3f ", hist_all[i] / ( float )sum_all);
		}
		printf("\n");
	}

	return true;
}

struct clip_text_hparams *clip_get_text_hparams(struct clip_ctx *ctx) {
	return &ctx->text_model.hparams;
}
struct clip_vision_hparams *clip_get_vision_hparams(struct clip_ctx *ctx) {
	return &ctx->vision_model.hparams;
}
