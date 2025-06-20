#define SDL_MAIN_USE_CALLBACKS 1
#include "renderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SDL_Window* window = NULL;
SDL_Renderer *sdl_renderer = NULL;
SDL_Texture *texture = NULL;

static renderer rend;
static camera cam;
static level_data *demo_level = NULL;
static uint64_t last_ticks;
static float delta_time;
static const int initial_window_width = 1024,
                 initial_window_height = 768;
static int scale = 1;

static struct {
  float forward, turn, raise;
} movement = { 0 };

static void create_demo_level();
static void create_grid_level();
static void process_camera_movement(const float delta_time);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return -1;
  }

  int i;
  int level = 0;
  bool fullscreen = false;

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-level")) {
      level = atoi(argv[i+1]);
    } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-fullscreen")) {
      fullscreen = true;
    }
  }

  SDL_CreateWindowAndRenderer(
    "Software Rendering Example",
    initial_window_width,
    initial_window_height,
    SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
    &window,
    &sdl_renderer
  );

  if (!window) {
      SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
      return -1;
  }

  printf("sdl_renderer: %s\n", SDL_GetRendererName(sdl_renderer));

  SDL_SetRenderVSync(sdl_renderer, 1);

  renderer_init(&rend, VEC2U(initial_window_width / scale, initial_window_height / scale));

  if (!rend.buffer) {
    return -1;
  }

  texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, rend.buffer_size.x, rend.buffer_size.y);
  
  if (!texture) return -1;

  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

  switch (level) {
  case 1: create_demo_level(); break;
  default: create_grid_level(); break;
  }
 
  camera_init(&cam, demo_level);

  last_ticks = SDL_GetTicks();

  return 0;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  renderer_destroy(&rend);
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
      return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
      if (event->key.key == SDLK_W) { movement.forward = 1.f; }
      else if (event->key.key == SDLK_S) { movement.forward = -1.f; }
      
      if (event->key.key == SDLK_A) { movement.turn = 1.f; }
      else if (event->key.key == SDLK_D) { movement.turn = -1.f; }
      
      if (event->key.key == SDLK_Q) { movement.raise = 1.f; }
      else if (event->key.key == SDLK_Z) { movement.raise = -1.f; }

      if (event->key.key == SDLK_PLUS ||event->key.key == SDLK_MINUS) {
        if (event->key.key == SDLK_PLUS) { scale += 1; }
        else if (scale > 1) { scale -= 1; }
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        printf("Resize buffer to %dx%d\n", w / scale, h / scale);
        renderer_resize(&rend, VEC2U(w / scale, h / scale));
        SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, rend.buffer_size.x, rend.buffer_size.y);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
      }

      if (event->key.key == SDLK_P) {
        camera_set_fov(&cam, M_MAX(0.1f, cam.fov*(1.0-delta_time*3)));
      } else if (event->key.key == SDLK_O) {
        camera_set_fov(&cam, M_MIN(4.0f, cam.fov*(1.0+delta_time*3)));
      }
    } else if (event->type == SDL_EVENT_KEY_UP) {
      if (event->key.key == SDLK_W) { movement.forward = 0.f; }
      else if (event->key.key == SDLK_S) { movement.forward = 0.f; }
      
      if (event->key.key == SDLK_A) { movement.turn = 0.f; }
      else if (event->key.key == SDLK_D) { movement.turn = 0.f; }
      
      if (event->key.key == SDLK_Q) { movement.raise = 0.f; }
      else if (event->key.key == SDLK_Z) { movement.raise = 0.f; }
    } else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
      printf("Resize buffer to %dx%d\n", event->window.data1 / scale, event->window.data2 / scale);
      renderer_resize(&rend, VEC2U(event->window.data1 / scale, event->window.data2 / scale));
      SDL_DestroyTexture(texture);
      texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, rend.buffer_size.x, rend.buffer_size.y);
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *userdata) {
  static char debug_buffer[64];
  static float fps_update_timer = 0.5f;

  uint64_t now_ticks = SDL_GetTicks();
  delta_time = (now_ticks - last_ticks) / 1000.0f;  // in seconds
  last_ticks = now_ticks;

  if (fps_update_timer >= 0.25f) {
    sprintf(debug_buffer, "%dx%d @ %dx, dt: %f, FPS: %i", rend.buffer_size.x, rend.buffer_size.y, scale, delta_time, (unsigned int)(1/delta_time));
    fps_update_timer = 0.f;
  } else {
    fps_update_timer += delta_time;
  }

  // SDL_ClearSurface(window_surface, 0, 0, 0, 1.f);

  process_camera_movement(delta_time);
  renderer_draw(&rend, &cam);

  /*void* pixels;
  int pitch;

  if (SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
    memcpy(pixels, rend.buffer, rend.buffer_size.y*pitch);
    SDL_UnlockTexture(texture);
  }*/

  SDL_UpdateTexture(texture, NULL, rend.buffer, rend.buffer_size.x*sizeof(pixel_type));

  SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(sdl_renderer);
  SDL_RenderTexture(sdl_renderer, texture, NULL, NULL);

  int y = 4, h = 10;

  SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
  SDL_RenderDebugText(sdl_renderer, 4, y, debug_buffer); y+=h;
  SDL_RenderDebugTextFormat(sdl_renderer, 4, y, "CAMERA pos: (%.1f, %.1f, %.1f), dir: (%.3f, %.3f), plane: (%.3f, %.3f), FOV: %.2f", cam.position.x, cam.position.y, cam.z, cam.direction.x, cam.direction.y, cam.plane.x, cam.plane.y, cam.fov); y+=h;
  SDL_RenderDebugTextFormat(sdl_renderer, 4, y, "Line vis checks: %d (%d visible)", rend.counters.line_visibility_checks, rend.counters.visible_lines); y+=h;
  SDL_RenderDebugTextFormat(sdl_renderer, 4, y, "Vertex checks:   %d (%d visible)", rend.counters.vertex_visibility_checks, rend.counters.visible_vertices); y+=h;
  SDL_RenderDebugTextFormat(sdl_renderer, 4, y, "Sectors visited: %d", rend.counters.sectors_visited); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[WASD] - Move & turn"); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[Q] - Go up"); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[Z] - Go down"); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[+] - Increase scale factor"); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[-] - Decrease scale factor"); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[O] - Zoom out"); y+=h;
  SDL_RenderDebugText(sdl_renderer, 4, y, "[P] - Zoom in"); y+=h;

  SDL_RenderPresent(sdl_renderer);

  return SDL_APP_CONTINUE;
}

