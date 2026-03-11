#include "esp_camera.h"
#include "img_converters.h"

#define PICTURE_BUTTON      41

// OLED
#define PIN_CS              21
#define PIN_RST             14
#define PIN_DC              47

// ov3660 CAM
#define PWDN_GPIO_NUM       -1
#define RESET_GPIO_NUM      -1
#define XCLK_GPIO_NUM       15
#define SIOD_GPIO_NUM        4
#define SIOC_GPIO_NUM        5
#define Y2_GPIO_NUM         11
#define Y3_GPIO_NUM          9
#define Y4_GPIO_NUM          8
#define Y5_GPIO_NUM         10
#define Y6_GPIO_NUM         12
#define Y7_GPIO_NUM         18
#define Y8_GPIO_NUM         17
#define Y9_GPIO_NUM         16
#define VSYNC_GPIO_NUM       6
#define HREF_GPIO_NUM        7
#define PCLK_GPIO_NUM       13

#include <Adafruit_GFX.h>
#include "Adafruit_SSD1331.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

Adafruit_SSD1331 display = Adafruit_SSD1331(PIN_CS, PIN_DC, PIN_RST);

const uint16_t width  = 360;
const uint16_t height = 360;
uint8_t img[width * height];

void printer_init() {
  // reset
  Serial.write("\x1B\x40", 2);
  // linefeed to 0
  Serial.write("\x1B\x33\x00", 3);
  // Codepage 858
  Serial.write("\x1B\x74\x13", 3);
  // font B
  Serial.write("\x1B\x4D\x01", 3);
  // font CPI mode
  Serial.write("\x1B\xC1\x01", 3);
  // rotate text 180
  Serial.write("\x1B\x7B\x01", 3);
}

static inline uint8_t rgb565_to_gray(uint16_t *rgb565)
{
    uint8_t r = (*rgb565 >> 3) & 0x1F;
    uint8_t g = ((*rgb565 << 3) & 0x38) | ((*rgb565 >> 13) & 0x07);
    uint8_t b = (*rgb565 >> 8) & 0x1F;

    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    uint16_t gray = (77 * r + 150 * g + 29 * b) >> 8;
    return (uint8_t)gray;
}

/**
 * 240x240 -> scaled in 360x360 (img)
 */
void blow_up()
{
    int sx,sy;
    for(int y=(height-1); y>=0; --y)
    {
        sy = (float)y * 0.667;
        for(int x=0; x<width; ++x)
        {
            sx = (float)x * 0.667;
            img[y*width + x] = img[sy*width + sx];
        }
    }
}

void stream_oled()
{
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.printf("Camera framebuffer get fails!");
        delay(1000);
        return;
    }
    display.goTo(0, 0);
    uint16_t *data = (uint16_t *)fb->buf;

    for(int y=0; y<240; ++y)
    {
        for(int x=0; x<240; ++x)
        {
            img[x*width +y] = rgb565_to_gray(data);

            // this shrinks it down to 96x64 pixel
            if (y%4 == 0 || y<3 || y>237) {
                if (x%4 == 0 || x<3 || x>237) {
                    display.writeData(*data);
                    display.writeData((*data)>>8);
                }
                if (x == 239) {
                    // fill 32 pixel black
                    for (int i = 0;i< 64; ++i) display.writeData(0);
                } 
            }
            data++;
        }

    }
    esp_camera_fb_return(fb);  
}


void dither_atkinson ()
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int i = y * width + x;
            uint8_t old = img[i];
            uint8_t act = (old < 128) ? 0 : 255;
            img[i] = act;

            int err = (int)old - (int)act;
            int diff = err / 8;

            // y, x+1
            if (x + 1 < width)
                img[i + 1] = (uint8_t)CLAMP(img[i + 1] + diff, 0, 255);

            // y, x+2
            if (x + 2 < width)
                img[i + 2] = (uint8_t)CLAMP(img[i + 2] + diff, 0, 255);

            // y+1, x-1
            if (y + 1 < height && x - 1 >= 0)
                img[i + width - 1] = (uint8_t)CLAMP(img[i + width - 1] + diff, 0, 255);

            // y+1, x
            if (y + 1 < height)
                img[i + width] = (uint8_t)CLAMP(img[i + width] + diff, 0, 255);

            // y+1, x+1
            if (y + 1 < height && x + 1 < width)
                img[i + width + 1] = (uint8_t)CLAMP(img[i + width + 1] + diff, 0, 255);

            // y+2, x
            if (y + 2 < height)
                img[i + 2 * width] = (uint8_t)CLAMP(img[i + 2 * width] + diff, 0, 255);
        }
    }
}


void print_raster ()
{
    int lines = height / 24;
    uint8_t nL = width & 0xFF;
    uint8_t nH = (width >> 8) & 0xFF;
    
    for (int line = 0; line < lines; ++line)
    {
        Serial.write("\x1B\x2A\x21", 3);
        Serial.write(nL);
        Serial.write(nH);

        for (int x = 0; x < width; x+=3)
        {
            for (int i = 0; i < 3; ++i)
            {
                for (int b = 0; b < 3; ++b)
                {
                    uint8_t byt = 0;
                    for (int y = 0; y < 8; ++y)
                    {
                        int posY = line * 24 + y + b*8;
                        if (posY >= height) continue;
                        if ((x+i) < width) {
                            uint8_t pixel = img[posY * width + x + i];
                            if (pixel < 128) {
                                byt |= (1 << (7 - y));
                            }
                        }
                    }
                    Serial.write(byt);
                }
            }
        }        
        Serial.write(10);
    }
}

void setup (void)
{
    pinMode(PICTURE_BUTTON, INPUT_PULLUP);

    Serial.begin(9600);
    display.begin();

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_240X240;    // QVGA is 320x240 or use 96X96
    config.pixel_format = PIXFORMAT_RGB565;   // YUV422 or RGB565
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID)
    {
        s->set_vflip(s, 1);
    }
}

void loop()
{
  stream_oled();
  if (digitalRead(PICTURE_BUTTON) == LOW) {
    printer_init();
    Serial.println("    Das wird teuer f\x81r Sie.");
    blow_up();
    dither_atkinson();
    print_raster();
    Serial.write("\n\n\n\n", 4);
    delay(250);
  }
}
