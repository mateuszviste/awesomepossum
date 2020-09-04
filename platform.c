#include <stdio.h>
#include <time.h>           /* struct timespec */
#include <unistd.h>         /* usleep() */
#include <SDL/SDL.h>        /* SDL */
#include <SDL/SDL_image.h>  /* SDL_image */

#include "sprites.h"        /* all sprites data here */


/* debug mode on/off */
#define DEBUGMODE


#ifdef DEBUGMODE
#include <sys/resource.h> /* required by setrlimit() */
static int enable_core_dumping() {
  struct rlimit varlimit;
  varlimit.rlim_cur = 1024*1024*1024;
  varlimit.rlim_max = 1024*1024*1024;
  /* Set the core limit to 1GiB */
  if (setrlimit(RLIMIT_CORE, &varlimit) == -1) return(-1);
  return(0);
}
#endif


struct character {
  int xpos;   /* current x position in the world */
  int ypos;   /* current y position in the world */
  int xposdelta; /* current x position delta, in micro pixels */
  int yposdelta; /* current y position delta, in micro pixels */
  int velocityx; /* horizontal velocity (is he moving? negative value for left movement or positive value for right movement) */
  int velocityy; /* vertical velocity (is he moving? negative value for down movement or positive value for up movement) */
  int airborne_timer;  /* how long the player is flying (used to limit the jump ability) */
  int spritedir;   /* the direction of the sprite (0 = left / 1 = right) */
  int spritestate;  /* the state of the sprite (0 = standing, 1, first step, etc) */
  int spritestate_duration; /* the time for which the current sprite was displayed (used for animations) */
  SDL_Surface *sprite;
  int collisionoffset_down;   /* how many pixels do we have to ignore for collision detection */
  int collisionoffset_up;     /* how many pixels do we have to ignore for collision detection */
  int collisionoffset_left;   /* how many pixels do we have to ignore for collision detection */
  int collisionoffset_right;  /* how many pixels do we have to ignore for collision detection */
  char neighbors_above_left;  /* what kind of neigbors we have */
  char neighbors_above;       /* what kind of neigbors we have */
  char neighbors_above_right; /* what kind of neigbors we have */
  char neighbors_right;       /* what kind of neigbors we have */
  char neighbors_below_right; /* what kind of neigbors we have */
  char neighbors_below;       /* what kind of neigbors we have */
  char neighbors_below_left;  /* what kind of neigbors we have */
  char neighbors_left;        /* what kind of neigbors we have */
};

struct virtualkeyboard {
  int left;
  int right;
  int up;
  int down;
  int jump;
  int shoot;
};

struct spritesstruct {
  SDL_Surface *player[2][16];
  SDL_Surface *tiles[64];
  int tilescount;
};


struct worldstruct {
  int width;
  int height;
  int tilemap[64][64][4]; /* x, y, z */
  SDL_Surface *bg;
};


static void draw_tiles(struct spritesstruct *sprites, struct worldstruct *world, SDL_Surface *screen, int displayoffset_x, int z1, int z2) {
  SDL_Rect rect, tilerect;
  int x, y, z;
  rect.w = sprites->tiles[0]->w;
  rect.h = sprites->tiles[0]->h;
  tilerect.w = sprites->tiles[0]->w;
  tilerect.h = sprites->tiles[0]->h;
  for (y = 0; y < 64; y++) {
    rect.y = screen->h - ((y + 1) * sprites->tiles[0]->h);
    for (x = (displayoffset_x / sprites->tiles[0]->w); x <= ((displayoffset_x + screen->w) / sprites->tiles[0]->w); x++) {
      rect.x = (x * sprites->tiles[0]->w) - displayoffset_x;
      if (rect.x >= 0) {
          tilerect.x = 0;
          tilerect.y = 0;
        } else {
          tilerect.x = 0 - rect.x;
          tilerect.y = 0;
          rect.x = 0;
      }
      for (z = z1; z <= z2; z++) {
        if (world->tilemap[x][y][z] > 0) SDL_BlitSurface(sprites->tiles[world->tilemap[x][y][z]], &tilerect, screen, &rect);
      }
    }
  }
}


