#include <WiFiClientSecure.h>
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "splash.h"
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
#define TIMER_DELAY 300000

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite general_price_sprite = TFT_eSprite(&tft);

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

void setClock() {
  configTime(0, 0, "pool.ntp.org");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  int attempts = 0;
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    if(attempts % 2 == 0) {
      tft.fillRect(93, 125, 54, 3, TFT_AMBER_GREEN);
    } else {
      tft.fillRect(93, 125, 54, 3, TFT_AMBER_DARK_BLUE);
    }
    attempts++;
    yield();
    nowSecs = time(nullptr);
  }
  tft.fillRect(93, 125, 54, 3, TFT_AMBER_GREEN);

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}


unsigned long lastRun = 0;
unsigned long renderLastRun = 0;
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
    setClock();  
    return true;
  }
}

price_t fetch()
{
  WiFiClientSecure client;
  client.setInsecure();
  HttpClient http(client);
  int err = 0;

  String path = String("/v1/sites/") + String(SITE_ID) + String("/prices/current");

  price_t price = {
    .descriptor = DESCRIPTOR_UNKNOWN,
    .price = 0
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
    return price;
  }

  String bearerToken = String("Bearer ") + String(API_KEY);
  http.sendHeader("Authorization", bearerToken.c_str());
  http.endRequest();

  err = http.responseStatusCode();
  if (err != 200)
  {
    Serial.printf("ERROR: Received Status Code: %d\n", err);
    http.stop();
    return price;
  }

  err = http.skipResponseHeaders();
  if (err != HTTP_SUCCESS)
  {
    Serial.printf("ERROR: Unable to skip headers: %d\n", err);
    http.stop();
    return price;
  }

  
  unsigned long timeoutStart = millis();
  char json[BUFFER_SIZE];
  memset((char *)&json, 0, BUFFER_SIZE);
  int i = 0;
  // Leave one character at the end, so the string is always terminated
  while (i < BUFFER_SIZE - 1 && (http.connected() || http.available()) && ((millis() - timeoutStart) < 30000))
  {
    if (http.available())
    {
      json[i++] = http.read();
      // Print out this character
      // We read something, reset the timeout counter
      timeoutStart = millis();
    }
    else
    {
      // We haven't got any data, so let's pause to allow some to
      // arrive
      delay(1000);
    }
  }

  http.stop();

  StaticJsonDocument<BUFFER_SIZE> doc;
  deserializeJson(doc, (char *)&json);
  if (!doc.is<JsonArray>())
  {
    Serial.println("Returned JSON object is not an array");
    return price;
  }

  JsonArray channels = doc.as<JsonArray>();
  for (JsonVariant c : channels)
  {
    if (c.is<JsonObject>())
    {
      JsonObject channel = c.as<JsonObject>();
      String channelType = channel["channelType"].as<String>();
      if (channelType == "general")
      {
        price.price = channel["perKwh"].as<float>();
        String descriptor = channel["descriptor"].as<String>();
        if (descriptor == String("spike"))
        {
          price.descriptor = DESCRIPTOR_SPIKE;
        }
        else if (descriptor == String("high"))
        {
          price.descriptor = DESCRIPTOR_HIGH;
        }
        else if (descriptor == String("neutral"))
        {
          price.descriptor = DESCRIPTOR_NEUTRAL;
        }
        else if (descriptor == String("low"))
        {
          price.descriptor = DESCRIPTOR_LOW;
        }
        else if (descriptor == String("veryLow"))
        {
          price.descriptor = DESCRIPTOR_VERY_LOW;
        }
        else
        {
          price.descriptor = DESCRIPTOR_EXTREMELY_LOW;
        }
        return price;
      }
    }
  }

  Serial.println("Error: General channel not found");
  return price;
}

void renderPrice(TFT_eSprite *sprite, float price, uint16_t text_colour, uint16_t circle_colour) {
  sprite->setTextColor(text_colour);
  sprite->fillCircle(sprite->width() / 2, sprite->height() / 2, (sprite->height() / 2) - 5, circle_colour);
  
  char formatted[5];
  char *ptr = (char *)&formatted;
  memset(ptr, 0, 5);
  if(price > 100) {
    snprintf(ptr, 5, "$%g", round(price / 100));
  } else {
    snprintf(ptr, 5, "%gc", round(price));
  }
  sprite->drawString(ptr,  sprite->width() / 2, sprite->height() / 2);
}

void setup()
{
  Serial.begin(115200);
  // Force a run on startup
  lastRun = TIMER_DELAY;

  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 240, 135, splash_image);
  general_price_sprite.createSprite(tft.height(), tft.height());
  general_price_sprite.setTextSize(3);
  general_price_sprite.setCursor(0, 0); 
  general_price_sprite.setTextDatum(MC_DATUM);

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


  if ((millis() - lastRun) > TIMER_DELAY)
  {
    Serial.println("Checking the price");
    price_t price = fetch();

    tft.fillScreen(TFT_AMBER_DARK_BLUE);
    general_price_sprite.fillScreen(TFT_AMBER_DARK_BLUE);

    if (price.descriptor != DESCRIPTOR_UNKNOWN)
    {
      
      if (price.descriptor == DESCRIPTOR_SPIKE)
      {
        Serial.printf("Price Spike!\n");
        renderPrice(&general_price_sprite, price.price, TFT_WHITE, TFT_AMBER_DARK_RED);
      }
      else if (price.descriptor == DESCRIPTOR_HIGH)
      {
        Serial.printf("High prices\n");
        renderPrice(&general_price_sprite, price.price, TFT_BLACK, TFT_AMBER_RED);
      }
      else if (price.descriptor == DESCRIPTOR_NEUTRAL)
      {
        Serial.printf("Average prices\n");
        renderPrice(&general_price_sprite, price.price, TFT_BLACK, TFT_AMBER_ORANGE);
        
      }
      else if (price.descriptor == DESCRIPTOR_LOW)
      {
        Serial.printf("Low prices\n");
        renderPrice(&general_price_sprite, price.price, TFT_BLACK, TFT_AMBER_YELLOW);
      }
      else if (price.descriptor == DESCRIPTOR_VERY_LOW)
      {
        Serial.printf("Very Low prices\n");
        renderPrice(&general_price_sprite, price.price, TFT_BLACK, TFT_AMBER_GREEN);
      }
      else
      {
        Serial.printf("Extremely low prices\n");
        renderPrice(&general_price_sprite, price.price, TFT_BLACK, TFT_AMBER_GREEN);
      }

      general_price_sprite.pushSprite(tft.width() / 2 - general_price_sprite.width() / 2, 0);
    }
    
    lastRun = millis();
  }
}
