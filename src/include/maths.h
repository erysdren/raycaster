#ifndef RAYCAST_MATH_INCLUDED
#define RAYCAST_MATH_INCLUDED

#include "macros.h"
#include "types.h"

M_INLINED float math_cross(vec2f a, vec2f b) {
  return (a.x * b.y) - (a.y * b.x);
}

M_INLINED float math_dot(vec2f v) {
  return (v.x * v.x + v.y * v.y);
}

M_INLINED float math_length(vec2f v) {
  return sqrtf(math_dot(v));
}

M_INLINED int32_t math_sign(vec2f p0, vec2f p1, vec2f point) {
  /* > 0 = is left of the line*/
  return (int32_t)((p1.x - p0.x) * (point.y - p0.y) - (point.x - p0.x) * (p1.y - p0.y));
}

M_INLINED bool math_lines_intersect(
  vec2f A,
  vec2f B,
  vec2f C,
  vec2f D,
  vec2f *r,
  float *determinant
) {
  register const vec2f BA = vec2f_sub(B, A);
  register const vec2f DC = vec2f_sub(D, C);
  register const vec2f AC = vec2f_sub(A, C);
  register float denom = 1.f / (DC.y*BA.x - DC.x*BA.y);
  register float uA, uB;

  // calculate the direction of the lines
  uB = (BA.x*AC.y - BA.y*AC.x) * denom;

  if (uB < 0.f || uB > 1.f) {
    return false;
  }

  uA = (DC.x*AC.y - DC.y*AC.x) * denom;

  if (uA < 0.f || uA > 1.f) {
    return false;
  }

  // if uA and uB are between [0...1], lines are colliding
  if (r) {
    (*r).x = A.x + (uA * BA.x);
    (*r).y = A.y + (uA * BA.y);
  }

  if (determinant) {
    *determinant = uA;
  }

  return true;
}

M_INLINED float math_line_segment_point_distance(vec2f a, vec2f b, vec2f point) {
  return fabs(math_cross(vec2f_sub(b, a), vec2f_sub(a, point))) / math_length(vec2f_sub(b, a));
}

#endif
