#include "xgpu.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.1415926f
#endif

NS_GPU_BEGIN

AffineTransform AffineTransformMake(float a, float b, float c, float d, float tx, float ty)
{
	AffineTransform t = { a, b, c, d, tx, ty };	
	return t;
}

GPU_Point PointApplyAffineTransform(float x, float y, const AffineTransform* t)
{
	GPU_Point p;
	p.x = t->a * x + t->c * y + t->tx;
	p.y = t->b * x + t->d * y + t->ty;
	return p;
}

GPU_Point SizeApplyAffineTransform(float w, float h, const AffineTransform* t)
{
	GPU_Point p;
	p.x = t->a * w + t->c * h;
	p.y = t->b * w + t->d * h;
	return p;
}

AffineTransform AffineTransformMakeIdentity()
{
	return AffineTransformMake(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
}

AffineTransform AffineTransformTranslate(const AffineTransform* t, float tx, float ty)
{
	return AffineTransformMake(t->a, t->b, t->c, t->d, t->tx + t->a * tx + t->c * ty, t->ty + t->b * tx + t->d * ty);
}

AffineTransform AffineTransformRotate(const AffineTransform* t, float anAngle)
{
	float fSin = sinf(anAngle);
	float fCos = cosf(anAngle);
	return AffineTransformMake(
		t->a * fCos + t->c * fSin, t->b * fCos + t->d * fSin,
		t->c * fCos - t->a * fSin, t->d * fCos - t->b * fSin,
		t->tx,	t->ty);
}

AffineTransform AffineTransformScale(const AffineTransform* t, float sx, float sy)
{
	return AffineTransformMake(t->a * sx, t->b * sx, t->c * sy, t->d * sy, t->tx, t->ty);
}

AffineTransform AffineTransformConcat(const AffineTransform* t1, const AffineTransform* t2)
{
	return AffineTransformMake(
		t1->a * t2->a + t1->b * t2->c, t1->a * t2->b + t1->b * t2->d, //a,b
		t1->c * t2->a + t1->d * t2->c, t1->c * t2->b + t1->d * t2->d, //c,d
		t1->tx * t2->a + t1->ty * t2->c + t2->tx, t1->tx * t2->b + t1->ty * t2->d + t2->ty);    // tx, ty
}

NS_GPU_END