void drawscreen(SDL_Surface *screen, struct spritesstruct *sprites, struct character *player, struct worldstruct *world, struct virtualkeyboard *keybstate, int elapsed_time) {
  SDL_Rect rect;
  int displayoffset_x;

  displayoffset_x = player->xpos + (player->sprite->w / 2) - (screen->w / 2);
  if (displayoffset_x < 0) displayoffset_x = 0;
  if (displayoffset_x >= (world->width * sprites->tiles[0]->w) - screen->w) displayoffset_x = (world->width * sprites->tiles[0]->w) - (screen->w + 1);
  if (screen->w >= (world->width * sprites->tiles[0]->w)) displayoffset_x = 0;

  SDL_FillRect(screen, NULL, 0);  /* fill the screen with black */
  if (world->bg != NULL) SDL_BlitSurface(world->bg, NULL, screen, NULL); /* apply the background image, if any */

  /* draw all the background tiles */
  draw_tiles(sprites, world, screen, displayoffset_x, 0, 2);

  /* compute the right sprite for current player's state */
  player->spritestate_duration += elapsed_time;
  if (keybstate->right != 0) { /* going right */
      player->spritedir = 1;
    } else if (keybstate->left != 0) { /* going left */
      player->spritedir = 0;
  }
  if (player->neighbors_below == 0) { /* if flying... */
      player->spritestate = 8;
      player->spritestate_duration = 0;
    } else {
      if ((player->velocityx == 0) && (player->velocityy == 0)) { /* player is standing still */
          if (player->spritestate > 3) player->spritestate_duration = 300; /* force a sprite change */
          if (player->spritestate_duration >= 200) {
            player->spritestate_duration -= 200;
            player->spritestate += 1;
            if (player->spritestate > 3) player->spritestate = 0;
          }
        } else {  /* player is in some kind of motion */
          if ((player->spritestate < 4) || (player->spritestate > 7)) player->spritestate_duration = 240; /* force a sprite change */
          if (player->spritestate_duration >= 160) {
            player->spritestate_duration -= 160;
            player->spritestate += 1;
            if (player->spritestate > 7) player->spritestate = 4;
          }
      }
  }
  player->sprite = sprites->player[player->spritedir][player->spritestate];
  /* put the player on screen */
  rect.x = player->xpos - displayoffset_x;
  rect.y = screen->h - (player->ypos + player->sprite->h);
  rect.h = 0;
  rect.w = 0;
  SDL_BlitSurface(player->sprite, NULL, screen, &rect);

  /* draw the foreground tiles */
  draw_tiles(sprites, world, screen, displayoffset_x, 3, 3);
}


static SDL_Surface *loadGraphic(void *memptr, int memlen) {
  SDL_Surface *result;
  SDL_RWops *rwop;
  rwop = SDL_RWFromMem(memptr, memlen);
  result = IMG_LoadPNG_RW(rwop);
  SDL_FreeRW(rwop);
  return(result);
}


static int loadlevel(char *file, struct worldstruct *world) {
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


static void flush_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event) != 0);
}


