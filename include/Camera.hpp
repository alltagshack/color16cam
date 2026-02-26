#ifndef _CAMERA_HPP_
#define _CAMERA_HPP_ 1

#include "soc/gpio_reg.h"
#include "CameraRegisters.hpp"

class Camera {

public:

    enum Resolution {
        RESOLUTION_QVGA_320x240 = 320,
        RESOLUTION_QQVGA_160x120 = 160
    };

    enum PLLMultiplier {
        PLL_MULTIPLIER_BYPASS = 0,
        PLL_MULTIPLIER_X4 = 1,
        PLL_MULTIPLIER_X6 = 2,
        PLL_MULTIPLIER_X8 = 3
    };


protected:
    static const uint8_t i2cAddress = 0x21;

    const Resolution resolution;
    uint8_t internalClockPreScaler;
    PLLMultiplier pllMultiplier;
    CameraRegisters registers;
    uint8_t verticalPadding = 0;

public:

    Camera(
        Resolution resolution,
        uint8_t internalClockPreScaler,
        PLLMultiplier pllMultiplier = PLL_MULTIPLIER_BYPASS
    ) :
        resolution(resolution),
        internalClockPreScaler(internalClockPreScaler),
        pllMultiplier(pllMultiplier),
        registers(i2cAddress) {};

    bool init();

    inline void waitForVsync(void) __attribute__((always_inline));
    inline void waitForPixelClockRisingEdge(void) __attribute__((always_inline));
    inline void ignoreHorizontalPaddingLeft(void) __attribute__((always_inline));
    inline void ignoreHorizontalPaddingRight(void) __attribute__((always_inline));
    inline void readPixelByte(uint8_t & byte) __attribute__((always_inline));

    virtual void ignoreVerticalPadding();

protected:
    virtual bool setUpCamera();

};

void Camera::waitForVsync() {
  while(!(GPIO_IN_REG & (1<<4))) ;
}

void Camera::waitForPixelClockRisingEdge() {
  while((GPIO_IN_REG & (1<<5))) ;
  while(!(GPIO_IN_REG & (1<<5))) ;
}

// One byte at the beginning
void Camera::ignoreHorizontalPaddingLeft() {
  waitForPixelClockRisingEdge();
}

// Three bytes at the end
void Camera::ignoreHorizontalPaddingRight() {
  volatile uint16_t pixelTime = 0;

  waitForPixelClockRisingEdge();
  waitForPixelClockRisingEdge();

  // After the last pixel byte of an image line there is a very small pixel clock pulse.
  // To avoid accidentally counting this small pulse we measure the length of the
  // last pulse and then wait the same time to add a byte wide delay at the
  // end of the line.
  while((GPIO_IN_REG & (1<<5))) pixelTime++;
  while(!(GPIO_IN_REG & (1<<5))) pixelTime++;
  while(pixelTime) pixelTime--;
}

void Camera::readPixelByte(uint8_t & byt) {
  byt = (uint8_t)((GPIO_IN_REG & 0x0F)<<4);
}


#endif // _CAMERA_HPP_