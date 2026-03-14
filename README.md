# DIY Instant camera

The original name "16cam" based on a 4bit gray value for each pixel. Actually 5bits are used!

**Features**

- atkinson dithering
- 320x240 is scaled to 480x360 for more raster points
- 24bit high res ESC/POS Printing (58mm) via TTL UART
- 32 gray values

![in action](screenshot.jpg)

## Hardware

- ESP32-C3-DevKitM1
- REPLACE: YOU NEED A GOOD 3.3V POWER REGULATOR! THE ORIGINAL DOES NOT WORK WITH >5V
- OV7670 Cam
- ESC/POS thermo printer with TTL UART
- 2 Lipo cells (ca. 8.5 V)
- 1 power switch
- 1 button

## Wired and Pinout

- 32 gray values via GPIO0 - GPIO4
- GPIO5 is PCLK
- GPIO6 is XCLK
- GPIO7 is the picture button
- GPIO20 is VSYNC (UART RX is unused)
