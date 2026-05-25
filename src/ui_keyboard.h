#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include "display.h"
#include "ui_widgets.h"

void keyboard_draw();
void keyboard_handle_touch(int tx, int ty);
const char* keyboard_get_text();
void keyboard_set_text(const char* text);
void keyboard_clear();
void keyboard_toggle_shift();

#endif
