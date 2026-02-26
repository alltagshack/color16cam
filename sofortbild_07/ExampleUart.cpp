void initializeScreenAndCamera();
void processFrame();

#include "Arduino.h"
#include "CamYUV.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

uint8_t gray;

const uint16_t lineLength = 320;
const uint16_t lineCount = 240;
CamYUV camera(CamYUV::RESOLUTION_QQVGA_160x120, 2);

uint8_t orginal    [lineLength * lineCount];


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

  for (uint16_t y = 0; y < lineCount/2; y++) {
    camera.ignoreHorizontalPaddingLeft();

    uint16_t x = 0;
    while ( x < lineLength/2)
    {
      camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte   
      camera.readPixelByte(gray);
      orginal[2*y*lineLength + 2*x] = formatPixelByteGrayscaleFirst(gray);
      orginal[2*y*lineLength + 2*x+1]     = orginal[2*y*lineLength + 2*x];
      orginal[(2*y+1)*lineLength + 2*x+1] = orginal[2*y*lineLength + 2*x];
      orginal[(2*y+1)*lineLength + 2*x]   = orginal[2*y*lineLength + 2*x];

      camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
      x++;

      camera.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
      camera.readPixelByte(gray);
      orginal[2*y*lineLength + 2*x] = formatPixelByteGrayscaleSecond(gray);
      orginal[2*y*lineLength + 2*x+1]     = orginal[2*y*lineLength + 2*x];
      orginal[(2*y+1)*lineLength + 2*x+1] = orginal[2*y*lineLength + 2*x];
      orginal[(2*y+1)*lineLength + 2*x]   = orginal[2*y*lineLength + 2*x];

      camera.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
      x++;
    }
    camera.ignoreHorizontalPaddingRight();

  }
  interrupts();

}

void ditherAtkinson (uint8_t *img, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            int i = y * w + x;
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
            if (x + 1 < w)
                img[i + 1] = (uint8_t)CLAMP(img[i + 1] + diff, 0, 255);

            // y, x+2
            if (x + 2 < w)
                img[i + 2] = (uint8_t)CLAMP(img[i + 2] + diff, 0, 255);

            // y+1, x-1
            if (y + 1 < h && x - 1 >= 0)
                img[i + w - 1] = (uint8_t)CLAMP(img[i + w - 1] + diff, 0, 255);

            // y+1, x
            if (y + 1 < h)
                img[i + w] = (uint8_t)CLAMP(img[i + w] + diff, 0, 255);

            // y+1, x+1
            if (y + 1 < h && x + 1 < w)
                img[i + w + 1] = (uint8_t)CLAMP(img[i + w + 1] + diff, 0, 255);

            // y+2, x
            if (y + 2 < h)
                img[i + 2 * w] = (uint8_t)CLAMP(img[i + 2 * w] + diff, 0, 255);
        }
    }
}

void printRaster (uint8_t *img, uint16_t width, int height)
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


// this is called in Arduino loop() function
void processFrame() {
  processFrameData();

  ditherAtkinson(orginal, lineLength, lineCount);
  printRaster(orginal, lineLength, lineCount);
}
