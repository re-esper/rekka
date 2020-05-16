#include "xgpu.h"
#include "gpu_Renderer.h"
#include <string.h>

#define CHECK_RENDERER() \
Renderer* renderer = GetCurrentRenderer(); \
if(renderer == NULL) \
    return;

#define CHECK_RENDERER_1(ret) \
Renderer* renderer = GetCurrentRenderer(); \
if(renderer == NULL) \
    return ret;

NS_GPU_BEGIN

float SetLineThickness(float thickness)
{
	CHECK_RENDERER_1(1.0f);
	return renderer->SetLineThickness(thickness);
}

float GetLineThickness(void)
{
	CHECK_RENDERER_1(1.0f);
	return renderer->GetLineThickness();
}

void Pixel(Target* target, float x, float y, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Pixel(target, x, y, color);
}

void Line(Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Line(target, x1, y1, x2, y2, color);
}

void Arc(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Arc(target, x, y, radius, start_angle, end_angle, color);
}

void ArcFilled(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->ArcFilled(target, x, y, radius, start_angle, end_angle, color);
}

void Circle(Target* target, float x, float y, float radius, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Circle(target, x, y, radius, color);
}

void CircleFilled(Target* target, float x, float y, float radius, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->CircleFilled(target, x, y, radius, color);
}

void Ellipse(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Ellipse(target, x, y, rx, ry, degrees, color);
}

void EllipseFilled(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->EllipseFilled(target, x, y, rx, ry, degrees, color);
}

void Sector(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Sector(target, x, y, inner_radius, outer_radius, start_angle, end_angle, color);
}

void SectorFilled(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->SectorFilled(target, x, y, inner_radius, outer_radius, start_angle, end_angle, color);
}

void Tri(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Tri(target, x1, y1, x2, y2, x3, y3, color);
}

void TriFilled(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->TriFilled(target, x1, y1, x2, y2, x3, y3, color);
}

void Rectangle(Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Rectangle(target, x1, y1, x2, y2, color);
}

void Rectangle2(Target* target, GPU_Rect rect, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Rectangle(target, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, color);
}

void RectangleFilled(Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->RectangleFilled(target, x1, y1, x2, y2, color);
}

void RectangleFilled2(Target* target, GPU_Rect rect, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->RectangleFilled(target, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, color);
}

void RectangleRound(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->RectangleRound(target, x1, y1, x2, y2, radius, color);
}

void RectangleRound2(Target* target, GPU_Rect rect, float radius, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->RectangleRound(target, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, radius, color);
}

void RectangleRoundFilled(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->RectangleRoundFilled(target, x1, y1, x2, y2, radius, color);
}

void RectangleRoundFilled2(Target* target, GPU_Rect rect, float radius, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->RectangleRoundFilled(target, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, radius, color);
}

void Polygon(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->Polygon(target, num_vertices, vertices, color);
}

void PolygonFilled(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color)
{
	CHECK_RENDERER();
	renderer->PolygonFilled(target, num_vertices, vertices, color);
}

void PolygonTextureFilled(Target* target, unsigned int num_vertices, float* vertices, Image* image, float texture_x, float texture_y)
{
	CHECK_RENDERER();
	renderer->PolygonTextureFilled(target, num_vertices, vertices, image, texture_x, texture_y);
}

void PolygonTextureFilledNPOT(Target* target, unsigned int num_vertices, float* vertices, Image* image)
{
	CHECK_RENDERER();
	renderer->PolygonTextureFilledNPOT(target, num_vertices, vertices, image);
}

void TextureLine(Target* target, float x1, float y1, float x2, float y2, Image* image, float texture_x, float texture_y)
{
	CHECK_RENDERER();
	renderer->TextureLine(target, x1, y1, x2, y2, image, texture_x, texture_y);
}
void TextureLineNPOT(Target* target, float x1, float y1, float x2, float y2, Image* image)
{
	CHECK_RENDERER();
	renderer->TextureLineNPOT(target, x1, y1, x2, y2, image);
}

void PolygonColorFilled(Target * target, unsigned int num_vertices, float * vertices, ColorCallback colorfunc, void* userdata)
{
	CHECK_RENDERER();
	renderer->PolygonColorFilled(target, num_vertices, vertices, colorfunc, userdata);
}

void ColorLine(Target * target, float x1, float y1, float x2, float y2, ColorCallback colorfunc, void* userdata)
{
	CHECK_RENDERER();
	renderer->ColorLine(target, x1, y1, x2, y2, colorfunc, userdata);
}

NS_GPU_END