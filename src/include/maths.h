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

M_INLINED bool math_point_in_triangle(vec2f point, vec2f v0, vec2f v1, vec2f v2) {
  register float d0 = math_sign(point, v0, v1), d1 = math_sign(point, v1, v2), d2 = math_sign(point, v2, v0);
  return !(((d0 < 0) || (d1 < 0) || (d2 < 0)) && ((d0 > 0) || (d1 > 0) || (d2 > 0)));
}

M_INLINED float math_line_segment_point_distance(vec2f a, vec2f b, vec2f point) {
  return fabs(math_cross(vec2f_sub(b, a), vec2f_sub(a, point))) / math_length(vec2f_sub(b, a));
}


// Helper: orientation of triplet (p, q, r)
// Returns: 0 = colinear, 1 = clockwise, 2 = counterclockwise
M_INLINED int orientation(vec2f p, vec2f q, vec2f r) {
    float val = (q.y - p.y) * (r.x - q.x) -
                (q.x - p.x) * (r.y - q.y);
    if (val == 0) return 0;              // colinear
    return (val > 0) ? 1 : 2;            // clockwise or counterclockwise
}

// Helper: check if point r lies on segment pq
M_INLINED int onSegment(vec2f p, vec2f q, vec2f r) {
    return r.x <= fmax(p.x, q.x) && r.x >= fmin(p.x, q.x) &&
           r.y <= fmax(p.y, q.y) && r.y >= fmin(p.y, q.y);
}

// Main function: checks if segments p1p2 and q1q2 intersect
M_INLINED int segmentsIntersect(vec2f p1, vec2f p2, vec2f q1, vec2f q2) {
    int o1 = orientation(p1, p2, q1);
    int o2 = orientation(p1, p2, q2);
    int o3 = orientation(q1, q2, p1);
    int o4 = orientation(q1, q2, p2);

    // General case
    if (o1 != o2 && o3 != o4)
        return 1;

    // Special cases (colinear)
    if (o1 == 0 && onSegment(p1, p2, q1)) return 1;
    if (o2 == 0 && onSegment(p1, p2, q2)) return 1;
    if (o3 == 0 && onSegment(q1, q2, p1)) return 1;
    if (o4 == 0 && onSegment(q1, q2, p2)) return 1;

    return 0; // No intersection
}


#endif
