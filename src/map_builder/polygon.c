#include "polygon.h"
#include "maths.h"

bool polygon_vertices_contains_point(polygon *this, vec2f point)
{
  register size_t i;
  for (i = 0; i < this->vertices_count; ++i) {
    if (math_length(vec2f_sub(this->vertices[i], point)) < 1) {
      return true;
    }
  }
  return false;
}

bool polygon_is_point_inside(polygon *this, vec2f point)
{
  register int i, wn = 0;
  vec2f v0, v1;

  /* Winding number algorithm */
  for (i = 0; i < this->vertices_count; ++i) {
    v0 = this->vertices[i];
    v1 = this->vertices[(i+1)%this->vertices_count];

    if (v0.y <= point.y) {
      if (v1.y > point.y) {
        if (math_sign(v0, v1, point) > 0) {
          ++wn;
        }
      }
    } else {
      if (v1.y <= point.y) {
        if (math_sign(v0, v1, point) < 0) {
          --wn;
        }
      }
    }
  }

  return wn == 1 || wn == -1;
}

bool polygon_overlaps_polygon(polygon *this, polygon *other)
{
  size_t i;

  for (i = 0; i < other->vertices_count; ++i) {
    if (polygon_vertices_contains_point(this, other->vertices[i])) {
      continue;
    }
    if (polygon_is_point_inside(this, other->vertices[i])) {
      return true;
    }
  }

  return false;
}

void polygon_insert_point(polygon *this, vec2f point, /* between */ vec2f v0, vec2f v1)
{
  register size_t i,i2,j;

  for (i = 0; i < this->vertices_count; ++i) {
    i2 = (i + 1) % this->vertices_count;

    if ((VEC2F_EQUAL(this->vertices[i], v0) && VEC2F_EQUAL(this->vertices[i2], v1)) ||
        (VEC2F_EQUAL(this->vertices[i], v1) && VEC2F_EQUAL(this->vertices[i2], v0))) {
      for (j = this->vertices_count; j > i; --j) {
        this->vertices[j] = this->vertices[j-1];
      }
      
      this->vertices_count ++;
      this->vertices[i+1] = point;
      
      break;
    }
  }
}

float polygon_signed_area(const polygon *this)
{
  size_t i;
  float area = 0.f;
  
  for (i = 0; i < this->vertices_count; ++i) {
    vec2f v0 = this->vertices[i];
    vec2f v1 = this->vertices[(i + 1) % this->vertices_count];
    area += math_cross(v0, v1);
  }

  return area * 0.5;
}
