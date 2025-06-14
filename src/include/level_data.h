#ifndef RAYCAST_LEVEL_DATA_INCLUDED
#define RAYCAST_LEVEL_DATA_INCLUDED

#include "sector.h"

typedef struct {
  size_t sectors_count;
  sector sectors[];
} level_data;

#endif
