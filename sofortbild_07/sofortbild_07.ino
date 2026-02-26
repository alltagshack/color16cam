// atkinson dithering high res printing, 160x120 blow up!

void initializeScreenAndCamera();
void processFrame();

#include <Wire.h>

#define PICTURE_BUTTON      GPIO_NUM_7

void setup() {
  Serial0.begin(9600);
  
  initializeScreenAndCamera();

  Wire.beginTransmission(0x21);
  Wire.write(0x13);
  Wire.write(0x20);
  Wire.endTransmission();

  // reset
  Serial0.write("\x1B\x40", 2);
  // linefeed to 0
  Serial0.write("\x1B\x33\x00", 3);
  // Codepage 858
  Serial0.write("\x1B\x74\x13", 3);
  // font B
  Serial0.write("\x1B\x4D\x01", 3);
  // font CPI mode
  Serial0.write("\x1B\xC1\x01", 3);
  // rotate text 180
  Serial0.write("\x1B\x7B\x01", 3);
  
  gpio_set_direction(PICTURE_BUTTON, GPIO_MODE_INPUT); 
  gpio_set_pull_mode(PICTURE_BUTTON, GPIO_PULLUP_ONLY);
}


void loop() {
  if (digitalRead(PICTURE_BUTTON) == LOW) {
    Serial0.println("Das wird teuer f\x81r Sie.");
    processFrame();
    Serial0.write("\n\n\n\n", 4);
  }
}
