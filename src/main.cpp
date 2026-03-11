#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Camera.hpp"

extern "C" {
    void app_main();
}

#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     9600
#define UART_TX_PIN        21    // TX0
#define UART_RX_PIN        20    // RX0
#define UART_BUF_SIZE      256
#define PICTURE_BUTTON     GPIO_NUM_7
#define PICTURE_BUTTON2    GPIO_NUM_10

const uart_config_t uart_config = {
    .baud_rate = UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};


#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

uint8_t gray;

const uint16_t width = 320;
const uint16_t height = 240;
uint8_t img[width * height];

Camera cam(Camera::RESOLUTION_QQVGA_160x120, 2);
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void sendByte (uint8_t b)
{ 
    uart_write_bytes(UART_PORT_NUM, (const char *)&b, 1);
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

void printRaster ()
{
    int lines = height / 24;
    uint8_t nL = width & 0xFF;
    uint8_t nH = (width >> 8) & 0xFF;
    
    for (int line = 0; line < lines; ++line)
    {
        uart_write_bytes(UART_PORT_NUM, "\x1B\x2A\x21", 3);
        sendByte(nL);
        sendByte(nH);

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
                    sendByte(byt);
                }
            }
        }        
        sendByte(10);
    }
}

void getPicture ()
{
    portENTER_CRITICAL(&mux);
    cam.waitForVsync();

    cam.ignoreVerticalPadding();

    for (uint16_t y = 0; y < height/2; y++) {
        cam.ignoreHorizontalPaddingLeft();

        uint16_t x = 0;
        while ( x < width/2)
        {
            cam.waitForPixelClockRisingEdge(); // YUV422 grayscale byte   
            cam.readPixelByte(gray);
            img[2*y*width + 2*x] = formatPixelByteFirst(gray);
            img[2*y*width + 2*x+1]     = img[2*y*width + 2*x];
            img[(2*y+1)*width + 2*x+1] = img[2*y*width + 2*x];
            img[(2*y+1)*width + 2*x]   = img[2*y*width + 2*x];

            cam.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            x++;
            cam.waitForPixelClockRisingEdge(); // YUV422 grayscale byte
            cam.readPixelByte(gray);

            img[2*y*width + 2*x] = formatPixelByteSecond(gray);
            img[2*y*width + 2*x+1]     = img[2*y*width + 2*x];
            img[(2*y+1)*width + 2*x+1] = img[2*y*width + 2*x];
            img[(2*y+1)*width + 2*x]   = img[2*y*width + 2*x];

            cam.waitForPixelClockRisingEdge(); // YUV422 color byte. Ignore.
            x++;
        }
        cam.ignoreHorizontalPaddingRight();
    }
    portEXIT_CRITICAL(&mux);
}

void initPrinter ()
{
    // reset
    uart_write_bytes(UART_PORT_NUM, "\x1B\x40", 2);
    // linefeed to 0
    uart_write_bytes(UART_PORT_NUM, "\x1B\x33\x00", 3);
    // Codepage 858
    uart_write_bytes(UART_PORT_NUM, "\x1B\x74\x13", 3);
    // font B
    uart_write_bytes(UART_PORT_NUM, "\x1B\x4D\x01", 3);
    // font CPI mode
    uart_write_bytes(UART_PORT_NUM, "\x1B\xC1\x01", 3);
    // rotate text 180
    uart_write_bytes(UART_PORT_NUM, "\x1B\x7B\x01", 3);
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++

void app_main (void)
{
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);

    cam.init();

    gpio_set_direction(PICTURE_BUTTON, GPIO_MODE_INPUT); 
    gpio_set_pull_mode(PICTURE_BUTTON, GPIO_PULLUP_ONLY);

    gpio_set_direction(PICTURE_BUTTON2, GPIO_MODE_INPUT); 
    gpio_set_pull_mode(PICTURE_BUTTON2, GPIO_PULLUP_ONLY);

    for (;;)
    {
        if (gpio_get_level(PICTURE_BUTTON) == 0)
        {
            initPrinter();
            uart_write_bytes(UART_PORT_NUM, "          Das wird teuer f\x81r Sie.\n", 34);
            getPicture();
            ditherAtkinson();
            //printRaster();
            uart_write_bytes(UART_PORT_NUM, "\n\n\n\n", 4);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
