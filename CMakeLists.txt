cmake_minimum_required(VERSION 3.20)
project(raycaster C)

option(RENDERER_DEBUG "Enable debug mode" ON)
option(RENDERER_LINE_VIS_CHECK "Enable linedef visibility checks" ON)
option(RENDERER_PARALLEL_RENDERING "Enable OpenMP parallel rendering" ON)
option(RENDERER_VECTORIZED_LIGHT_MUL "Enable vectorized light multiplication" ON)
option(RENDERER_DYNAMIC_SHADOWS "Enable dynamic shadows" ON)
set(RENDERER_LIGHT_STEPS 0 CACHE STRING "Number of light steps (0 for smooth lighting)")

set(RENDERER_DEFINES
  $<$<BOOL:${RENDERER_DEBUG}>:DEBUG>
  LIGHT_STEPS=${RENDERER_LIGHT_STEPS}
  $<$<BOOL:${RENDERER_LINE_VIS_CHECK}>:LINE_VIS_CHECK>
  $<$<BOOL:${RENDERER_PARALLEL_RENDERING}>:PARALLEL_RENDERING>
  $<$<BOOL:${RENDERER_VECTORIZED_LIGHT_MUL}>:VECTORIZED_LIGHT_MUL>
  $<$<BOOL:${RENDERER_DYNAMIC_SHADOWS}>:DYNAMIC_SHADOWS>
)

# Does the demo target need these flags, or just the renderer?
set(RENDERER_FLAGS
  -Wall
  -Wfatal-errors
  -g
  -no-pie
  -ffast-math
  -funroll-loops
  -fomit-frame-pointer
  # -flto
  -O3
  -msse2
  -mfpmath=sse
  $<$<BOOL:${RENDERER_PARALLEL_RENDERING}>:-fopenmp>
  $<$<BOOL:${RENDERER_PARALLEL_RENDERING}>:-fopenmp-simd>
)

# CMake had me separate the renderer from the demo/tests, so I'm unable to get LTO working at this time,
# since they're built separately, but I think it made no difference anyway

# if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
#   set(CMAKE_AR gcc-ar)
#   set(CMAKE_RANLIB gcc-ranlib)
#   set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
#   set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
# endif()

set(CMAKE_C_STANDARD 99)

include(FetchContent)

# SDL3
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.16
)
FetchContent_MakeAvailable(SDL3)

# SDL3_image
FetchContent_Declare(
  SDL3_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-3.2.4
)
FetchContent_MakeAvailable(SDL3_image)

############
# RENDERER #
############

file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS src/*.c deps/gpc/gpc.c)
add_library(renderer STATIC ${CORE_SRC})
target_include_directories(renderer
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/map_builder/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/gpc
)
target_compile_definitions(renderer PRIVATE ${RENDERER_DEFINES})
target_compile_options(renderer
  PRIVATE
    ${RENDERER_FLAGS}
    # Uncomment if profiling with gprof
    # -pg
)


###############
# DEMO TARGET #
###############

file(GLOB DEMO_SOURCES CONFIGURE_DEPENDS demo/*.c)
add_executable(demo ${DEMO_SOURCES})
target_link_libraries(demo PRIVATE renderer SDL3::SDL3 SDL3_image::SDL3_image m)
target_link_options(demo PRIVATE $<$<BOOL:${RENDERER_PARALLEL_RENDERING}>:-fopenmp>)
target_compile_definitions(demo PRIVATE ${RENDERER_DEFINES})
target_compile_options(demo PRIVATE ${RENDERER_FLAGS})
add_custom_command(TARGET demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:demo>
    $<TARGET_FILE_DIR:demo>
  COMMAND_EXPAND_LISTS
)
add_custom_command(TARGET demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/demo/res"
    $<TARGET_FILE_DIR:demo>/res
)


##############
# UNIT TESTS #
##############

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.c deps/unity/src/*.c deps/unity/extras/fixture/src/*.c)
add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests PRIVATE renderer)
target_include_directories(tests
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/unity/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/unity/extras/fixture/src
)
target_compile_definitions(tests PRIVATE
  ${RENDERER_DEFINES}
  UNITY_INCLUDE_PRINT_FORMATTED
  UNITY_INCLUDE_DOUBLE
)

enable_testing()
add_test(NAME tests COMMAND tests)
