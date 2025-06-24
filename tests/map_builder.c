#include "unity.h"
#include "fixture.h"
#include "map_builder.h"
#include "level_data.h"

TEST_GROUP(map_builder);

TEST_SETUP(map_builder) {}
TEST_TEAR_DOWN(map_builder) {}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(map_builder, convex_polygon) {
  int i;
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  TEST_ASSERT_EQUAL_INT(0, builder.polygons[0].floor_height);
  TEST_ASSERT_EQUAL_INT(128, builder.polygons[0].ceiling_height);
  TEST_ASSERT_EQUAL_DOUBLE(1, builder.polygons[0].light);

  TEST_ASSERT_EQUAL_PTR(&builder.polygons[0], map_builder_polygon_at_point(&builder, VEC2F(50, 75)));
  TEST_ASSERT_NULL(map_builder_polygon_at_point(&builder, VEC2F(-10, -10)));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(4, level->vertices_count);
  TEST_ASSERT_EQUAL(4, level->linedefs_count);
  TEST_ASSERT_EQUAL(1, level->sectors_count);

  for (i = 0; i < level->linedefs_count; ++i) {
    TEST_ASSERT_NOT_NULL(level->sectors[0].linedefs[i]->side_sector[0]); /* Front */
    TEST_ASSERT_NULL(level->sectors[0].linedefs[i]->side_sector[1]); /* Back */
  }

  TEST_ASSERT_TRUE(sector_point_inside(&level->sectors[0], VEC2F(50, 75)));
  TEST_ASSERT_FALSE(sector_point_inside(&level->sectors[0], VEC2F(-10, -10)));

  free(level);
}

TEST(map_builder, concave_polygon) {
  int i;
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 0.5, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(50, 50),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  TEST_ASSERT_NULL(map_builder_polygon_at_point(&builder, VEC2F(50, 75)));
  TEST_ASSERT_EQUAL_PTR(&builder.polygons[0], map_builder_polygon_at_point(&builder, VEC2F(10, 10)));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(5, level->vertices_count);
  TEST_ASSERT_EQUAL(5, level->linedefs_count);
  TEST_ASSERT_EQUAL(1, level->sectors_count);

  TEST_ASSERT_EQUAL_INT(0, level->sectors[0].floor_height);
  TEST_ASSERT_EQUAL_INT(128, level->sectors[0].ceiling_height);
  TEST_ASSERT_EQUAL_DOUBLE(0.5, level->sectors[0].light);

  for (i = 0; i < level->linedefs_count; ++i) {
    TEST_ASSERT_NOT_NULL(level->sectors[0].linedefs[i]->side_sector[0]); /* Front */
    TEST_ASSERT_NULL(level->sectors[0].linedefs[i]->side_sector[1]); /* Back */
  }

  TEST_ASSERT_FALSE(sector_point_inside(&level->sectors[0], VEC2F(50, 75)));
  TEST_ASSERT_TRUE(sector_point_inside(&level->sectors[0], VEC2F(10, 10)));

  free(level);
}

/*
 * Non-overlapping sectors can connect simply by sharing a linedef.
 * The front side references the original sector; the back references the other.
 */
TEST(map_builder, neighbouring_sectors) {
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 100, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  map_builder_add_polygon(&builder, 10, 90, 1, VERTICES(
    VEC2F(100, 0),
    VEC2F(100, 100),
    VEC2F(200, 100),
    VEC2F(200, 0)
  ));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(6, level->vertices_count);
  TEST_ASSERT_EQUAL(7, level->linedefs_count);
  TEST_ASSERT_EQUAL(2, level->sectors_count);

  /* Same linedef is referenced by both sectors */
  TEST_ASSERT_EQUAL_PTR(&level->sectors[0], level->sectors[0].linedefs[2]->side_sector[0]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[1], level->sectors[1].linedefs[0]->side_sector[1]);
  TEST_ASSERT_EQUAL_PTR(level->sectors[0].linedefs[2], level->sectors[1].linedefs[0]);

  free(level);
}

TEST_GROUP_RUNNER(map_builder) {
  RUN_TEST_CASE(map_builder, convex_polygon);
  RUN_TEST_CASE(map_builder, concave_polygon);
  RUN_TEST_CASE(map_builder, neighbouring_sectors);
}
