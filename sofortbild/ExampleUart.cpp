void initializeScreenAndCamera();
void processFrame();

#include "Arduino.h"
#include "CamYUV.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

uint8_t gray;

const uint16_t width  = 320;
const uint16_t height = 240;
CamYUV camera(CamYUV::RESOLUTION_QQVGA_160x120, 2);

uint8_t img[width * height];


static const uint8_t bayer[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

uint8_t bayerThreshold (uint16_t x, uint16_t y)
{
    // (value + 0.5) * 255 / 16  →  round‑to‑nearest
    return (bayer[y & 3][x & 3] * 255 + 127) / 16;
}

// this is called in Arduino setup() function
void initializeScreenAndCamera() {
  camera.init();
  delay(1000);
}

uint8_t formatPixelByteGrayscaleFirst(uint8_t pixelByte) {
  // For the First byte in the parity chek byte pair the last bit is always 0.
  pixelByte &= 0b11111110;
  if (pixelByte == 0) {
    // Make pixel color always slightly above 0 since zero is a command marker.
    pixelByte |= 0b00000010;
  }
  return pixelByte;
}

uint8_t formatPixelByteGrayscaleSecond(uint8_t pixelByte) {
  // For the second byte in the parity chek byte pair the last bit is always 1.
  return pixelByte | 0b00000001;
}

void processFrameData() {
  noInterrupts();
  camera.waitForVsync();

  camera.ignoreVerticalPadding();

  for (uint16_t y = 0; y < height/2; y++) {
    camera.ignoreHorizontalPaddingLeft();

    uint16_t x = 0;
    while ( x < width/2)
    {
      camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte   
      camera.readPixelByte(gray);
      img[2*y*width + 2*x] = formatPixelByteGrayscaleFirst(gray);
      img[2*y*width + 2*x+1]     = img[2*y*width + 2*x];
      img[(2*y+1)*width + 2*x+1] = img[2*y*width + 2*x];
      img[(2*y+1)*width + 2*x]   = img[2*y*width + 2*x];

      camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
      x++;

      camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
      camera.readPixelByte(gray);
      img[2*y*width + 2*x] = formatPixelByteGrayscaleSecond(gray);
      img[2*y*width + 2*x+1]     = img[2*y*width + 2*x];
      img[(2*y+1)*width + 2*x+1] = img[2*y*width + 2*x];
      img[(2*y+1)*width + 2*x]   = img[2*y*width + 2*x];

      camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
      x++;
    }
    camera.ignoreHorizontalPaddingRight();

  }
  interrupts();

}

void ditherAtkinson ()
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            int i = y * width + x;
            uint8_t old = img[i];
            uint8_t neu = (old < 128) ? 0 : 255;
            img[i] = neu;

            int err = (int)old - (int)neu;
            int diff = err / 8;  // Fehleranteil pro Nachbar

            // Atkinson-Maske:
            //       X   1   1
            //   1   1   1
            //       1

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

void printRaster ()
{
    int lines = height / 24;
    uint8_t nL = width & 0xFF;
    uint8_t nH = (width >> 8) & 0xFF;
    
    for (int line = 0; line < lines; ++line)
    {
        Serial0.write("\x1B\x2A\x21", 3);
        Serial0.write(nL);
        Serial0.write(nH);

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
                    Serial0.write(byt);
                }
            }
        }        

        Serial0.write(10);
    }
}

void printRasterFast ()
{
    uint32_t cols  = width/2;
    uint8_t nL = cols & 0xFF;
    uint8_t nH = (cols >> 8) & 0xFF;
    
    for (int y = 0; y < height/3; y+=8)
    {        
        Serial0.write("\x1B\x2A\x00", 3);
        Serial0.write(nL);
        Serial0.write(nH);

        for (int x = 0; x < cols; ++x)
        {
           uint8_t byt = 0;
           for (int i = 0; i < 8; ++i)
            {
                int posY = y + i;
                if (posY >= height/3) continue;
                
                uint8_t pixel = img[3*posY*width + 2*x];
                uint8_t th = bayerThreshold(x, posY);
                if (pixel < th) {
                    byt |= (1 << (7 - i));
                }
            }
            Serial0.write(byt);
        }      

        Serial0.write(10);
    }
}


// this is called in Arduino loop() function
void processFrame(int modus) {
  if (modus == 0) {
    ditherAtkinson();
    printRaster();
  } else {
    printRasterFast();
  }
}
