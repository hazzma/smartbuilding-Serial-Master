#include "lgfx_adapter.h"

LGFXAdapter::LGFXAdapter(LGFX* lgfx_ptr)
    : _tft(lgfx_ptr), _canvas0(lgfx_ptr), _canvas1(lgfx_ptr) {
    _active = &_canvas0;
}

bool LGFXAdapter::init(int w, int h) {
    _w = w; _h = h;

    Serial.println("[HAL] --- Memory Check ---");
    Serial.printf("[HAL] Internal Free Heap : %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("[HAL] PSRAM Total Size   : %d KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("[HAL] PSRAM Free         : %d KB\n", ESP.getFreePsram() / 1024);
    if (ESP.getPsramSize() == 0)
        Serial.println("[HAL] WARNING: No PSRAM detected! Check platformio.ini");

    _canvas0.setColorDepth(16);
    if (psramFound()) _canvas0.setPsram(true);
    Serial.print("[HAL] Canvas 0... ");
    if (!_canvas0.createSprite(w, h)) {
        Serial.println("FAIL");
        return false;
    }
    Serial.println("OK");

    if (psramFound()) {
        _canvas1.setColorDepth(16);
        _canvas1.setPsram(true);
        Serial.print("[HAL] Canvas 1... ");
        if (_canvas1.createSprite(w, h)) Serial.println("OK (Double Buffer)");
        else Serial.println("FAIL (Single Buffer)");
    }

    return true;
}

void LGFXAdapter::swapBuffers() {
    if (_canvas1.getBuffer())
        _active = (_active == &_canvas0) ? &_canvas1 : &_canvas0;
}

// Tanpa parameter — sesuai interface library hazzma/UI-SmartBuilding
void LGFXAdapter::pushToDisplay() {
    if (_active->getBuffer()) _active->pushSprite(0, 0);
}

void LGFXAdapter::clearAll(uint16_t color) {
    _tft->fillScreen(color);
    if (_canvas0.getBuffer()) _canvas0.fillScreen(color);
    if (_canvas1.getBuffer()) _canvas1.fillScreen(color);
}

void LGFXAdapter::fillScreen(uint16_t c)                           { _active->fillScreen(c); }
void LGFXAdapter::fillRect(int x, int y, int w, int h, uint16_t c){ _active->fillRect(x, y, w, h, c); }
void LGFXAdapter::drawRect(int x, int y, int w, int h, uint16_t c){ _active->drawRect(x, y, w, h, c); }
void LGFXAdapter::fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { _active->fillRoundRect(x, y, w, h, r, c); }
void LGFXAdapter::drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) { _active->drawRoundRect(x, y, w, h, r, c); }
void LGFXAdapter::fillCircle(int x, int y, int r, uint16_t c)     { _active->fillCircle(x, y, r, c); }
void LGFXAdapter::drawFastHLine(int x, int y, int w, uint16_t c)  { _active->drawFastHLine(x, y, w, c); }
void LGFXAdapter::drawRGB565Image(int x, int y, int w, int h, const uint16_t* data) {
    bool old_swap = _active->getSwapBytes();
    _active->setSwapBytes(true);
    _active->pushImage(x, y, w, h, data);
    _active->setSwapBytes(old_swap);
}
void LGFXAdapter::setTextColor(uint16_t c)                        { _active->setTextColor(c); }
void LGFXAdapter::setTextColor(uint16_t c, uint16_t bg)           { _active->setTextColor(c, bg); }
void LGFXAdapter::setTextFont(int f)                              { _active->setTextFont(f); }
void LGFXAdapter::setTextSize(uint8_t s)                          { _active->setTextSize(s); }
void LGFXAdapter::setTextDatum(TextDatum d)                       { _active->setTextDatum((uint8_t)d); }
void LGFXAdapter::drawString(const char* s, int x, int y)         { _active->drawString(s, x, y); }
int  LGFXAdapter::textWidth(const char* s)                        { return (int)_active->textWidth(s); }
uint16_t LGFXAdapter::color565(uint8_t r, uint8_t g, uint8_t b)  { return _active->color565(r, g, b); }
