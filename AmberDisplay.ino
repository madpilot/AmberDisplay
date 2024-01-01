#include <WiFiClientSecure.h>
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "splash.h"
#include "price_extremely_low.h"
#include "price_very_low.h"
#include "price_low.h"
#include "price_neutral.h"
#include "price_high.h"
#include "price_spike.h"
#include "font_large.h"
#include "config.h"

#include <TFT_eSPI.h>

#define TFT_AMBER_DARK_BLUE 0x1969
#define TFT_AMBER_GREEN 0x07F4
#define TFT_AMBER_YELLOW 0xFEE9
#define TFT_AMBER_ORANGE 0xFD41
#define TFT_AMBER_RED 0xF38C
#define TFT_AMBER_DARK_RED 0xB8C1

#define BUFFER_SIZE 1024
#define WIFI_ATTEMPTS 10
#define TIMER_DELAY 60000

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite price_sprite_a = TFT_eSprite(&tft);
TFT_eSprite price_sprite_b = TFT_eSprite(&tft);


typedef enum {
  DESCRIPTOR_UNKNOWN,
  DESCRIPTOR_EXTREMELY_LOW,
  DESCRIPTOR_VERY_LOW,
  DESCRIPTOR_LOW,
  DESCRIPTOR_NEUTRAL,
  DESCRIPTOR_HIGH,
  DESCRIPTOR_SPIKE,
} price_descriptor_t;

typedef struct _price {
  price_descriptor_t descriptor;
  float price;
} price_t;

typedef struct _channels {
  price_t general;
  price_t feed_in;
  price_t controlled_load;
} channels_t;

void set_clock() {
  configTime(0, 0, "pool.ntp.org");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now_secs = time(nullptr);
  int attempts = 0;
  while (now_secs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    if(attempts % 2 == 0) {
      tft.fillRect(93, 125, 54, 3, TFT_AMBER_GREEN);
    } else {
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
    if(attempts % 2 == 0) {
      tft.fillRect(26, 125, 54, 3, TFT_AMBER_GREEN);
    } else {
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

  price_t general = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0
  };
  price_t feed_in = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0
  };
  price_t controlled_load = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0
  };

  channels_t channels = {
    .general = general,
    .feed_in = feed_in,
    .controlled_load = controlled_load
  };
  
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
      else if(channel_type == String("feedIn")) 
      {
        price = &(channels.feed_in);
      } 
      else if(channel_type == String("controlledLoad")) 
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

void render_price(TFT_eSprite *sprite, float price, uint16_t text_colour, const unsigned short (*background)[18225]) {
  sprite->loadFont(NotoSansBold36);
  sprite->setTextColor(text_colour);
  sprite->pushImage(0, 0, sprite->width(), sprite->height(), *background);
  char formatted[7];
  char *ptr = (char *)&formatted;
  memset(ptr, 0, 7);
  if(abs(price) > 100) {
    snprintf(ptr, 7, "$%g", round(price / 100));
  } else {
    snprintf(ptr, 7, "%gc", round(price));
  }
  sprite->drawString(ptr, sprite->width() / 2, sprite->height() / 2);
  sprite->unloadFont();
}

void render_general_price(TFT_eSprite *sprite, price_t *price) {
  switch(price->descriptor) {
    case DESCRIPTOR_SPIKE: {
      Serial.printf("Price Spike!: %f\n", price->price);
      render_price(sprite, price->price, TFT_WHITE, &price_spike);
      break;
    }
    case DESCRIPTOR_HIGH: {
      Serial.printf("High prices: %f\n", price->price);
      render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_high);
      break;
    }
    case DESCRIPTOR_NEUTRAL: {
      Serial.printf("Average prices: %f\n", price->price);
      render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_neutral);
      break;
    }
    case DESCRIPTOR_LOW: {
      Serial.printf("Low prices: %f\n", price->price);
      render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_low);
      break;
    }
    case DESCRIPTOR_VERY_LOW: {
      Serial.printf("Very Low prices: %f\n", price->price);
      render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_very_low);
      break;
    }
    case DESCRIPTOR_EXTREMELY_LOW: {
      Serial.printf("Extremely low prices: %f\n", price->price);
      render_price(sprite, price->price, TFT_AMBER_DARK_BLUE, &price_extremely_low);
      break;
    }
    default: {
      Serial.printf("Unknown Price\n");
    }
  }
}

void render_feed_in(TFT_eSprite *sprite, price_t *price) {
  switch(price->descriptor) {
    case DESCRIPTOR_SPIKE: {
      Serial.printf("Price Spike!: %f\n", price->price);
      render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_low);
      break;
    }
    case DESCRIPTOR_HIGH: {
      Serial.printf("High prices: %f\n", price->price);
      render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_low);
      break;
    }
    case DESCRIPTOR_NEUTRAL: {
      Serial.printf("Average prices: %f\n", price->price);
      render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_low);
      break;
    }
    case DESCRIPTOR_LOW: {
      Serial.printf("Low prices: %f\n", price->price);
      render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_low);
      break;
    }
    case DESCRIPTOR_VERY_LOW: {
      Serial.printf("Very Low prices: %f\n", price->price);
      render_price(sprite, -1 * price->price, TFT_AMBER_DARK_BLUE, &price_low);
      break;
    }
    case DESCRIPTOR_EXTREMELY_LOW: {
      Serial.printf("Extremely low prices: %f\n", price->price);
      render_price(sprite, -1 * price->price, TFT_AMBER_RED, &price_low);
      break;
    }
    default: {
      Serial.printf("Unknown Price\n");
    }
  }
}

bool first_run = false;
void setup()
{
  Serial.begin(115200);
  // Force a run on startup
  last_run = TIMER_DELAY;

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
    
    if(price_sprite_a.created()) {
      price_sprite_a.deleteSprite();
    }
    price_sprite_a.createSprite(tft.height(), tft.height());
    price_sprite_a.setTextSize(3);
    price_sprite_a.setCursor(0, 0); 
    price_sprite_a.setTextDatum(MC_DATUM);
    price_sprite_a.setSwapBytes(true);

    if(price_sprite_b.created()) {
      price_sprite_b.deleteSprite();
    }
    
    price_sprite_b.createSprite(tft.height(), tft.height());
    price_sprite_b.setTextSize(3);
    price_sprite_b.setCursor(0, 0); 
    price_sprite_b.setTextDatum(MC_DATUM);
    price_sprite_b.setSwapBytes(true);

    if(first_run) {
      first_run = false;
      tft.fillRect(0, 0, 240, 135, TFT_AMBER_DARK_BLUE);
    }
    
    render_general_price(&price_sprite_a, &(channels.general));
    render_feed_in(&price_sprite_b, &(channels.feed_in));
    
    if(first_run) {
      first_run = false;
      tft.fillRect(0, 0, 240, 135, TFT_AMBER_DARK_BLUE);
    }
    price_sprite_a.pushSprite(tft.width() / 2 - price_sprite_a.width() / 2, tft.height() / 2 - price_sprite_a.height() / 2);
    price_sprite_b.pushSprite(tft.width() / 2 - price_sprite_b.width() / 2, (tft.height() / 2 - price_sprite_b.height() / 2) + tft.height());
    last_run = millis();
  }
}
