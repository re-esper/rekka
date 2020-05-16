#pragma once

#include "rekka.h"
#include <map>

NS_REK_BEGIN

struct ShaderData {	
	uint32_t program;
	gpu::ShaderBlock block;
	int locations[4];
	ShaderData() : program(0) {};
};

class CanvasExtra {
public:
	static bool supportNPOTRepeat;
	static void initialize();	
public:	
	static void tintImage(gpu::Image* image, gpu::Target* target, float x, float y, float w, float h, GPU_Color blendColor, GPU_Color toneColor = { 0, 0, 0, 0 });
	static void beginNPOTRepeat(gpu::Target* target, gpu::Image* image, float texture_x, float texture_y);
	static void endNPOTRepeat();
};

NS_REK_END