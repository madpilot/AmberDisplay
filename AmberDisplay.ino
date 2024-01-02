#include <WiFiClientSecure.h>
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "price.h"
#include "splash.h"
#include "config.h"
#include "render.h"

#include "Button2.h"

#include <TFT_eSPI.h>

#define BUFFER_SIZE 1024
#define WIFI_ATTEMPTS 10
#define TIMER_DELAY 60000

#define BUTTON_1 35
#define BUTTON_2 0

typedef enum
{
  SCREEN_SPLASH,
  SCREEN_CONTROLLED_LOAD,
  SCREEN_GENERAL,
  SCREEN_FEED_IN,
} screens_t;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite price_sprite_a = TFT_eSprite(&tft);
TFT_eSprite price_sprite_b = TFT_eSprite(&tft);

Button2 button_1(BUTTON_1);
Button2 button_2(BUTTON_2);

bool animating = false;
bool first_run = false;

price_t general = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0};
price_t feed_in = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0};
price_t controlled_load = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0};

channels_t channels = {
    .general = general,
    .feed_in = feed_in,
    .controlled_load = controlled_load};

screens_t current_screen = SCREEN_SPLASH;

void set_clock()
{
  configTime(0, 0, "pool.ntp.org");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now_secs = time(nullptr);
  int attempts = 0;
  while (now_secs < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(F("."));
    if (attempts % 2 == 0)
    {
      tft.fillRect(93, 125, 54, 3, TFT_AMBER_GREEN);
    }
    else
    {
      tft.fillRect(93, 125, 54, 3, TFT_AMBER_DARK_BLUE);
    }
    attempts++;
    yield();
    now_secs = time(nullptr);
  }
  tft.fillRect(93, 125, 54, 3, TFT_AMBER_GREEN);

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&now_secs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}

unsigned long last_run = 0;
bool connect()
{
  Serial.print("Connecting to WiFi");
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_ATTEMPTS)
  {
    delay(500);
    Serial.print(".");
    if (attempts % 2 == 0)
    {
      tft.fillRect(26, 125, 54, 3, TFT_AMBER_GREEN);
    }
    else
    {
      tft.fillRect(26, 125, 54, 3, TFT_AMBER_DARK_BLUE);
    }
    attempts++;
  }
  tft.fillRect(26, 125, 54, 3, TFT_AMBER_GREEN);

  Serial.println("");

  if (attempts == WIFI_ATTEMPTS)
  {
    Serial.println("Unable to connect to the WiFi");
    return false;
  }
  else
  {
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    set_clock();
    return true;
  }
}

channels_t fetch()
{
  WiFiClientSecure client;
  client.setInsecure();
  HttpClient http(client);
  int err = 0;

  String path = String("/v1/sites/") + String(SITE_ID) + String("/prices/current");

  http.beginRequest();
  err = http.startRequest("api.amber.com.au", 443, path.c_str(), HTTP_METHOD_GET, "ArduinoAmberLight");
  if (err != HTTP_SUCCESS)
  {
    switch (err)
    {
    case HTTP_ERROR_CONNECTION_FAILED:
      Serial.println("ERROR: Unable to connect to the API");
      break;
    case HTTP_ERROR_API:
      Serial.println("ERROR: Configuration issue");
      break;
    case HTTP_ERROR_TIMED_OUT:
      Serial.println("ERROR: Connection timed out");
      break;
    case HTTP_ERROR_INVALID_RESPONSE:
      Serial.println("ERROR: Invalid response received");
      break;
    }
    http.stop();
    return channels;
  }

  String bearerToken = String("Bearer ") + String(API_KEY);
  http.sendHeader("Authorization", bearerToken.c_str());
  http.endRequest();

  err = http.responseStatusCode();
  if (err != 200)
  {
    Serial.printf("ERROR: Received Status Code: %d\n", err);
    http.stop();
    return channels;
  }

  err = http.skipResponseHeaders();
  if (err != HTTP_SUCCESS)
  {
    Serial.printf("ERROR: Unable to skip headers: %d\n", err);
    http.stop();
    return channels;
  }

  unsigned long timeout_start = millis();
  char json[BUFFER_SIZE];
  memset((char *)&json, 0, BUFFER_SIZE);
  int i = 0;
  // Leave one character at the end, so the string is always terminated
  while (i < BUFFER_SIZE - 1 && (http.connected() || http.available()) && ((millis() - timeout_start) < 30000))
  {
    if (http.available())
    {
      json[i++] = http.read();
      // Print out this character
      // We read something, reset the timeout counter
      timeout_start = millis();
    }
    else
    {
      // We haven't got any data, so let's pause to allow some to
      // arrive
      delay(500);
    }
  }
  http.stop();

  StaticJsonDocument<BUFFER_SIZE> doc;
  deserializeJson(doc, (char *)&json);
  if (!doc.is<JsonArray>())
  {
    Serial.println("Returned JSON object is not an array");
    return channels;
  }

  JsonArray jsonChannels = doc.as<JsonArray>();
  for (JsonVariant c : jsonChannels)
  {
    if (c.is<JsonObject>())
    {
      JsonObject channel = c.as<JsonObject>();
      String channel_type = channel["channelType"].as<String>();
      price_t *price = NULL;
      if (channel_type == String("general"))
      {
        price = &(channels.general);
      }
      else if (channel_type == String("feedIn"))
      {
        price = &(channels.feed_in);
      }
      else if (channel_type == String("controlledLoad"))
      {
        price = &(channels.controlled_load);
      }
      else
      {
        continue;
      }

      price->price = channel["perKwh"].as<float>();

      String descriptor = channel["descriptor"].as<String>();
      if (descriptor == String("spike"))
      {
        price->descriptor = DESCRIPTOR_SPIKE;
      }
      else if (descriptor == String("high"))
      {
        price->descriptor = DESCRIPTOR_HIGH;
      }
      else if (descriptor == String("neutral"))
      {
        price->descriptor = DESCRIPTOR_NEUTRAL;
      }
      else if (descriptor == String("low"))
      {
        price->descriptor = DESCRIPTOR_LOW;
      }
      else if (descriptor == String("veryLow"))
      {
        price->descriptor = DESCRIPTOR_VERY_LOW;
      }
      else
      {
        price->descriptor = DESCRIPTOR_EXTREMELY_LOW;
      }
    }
  }

  return channels;
}

