#include "unity.h"
#include "fixture.h"
#include "sector.h"

TEST_GROUP(sector);

TEST_SETUP(sector) {}
TEST_TEAR_DOWN(sector) {}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(sector, init) {
#ifdef OLD
  register size_t i;

  /*
   * ┌─────100
   * │       │
   * y       │
   * │       │
   * 0───x───┘
   */
  sector sect;
  sector_init(&sect, 0, 0, LINEDEFS(
    LDEF(.v0.point = vec2f_make(0, 0), .v1.point = vec2f_make(0, 100) ),
    LDEF(.v0.point = vec2f_make(0, 100), .v1.point = vec2f_make(100, 100) ),
    LDEF(.v0.point = vec2f_make(100, 100), .v1.point = vec2f_make(100, 0) ),
    LDEF(.v0.point = vec2f_make(100, 0), .v1.point = vec2f_make(0, 0) )
  ));

  for (i = 0; i < sect.linedefs_count; ++i) {
    TEST_ASSERT_EQUAL_PTR(&sect, sect.linedefs[i].side_sector[LINEDEF_FRONT]);
  }
#endif
}

TEST(sector, point_inside) {
  /*
   * ┌───────┐
   * │       │
   * │       │
   * │       │
   * └───────┘
   */
#ifdef OLD
  sector s0;
  sector_init(&s0, 0, 0, LINEDEFS(
    LDEF(.v0.point = vec2f_make(0, 0), .v1.point = vec2f_make(0, 100) ),
    LDEF(.v0.point = vec2f_make(0, 100), .v1.point = vec2f_make(100, 100) ),
    LDEF(.v0.point = vec2f_make(100, 100), .v1.point = vec2f_make(100, 0) ),
    LDEF(.v0.point = vec2f_make(100, 0), .v1.point = vec2f_make(0, 0) )
  ));

  TEST_ASSERT_TRUE(sector_point_inside(&s0, VEC2F(50, 50)));
  TEST_ASSERT_TRUE(sector_point_inside(&s0, VEC2F(0, 0)));
  TEST_ASSERT_FALSE(sector_point_inside(&s0, VEC2F(-50, -50)));

  /*
   * ┌───┐
   * │   │ Out
   * │   │
   * │   └───┐
   * │ In    │
   * └───────┘
   */
  sector s1;
  sector_init(&s1, 0, 0, LINEDEFS(
    LDEF(.v0.point = vec2f_make(0, 0), .v1.point = vec2f_make(0, 100) ),
    LDEF(.v0.point = vec2f_make(0, 100), .v1.point = vec2f_make(50, 100) ),
    LDEF(.v0.point = vec2f_make(50, 100), .v1.point = vec2f_make(50, 50) ),
    LDEF(.v0.point = vec2f_make(50, 50), .v1.point = vec2f_make(100, 50) ),
    LDEF(.v0.point = vec2f_make(100, 50), .v1.point = vec2f_make(100, 0) ),
    LDEF(.v0.point = vec2f_make(100, 0), .v1.point = vec2f_make(0, 0) )
  ));

  TEST_ASSERT_TRUE(sector_point_inside(&s1, VEC2F(25, 25)));
  TEST_ASSERT_FALSE(sector_point_inside(&s1, VEC2F(75, 75)));
#endif
}

TEST_GROUP_RUNNER(sector) {
  RUN_TEST_CASE(sector, init);
  RUN_TEST_CASE(sector, point_inside);
}
