#include "CamYUV.h"

bool CamYUV::init() {
  registers.init();
  initIO();
  delay(10);
  return setUpCamera();
}


void CamYUV::initIO() {
#ifdef OV7670_INIT_INPUTS
  OV7670_INIT_INPUTS;
#endif
  OV7670_INIT_CLOCK_OUT;
}


bool CamYUV::setUpCamera() {
  if (registers.resetSettings()) {
    registers.setRegisters(CameraOV7670Registers::regsDefault);
    registers.setRegisters(CameraOV7670Registers::regsYUV422);

    switch (resolution) {
      case RESOLUTION_QVGA_320x240:
        registers.setRegisters(CameraOV7670Registers::regsQVGA);
        verticalPadding = CameraOV7670Registers::QVGA_VERTICAL_PADDING;
        break;
      default:
      case RESOLUTION_QQVGA_160x120:
        registers.setRegisters(CameraOV7670Registers::regsQQVGA);
        verticalPadding = CameraOV7670Registers::QQVGA_VERTICAL_PADDING;
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


bool CamYUV::setRegister(uint8_t addr, uint8_t val) {
  registers.setRegister(addr, val);
}

uint8_t CamYUV::readRegister(uint8_t addr) {
  return registers.readRegister(addr);
}

void CamYUV::setRegisterBitsOR(uint8_t addr, uint8_t bits) {
  registers.setRegisterBitsOR(addr, bits);
}

void CamYUV::setRegisterBitsAND(uint8_t addr, uint8_t bits) {
  registers.setRegisterBitsAND(addr, bits);
}

void CamYUV::setManualContrastCenter(uint8_t contrastCenter) {
  registers.setManualContrastCenter(contrastCenter);
}


void CamYUV::setContrast(uint8_t contrast) {
  registers.setContrast(contrast);
}


void CamYUV::setBrightness(uint8_t birghtness) {
  registers.setBrightness(birghtness);
}


void CamYUV::reversePixelBits() {
  registers.reversePixelBits();
}

void CamYUV::showColorBars(bool transparent) {
  registers.setShowColorBar(transparent);
}

void CamYUV::ignoreVerticalPadding() {
  for (uint8_t i = 0; i < verticalPadding; i++) {
    ignoreHorizontalPaddingLeft();
    for (uint16_t x = 0; x < resolution * 2; x++) {
      waitForPixelClockRisingEdge();
    }
    ignoreHorizontalPaddingRight();
  }
}
