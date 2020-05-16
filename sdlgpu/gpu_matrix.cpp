#include "xgpu.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.1415926f
#endif

// Column-major
#define INDEX(row,col) ((col)*4 + (row))

NS_GPU_BEGIN

float VectorLength(float* vec3)
{
	return sqrtf(vec3[0] * vec3[0] + vec3[1] * vec3[1] + vec3[2] * vec3[2]);
}

void VectorNormalize(float* vec3)
{
	float mag = VectorLength(vec3);
	vec3[0] /= mag;
	vec3[1] /= mag;
	vec3[2] /= mag;
}

float VectorDot(float* A, float* B)
{
	return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
}

void VectorCross(float* result, float* A, float* B)
{
	result[0] = A[1] * B[2] - A[2] * B[1];
	result[1] = A[2] * B[0] - A[0] * B[2];
	result[2] = A[0] * B[1] - A[1] * B[0];
}

void VectorCopy(float* result, float* A)
{
	result[0] = A[0];
	result[1] = A[1];
	result[2] = A[2];
}

void VectorApplyMatrix(float* vec3, float* matrix_4x4)
{
	float x = matrix_4x4[INDEX(0, 0)] * vec3[0] + matrix_4x4[INDEX(0, 1)] * vec3[1] + matrix_4x4[INDEX(0, 2)] * vec3[2] + matrix_4x4[INDEX(0, 3)];
	float y = matrix_4x4[INDEX(1, 0)] * vec3[0] + matrix_4x4[INDEX(1, 1)] * vec3[1] + matrix_4x4[INDEX(1, 2)] * vec3[2] + matrix_4x4[INDEX(1, 3)];
	float z = matrix_4x4[INDEX(2, 0)] * vec3[0] + matrix_4x4[INDEX(2, 1)] * vec3[1] + matrix_4x4[INDEX(2, 2)] * vec3[2] + matrix_4x4[INDEX(2, 3)];
	float w = matrix_4x4[INDEX(3, 0)] * vec3[0] + matrix_4x4[INDEX(3, 1)] * vec3[1] + matrix_4x4[INDEX(3, 2)] * vec3[2] + matrix_4x4[INDEX(3, 3)];
	vec3[0] = x / w;
	vec3[1] = y / w;
	vec3[2] = z / w;
}


// Matrix math implementations based on Wayne Cochran's (wcochran) matrix.c
#define FILL_MATRIX_4x4(A, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
	A[0] = a0; \
	A[1] = a1; \
	A[2] = a2; \
	A[3] = a3; \
	A[4] = a4; \
	A[5] = a5; \
	A[6] = a6; \
	A[7] = a7; \
	A[8] = a8; \
	A[9] = a9; \
	A[10] = a10; \
	A[11] = a11; \
	A[12] = a12; \
	A[13] = a13; \
	A[14] = a14; \
	A[15] = a15;

void MatrixCopy(float* result, const float* A)
{
    memcpy(result, A, 16*sizeof(float));
}

void MatrixIdentity(float* result)
{
    memset(result, 0, 16*sizeof(float));
    result[0] = result[5] = result[10] = result[15] = 1;
}

