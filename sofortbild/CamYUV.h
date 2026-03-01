#ifndef _CamYUV_h_
#define _CamYUV_h_

#include "Arduino.h"
#include "CameraOV7670Registers.h"

#if defined(ESP32)

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define GPIO_IN_REG  (*((volatile uint32_t *)0x6000403C))

#ifndef OV7670_INIT_INPUTS
#define OV7670_INIT_INPUTS \
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT); \
    gpio_set_direction(GPIO_NUM_1, GPIO_MODE_INPUT); \
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT); \
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_INPUT); \
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT); /* VSYNC */ \
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT);  /* PCLK */
#endif

#ifndef OV7670_VSYNC
#define OV7670_VSYNC (GPIO_IN_REG & (1<<4))
#endif

#ifndef OV7670_PIXEL_CLOCK
#define OV7670_PIXEL_CLOCK (GPIO_IN_REG & (1<<5))
#endif

#ifndef OV7670_READ_PIXEL_BYTE
#define OV7670_READ_PIXEL_BYTE(b) \
    { \
      b = (uint8_t)((GPIO_IN_REG & 0x0F)<<4); \
    }
#endif

#ifndef OV7670_INIT_CLOCK_OUT
#define OV7670_INIT_CLOCK_OUT \
  do { \
    /* Timer konfigurieren */ \
    ledc_timer_config_t ledc_timer = { \
      .speed_mode = LEDC_LOW_SPEED_MODE, \
      .duty_resolution = LEDC_TIMER_1_BIT, \
      .timer_num = LEDC_TIMER_0, \
      .freq_hz = 20000000, \
      .clk_cfg = LEDC_AUTO_CLK \
    }; \
    ledc_timer_config(&ledc_timer); \
    /* Channel konfigurieren */ \
    ledc_channel_config_t ledc_channel = { \
      .gpio_num = 6, \
      .speed_mode = LEDC_LOW_SPEED_MODE, \
      .channel = LEDC_CHANNEL_0, \
      .intr_type = LEDC_INTR_FADE_END, \
      .timer_sel = LEDC_TIMER_0, \
      .duty = 1 \
    }; \
    ledc_channel_config(&ledc_channel); \
  } while(0)
#endif

#endif // ESP32c3


class CamYUV {

public:

    enum Resolution {
        RESOLUTION_VGA_640x480 = 640,
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
    CameraOV7670Registers registers;
    uint8_t verticalPadding = 0;

public:

    CamYUV(
        Resolution resolution,
        uint8_t internalClockPreScaler,
        PLLMultiplier pllMultiplier = PLL_MULTIPLIER_BYPASS
    ) :
        resolution(resolution),
        internalClockPreScaler(internalClockPreScaler),
        pllMultiplier(pllMultiplier),
        registers(i2cAddress) {};

    bool init();
    bool setRegister(uint8_t addr, uint8_t val);
    uint8_t readRegister(uint8_t addr);
    void setRegisterBitsOR(uint8_t addr, uint8_t bits);
    void setRegisterBitsAND(uint8_t addr, uint8_t bits);
    void setManualContrastCenter(uint8_t center);
    void setContrast(uint8_t contrast);
    void setBrightness(uint8_t birghtness);
    void reversePixelBits();
    void showColorBars(bool transparent);

    inline void waitForVsync(void) __attribute__((always_inline));
    inline void waitForPixelClockRisingEdge(void) __attribute__((always_inline));
    inline void waitForPixelClockLow(void) __attribute__((always_inline));
    inline void waitForPixelClockHigh(void) __attribute__((always_inline));
    inline void ignoreHorizontalPaddingLeft(void) __attribute__((always_inline));
    inline void ignoreHorizontalPaddingRight(void) __attribute__((always_inline));
    inline void readPixelByte(uint8_t & byte) __attribute__((always_inline));

    virtual void ignoreVerticalPadding();

protected:
    virtual bool setUpCamera();

private:
    void initIO();

};



void CamYUV::waitForVsync() {
  while(!OV7670_VSYNC);
}

void CamYUV::waitForPixelClockRisingEdge() {
  waitForPixelClockLow();
  waitForPixelClockHigh();
}

void CamYUV::waitForPixelClockLow() {
  while(OV7670_PIXEL_CLOCK);
}

void CamYUV::waitForPixelClockHigh() {
  while(!OV7670_PIXEL_CLOCK);
}

// One byte at the beginning
void CamYUV::ignoreHorizontalPaddingLeft() {
  waitForPixelClockRisingEdge();
}

// Three bytes at the end
void CamYUV::ignoreHorizontalPaddingRight() {
  volatile uint16_t pixelTime = 0;

  waitForPixelClockRisingEdge();
  waitForPixelClockRisingEdge();

  // After the last pixel byte of an image line there is a very small pixel clock pulse.
  // To avoid accidentally counting this small pulse we measure the length of the
  // last pulse and then wait the same time to add a byte wide delay at the
  // end of the line.
  while(OV7670_PIXEL_CLOCK) pixelTime++;
  while(!OV7670_PIXEL_CLOCK) pixelTime++;
  while(pixelTime) pixelTime--;
}

void CamYUV::readPixelByte(uint8_t & byte) {
  OV7670_READ_PIXEL_BYTE(byte);
}


#endif // _CamYUV_h_
