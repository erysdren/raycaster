#include "sector.h"

bool
sector_references_vertex(sector *this, vertex *v, size_t linedefs_count)
{
  register size_t i;
  for (i = 0; i < (linedefs_count ? linedefs_count : this->linedefs_count); ++i) {
    if (this->linedefs[i]->v0 == v || this->linedefs[i]->v1 == v) {
      return true;
    }
  }
  return false;
}

bool
sector_connects_vertices(sector *this, vertex *v0, vertex *v1)
{
  register size_t i;
  for (i = 0; i < this->linedefs_count; ++i) {
    if ((this->linedefs[i]->v0 == v0 && this->linedefs[i]->v1 == v1) || (this->linedefs[i]->v0 == v1 && this->linedefs[i]->v1 == v0)) {
      return true;
    }
  }
  return false;
}

linedef*
sector_add_linedef(sector *sect, linedef *line)
{
  if (sect->linedefs_count) {
    sect->linedefs = realloc(sect->linedefs, sizeof(linedef*) * (sect->linedefs_count+1));
  } else {
    sect->linedefs = malloc(sizeof(linedef*));
  }
  sect->linedefs[sect->linedefs_count++] = line;
  return line;
}

void
sector_remove_linedef(sector *this, linedef *line)
{
  register size_t i,j;
  for (i = 0; i < this->linedefs_count; ++i) {
    if (this->linedefs[i] == line) {
      this->linedefs_count--;
      for (j = i; j < this->linedefs_count; ++j) {
        this->linedefs[j] = this->linedefs[j+1];
      }
      this->linedefs = realloc(this->linedefs, sizeof(linedef*) * this->linedefs_count);
      if (line->side[0].sector == this) { line->side[0].sector = NULL; }
      else if (line->side[1].sector == this) { line->side[1].sector = NULL; }
      return;
    }
  }
}

void
sector_update_floor_ceiling_limits(sector *this)
{
  size_t li;
  for (li = 0; li < this->linedefs_count; ++li) {
    linedef_update_floor_ceiling_limits(this->linedefs[li]);
  }
}
