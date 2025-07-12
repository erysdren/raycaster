#ifndef RAYCASTER_TYPES_INCLUDED
#define RAYCASTER_TYPES_INCLUDED

#include <stdint.h>
#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "types/vec2.h"
DECLARE_VEC2_T(float, vec2f)
DECLARE_VEC2_T(uint32_t, vec2u)
DECLARE_VEC2_T(int32_t, vec2i)

#include "types/vec3.h"
DECLARE_VEC3_T(float, vec3f)

/* Helper macros */
#define VEC2U(X, Y) ((vec2u) {X,Y})
#define VEC2I(X, Y) ((vec2i) {X,Y})
#define VEC2F(X, Y) ((vec2f) {X,Y})
#define VEC2F_EQUAL(A, B) (fabsf(A.x-B.x)<=FLT_EPSILON&&fabsf(A.y-B.y)<=FLT_EPSILON)

#define VEC3F(X, Y, Z) ((vec3f) {X,Y,Z})

#endif
