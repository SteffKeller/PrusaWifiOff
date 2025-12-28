#include "LedDisplay.h"

void clearMatrix() {
  M5.dis.clear();
}

void drawI(uint32_t col) {
  M5.dis.drawpix(2, 0, col);
  M5.dis.drawpix(2, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(2, 3, col);
  M5.dis.drawpix(2, 4, col);
}

void showAutoOffEnabledBase() {
  clearMatrix();
  M5.dis.fillpix(0x000000);

  uint32_t col = 0x0000FF;  // blue

  M5.dis.drawpix(0, 0, col);
  M5.dis.drawpix(1, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(3, 3, col);
  M5.dis.drawpix(4, 4, col);

  M5.dis.drawpix(4, 0, col);
  M5.dis.drawpix(3, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(1, 3, col);
  M5.dis.drawpix(0, 4, col);
}

void showAutoOffDisabled() {
  clearMatrix();
  drawI(0x00FF00);
}

void drawProgressBar(uint8_t filledRows) {
  clearMatrix();
  uint32_t bgBase = 0x000000;
  uint32_t xCol   = 0x0000FF;
  uint32_t barCol = 0xFF8000;

  M5.dis.fillpix(bgBase);

  for (int y = 4; y >= 0; --y) {
    bool fillRow = (4 - y) < filledRows;
    if (fillRow) {
      for (int x = 0; x < 5; ++x) {
        M5.dis.drawpix(x, y, barCol);
      }
    }
  }

  // redraw X
  M5.dis.drawpix(0, 0, xCol);
  M5.dis.drawpix(1, 1, xCol);
  M5.dis.drawpix(2, 2, xCol);
  M5.dis.drawpix(3, 3, xCol);
  M5.dis.drawpix(4, 4, xCol);

  M5.dis.drawpix(4, 0, xCol);
  M5.dis.drawpix(3, 1, xCol);
  M5.dis.drawpix(2, 2, xCol);
  M5.dis.drawpix(1, 3, xCol);
  M5.dis.drawpix(0, 4, xCol);
}