void MatrixOrtho(float* result, float left, float right, float bottom, float top, float near, float far)
{
    if(result == NULL)
		return;
	float A[16];
	FILL_MATRIX_4x4(A,
		2 / (right - left), 0, 0, 0,
		0, 2 / (top - bottom), 0, 0,
		0, 0, -2 / (far - near), 0,
		-(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1
	);
	MultiplyAndAssign(result, A);
}


void MatrixFrustum(float* result, float left, float right, float bottom, float top, float near, float far)
{
    if(result == NULL)
		return;

	float A[16];
	FILL_MATRIX_4x4(A,
		2 * near / (right - left), 0, 0, 0,
		0, 2 * near / (top - bottom), 0, 0,
		(right + left) / (right - left), (top + bottom) / (top - bottom), -(far + near) / (far - near), -1,
		0, 0, -(2 * far * near) / (far - near), 0
	);

	MultiplyAndAssign(result, A);
}

void MatrixPerspective(float* result, float fovy, float aspect, float zNear, float zFar)
{
	float fW, fH;
    
    // Make it left-handed?
    fovy = -fovy;
    aspect = -aspect;
    
	fH = tanf(fovy / 360 * M_PI) * zNear;
	fW = fH * aspect;
	MatrixFrustum(result, -fW, fW, -fH, fH, zNear, zFar);
}

void MatrixLookAt(float* matrix, float eye_x, float eye_y, float eye_z, float target_x, float target_y, float target_z, float up_x, float up_y, float up_z)
{
	float forward[3] = {target_x - eye_x, target_y - eye_y, target_z - eye_z};
	float up[3] = {up_x, up_y, up_z};
	float side[3];
	float view[16];

	VectorNormalize(forward);
	VectorNormalize(up);

	// Calculate sideways vector
	VectorCross(side, forward, up);

	// Calculate new up vector
	VectorCross(up, side, forward);

	// Set up view matrix
	view[0] = side[0];
	view[4] = side[1];
	view[8] = side[2];
	view[12] = 0.0f;

	view[1] = up[0];
	view[5] = up[1];
	view[9] = up[2];
	view[13] = 0.0f;

	view[2] = -forward[0];
	view[6] = -forward[1];
	view[10] = -forward[2];
	view[14] = 0.0f;

	view[3] = view[7] = view[11] = 0.0f;
	view[15] = 1.0f;

	MultiplyAndAssign(matrix, view);
	MatrixTranslate(matrix, -eye_x, -eye_y, -eye_z);
}

void MatrixTranslate(float* result, float x, float y, float z)
{
    if(result == NULL)
		return;
	float A[16];
	FILL_MATRIX_4x4(A,
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		x, y, z, 1
	);
	MultiplyAndAssign(result, A);
}

void MatrixScale(float* result, float sx, float sy, float sz)
{
    if(result == NULL)
		return;

	float A[16];
	FILL_MATRIX_4x4(A,
		sx, 0, 0, 0,
		0, sy, 0, 0,
		0, 0, sz, 0,
		0, 0, 0, 1
	);
	MultiplyAndAssign(result, A);
}

void MatrixRotate(float* result, float degrees, float x, float y, float z)
{
	float p, radians, c, s, c_, zc_, yc_, xzc_, xyc_, yzc_, xs, ys, zs;

    if(result == NULL)
		return;

    p = 1/sqrtf(x*x + y*y + z*z);
    x *= p; y *= p; z *= p;
    radians = degrees * (M_PI/180);
    c = cosf(radians);
    s = sinf(radians);
    c_ = 1 - c;
    zc_ = z*c_;
    yc_ = y*c_;
    xzc_ = x*zc_;
    xyc_ = x*y*c_;
    yzc_ = y*zc_;
    xs = x*s;
    ys = y*s;
    zs = z*s;

	float A[16];
	FILL_MATRIX_4x4(A,
		x*x*c_ + c, xyc_ + zs, xzc_ - ys, 0,
		xyc_ - zs, y*yc_ + c, yzc_ + xs, 0,
		xzc_ + ys, yzc_ - xs, z*zc_ + c, 0,
		0, 0, 0, 1
	);
	MultiplyAndAssign(result, A);
}

// Matrix multiply: result = A * B
void Multiply4x4(float* result, float* A, float* B)
{
    float (*matR)[4] = (float(*)[4])result;
    float (*matA)[4] = (float(*)[4])A;
    float (*matB)[4] = (float(*)[4])B;
    matR[0][0] = matB[0][0] * matA[0][0] + matB[0][1] * matA[1][0] + matB[0][2] * matA[2][0] + matB[0][3] * matA[3][0]; 
    matR[0][1] = matB[0][0] * matA[0][1] + matB[0][1] * matA[1][1] + matB[0][2] * matA[2][1] + matB[0][3] * matA[3][1]; 
    matR[0][2] = matB[0][0] * matA[0][2] + matB[0][1] * matA[1][2] + matB[0][2] * matA[2][2] + matB[0][3] * matA[3][2]; 
    matR[0][3] = matB[0][0] * matA[0][3] + matB[0][1] * matA[1][3] + matB[0][2] * matA[2][3] + matB[0][3] * matA[3][3]; 
    matR[1][0] = matB[1][0] * matA[0][0] + matB[1][1] * matA[1][0] + matB[1][2] * matA[2][0] + matB[1][3] * matA[3][0]; 
    matR[1][1] = matB[1][0] * matA[0][1] + matB[1][1] * matA[1][1] + matB[1][2] * matA[2][1] + matB[1][3] * matA[3][1]; 
    matR[1][2] = matB[1][0] * matA[0][2] + matB[1][1] * matA[1][2] + matB[1][2] * matA[2][2] + matB[1][3] * matA[3][2]; 
    matR[1][3] = matB[1][0] * matA[0][3] + matB[1][1] * matA[1][3] + matB[1][2] * matA[2][3] + matB[1][3] * matA[3][3]; 
    matR[2][0] = matB[2][0] * matA[0][0] + matB[2][1] * matA[1][0] + matB[2][2] * matA[2][0] + matB[2][3] * matA[3][0]; 
    matR[2][1] = matB[2][0] * matA[0][1] + matB[2][1] * matA[1][1] + matB[2][2] * matA[2][1] + matB[2][3] * matA[3][1]; 
    matR[2][2] = matB[2][0] * matA[0][2] + matB[2][1] * matA[1][2] + matB[2][2] * matA[2][2] + matB[2][3] * matA[3][2]; 
    matR[2][3] = matB[2][0] * matA[0][3] + matB[2][1] * matA[1][3] + matB[2][2] * matA[2][3] + matB[2][3] * matA[3][3]; 
    matR[3][0] = matB[3][0] * matA[0][0] + matB[3][1] * matA[1][0] + matB[3][2] * matA[2][0] + matB[3][3] * matA[3][0]; 
    matR[3][1] = matB[3][0] * matA[0][1] + matB[3][1] * matA[1][1] + matB[3][2] * matA[2][1] + matB[3][3] * matA[3][1]; 
    matR[3][2] = matB[3][0] * matA[0][2] + matB[3][1] * matA[1][2] + matB[3][2] * matA[2][2] + matB[3][3] * matA[3][2]; 
    matR[3][3] = matB[3][0] * matA[0][3] + matB[3][1] * matA[1][3] + matB[3][2] * matA[2][3] + matB[3][3] * matA[3][3];
}

void MultiplyAndAssign(float* result, float* B)
{
    float temp[16];
    Multiply4x4(temp, result, B);
    MatrixCopy(result, temp);
}

// Can be used up to two times per line evaluation...
const char* GetMatrixString(float* A)
{
    static char buffer[512];
    static char buffer2[512];
    static char flip = 0;
    
    char* b = (flip? buffer : buffer2);
    flip = !flip;
    
    snprintf(b, 512, "%.1f %.1f %.1f %.1f\n"
                          "%.1f %.1f %.1f %.1f\n"
                          "%.1f %.1f %.1f %.1f\n"
                          "%.1f %.1f %.1f %.1f", 
                          A[0], A[1], A[2], A[3], 
                          A[4], A[5], A[6], A[7], 
                          A[8], A[9], A[10], A[11], 
                          A[12], A[13], A[14], A[15]);
    return b;
}

float* GetModelView(void)
{
    Target* target = GetContextTarget();
	MatrixStack* stack;

    if(target == NULL || target->context == NULL)
        return NULL;
    stack = &target->context->modelview_matrix;
    if(stack->size == 0)
        return NULL;
    return stack->matrix[stack->size-1];
}

float* GetProjection(void)
{
    Target* target = GetContextTarget();
	MatrixStack* stack;

    if(target == NULL || target->context == NULL)
        return NULL;
    stack = &target->context->projection_matrix;
    if(stack->size == 0)
        return NULL;
    return stack->matrix[stack->size-1];
}

float* GetCurrentMatrix(void)
{
    Target* target = GetContextTarget();
    MatrixStack* stack;

    if(target == NULL || target->context == NULL)
        return NULL;
    if(target->context->matrix_mode == MODELVIEW)
        stack = &target->context->modelview_matrix;
    else
        stack = &target->context->projection_matrix;
    
    if(stack->size == 0)
        return NULL;
    return stack->matrix[stack->size-1];
}

void GetModelViewProjection(float* result)
{
    // MVP = P * MV
    Multiply4x4(result, GetProjection(), GetModelView());
}

NS_GPU_END