void compute_neighbors(struct worldstruct *world, struct character *player, struct spritesstruct *sprites) {
  int x, y;
  /* check neighbors below us */
  player->neighbors_below = 0; /* by default, we assume there is nobody below */
  player->neighbors_below_left = 0; /* by default, we assume there is nobody below */
  player->neighbors_below_right = 0; /* by default, we assume there is nobody below */
  if (player->ypos == 0) {
    player->neighbors_below_left = 1;
    player->neighbors_below_right = 1;
    player->neighbors_below = 1;
  } else {
    for (x = player->collisionoffset_left ; x < (player->sprite->w - player->collisionoffset_right) ; x++) {
      if (world->tilemap[((player->xpos + x) / sprites->tiles[0]->w)][((player->ypos + player->collisionoffset_down - 1) / sprites->tiles[0]->h)][2] != 0) player->neighbors_below = 1;
    }
  }
  
  /* check neighbors above us */
  player->neighbors_above = 0; /* by default, we assume there is nobody above */
  player->neighbors_above_left = 0; /* by default, we assume there is nobody above */
  player->neighbors_above_right = 0; /* by default, we assume there is nobody above */
  for (x = player->collisionoffset_left ; x < (player->sprite->w - player->collisionoffset_right) ; x++) {
    if (world->tilemap[((player->xpos + x) / sprites->tiles[0]->w)][((player->ypos + player->sprite->h + 1 - player->collisionoffset_up) / sprites->tiles[0]->h)][2] != 0) player->neighbors_above = 1;
  }

  /* check neighbors at left */
  player->neighbors_left = 0; /* by default, we assume there is nobody at the left */
  for (y = player->collisionoffset_down ; y < (player->sprite->h - player->collisionoffset_up) ; y++) {
    if (world->tilemap[((player->xpos + player->collisionoffset_left - 1) / sprites->tiles[0]->w)][((player->ypos + y) / sprites->tiles[0]->h)][2] != 0) player->neighbors_left = 1;
  }

  /* check neighbors at right */
  player->neighbors_right = 0; /* by default, we assume there is nobody at the right */
  for (y = player->collisionoffset_down ; y < (player->sprite->h - player->collisionoffset_up) ; y++) {
    if (world->tilemap[((player->xpos + player->sprite->w + 1 - player->collisionoffset_right) / sprites->tiles[0]->w)][((player->ypos + y) / sprites->tiles[0]->h)][2] != 0) player->neighbors_right = 1;
  }

}


