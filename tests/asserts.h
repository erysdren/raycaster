#ifndef RAYCAST_CUSTOM_TYPES_ASSERTS
#define RAYCAST_CUSTOM_TYPES_ASSERTS

M_INLINED void assert_vec2f_equal(const vec2f exp, const vec2f act, unsigned int line) {
  char message[48];
  sprintf(message, "(%.2f, %.2f) != (%.2f, %.2f)", exp.x, exp.y, act.x, act.y);
  UNITY_TEST_ASSERT(VEC2F_EQUAL(exp, act), line, message);
}

#define TEST_ASSERT_EQUAL_VEC2F(expected, actual) assert_vec2f_equal(expected, actual, __LINE__)

#endif
