#ifndef _XGPU_DEFINE_H
#define _XGPU_DEFINE_H

#include "SDL.h"
#include <stdio.h>
#include <stdarg.h>

#define NS_GPU_BEGIN		namespace gpu {
#define NS_GPU_END			}
#define USING_NS_GPU		using namespace gpu

#include "stdbool.h"

typedef struct GPU_Point {
	float x, y;
} GPU_Point;

typedef struct GPU_Color {
	float r, g, b, a;
} GPU_Color;

typedef struct GPU_Rect {
	float x, y;
	float w, h;
} GPU_Rect;

typedef struct AffineTransform {
	float a, b, c, d;
	float tx, ty;
} AffineTransform;

#endif