/* computes all the physics in the world */
void run_engine(struct worldstruct *world, struct character *player, int elapsed_time, struct spritesstruct *sprites, struct virtualkeyboard *keybstate) {
  #define gravityforce          1800    /* I gain this much falling momentum per ms when in the air */
  #define frictionforce_ground  400     /* I loose this much horizontal momentum per ms when on the ground */
  #define frictionforce_air     150     /* I loose this much horizontal momentum per ms when in the air */
  #define maxvvelocity          600000  /* I cannot go up faster than that */
  #define minvvelocity         -600000  /* I cannot fall faster than that */
  #define maxhvelocity          300000  /* I cannot be propulsed right faster than that */
  #define minhvelocity         -300000  /* I cannot be propulsed left faster than that */
  #define jumptimelimit         100     /* I can hold the jump key this many ms at most to control my jump force */
  #define jumpimpulse           14000   /* I gain this much momentum per ms when jump key is pressed */
  #define collisionvelocityloss 2000    /* I loose this much momentum per ms when hitting an obstacle */
  int airborne, frictionforce;  /* airborne flag. will be set if the player is flying */

  /* set the airborne flag if we are flying, and update the airborne time accordingly */
  compute_neighbors(world, player, sprites);
  if (player->neighbors_below == 0) airborne = 1; else airborne = 0;
  if (airborne != 0) {
      player->airborne_timer += elapsed_time;
      frictionforce = frictionforce_air;
    } else {
      player->airborne_timer = 0;
      frictionforce = frictionforce_ground;
  }

  /* apply velocity to the player */
  player->yposdelta += (player->velocityy * elapsed_time);
  player->xposdelta += (player->velocityx * elapsed_time);

  /* update player's position */
  while (player->yposdelta <= -1000000) { /* DOWN */
    player->yposdelta += 1000000;
    if (player->ypos > 0) {
      if (airborne != 0) {
        player->ypos -= 1;
        /* recompute neighbors to check if we are still flying */
        compute_neighbors(world, player, sprites);
        if (player->neighbors_below == 0) airborne = 1; else airborne = 0;
      }
    }
  }
  while (player->yposdelta >= 1000000) { /* UP */
    player->yposdelta -= 1000000;
    if (player->ypos < 0xFFFFFFF) { /* just a dumb limit to avoid the player going that high in case of a bug in the game... */
      if (player->neighbors_above == 0) player->ypos += 1;
      compute_neighbors(world, player, sprites); /* recompute neighbors */
    }
  }
  while (player->xposdelta >= 1000000) { /* RIGHT */
    player->xposdelta -= 1000000;
    if (player->xpos < 0xFFFFFFF) { /* just a dumb limit to avoid the player going that high in case of a bug in the game... */
      if (player->neighbors_right == 0) player->xpos += 1;
      compute_neighbors(world, player, sprites); /* recompute neighbors */
    }
  }
  while (player->xposdelta <= -1000000) { /* LEFT */
    player->xposdelta += 1000000;
    if (player->xpos > 0) {
      if (player->neighbors_left == 0) player->xpos -= 1;
      compute_neighbors(world, player, sprites); /* recompute neighbors */
    }
  }

  /* apply gravity if we are airborne */
  if (airborne != 0) {
      player->velocityy -= (gravityforce * elapsed_time);
    } else {  /* we are on ground, so no velocity */
      player->velocityy = 0;
  }

  /* apply collisions static forces if we are currently colliding with something */
  if ((player->neighbors_above != 0) && (player->velocityy > 0)) { /* collision above */
    player->velocityy -= (collisionvelocityloss * elapsed_time);
    if (player->velocityy < 0) player->velocityy = 0;
    player->airborne_timer = 1000; /* special case for upward collisions: forbid any more jump acceleration on collision */
  }
  if ((player->neighbors_below != 0) && (player->velocityy < 0)) { /* collision below */
    player->velocityy += (collisionvelocityloss * elapsed_time);
    if (player->velocityy > 0) player->velocityy = 0;
  }
  if ((player->neighbors_left != 0) && (player->velocityx < 0)) { /* collision left */
    player->velocityx += (collisionvelocityloss * elapsed_time);
    if (player->velocityx > 0) player->velocityx = 0;
  }
  if ((player->neighbors_right != 0) && (player->velocityx > 0)) { /* collision right */
    player->velocityx -= (collisionvelocityloss * elapsed_time);
    if (player->velocityx < 0) player->velocityx = 0;
  }

  /* apply friction force */
  if (player->velocityx > 0) {
      player->velocityx -= (frictionforce * elapsed_time);
      if (player->velocityx < 0) player->velocityx = 0;
    } else if (player->velocityx < 0) {
      player->velocityx += (frictionforce * elapsed_time);
      if (player->velocityx > 0) player->velocityx = 0;
  }

  /* apply user-driven impulses */
  if (keybstate->jump != 0) {
    if (player->airborne_timer < jumptimelimit) { /* jumping is authorized only for a short time */
      player->velocityy += (jumpimpulse * elapsed_time); /* jumping requires a tremendous acceleration */
    } else { /* if time limit reached, turn off the jump key manually */
      keybstate->jump = 0;
    }
  }
  if (keybstate->left != 0) player->velocityx -= (frictionforce * 3 * elapsed_time);
  if (keybstate->right != 0) player->velocityx += (frictionforce * 3 * elapsed_time);

  /* limit max velocity (speed) */
  if (player->velocityx < minhvelocity) player->velocityx = minhvelocity;
  if (player->velocityx > maxhvelocity) player->velocityx = maxhvelocity;
  if (player->velocityy < minvvelocity) player->velocityy = minvvelocity;
  if (player->velocityy > maxvvelocity) player->velocityy = maxvvelocity;

  /* printf("air: %d / vely: %d / yposdelta: %d / up: %d\n", airborne, player->velocityy, player->yposdelta, keybstate->up); */

  #undef gravityforce
  #undef frictionforce_air
  #undef frictionforce_ground
  #undef maxvelocity
  #undef minvelocity
  #undef jumpimpulse
  #undef jumptimelimit
  #undef collisionvelocityloss
}


