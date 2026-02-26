#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "Camera.hpp"

#define CAMERA_XCLK_PIN     6

bool Camera::init() {
    registers.init();

    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_NUM_1, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT); /* VSYNC */ 
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT);  /* PCLK */

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    
    ledc_channel_config_t ledc_channel = {
        .gpio_num = CAMERA_XCLK_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_FADE_END,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1
    };
    ledc_channel_config(&ledc_channel);

    vTaskDelay(pdMS_TO_TICKS(10));
    return setUpCamera();
}

bool Camera::setUpCamera() {
    if (registers.resetSettings()) {
        registers.setRegisters(CameraRegisters::regsDefault);
        registers.setRegisters(CameraRegisters::regsYUV422);

        switch (resolution) {
        case RESOLUTION_QVGA_320x240:
            registers.setRegisters(CameraRegisters::regsQVGA);
            verticalPadding = CameraRegisters::QVGA_VERTICAL_PADDING;
            break;
        default:
        case RESOLUTION_QQVGA_160x120:
            registers.setRegisters(CameraRegisters::regsQQVGA);
            verticalPadding = CameraRegisters::QQVGA_VERTICAL_PADDING;
            break;
        }

        registers.setDisablePixelClockDuringBlankLines();
        registers.setDisableHREFDuringBlankLines();
        registers.setInternalClockPreScaler(internalClockPreScaler);
        registers.setPLLMultiplier(pllMultiplier);

        return true;
    } else {
        return false;
    }
}

void Camera::ignoreVerticalPadding()
{
    for (uint8_t i = 0; i < verticalPadding; i++) {
        ignoreHorizontalPaddingLeft();
        for (uint16_t x = 0; x < resolution * 2; x++) {
            waitForPixelClockRisingEdge();
        }
        ignoreHorizontalPaddingRight();
    }
}