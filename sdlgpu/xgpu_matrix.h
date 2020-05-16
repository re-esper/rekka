#ifndef _XGPU_MATRIX_H
#define _XGPU_MATRIX_H

#include "xgpu_define.h"

NS_GPU_BEGIN

// Basic vector operations (3D)

/* Returns the magnitude (length) of the given vector. */
float VectorLength(float* vec3);

/* Modifies the given vector so that it has a new length of 1. */
void VectorNormalize(float* vec3);

/* Returns the dot product of two vectors. */
float VectorDot(float* A, float* B);

/* Performs the cross product of vectors A and B (result = A x B).  Do not use A or B as 'result'. */
void VectorCross(float* result, float* A, float* B);

/* Overwrite 'result' vector with the values from vector A. */
void VectorCopy(float* result, float* A);

/* Multiplies the given matrix into the given vector (vec3 = matrix*vec3). */
void VectorApplyMatrix(float* vec3, float* matrix_4x4);

// Basic matrix operations (4x4)

/* Overwrite 'result' matrix with the values from matrix A. */
void MatrixCopy(float* result, const float* A);

/* Fills 'result' matrix with the identity matrix. */
void MatrixIdentity(float* result);

/* Multiplies an orthographic projection matrix into the given matrix. */
void MatrixOrtho(float* result, float left, float right, float bottom, float top, float near, float far);

/* Multiplies a perspective projection matrix into the given matrix. */
void MatrixFrustum(float* result, float left, float right, float bottom, float top, float near, float far);

/* Multiplies a perspective projection matrix into the given matrix. */
void MatrixPerspective(float* result, float fovy, float aspect, float zNear, float zFar);

/* Multiplies a view matrix into the given matrix. */
void MatrixLookAt(float* matrix, float eye_x, float eye_y, float eye_z, float target_x, float target_y, float target_z, float up_x, float up_y, float up_z);

/* Adds a translation into the given matrix. */
void MatrixTranslate(float* result, float x, float y, float z);

/* Multiplies a scaling matrix into the given matrix. */
void MatrixScale(float* result, float sx, float sy, float sz);

/* Multiplies a rotation matrix into the given matrix. */
void MatrixRotate(float* result, float degrees, float x, float y, float z);

/* Multiplies matrices A and B and stores the result in the given 'result' matrix (result = A*B).  Do not use A or B as 'result'.
* \see MultiplyAndAssign
*/
void Multiply4x4(float* result, float* A, float* B);

/* Multiplies matrices 'result' and B and stores the result in the given 'result' matrix (result = result * B). */
void MultiplyAndAssign(float* result, float* B);


// Matrix stack accessors

/* Returns an internal string that represents the contents of matrix A. */
const char* GetMatrixString(float* A);

/* Returns the current matrix from the top of the matrix stack.  Returns NULL if stack is empty. */
float* GetCurrentMatrix(void);

/* Returns the current modelview matrix from the top of the matrix stack.  Returns NULL if stack is empty. */
float* GetModelView(void);

/* Returns the current projection matrix from the top of the matrix stack.  Returns NULL if stack is empty. */
float* GetProjection(void);

/* Copies the current modelview-projection matrix into the given 'result' matrix (result = P*M). */
void GetModelViewProjection(float* result);

NS_GPU_END

#endif
