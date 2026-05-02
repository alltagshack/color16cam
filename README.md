# DIY Instant camera

The original name "16cam" based on a 4bit gray value for each pixel. Actually 5bits are used!

**Features**

- atkinson dithering
- 320x240 is scaled to 480x360 for more raster points
- 24bit high res ESC/POS Printing (58mm) via TTL UART
- 32 gray values
- I2C oled SSD1306 preview display (128x64)

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

- 32 gray values via GPIO0 - GPIO4 (Cam D3 - D7)
- GPIO5 to Cam PCLK
- GPIO6 to Cam XCLK (= MCLK)
- GPIO7 as the picture button (active on GND)
- GPIO21 (TX) to Printer RX
- GPIO20
  - Cam VSYNC (= VS)
  - UART RX is unused
- Cam D0 - D2 and HSync unused

I2C

- GPIO9 to CLK (Oled and Cam)
- GPIO8 to SDA (Oled and Cam)

Power

- GND
  - to Cam GND
  - to Cam PWDN
  - to Printer GND
  - to Oled GND/VDD
- 3.3 V 
  - to Cam VCC
  - to Cam RST
  - to Oled VCC