static void process_camera_movement(const float delta_time) {
  if ((int)movement.forward != 0) {
    camera_move(&cam, 400 * movement.forward * delta_time);
  }

  if ((int)movement.turn != 0) {
    camera_rotate(&cam, 2.f * movement.turn * delta_time);
  }

  if ((int)movement.raise != 0) {
    cam.z += 88 * movement.raise * delta_time;
  }
}

static void create_grid_level() {
  const int w = 24;
  const int h = 24;
  const int size = 256;

  register int x, y, c, f;

  map_data *map = malloc(sizeof(map_data));
  map->polygons_count = 0;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      if (rand() % 20 == 5) {
        c = f = 0;
      } else {
        f = 8 * (rand() % 16);
        c = 1024 - 32 * (rand() % 24);
      }

      map_data_add_polygon(map, f, c, VERTICES(
        VEC2F(x*size, y*size),
        VEC2F(x*size + size, y*size),
        VEC2F(x*size + size, y*size + size),
        VEC2F(x*size, y*size + size)
      ));
    }
  }

  demo_level = map_data_build(map);

  free(map);
}

static void create_demo_level() {
  map_data *map = malloc(sizeof(map_data));
  map->polygons_count = 0;

  map_data_add_polygon(map, 0, 144, VERTICES(
    VEC2F(0, 0),
    VEC2F(400, 0),
    VEC2F(400, 400),
    VEC2F(200, 300),
    VEC2F(0, 400)
  ));

  map_data_add_polygon(map, -32, 160, VERTICES(
    VEC2F(50, 50),
    VEC2F(50, 200),
    VEC2F(200, 200),
    VEC2F(200, 50)
  ));

  map_data_add_polygon(map, 128, 128, VERTICES(
    VEC2F(100, 100),
    VEC2F(125, 100),
    VEC2F(125, 125),
    VEC2F(100, 125)
  ));

  map_data_add_polygon(map, 32, 96, VERTICES(
    VEC2F(0, 0),
    VEC2F(400, 0),
    VEC2F(300, -200),
    VEC2F(0, -100)
  ));

  map_data_add_polygon(map, -128, 256, VERTICES(
    VEC2F(400, 400),
    VEC2F(200, 300),
    VEC2F(100, 1000),
    VEC2F(500, 1000)
  ));

  map_data_add_polygon(map, 0, 224, VERTICES(
    VEC2F(275, 500),
    VEC2F(325, 500),
    VEC2F(325, 700),
    VEC2F(275, 700)
  ));

  demo_level = map_data_build(map);

  free(map);
}