#ifndef UI_H
#define UI_H

#include <Adafruit_GFX_AS.h>
#include "SSD1306_STM32.h"

#define OLED_RESET -1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCREEN_WIDTH_MID ((SCREEN_WIDTH / 2) - 1)
#define SCREEN_HEIGHT_MID ((SCREEN_HEIGHT / 2) - 1)

#define CHAR_WIDTH 5
#define CHAR_HEIGHT 7

namespace Ui {
    extern SSD1306 display;
    extern bool shouldDrawUpdate;
    extern bool shouldDisplay;
    extern bool shouldFullRedraw;

    void setup(I2C_HandleTypeDef *i2c_handle);
    void update();

    //void drawLogo();

    void drawGraph(
        const uint8_t data[],
        uint8_t dataSize,
        uint8_t dataScale,
        uint8_t x,
        uint8_t y,
        uint8_t w,
        uint8_t h
    );

    void drawDashedHLine(int x,  int y, int w, int step);
    void drawDashedVLine( int x,  int y,  int w,  int step);

    void clear();
    void clearRect( int x,  int y,  int w,  int h);

    void needUpdate();
    void needDisplay();
    void needFullRedraw();
}

#endif // file_defined


