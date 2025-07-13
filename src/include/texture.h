#ifndef RAYCASTER_TEXTURE_INCLUDED
#define RAYCASTER_TEXTURE_INCLUDED

#include "macros.h"

/* You may define your own type or reference */
typedef int32_t texture_ref;

/* Some value for your type to identify a no-texture */
#define TEXTURE_NONE -1

typedef void (*texture_coordinates_func)(float, float, int, int, int32_t*, int32_t*);

extern void (*texture_sampler)(texture_ref, float, float, texture_coordinates_func, uint8_t, uint8_t*);

M_INLINED void
debug_texture_sampler(texture_ref texture, float x, float y, texture_coordinates_func coords, uint8_t mip_level, uint8_t *rgb)
{
  rgb[0] = (int32_t)floorf(x) & 127;
  rgb[1] = (int32_t)floorf(y) & 127;
  rgb[2] = (int32_t)floorf(y) & 127;
}

M_INLINED void
texture_coordinates_scaled(float fx, float fy, int w, int h, int32_t *x, int32_t *y) {
  *x = (int32_t)floorf(fx) & (w-1); // / mip_level) * mip_level;
  *y = (int32_t)floorf(fy) & (h-1); // / mip_level) * mip_level;
}

M_INLINED void
texture_coordinates_normalized(float fx, float fy, int w, int h, int32_t *x, int32_t *y) {
  *x = (int32_t)(fx * (w-1)); // / mip_level) * mip_level;
  *y = (int32_t)(fy * (h-1)); // / mip_level) * mip_level;
}

#endif
