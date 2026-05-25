#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include <Arduino.h>

enum class TextDatum {
    TopLeft = 0, TopCenter = 1, TopRight = 2,
    MiddleLeft = 4, MiddleCenter = 5, MiddleRight = 6,
    BottomLeft = 8, BottomCenter = 9, BottomRight = 10
};

class DisplayEngine {
public:
    virtual ~DisplayEngine() {}

    // Lifecycle
    virtual bool     init(int w, int h) = 0;
    virtual void     swapBuffers() = 0;
    virtual void     pushToDisplay() = 0;   // Signature dari library hazzma/UI-SmartBuilding
    virtual void     clearAll(uint16_t color) = 0;

    // Drawing
    virtual void     fillScreen(uint16_t color) = 0;
    virtual void     fillRect(int x, int y, int w, int h, uint16_t color) = 0;
    virtual void     drawRect(int x, int y, int w, int h, uint16_t color) = 0;
    virtual void     fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) = 0;
    virtual void     drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) = 0;
    virtual void     fillCircle(int x, int y, int r, uint16_t color) = 0;
    virtual void     drawFastHLine(int x, int y, int w, uint16_t color) = 0;
    virtual void     drawRGB565Image(int x, int y, int w, int h, const uint16_t* data) = 0;

    // Text
    virtual void     setTextColor(uint16_t color) = 0;
    virtual void     setTextColor(uint16_t color, uint16_t bg) = 0;
    virtual void     setTextFont(int font) = 0;
    virtual void     setTextSize(uint8_t size) = 0;
    virtual void     setTextDatum(TextDatum datum) = 0;
    virtual void     drawString(const char* s, int x, int y) = 0;
    virtual int      textWidth(const char* s) = 0;

    // Utility
    virtual uint16_t color565(uint8_t r, uint8_t g, uint8_t b) = 0;
};

#endif
