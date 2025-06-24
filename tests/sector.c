#include "unity.h"
#include "fixture.h"
#include "sector.h"
#include "asserts.h"

#define LINE(...) ((linedef) { __VA_ARGS__ })

TEST_GROUP(sector);

TEST_SETUP(sector) {}
TEST_TEAR_DOWN(sector) {}

static sector* create_test_sector(vertex**, linedef**);

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(sector, references_vertex) {
  vertex *vertices;
  sector *sect = create_test_sector(&vertices, NULL);

  TEST_ASSERT_TRUE(sector_references_vertex(sect, &vertices[0], 0));
  TEST_ASSERT_FALSE(sector_references_vertex(sect, &vertices[4], 0));
}

TEST(sector, connects_vertices) {
  vertex *vertices;
  sector *sect = create_test_sector(&vertices, NULL);

  TEST_ASSERT_TRUE(sector_connects_vertices(sect, &vertices[0], &vertices[1]));
  TEST_ASSERT_FALSE(sector_connects_vertices(sect, &vertices[1], &vertices[1]));
  TEST_ASSERT_FALSE(sector_connects_vertices(sect, &vertices[4], &vertices[3]));
}

TEST(sector, add_linedef) {
  vertex *vertices;
  linedef *linedefs;
  sector *sect = create_test_sector(&vertices, &linedefs);

  TEST_ASSERT_EQUAL_INT(4, sect->linedefs_count);

  sector_add_linedef(sect, &linedefs[4]);

  TEST_ASSERT_EQUAL_INT(5, sect->linedefs_count);
  TEST_ASSERT_TRUE(sector_references_vertex(sect, &vertices[4], 0));
}

TEST(sector, remove_linedef) {
  vertex *vertices;
  linedef *linedefs;
  sector *sect = create_test_sector(&vertices, &linedefs);

  TEST_ASSERT_EQUAL_INT(4, sect->linedefs_count);

  sector_remove_linedef(sect, &linedefs[3]);

  TEST_ASSERT_EQUAL_INT(3, sect->linedefs_count);
  TEST_ASSERT_FALSE(sector_connects_vertices(sect, &vertices[0], &vertices[3]));
}

TEST(sector, point_inside) {
  sector *sect = create_test_sector(NULL, NULL);

  TEST_ASSERT_TRUE(sector_point_inside(sect, VEC2F(75, 50)));
  TEST_ASSERT_FALSE(sector_point_inside(sect, VEC2F(10, 50)));
}

TEST_GROUP_RUNNER(sector) {
  RUN_TEST_CASE(sector, references_vertex);
  RUN_TEST_CASE(sector, connects_vertices);
  RUN_TEST_CASE(sector, add_linedef);
  RUN_TEST_CASE(sector, remove_linedef);
  RUN_TEST_CASE(sector, point_inside);
}

static sector* create_test_sector(vertex **vertices, linedef **linedefs) {
  vertex *verts = (vertex *)malloc(5 * sizeof(vertex));
  linedef *lines = (linedef *)malloc(5 * sizeof(linedef));
  sector *sect = (sector *)malloc(sizeof(sector));

  verts[0] = (vertex) { VEC2F(0, 0) };
  verts[1] = (vertex) { VEC2F(100, 0) };
  verts[2] = (vertex) { VEC2F(100, 100) };
  verts[3] = (vertex) { VEC2F(50, 100) };
  verts[4] = (vertex) { VEC2F(0, 100) };

  lines[0] = LINE(&verts[0], &verts[1]);
  lines[1] = LINE(&verts[1], &verts[2]);
  lines[2] = LINE(&verts[2], &verts[3]);
  lines[3] = LINE(&verts[3], &verts[0]);
  lines[4] = LINE(&verts[4], &verts[0]);

  sect->linedefs_count = 4;
  sect->linedefs = malloc(4 * sizeof(linedef*));

  sect->linedefs[0] = &lines[0];
  sect->linedefs[1] = &lines[1];
  sect->linedefs[2] = &lines[2];
  sect->linedefs[3] = &lines[3];

  if (vertices) {
    *vertices = verts;
  }

  if (linedefs) {
    *linedefs = lines;
  }

  return sect;
}
