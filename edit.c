/* level editor for Mike O'Possum */

#include <stdio.h>
#include <unistd.h>  /* usleep() */
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>  /* SDL_image */

#include "sprites.h"

struct spritesstruct {
  SDL_Surface *player[2][8];
  SDL_Surface *tiles[64];
  int tilescount;
};

struct worldstruct {
  int width;
  int height;
  int tilemap[64][64][4]; /* x, y, z */
  SDL_Surface *bg;
};



static SDL_Surface *loadGraphic(void *memptr, int memlen) {
  SDL_Surface *result;
  SDL_RWops *rwop;
  rwop = SDL_RWFromMem(memptr, memlen);
  result = IMG_LoadPNG_RW(rwop);
  SDL_FreeRW(rwop);
  return(result);
}


static void loadSpriteSheet(SDL_Surface **surface, int width, int height, int itemcount, void *memptr, int memlen) {
  SDL_Surface *spritesheet;
  SDL_Rect rect;
  int i;
  spritesheet = loadGraphic(memptr, memlen);
  for (i = 0; i < itemcount; i++) {
    rect.x = i * width;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    surface[i] = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, width, height, 32, 0xFF000000L, 0x00FF0000L, 0x0000FF00L, 0x000000FFL);  /* I'm setting alpha to 0, because otherwise sdl for some strange reason uses the destination alpha mask :/ */
    SDL_FillRect(surface[i], NULL, 0x0);
    SDL_BlitSurface(spritesheet, &rect, surface[i], NULL);
  }
  SDL_FreeSurface(spritesheet);
}


void drawscreen(SDL_Surface *screen, struct spritesstruct *sprites, struct worldstruct *world, int controlcolumn, int selectedtile, int selectedtile_offset, int viewmode) {
  SDL_Rect rect;
  int x, y, z;

  SDL_FillRect(screen, NULL, 0xFFFFFF); /* fill the screen with black */
  if (world->bg != NULL) SDL_BlitSurface(world->bg, NULL, screen, NULL); /* apply the background image, if any */

  /* draw all the background */
  rect.w = sprites->tiles[0]->w;
  rect.h = sprites->tiles[0]->h;
  for (y = 0; y < 64; y++) {
    rect.y = screen->h - ((y + 1) * sprites->tiles[0]->h);
    for (x = 0; x < 64; x++) {
      rect.x = x * sprites->tiles[0]->w;
      for (z = 0; z < 4; z++) {
        if ((viewmode == z) || (viewmode == 4)) {
          if (world->tilemap[x][y][z] > 0) SDL_BlitSurface(sprites->tiles[world->tilemap[x][y][z]], NULL, screen, &rect);
        }
      }
    }
  }

  /* draw all available tiles */
  for (x = 0; x < sprites->tilescount; x++) {
    rect.x = controlcolumn * sprites->tiles[0]->w;
    rect.y = (x + 1) * sprites->tiles[0]->h;
    SDL_BlitSurface(sprites->tiles[x + selectedtile_offset], NULL, screen, &rect);
    if (rect.y > screen->h) break;
  }

  /* draw the cursor */
  SDL_GetMouseState(&x, &y);
  x /= sprites->tiles[0]->w;
  y /= sprites->tiles[0]->h;
  rect.x = x * sprites->tiles[0]->w;
  rect.y = y * sprites->tiles[0]->h;
  if (x != controlcolumn) SDL_BlitSurface(sprites->tiles[selectedtile], NULL, screen, &rect);
  SDL_BlitSurface(sprites->tiles[17], NULL, screen, &rect);

  SDL_Flip(screen);
}


int loadlevel(char *file, struct worldstruct *world) {
  FILE *worldfile;
  int x, y, z;
  unsigned char buff[4];
  worldfile = fopen(file, "rb");
  if (worldfile == NULL) return(-1);
  /* compute the width/height of the world */
  fread(buff, 4, 1, worldfile);
  world->width = buff[0];
  world->width <<= 8;
  world->width |= buff[1];
  world->height = buff[0];
  world->height <<= 8;
  world->height |= buff[1];
  /* read the world and populate the data table */
  for (y = 0; y < world->width; y++) {
    for (x = 0; x < world->width; x++) {
      fread(buff, 4, 1, worldfile);
      for (z = 0; z < 4; z++) {
        world->tilemap[x][y][z] = buff[z];
      }
    }
  }
  fclose(worldfile);
  return(0);
}


