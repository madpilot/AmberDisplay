#ifndef _PRICE_h
#define _PRICE_h
typedef enum
{
  DESCRIPTOR_UNKNOWN,
  DESCRIPTOR_EXTREMELY_LOW,
  DESCRIPTOR_VERY_LOW,
  DESCRIPTOR_LOW,
  DESCRIPTOR_NEUTRAL,
  DESCRIPTOR_HIGH,
  DESCRIPTOR_SPIKE,
} price_descriptor_t;

typedef struct _price
{
  price_descriptor_t descriptor;
  float price;
} price_t;

typedef struct _channels
{
  price_t general;
  price_t feed_in;
  price_t controlled_load;
} channels_t;
#endif