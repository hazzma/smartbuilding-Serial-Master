#include "ui_keyboard.h"
#include <string.h>

static char keyboard_buffer[64] = "";
static bool shift_mode = true;
static bool symbol_mode = false;

const char* keys_row1_up[]  = {"Q","W","E","R","T","Y","U","I","O","P"};
const char* keys_row1_low[] = {"q","w","e","r","t","y","u","i","o","p"};
const char* keys_row1_sym[] = {"1","2","3","4","5","6","7","8","9","0"};

const char* keys_row2_up[]  = {"A","S","D","F","G","H","J","K","L"};
const char* keys_row2_low[] = {"a","s","d","f","g","h","j","k","l"};
const char* keys_row2_sym[] = {"-","/",":",";","(",")","$","&","@"};

const char* keys_row3_up[]  = {"^","Z","X","C","V","B","N","M","<"};
const char* keys_row3_low[] = {"^","z","x","c","v","b","n","m","<"};
const char* keys_row3_sym[] = {"#",".",",","?","!","'","\"","%","<"};

void keyboard_draw() {
    p_canvas->fillScreen(COLOR_BG_MAIN);

    // Preview bar
    drawCardBase(10, 20, 460, 45, COLOR_CARD_BG);
    p_canvas->setTextColor(COLOR_TEXT_MAIN);
    p_canvas->setTextFont(4);
    p_canvas->setTextDatum(TextDatum::MiddleLeft);
    p_canvas->drawString(keyboard_buffer, 25, 42);

    int kX = 10, kY = 90;
    int kW = 41, kH = 45;
    int spacing = 6;

    p_canvas->setTextDatum(TextDatum::MiddleCenter);
    p_canvas->setTextFont(2);

    // Row 1
    for (int i = 0; i < 10; i++) {
        drawCardBase(kX + i*(kW+spacing), kY, kW, kH, COLOR_CARD_BG);
        const char* label = symbol_mode ? keys_row1_sym[i] : (shift_mode ? keys_row1_up[i] : keys_row1_low[i]);
        p_canvas->drawString(label, kX + i*(kW+spacing) + kW/2, kY + kH/2);
    }

    // Row 2
    kY += kH + spacing;
    int off2 = (480 - (9*(kW+spacing))) / 2;
    for (int i = 0; i < 9; i++) {
        drawCardBase(off2 + i*(kW+spacing), kY, kW, kH, COLOR_CARD_BG);
        const char* label = symbol_mode ? keys_row2_sym[i] : (shift_mode ? keys_row2_up[i] : keys_row2_low[i]);
        p_canvas->drawString(label, off2 + i*(kW+spacing) + kW/2, kY + kH/2);
    }

    // Row 3
    kY += kH + spacing;
    int off3 = (480 - (9*(kW+spacing))) / 2;
    for (int i = 0; i < 9; i++) {
        uint16_t color = COLOR_CARD_BG;
        if (i == 0 && !symbol_mode) color = shift_mode ? COLOR_ACCENT_MAIN : COLOR_STAT_OFF;
        if (i == 8) color = COLOR_STAT_ERR;
        
        drawCardBase(off3 + i*(kW+spacing), kY, kW, kH, color);
        const char* label = symbol_mode ? keys_row3_sym[i] : (shift_mode ? keys_row3_up[i] : keys_row3_low[i]);
        p_canvas->drawString(label, off3 + i*(kW+spacing) + kW/2, kY + kH/2);
    }

    // Row 4: Mode Switch / Space / Cancel / OK
    kY += kH + spacing;
    // Toggle Button 123 / abc
    drawCardBase(10, kY, 65, kH, symbol_mode ? COLOR_ACCENT_MAIN : COLOR_CARD_BG);
    p_canvas->drawString(symbol_mode ? "abc" : "123", 10 + 32, kY + kH/2);

    // Space
    drawCardBase(85, kY, 155, kH, COLOR_CARD_BG);
    p_canvas->drawString("SPACE", 85 + 77, kY + kH/2);

    // Cancel
    drawCardBase(250, kY, 80, kH, COLOR_STAT_OFF);
    p_canvas->drawString("BACK", 250 + 40, kY + kH/2);

    // OK
    drawCardBase(340, kY, 130, kH, COLOR_STAT_ON);
    p_canvas->drawString("ENTER", 340 + 65, kY + kH/2);

    p_canvas->setTextDatum(TextDatum::TopLeft);
}

static bool checkHit(int tx, int ty, int x, int y, int w, int h) {
    return (tx >= x && tx <= x + w && ty >= y && ty <= y + h);
}

void keyboard_handle_touch(int tx, int ty) {
    int kX = 10, kY = 90;
    int kW = 41, kH = 45;
    int spacing = 6;

    // Row 1
    for (int i = 0; i < 10; i++) {
        if (checkHit(tx, ty, kX + i*(kW+spacing), kY, kW, kH)) {
            size_t len = strlen(keyboard_buffer);
            if (len < 63) {
                const char* key = symbol_mode ? keys_row1_sym[i] : (shift_mode ? keys_row1_up[i] : keys_row1_low[i]);
                strcat(keyboard_buffer, key);
            }
            return;
        }
    }

    // Row 2
    kY += kH + spacing;
    int off2 = (480 - (9*(kW+spacing))) / 2;
    for (int i = 0; i < 9; i++) {
        if (checkHit(tx, ty, off2 + i*(kW+spacing), kY, kW, kH)) {
            size_t len = strlen(keyboard_buffer);
            if (len < 63) {
                const char* key = symbol_mode ? keys_row2_sym[i] : (shift_mode ? keys_row2_up[i] : keys_row2_low[i]);
                strcat(keyboard_buffer, key);
            }
            return;
        }
    }

    // Row 3
    kY += kH + spacing;
    int off3 = (480 - (9*(kW+spacing))) / 2;
    for (int i = 0; i < 9; i++) {
        if (checkHit(tx, ty, off3 + i*(kW+spacing), kY, kW, kH)) {
            if (i == 0 && !symbol_mode) {
                shift_mode = !shift_mode;
            } else if (i == 8) {
                size_t len = strlen(keyboard_buffer);
                if (len > 0) keyboard_buffer[len-1] = '\0';
            } else {
                size_t len = strlen(keyboard_buffer);
                if (len < 63) {
                    const char* key = symbol_mode ? keys_row3_sym[i] : (shift_mode ? keys_row3_up[i] : keys_row3_low[i]);
                    strcat(keyboard_buffer, key);
                }
            }
            return;
        }
    }

    // Row 4
    kY += kH + spacing;
    if (checkHit(tx, ty, 10, kY, 65, kH)) { symbol_mode = !symbol_mode; return; }
    if (checkHit(tx, ty, 85, kY, 155, kH)) {
        size_t len = strlen(keyboard_buffer);
        if (len < 63) strcat(keyboard_buffer, " ");
        return;
    }
}

const char* keyboard_get_text()        { return keyboard_buffer; }
void keyboard_set_text(const char* t)  { if (t) strncpy(keyboard_buffer, t, 63); else keyboard_buffer[0] = '\0'; }
void keyboard_clear()                  { keyboard_buffer[0] = '\0'; }
void keyboard_toggle_shift()           { shift_mode = !shift_mode; }
