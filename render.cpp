#include "render.h"
#include "price_extremely_low.h"
#include "price_very_low.h"
#include "price_low.h"
#include "price_neutral.h"
#include "price_high.h"
#include "price_spike.h"
#include "price_negative_feed_in.h"
#include "price_solar_feed_in.h"
#include "price_spike_feed_in.h"
#include "price_error.h"
#include "font_large.h"

void render_price(TFT_eSprite *sprite, float price, uint16_t text_colour, const unsigned short (*background)[18225])
{
  sprite->loadFont(NotoSansBold36);
  sprite->setTextColor(text_colour);
  sprite->pushImage(0, 0, sprite->width(), sprite->height(), *background);
  char formatted[7];
  char *ptr = (char *)&formatted;
  memset(ptr, 0, 7);
  if (abs(price) > 100)
  {
    snprintf(ptr, 7, "$%g", round(price / 100));
  }
  else
  {
    snprintf(ptr, 7, "%gc", round(price));
  }
  sprite->drawString(ptr, sprite->width() / 2, sprite->height() / 2);
  sprite->unloadFont();
}

void render_controlled_load_price(TFT_eSprite *sprite, price_t *price)
{
  switch (price->descriptor)
  {
  case DESCRIPTOR_SPIKE:
  {
    Serial.printf("Price Spike!: %f\n", price->price);
    render_price(sprite, price->price, TFT_WHITE, &price_spike);
    break;
  }
  case DESCRIPTOR_HIGH:
  {
    Serial.printf("High prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_high);
    break;
  }
  case DESCRIPTOR_NEUTRAL:
  {
    Serial.printf("Average prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_neutral);
    break;
  }
  case DESCRIPTOR_LOW:
  {
    Serial.printf("Low prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_low);
    break;
  }
  case DESCRIPTOR_VERY_LOW:
  {
    Serial.printf("Very Low prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_very_low);
    break;
  }
  case DESCRIPTOR_EXTREMELY_LOW:
  {
    Serial.printf("Extremely low prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_extremely_low);
    break;
  }
  default:
  {
    Serial.printf("Unknown Controlled Load Price\n");
    sprite->pushImage(0, 0, sprite->width(), sprite->height(), price_error);
  }
  }
}

void render_general_price(TFT_eSprite *sprite, price_t *price)
{
  switch (price->descriptor)
  {
  case DESCRIPTOR_SPIKE:
  {
    Serial.printf("Price Spike!: %f\n", price->price);
    render_price(sprite, price->price, TFT_WHITE, &price_spike);
    break;
  }
  case DESCRIPTOR_HIGH:
  {
    Serial.printf("High prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_high);
    break;
  }
  case DESCRIPTOR_NEUTRAL:
  {
    Serial.printf("Average prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_neutral);
    break;
  }
  case DESCRIPTOR_LOW:
  {
    Serial.printf("Low prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_low);
    break;
  }
  case DESCRIPTOR_VERY_LOW:
  {
    Serial.printf("Very Low prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_very_low);
    break;
  }
  case DESCRIPTOR_EXTREMELY_LOW:
  {
    Serial.printf("Extremely low prices: %f\n", price->price);
    render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_extremely_low);
    break;
  }
  default:
  {
    Serial.printf("Unknown General Price\n");
    sprite->pushImage(0, 0, sprite->width(), sprite->height(), price_error);
  }
  }
}

void render_feed_in(TFT_eSprite *sprite, price_t *price)
{
  switch (price->descriptor)
  {
  case DESCRIPTOR_SPIKE:
  {
    Serial.printf("Price Spike!: %f\n", price->price);
    render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_spike_feed_in);
    break;
  }
  case DESCRIPTOR_HIGH:
  {
    Serial.printf("High prices: %f\n", price->price);
    render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_solar_feed_in);
    break;
  }
  case DESCRIPTOR_NEUTRAL:
  {
    Serial.printf("Average prices: %f\n", price->price);
    render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_solar_feed_in);
    break;
  }
  case DESCRIPTOR_LOW:
  {
    Serial.printf("Low prices: %f\n", price->price);
    render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_solar_feed_in);
    break;
  }
  case DESCRIPTOR_VERY_LOW:
  {
    Serial.printf("Very Low prices: %f\n", price->price);
    render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_solar_feed_in);
    break;
  }
  case DESCRIPTOR_EXTREMELY_LOW:
  {
    Serial.printf("Extremely low prices: %f\n", price->price);
    render_price(sprite, -1 * price->price, TFT_AMBER_RED, &price_negative_feed_in);
    break;
  }
  default:
  {
    Serial.printf("Unknown Feed In Price\n");
    sprite->pushImage(0, 0, sprite->width(), sprite->height(), price_error);
  }
  }
}

void render_pills(TFT_eSPI *tft, channels_t *channels, screens_t *current_screen)
{
  uint8_t screen_count = 0;
  uint8_t current = 0;
  if (channels->general.descriptor != DESCRIPTOR_UNKNOWN)
  {
    if (*current_screen == SCREEN_GENERAL)
    {
      current = screen_count;
    }
    screen_count++;
  }
  if (channels->feed_in.descriptor != DESCRIPTOR_UNKNOWN)
  {
    if (*current_screen == SCREEN_FEED_IN)
    {
      current = screen_count;
    }
    screen_count++;
  }
  if (channels->controlled_load.descriptor != DESCRIPTOR_UNKNOWN)
  {
    if (*current_screen == SCREEN_CONTROLLED_LOAD)
    {
      current = screen_count;
    }
    screen_count++;
  }

  if (screen_count < 2)
  {
    return;
  }

  uint8_t x = 16;
  uint8_t y = tft->height() / 2;
  uint8_t height = 60;
  uint8_t space = (height / screen_count);
  uint8_t start = screen_count % 2 == 0 ? y - (space / screen_count) : y - space;

  for (uint8_t i = 0; i < screen_count; i++)
  {
    if (i == current)
    {
      tft->fillCircle(x, start + (i * space), 4, TFT_WHITE);
    }
    else
    {
      tft->fillCircle(x, start + (i * space), 4, TFT_AMBER_DARK_BLUE);
      tft->fillCircle(x, start + (i * space), 3, TFT_DARKGREY);
    }
  }
}

bool animating(animation_state_t *state)
{
  return state->current_frame < state->target_frame;
}

void animate(animation_state_t *state)
{
  if (!animating(state))
  {
    return;
  }

  int height = state->tft->height();

  int x = state->tft->width() / 2 - state->price_sprite_a->width() / 2;
  int y = height / 2 - state->price_sprite_a->height() / 2;

  state->current_frame++;
  if (state->current_frame >= state->target_frame)
  {
    state->current_frame = state->target_frame;
  }

  int steps = height * state->current_frame / state->target_frame;

  if (state->direction == DIRECTION_DOWN)
  {
    state->price_sprite_a->pushSprite(x, y - steps);
    state->price_sprite_b->pushSprite(x, y + height - steps);
  }
  else if (state->direction == DIRECTION_UP)
  {
    state->price_sprite_b->pushSprite(x, y - height + steps);
    state->price_sprite_a->pushSprite(x, y + steps);
  }
}