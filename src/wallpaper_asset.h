#ifndef WALLPAPER_ASSET_H
#define WALLPAPER_ASSET_H

#include <Arduino.h>

static const int WALLPAPER_BG_WIDTH = 480;
static const int WALLPAPER_BG_HEIGHT = 320;
static const int WALLPAPER_BG_PIXELS = WALLPAPER_BG_WIDTH * WALLPAPER_BG_HEIGHT;

extern const uint16_t wallpaper_bg_rgb565[WALLPAPER_BG_PIXELS];

#endif
