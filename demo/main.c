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
static const int scale = 1,
                 initial_window_width = 1024,
                 initial_window_height = 768;

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

  SDL_CreateWindowAndRenderer(
    "Software Rendering Example",
    initial_window_width,
    initial_window_height,
    SDL_WINDOW_RESIZABLE,
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

  // create_demo_level();
  create_grid_level();
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
  static float titlebar_update_time = 0.5f;

  uint64_t now_ticks = SDL_GetTicks();
  float delta_time = (now_ticks - last_ticks) / 1000.0f;  // in seconds
  last_ticks = now_ticks;

  if (titlebar_update_time >= 0.5f) {
    sprintf(debug_buffer, "Raycaster ::: %dx%d @ %dx, dt: %f, fps: %i", rend.buffer_size.x, rend.buffer_size.y, scale, delta_time, (unsigned int)(1/delta_time));
    SDL_SetWindowTitle(window, debug_buffer);
    titlebar_update_time = 0.f;
  } else {
    titlebar_update_time += delta_time;
  }

  // SDL_ClearSurface(window_surface, 0, 0, 0, 1.f);

  process_camera_movement(delta_time);
  renderer_draw(&rend, &cam);


  void* pixels;
  int pitch;

  /*if (SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
    memcpy(pixels, rend.buffer, rend.buffer_size.y*pitch);
    SDL_UnlockTexture(texture);
  }*/

  SDL_UpdateTexture(texture, NULL, rend.buffer, rend.buffer_size.x*sizeof(pixel_type));

  SDL_RenderTexture(sdl_renderer, texture, NULL, NULL);
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
  const int w = 28;
  const int h = 28;
  const int size = 256;

  register int i, x, y, c, f;

  demo_level = malloc(sizeof(level_data) + w*h*sizeof(sector));
  demo_level->sectors_count = w*h;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      i = (y*w)+x;

      if (rand() % 20 == 5) {
        c = f = 0;
      } else {
        f = 8 * (rand() % 16);
        c = 1024 - 32 * (rand() % 24);
      }

      sector_init(
        &demo_level->sectors[i], 
        f,
        c,
        LINEDEFS(
          LDEF( /* Top */
            .v0.point = vec2f_make(x*size, y*size),
            .v1.point = vec2f_make(x*size + size, y*size),
            .side_sector[LINEDEF_BACK] = (y > 0) ? &demo_level->sectors[i-w] : NULL
          ),
          LDEF( /* Right */
            .v0.point = vec2f_make(x*size + size, y*size),
            .v1.point = vec2f_make(x*size + size, y*size + size),
            .side_sector[LINEDEF_BACK] = (x < w-1) ? &demo_level->sectors[i+1] : NULL
          ),
          LDEF( /* Bottom */
            .v0.point = vec2f_make(x*size + size, y*size + size),
            .v1.point = vec2f_make(x*size, y*size + size),
            .side_sector[LINEDEF_BACK] = (y < h-1) ? &demo_level->sectors[i+w] : NULL
          ),
          LDEF( /* Left */
            .v0.point = vec2f_make(x*size, y*size + size),
            .v1.point = vec2f_make(x*size, y*size),
            .side_sector[LINEDEF_BACK] = (x > 0) ? &demo_level->sectors[i-1] : NULL
          )
        )
      );
    }
  }
}

static void create_demo_level() {
  demo_level = malloc(sizeof(level_data) + 3*sizeof(sector));

  demo_level->sectors_count = 3;

  sector_init(
    &demo_level->sectors[0], 
    0,
    144,
    LINEDEFS(
      LDEF(.v0.point = vec2f_make(0, 0), .v1.point = vec2f_make(400, 0), .side_sector[LINEDEF_BACK] = &demo_level->sectors[1] ),
      LDEF(.v0.point = vec2f_make(400, 0), .v1.point = vec2f_make(400, 400) ),
      LDEF(.v0.point = vec2f_make(400, 400), .v1.point = vec2f_make(200, 400), .side_sector[LINEDEF_BACK] = &demo_level->sectors[2] ),
      LDEF(.v0.point = vec2f_make(200, 400), .v1.point = vec2f_make(0, 400) ),
      LDEF(.v0.point = vec2f_make(0, 400), .v1.point = vec2f_make(0, 0) )
    )
  );

  sector_init(
    &demo_level->sectors[1], 
    32,
    96,
    LINEDEFS(
      LDEF(.v0.point = vec2f_make(0, 0), .v1.point = vec2f_make(400, 0), .side_sector[LINEDEF_BACK] = &demo_level->sectors[0] ),
      LDEF(.v0.point = vec2f_make(400, 0), .v1.point = vec2f_make(300, -200) ),
      LDEF(.v0.point = vec2f_make(300, -200), .v1.point = vec2f_make(0, -100) ),
      LDEF(.v0.point = vec2f_make(0, -100), .v1.point = vec2f_make(0, 0) )
    )
  );

  sector_init(
    &demo_level->sectors[2], 
    -128,
    256,
    LINEDEFS(
      LDEF(.v0.point = vec2f_make(400, 400), .v1.point = vec2f_make(200, 400), .side_sector[LINEDEF_BACK] = &demo_level->sectors[0] ),
      LDEF(.v0.point = vec2f_make(200, 400), .v1.point = vec2f_make(200, 800) ),
      LDEF(.v0.point = vec2f_make(200, 800), .v1.point = vec2f_make(400, 800) ),
      LDEF(.v0.point = vec2f_make(400, 800), .v1.point = vec2f_make(400, 400) )
    )
  );
}