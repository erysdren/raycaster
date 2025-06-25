#include "unity.h"
#include "fixture.h"
#include "polygon.h"
#include "asserts.h"

TEST_GROUP(polygon);

TEST_SETUP(polygon) {}
TEST_TEAR_DOWN(polygon) {}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(polygon, vertices_contains_point) {
  polygon poly = {
    .vertices_count = 4,
    .vertices = {
      VEC2F(0, 0),
      VEC2F(0, 100),
      VEC2F(100, 100),
      VEC2F(100, 0)
    }
  };

  TEST_ASSERT_TRUE(polygon_vertices_contains_point(&poly, VEC2F(100, 100)));
  TEST_ASSERT_FALSE(polygon_vertices_contains_point(&poly, VEC2F(101, 102)));
}

TEST(polygon, is_point_inside) {
  polygon poly = {
    .vertices_count = 4,
    .vertices = {
      VEC2F(0, 0),
      VEC2F(0, 100),
      VEC2F(100, 100),
      VEC2F(100, 0)
    }
  };

  TEST_ASSERT_TRUE(polygon_is_point_inside(&poly, VEC2F(50, 50)));
  TEST_ASSERT_TRUE(polygon_is_point_inside(&poly, VEC2F(0, 0)));
  TEST_ASSERT_FALSE(polygon_is_point_inside(&poly, VEC2F(100, 100))); /* TODO: Should this be inside? */
  TEST_ASSERT_FALSE(polygon_is_point_inside(&poly, VEC2F(-1, -1)));
}

TEST(polygon, insert_point) {
  polygon poly = {
    .vertices_count = 3,
    .vertices = {
      VEC2F(0, 0),
      VEC2F(100, 0),
      VEC2F(50, 100)
    }
  };

  polygon_insert_point(&poly, VEC2F(100, 100), VEC2F(100, 0), VEC2F(50, 100));

  TEST_ASSERT_EQUAL_INT(4, poly.vertices_count);
  TEST_ASSERT_EQUAL_VEC2F(VEC2F(100, 0), poly.vertices[1]);
  TEST_ASSERT_EQUAL_VEC2F(VEC2F(100, 100), poly.vertices[2]);
  TEST_ASSERT_EQUAL_VEC2F(VEC2F(50, 100), poly.vertices[3]);
}

TEST(polygon, overlaps_polygon) {
  polygon poly0 = {
    .vertices_count = 4,
    .vertices = {
      VEC2F(0, 0),
      VEC2F(100, 0),
      VEC2F(100, 100),
      VEC2F(0, 100)
    }
  };

  polygon poly1 = {
    .vertices_count = 3,
    .vertices = {
      VEC2F(0, 0),
      VEC2F(50, 50),
      VEC2F(0, 100)
    }
  };

  polygon poly2 = {
    .vertices_count = 4,
    .vertices = {
      VEC2F(100, 0),
      VEC2F(200, 0),
      VEC2F(200, 100),
      VEC2F(100, 100)
    }
  };

  TEST_ASSERT_TRUE(polygon_overlaps_polygon(&poly0, &poly1));
  TEST_ASSERT_FALSE(polygon_overlaps_polygon(&poly0, &poly2));
}

TEST(polygon, signed_area) {
  /* Anti-clockwise winding */
  polygon poly0 = {
    .vertices_count = 4,
    .vertices = {
      VEC2F(0, 0),
      VEC2F(100, 0),
      VEC2F(100, 100),
      VEC2F(0, 100)
    }
  };

  /* Clockwise winding */
  polygon poly1 = {
    .vertices_count = 4,
    .vertices = {
      VEC2F(0, 100),
      VEC2F(100, 100),
      VEC2F(100, 0),
      VEC2F(0, 0)
    }
  };

  float a0 = polygon_signed_area(&poly0);
  float a1 = polygon_signed_area(&poly1);

  TEST_ASSERT_EQUAL_FLOAT(10000.f, a0);
  TEST_ASSERT_EQUAL_FLOAT(-10000.f, a1);
  TEST_ASSERT_TRUE(POLYGON_CLOCKWISE_WINDING(&poly1));
}

TEST_GROUP_RUNNER(polygon) {
  RUN_TEST_CASE(polygon, vertices_contains_point);
  RUN_TEST_CASE(polygon, is_point_inside);
  RUN_TEST_CASE(polygon, insert_point);
  RUN_TEST_CASE(polygon, overlaps_polygon);
  RUN_TEST_CASE(polygon, signed_area);
}
