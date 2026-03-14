#include <Arduino.h>
#include "Camera.hpp"

extern "C" {
    void app_main();
}

#define PICTURE_BUTTON      7

// unused
#define PICTURE_BUTTON2    10

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

uint8_t gray;

const uint16_t width = 360;
const uint16_t height = 480;
uint8_t img[width * height];

Camera cam(Camera::RESOLUTION_QVGA_320x240, 8);

void blow_up()
{
    int sx,sy;
    for(int y=(width-1); y>=0; --y)
    {
        sy = (float)y * 0.6667;
        for(int x=(height-1); x>=0; --x)
        {
            sx = (float)x * 0.6667;
            img[y*height + x] = img[sy*height + sx];
        }
    }
}

uint8_t formatPixelByteFirst(uint8_t pixelByte)
{
    // For the First byte in the parity check byte pair the last bit is always 0.
    pixelByte &= 0b11111110;
    if (pixelByte == 0) {
        // Make pixel color always slightly above 0 since zero is a command marker.
        pixelByte |= 0b00000010;
    }
    return pixelByte;
}

uint8_t formatPixelByteSecond(uint8_t pixelByte)
{
    // For the second byte in the parity check byte pair the last bit is always 1.
    return pixelByte | 0b00000001;
}

void ditherAtkinson ()
{
    for (int y = 0; y < width; y++)
    {
        for (int x = 0; x < height; x++)
        {
            int i = y * height + x;
            uint8_t old = img[i];
            uint8_t act = (old < 128) ? 0 : 255;
            img[i] = act;

            int err = (int)old - (int)act;
            int diff = err / 8;

            // y, x+1
            if (x + 1 < height)
                img[i + 1] = (uint8_t)CLAMP(img[i + 1] + diff, 0, 255);

            // y, x+2
            if (x + 2 < height)
                img[i + 2] = (uint8_t)CLAMP(img[i + 2] + diff, 0, 255);

            // y+1, x-1
            if (y + 1 < width && x - 1 >= 0)
                img[i + height - 1] = (uint8_t)CLAMP(img[i + height - 1] + diff, 0, 255);

            // y+1, x
            if (y + 1 < width)
                img[i + height] = (uint8_t)CLAMP(img[i + height] + diff, 0, 255);

            // y+1, x+1
            if (y + 1 < width && x + 1 < height)
                img[i + height + 1] = (uint8_t)CLAMP(img[i + height + 1] + diff, 0, 255);

            // y+2, x
            if (y + 2 < width)
                img[i + 2 * height] = (uint8_t)CLAMP(img[i + 2 * height] + diff, 0, 255);
        }
    }
}

void printRaster ()
{
    int lines = height / 24;
    uint8_t nL = width & 0xFF;
    uint8_t nH = (width >> 8) & 0xFF;
    uint8_t hdr[5] = {0x1B, 0x2A, 0x21, nL, nH};

    for (int line = 0; line < lines; ++line)
    {
        Serial.write(hdr, sizeof(hdr));

        for (int y = 0; y < width; y+=3)
        {
            for (int i = 0; i < 3; ++i)
            {
                for (int b = 0; b < 3; ++b)
                {
                    uint8_t byt = 0;
                    for (int x = 0; x < 8; ++x)
                    {
                        int posX = line * 24 + x + b*8;
                        if (posX >= height) continue;
                        if ((y+i) < width) {
                            uint8_t pixel = img[(y + i)*height + posX];
                            if (pixel < 128) {
                                byt |= (1 << (7 - x));
                            }
                        }
                    }
                    Serial.write(byt);
                }
            }
        }
        Serial.write("\x0A", 1);
    }
}

void getPicture ()
{
    noInterrupts();
    cam.waitForVsync();

    cam.ignoreVerticalPadding();

    for (uint16_t y = 0; y < 240; ++y) {
        cam.ignoreHorizontalPaddingLeft();

        uint16_t x = 0;
        while (x < 320)
        {
            cam.waitForPixelClockRisingEdge(); // YUV422 grayscale byte   
            cam.readPixelByte(gray);
            img[(239-y)*height + x] = formatPixelByteFirst(gray);

            cam.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            x++;
            cam.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            cam.readPixelByte(gray);
            img[(239-y)*height + x] = formatPixelByteSecond(gray);

            cam.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            x++;
        }
        cam.ignoreHorizontalPaddingRight();
    }
    interrupts();
}

void printHint ()
{
    // reset
    Serial.write("\x1B\x40", 2);
    // Codepage 858
    Serial.write("\x1B\x74\x13", 3);
    // font B
    Serial.write("\x1B\x4D\x01", 3);
    // font CPI mode
    Serial.write("\x1B\xC1\x01", 3);
    // rotate text 180
    Serial.write("\x1B\x7B\x01", 3);
    // linefeed 0
    Serial.write("\x1B\x33\x00", 3);
    Serial.write("          Das wird teuer f\x81r Sie.\n", 34);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++

void app_main (void)
{
    cam.init();
    cam.setBrightness(62);
    cam.setContrast(70);

    pinMode(PICTURE_BUTTON, INPUT_PULLUP);
    Serial.begin(9600);

    for(;;)
    {
        getPicture();

        if (digitalRead(PICTURE_BUTTON) == LOW)
        {
            printHint();
            blow_up();
            ditherAtkinson();
            printRaster();
            Serial.write("\n\n\n\n", 4);
        }
        delay(250);
    }
}

