#ifndef _RENDER_h
#define _RENDER_h
#include <TFT_eSPI.h>
#include "price.h"

#define TFT_AMBER_DARK_BLUE 0x1969
#define TFT_AMBER_GREEN 0x07F4
#define TFT_AMBER_YELLOW 0xFEE9
#define TFT_AMBER_ORANGE 0xFD41
#define TFT_AMBER_RED 0xF38C
#define TFT_AMBER_DARK_RED 0xB8C1

typedef enum
{
  DIRECTION_UP = -1,
  DIRECTION_DOWN = 1
} animation_direction_t;

typedef struct _animation_state
{
  uint8_t current_frame;
  uint8_t target_frame;
  animation_direction_t direction;
  TFT_eSPI *tft;
  TFT_eSprite *price_sprite_a;
  TFT_eSprite *price_sprite_b;
} animation_state_t;

void render_controlled_load_price(TFT_eSprite *sprite, price_t *price);
void render_general_price(TFT_eSprite *sprite, price_t *price);
void render_feed_in(TFT_eSprite *sprite, price_t *price);

bool animating(animation_state_t *state);
void animate(animation_state_t *state);
#endif