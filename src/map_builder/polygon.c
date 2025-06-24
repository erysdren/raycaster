#include "polygon.h"
#include "maths.h"
#include <stdio.h>

#define XY(V) (int)V.x, (int)V.y

bool polygon_vertices_contains_point(polygon *this, vec2f point) {
  register size_t i;
  for (i = 0; i < this->vertices_count; ++i) {
    if (math_length(vec2f_sub(this->vertices[i], point)) < 1) {
      return true;
    }
  }
  return false;
}

bool polygon_is_point_inside(polygon *this, vec2f point) {
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

void polygon_insert_point(polygon *this, vec2f point, /* between */ vec2f v0, vec2f v1) {
  register size_t i,i2,j;

  for (i = 0; i < this->vertices_count; ++i) {
    i2 = (i + 1) % this->vertices_count;

    if ((VEC2F_EQUAL(this->vertices[i], v0) && VEC2F_EQUAL(this->vertices[i2], v1)) ||
        (VEC2F_EQUAL(this->vertices[i], v1) && VEC2F_EQUAL(this->vertices[i2], v0))) {
      M_DEBUG(printf("\t\t\tInserting (%d,%d) between (%d,%d) and (%d,%d)\n", XY(point), XY(v0), XY(v1)));
      
      for (j = this->vertices_count; j > i; --j) {
        this->vertices[j] = this->vertices[j-1];
      }
      
      this->vertices_count ++;
      this->vertices[i+1] = point;
      
      break;
    }
  }
}
