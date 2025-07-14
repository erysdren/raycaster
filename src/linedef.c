#include "linedef.h"
#include "sector.h"

void
linedef_update_floor_ceiling_limits(linedef *this)
{
  this->max_floor_height = M_MAX(
    this->side[0].sector->floor.height,
    this->side[1].sector ? this->side[1].sector->floor.height : 0
  );
  this->min_ceiling_height = M_MIN(
    this->side[0].sector->ceiling.height,
    this->side[1].sector ? this->side[1].sector->ceiling.height : 0
  );
}
