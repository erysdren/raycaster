#include "level_data.h"
#include "polygon.h"

#define XY(V) (int)V.x, (int)V.y

/* Just for debbuging for now */
static uint32_t linedef_color = 0, sector_color = 0;

/* FIND a vertex at given point OR CREATE a new one */
vertex* level_data_get_vertex(level_data *this, vec2f point)
{
  register size_t i;

  for (i = 0; i < this->vertices_count; ++i) {
    if (math_length(vec2f_sub(this->vertices[i].point, point)) < 1) {
      return &this->vertices[i];
    }
  }

  this->vertices[this->vertices_count] = (vertex) {
    .point = point
  };

  return &this->vertices[this->vertices_count++];
}

/* FIND a linedef with given vertices OR CREATE a new one */
linedef* level_data_get_linedef(level_data *this, sector *sect, vertex *v0, vertex *v1)
{
  register size_t i;
  linedef *line;

  /* Check for existing linedef with these vertices */
  for (i = 0; i < this->linedefs_count; ++i) {
    line = &this->linedefs[i];

    if ((line->v0 == v0 && line->v1 == v1) || (line->v0 == v1 && line->v1 == v0)) {
      line->side_sector[1] = sect;
      M_DEBUG(printf("\t\tRe-use linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d)\n", line, XY(v0->point), XY(v1->point), line->color));
      return line;
    }
  }

  this->linedefs[this->linedefs_count] = (linedef) {
    .v0 = v0,
    .v1 = v1,
    .side_sector[0] = sect,
    .side_sector[1] = NULL,
    .color = linedef_color++
  };

  M_DEBUG(printf("\t\tNew linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d)\n", &this->linedefs[this->linedefs_count], XY(v0->point), XY(v1->point), linedef_color-1));

  return &this->linedefs[this->linedefs_count++];
}

sector* level_data_create_sector_from_polygon(level_data *this, polygon *poly)
{
  register size_t i;

  sector *sect = &this->sectors[this->sectors_count++];

  M_DEBUG(printf("\tNew sector (0x%p):\n", sect));

  sect->floor_height = poly->floor_height;
  sect->ceiling_height = poly->ceiling_height;
  sect->light = poly->light;
  sect->color = sector_color++;
  sect->linedefs = NULL;
  sect->linedefs_count = 0;

  for (i = 0; i < poly->vertices_count; ++i) {
    sector_add_linedef(
      sect,
      level_data_get_linedef(
        this,
        sect,
        level_data_get_vertex(this, poly->vertices[i]),
        level_data_get_vertex(this, poly->vertices[(i+1)%poly->vertices_count])
      )
    );
  }

  return sect;
}