int saveworld(char *file, struct worldstruct *world) {
  FILE *worldfile;
  int x, y, z;
  unsigned char buff[4];
  worldfile = fopen(file, "wb");
  if (worldfile == NULL) return(-1);
  /* compute the width/height of the world */
  fprintf(worldfile, "%c", (world->width >> 8) & 0xFF);
  fprintf(worldfile, "%c", world->width & 0xFF);
  fprintf(worldfile, "%c", (world->height >> 8) & 0xFF);
  fprintf(worldfile, "%c", world->height & 0xFF);
  /* write the world into the data file */
  for (y = 0; y < world->width; y++) {
    for (x = 0; x < world->width; x++) {
      fread(buff, 3, 1, worldfile);
      for (z = 0; z < 4; z++) {
        fprintf(worldfile, "%c", world->tilemap[x][y][z]);
      }
    }
  }
  fclose(worldfile);
  return(0);
}

void createemptyworld(struct worldstruct *world, int w, int h) {
  int x, y, z;
  world->width = w;
  world->height = h;
  for (y = 0; y < world->height; y++) {
    for (x = 0; x < world->width; x++) {
      for (z = 0; z < 3; z++) world->tilemap[x][y][z] = 0;
    }
  }
}


int main(int argc, char **argv) {
  struct worldstruct world;  /* the world is a set of 64x64 tiles */
  struct spritesstruct sprites;
  char *worldfilename;
  SDL_Surface *screen;
  SDL_Event event;
  int exitflag = 0, refreshscreen = 1, viewmode = 4;
  int controlcolumn = 42, selectedtile = 0, selectedtile_offset = 0, painting = 0;

  if (argc != 2) {
    printf("Usage: edit file.dat\n");
    return(0);
  }

  worldfilename = argv[1];

  if (loadlevel(worldfilename, &world) != 0) {
    printf("The world file do not exist yet. Loading a default world.\n");
    createemptyworld(&world, 64, 64);
  }

  /* init the SDL library */
  SDL_Init(SDL_INIT_VIDEO);

  /* init the video mode on screen */
  screen = SDL_SetVideoMode(690, 480, 32, SDL_SWSURFACE | SDL_DOUBLEBUF);

  /* load tiles */
  sprites.tilescount = 64;
  loadSpriteSheet(sprites.tiles, 16, 16, sprites.tilescount, tiles_png, tiles_png_len);

  world.bg = NULL; /* loadGraphic(bg_png, bg_png_len); */

  while (exitflag == 0) {
    if (painting != 0) {
      int tilex, tiley;
      SDL_GetMouseState(&tilex, &tiley);
      tilex /= sprites.tiles[0]->w;
      tiley /= sprites.tiles[0]->h;
      if (viewmode < 4) world.tilemap[tilex][(screen->h / sprites.tiles[0]->h) - (tiley + 1)][viewmode] = selectedtile;
    }
    if (refreshscreen != 0) drawscreen(screen, &sprites, &world, controlcolumn, selectedtile, selectedtile_offset, viewmode);
    refreshscreen = 0;

    while (SDL_PollEvent(&event) != 0) {
      refreshscreen = 1;
      if (event.type == SDL_QUIT) {
          exitflag = 1;
        } else if (event.type == SDL_KEYDOWN) {
          switch (event.key.keysym.sym) {
            case SDLK_F1:
              viewmode = 0;
              break;
            case SDLK_F2:
              viewmode = 1;
              break;
            case SDLK_F3:
              viewmode = 2;
              break;
            case SDLK_F4:
              viewmode = 3;
              break;
            case SDLK_F5:
              viewmode = 4;
              break;
            default:
              break;
          }
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
          int tilex, tiley;
          tilex = event.button.x / sprites.tiles[0]->w;
          tiley = event.button.y / sprites.tiles[0]->h;
          printf("MOUSE BUTTON at tile [%d,%d] (control is %d)\n", tilex, tiley, controlcolumn);
          if (tilex == controlcolumn) { /* select a tile */
              if (tiley == 0) {
                  if (selectedtile_offset > 0) selectedtile_offset -= 1;
                } else if (tiley == (screen->h - 1) / sprites.tiles[0]->h) {
                  if (selectedtile_offset < 30) selectedtile_offset += 1;
                } else {
                  selectedtile = tiley - 1 + selectedtile_offset;
              }
            } else { /* put a tile in the world */
              painting = 1;
          }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
          painting = 0;
      }
    }
    if ((refreshscreen == 0) && (painting == 0)) usleep(100000);
  }

  /* clean up SDL */
  SDL_Quit();

  saveworld(worldfilename, &world);

  return(0);
}
