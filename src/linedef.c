#include "linedef.h"
#include "sector.h"

void
linedef_update_floor_ceiling_limits(linedef *this)
{
  this->max_floor_height = M_MAX(
    this->side_sector[0]->floor_height,
    this->side_sector[1] ? this->side_sector[1]->floor_height : 0
  );
  this->min_ceiling_height = M_MIN(
    this->side_sector[0]->ceiling_height,
    this->side_sector[1] ? this->side_sector[1]->ceiling_height : 0
  );
}
