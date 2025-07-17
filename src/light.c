#include "light.h"
#include "level_data.h"

void light_set_position(light *this, vec3f position) {
  vec3f previous_position = entity_world_position(&this->entity);
  this->entity.position.x = position.x;
  this->entity.position.y = position.y;
  this->entity.z = position.z;
  level_data_update_lights(this->entity.level);
  map_cache_process_light(&this->entity.level->cache, this, previous_position);
}
