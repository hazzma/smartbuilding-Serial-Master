#ifndef LGFX_ADAPTER_H
#define LGFX_ADAPTER_H

#include "display_engine.h"
#include "display.h"

class LGFXAdapter : public DisplayEngine {
public:
    explicit LGFXAdapter(LGFX* lgfx_ptr);

    bool     init(int w, int h) override;
    void     swapBuffers() override;
    void     pushToDisplay() override;         // Tanpa param — sesuai library
    void     clearAll(uint16_t color) override;

    void     fillScreen(uint16_t color) override;
    void     fillRect(int x, int y, int w, int h, uint16_t color) override;
    void     drawRect(int x, int y, int w, int h, uint16_t color) override;
    void     fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) override;
    void     drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) override;
    void     fillCircle(int x, int y, int r, uint16_t color) override;
    void     drawFastHLine(int x, int y, int w, uint16_t color) override;
    void     drawRGB565Image(int x, int y, int w, int h, const uint16_t* data) override;

    void     setTextColor(uint16_t color) override;
    void     setTextColor(uint16_t color, uint16_t bg) override;
    void     setTextFont(int font) override;
    void     setTextSize(uint8_t size) override;
    void     setTextDatum(TextDatum datum) override;
    void     drawString(const char* s, int x, int y) override;
    int      textWidth(const char* s) override;

    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) override;

private:
    LGFX*        _tft;
    LGFX_Sprite  _canvas0;
    LGFX_Sprite  _canvas1;
    LGFX_Sprite* _active;
    int          _w = 0, _h = 0;
};

#endif
