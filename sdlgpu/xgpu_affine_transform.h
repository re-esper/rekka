#ifndef _XGPU_AFFINE_TRANSFORM_H
#define _XGPU_AFFINE_TRANSFORM_H

#include "xgpu_define.h"

NS_GPU_BEGIN

AffineTransform AffineTransformMake(float a, float b, float c, float d, float tx, float ty);
GPU_Point PointApplyAffineTransform(float x, float y, const AffineTransform* t);
GPU_Point SizeApplyAffineTransform(float w, float h, const AffineTransform* t);
AffineTransform AffineTransformMakeIdentity();

AffineTransform AffineTransformTranslate(const AffineTransform* t, float tx, float ty);
AffineTransform AffineTransformRotate(const AffineTransform* t, float anAngle);
AffineTransform AffineTransformScale(const AffineTransform* t, float sx, float sy);
AffineTransform AffineTransformConcat(const AffineTransform* t1, const AffineTransform* t2);

NS_GPU_END

#endif
