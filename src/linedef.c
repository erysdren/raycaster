#include "linedef.h"
#include "sector.h"

void
linedef_update_floor_ceiling_limits(linedef *this)
{
  this->max_floor_height = M_MAX(
    this->side[0].sector->floor_height,
    this->side[1].sector ? this->side[1].sector->floor_height : 0
  );
  this->min_ceiling_height = M_MIN(
    this->side[0].sector->ceiling_height,
    this->side[1].sector ? this->side[1].sector->ceiling_height : 0
  );
}
