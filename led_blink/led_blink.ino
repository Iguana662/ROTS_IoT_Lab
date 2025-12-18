#include <Arduino.h>

#define LED_PIN 2

void led_blinky(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  
  while(1) {                         
    digitalWrite(LED_PIN, HIGH);   
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    digitalWrite(LED_PIN, LOW);   
    
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);

  // 2. Create the Task
  xTaskCreate(led_blinky,"blink led", 1000, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL); // kill loop
}