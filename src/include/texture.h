#ifndef RAYCASTER_TEXTURE_INCLUDED
#define RAYCASTER_TEXTURE_INCLUDED

/* You may define your own type or reference */
typedef int32_t texture_ref;

/* Some value for your type to identify a no-texture */
#define TEXTURE_NONE -1

extern bool (*texture_sampler)(texture_ref, int32_t, int32_t, uint8_t, uint8_t*);

M_INLINED bool
debug_texture_sampler(texture_ref texture, int32_t x, int32_t y, uint8_t mip_level, uint8_t *rgb)
{
  rgb[0] = (x & 127);
  rgb[2] = (y & 127);
  return true;
}

#endif
