#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN GPIO_NUM_0
#define NUM_LEDS 1

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Update Neo Led 
void neo_update_task(void *pvParameters){
strip.begin();
  strip.setBrightness(50);
  uint16_t hue = 0;

  while(1) {
    uint32_t color = strip.gamma32(strip.ColorHSV(hue, 255, 255));
    strip.setPixelColor(0, color);
    strip.show();
    
    hue += 250; 
    
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

void setup() {
  xTaskCreate(neo_update_task, "LED Task", 2048, NULL, 1, NULL);
}

void loop() {
}