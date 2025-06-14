#ifndef RAYCASTER_TYPES_INCLUDED
#define RAYCASTER_TYPES_INCLUDED

#include <stdint.h>

#include "types/vec2.h"
DECLARE_VEC2_T(float, vec2f)
DECLARE_VEC2_T(uint32_t, vec2u)
DECLARE_VEC2_T(int32_t, vec2i)

/* Helper macros */
#define VEC2F(X, Y) (vec2f) {X,Y}
#define VEC2U(X, Y) (vec2u) {X,Y}
#define VEC2I(X, Y) (vec2i) {X,Y}

#endif
