// atkinson dithering high res printing, 160x120 blow up!

#define PICTURE_BUTTON      GPIO_NUM_7
#define PICTURE_BUTTON2     GPIO_NUM_10

void initializeScreenAndCamera();
void processFrameData();
void processFrame();

void setup() {
  gpio_set_direction(PICTURE_BUTTON, GPIO_MODE_INPUT); 
  gpio_set_pull_mode(PICTURE_BUTTON, GPIO_PULLUP_ONLY);
  
  gpio_set_direction(PICTURE_BUTTON2, GPIO_MODE_INPUT); 
  gpio_set_pull_mode(PICTURE_BUTTON2, GPIO_PULLUP_ONLY);

  initializeScreenAndCamera();

  Serial0.begin(9600);  
}


void loop() {
  processFrameData();
  if (digitalRead(PICTURE_BUTTON) == LOW) {
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
    
    Serial0.println("                Das wird teuer f\x81r Sie.");
    processFrame();
    Serial0.write("\n\n\n\n", 4);
  }
}