int main(void) {
  struct worldstruct world;  /* the world is a set of 64x64 tiles */
  int x, y, z, elapsed_time, exitflag = 0;
  struct virtualkeyboard keybstate;
  struct character player;
  struct timespec ts[2]; /* these timestamps will be used to compute elapsed time between two frames */
  struct spritesstruct sprites;
  SDL_Surface *screen = NULL; /* this will be used as a pointer to the screen content */
  SDL_Event event; /* Event structure */

  #ifdef DEBUGMODE
  enable_core_dumping();
  #endif

  /* init the SDL library */
  SDL_Init(SDL_INIT_VIDEO);

  /* init the video mode on screen */
  screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE | SDL_DOUBLEBUF);

  /* hide the mouse cursor */
  SDL_ShowCursor(SDL_DISABLE);

  /* reset the whole virtual keyboard structure */
  memset(&keybstate, 0, sizeof(keybstate));

  /* load all sprites */
  puts("load player.png");
  loadSpriteSheet(sprites.player[0], 54, 75, 9, possum_left_png, possum_left_png_len);
  loadSpriteSheet(sprites.player[1], 54, 75, 9, possum_right_png, possum_right_png_len);
  player.collisionoffset_up = 12;
  player.collisionoffset_down = 4;
  player.collisionoffset_left = 8;
  player.collisionoffset_right = 8;
  player.spritedir = 1;
  player.spritestate = 0;
  player.sprite = sprites.player[1][0];

  /* load all tiles */
  puts("load tiles.png");
  sprites.tilescount = 64;
  loadSpriteSheet(sprites.tiles, 16, 16, sprites.tilescount, tiles_png, tiles_png_len);

  /* set the background layer of the world to be null */
  world.bg = NULL; /* loadGraphic(bg_png, bg_png_len); */

  /* clear the entire world */
  for (x = 0; x < 64; x++) {
    for (y = 0; y < 64; y++) {
      for (z = 0; z < 4; z++) {
        world.tilemap[x][y][z] = 0;
      }
    }
  }

  /* load a level */
  loadlevel("level01.dat", &world);

  /* set the initial position of the player and movement */
  player.xpos = 18;
  player.ypos = 400;
  player.xposdelta = 0;
  player.yposdelta = 0;
  player.velocityx = 0;
  player.velocityy = 0;

  /* set timestamps to some initial value */
  clock_gettime(CLOCK_MONOTONIC, &ts[0]);

  /* here starts the main loop of the game */
  while (exitflag == 0) {

    for (;;) {
      /* compute the time spent since last time */
      clock_gettime(CLOCK_MONOTONIC, &ts[1]);
      elapsed_time = ((ts[1].tv_sec - ts[0].tv_sec) * 1000); /* elapsed seconds */
      elapsed_time += ((ts[1].tv_nsec - ts[0].tv_nsec) / 1000000L);

      if (elapsed_time >= 20) break;
      /* artificially slow down the game engine to not waste to much cpu */
      usleep(8000);   /* wait 8ms */
    }
    ts[0].tv_sec = ts[1].tv_sec;
    ts[0].tv_nsec = ts[1].tv_nsec;

    while (SDL_PollEvent(&event) != 0) {
      if ((event.type == SDL_KEYDOWN) || (event.type == SDL_KEYUP)) {
          int newstate = 0;
          if (event.type == SDL_KEYDOWN) newstate = 1;
          switch (event.key.keysym.sym) {
            case SDLK_LALT:
              keybstate.jump = newstate;
              break;
            case SDLK_DOWN:
              keybstate.down = newstate;
              break;
            case SDLK_LEFT:
              keybstate.left = newstate;
              break;
            case SDLK_RIGHT:
              keybstate.right = newstate;
              break;
            default:
              /* nothing here - but gcc complains if I don't handle a default case */                
              break;
          }
        } else if (event.type == SDL_QUIT) {
          exitflag = 1;
      }
    }

    /* run the world  */
    run_engine(&world, &player, elapsed_time, &sprites, &keybstate);

    /* draw the world */
    drawscreen(screen, &sprites, &player, &world, &keybstate, elapsed_time);
    SDL_Flip(screen);  /* refresh the screen */

  }

  /* clean up SDL */
  SDL_Quit();

  return(0);
}
