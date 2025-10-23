#include "ggml.h"
#include "clip.h"

#include <string>

void modelHandler();

void finishTextRequests();
float *embedText(std::string &text,
	clip_ctx *clip,
	ggml_context *text_ctx,
	ggml_cgraph *text_graph);