void place_sprites()
{
  int offset = 0;
  int height = tft.height();
  int x = tft.width() / 2 - price_sprite_a.width() / 2;
  int y = tft.height() / 2 - price_sprite_a.height() / 2;
  price_sprite_a.pushSprite(x, y);
}

void onUp(Button2 &btn)
{
  if (current_screen == SCREEN_GENERAL)
  {
    if (channels.controlled_load.descriptor != DESCRIPTOR_UNKNOWN)
    {
      current_screen = SCREEN_CONTROLLED_LOAD;
      render_controlled_load_price(&price_sprite_a, &(channels.controlled_load));
    }
  }
  else if (current_screen == SCREEN_FEED_IN)
  {
    if (channels.general.descriptor != DESCRIPTOR_UNKNOWN)
    {
      current_screen = SCREEN_GENERAL;
      render_general_price(&price_sprite_a, &(channels.general));
    }
    else if (channels.controlled_load.descriptor != DESCRIPTOR_UNKNOWN)
    {
      current_screen = SCREEN_CONTROLLED_LOAD;
      render_controlled_load_price(&price_sprite_a, &(channels.controlled_load));
    }
  }
  place_sprites();
}

void onDown(Button2 &btn)
{
  if (current_screen == SCREEN_GENERAL)
  {
    Serial.println("Current Screen: General");
    if (channels.feed_in.descriptor != DESCRIPTOR_UNKNOWN)
    {
      current_screen = SCREEN_FEED_IN;
      Serial.println("Rendering Feed in");
      render_feed_in(&price_sprite_a, &(channels.feed_in));
    }
  }
  else if (current_screen == SCREEN_CONTROLLED_LOAD)
  {
    if (channels.general.descriptor != DESCRIPTOR_UNKNOWN)
    {
      current_screen = SCREEN_GENERAL;
      render_general_price(&price_sprite_a, &(channels.general));
    }
    else if (channels.feed_in.descriptor != DESCRIPTOR_UNKNOWN)
    {
      current_screen = SCREEN_FEED_IN;
      render_feed_in(&price_sprite_a, &(channels.feed_in));
    }
  }
  place_sprites();
}

void setup()
{
  Serial.begin(115200);
  // Force a run on startup
  last_run = TIMER_DELAY;

  button_1.setPressedHandler(onUp);
  button_2.setPressedHandler(onDown);

  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 240, 135, splash_image);
  first_run = true;
  WiFi.begin(WIFI_SSID, WIFI_PASSKEY);
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED && !connect())
  {
    tft.pushImage(0, 0, 240, 135, splash_image);
    tft.fillRect(26, 125, 54, 3, TFT_AMBER_RED);
    Serial.println("WiFi connection attempts timed out. Will try again in a second.");
    delay(1000);
    return;
  }

  if ((millis() - last_run) > TIMER_DELAY)
  {
    Serial.println("Checking the price");
    channels_t channels = fetch();

    if (price_sprite_a.created())
    {
      price_sprite_a.deleteSprite();
    }
    price_sprite_a.createSprite(tft.height(), tft.height());
    price_sprite_a.setTextSize(3);
    price_sprite_a.setCursor(0, 0);
    price_sprite_a.setTextDatum(MC_DATUM);
    price_sprite_a.setSwapBytes(true);

    if (price_sprite_b.created())
    {
      price_sprite_b.deleteSprite();
    }

    price_sprite_b.createSprite(tft.height(), tft.height());
    price_sprite_b.setTextSize(3);
    price_sprite_b.setCursor(0, 0);
    price_sprite_b.setTextDatum(MC_DATUM);
    price_sprite_b.setSwapBytes(true);

    if (first_run)
    {
      current_screen = SCREEN_GENERAL;
    }

    if (current_screen == SCREEN_GENERAL)
    {
      render_general_price(&price_sprite_a, &(channels.general));
    }
    else if (current_screen == SCREEN_FEED_IN)
    {
      render_feed_in(&price_sprite_a, &(channels.feed_in));
    }
    else if (current_screen == SCREEN_CONTROLLED_LOAD)
    {
      render_controlled_load_price(&price_sprite_a, &(channels.controlled_load));
    }

    if (first_run)
    {
      first_run = false;
      tft.fillRect(0, 0, 240, 135, TFT_AMBER_DARK_BLUE);
    }

    place_sprites();
    last_run = millis();
  }

  button_1.loop();
  button_2.loop();
}
