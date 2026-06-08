#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "display.h"
#include "display_engine.h"

// FSD 4.2: Abstraction Layer — LovyanGFX via LGFXAdapter
extern DisplayEngine* p_engine;

// Macro: 'canvas' redirects to the Engine Interface
#define canvas  (*p_engine)
#define p_canvas p_engine

void widgets_init();
void widgets_swap();
void drawWallpaperBackground();

// Design tokens
#define CARD_RAD 8

// Widget prototypes
void drawCardBase(int x, int y, int w, int h, uint16_t color);
void drawTempCard(int x, int y, int w, int h, const char* label, float value, bool error);
void drawToggleButton(int x, int y, int w, int h, const char* label, bool state);
void drawUpDownButton(int x, int y, int w, int h, const char* label, bool isUp);
void drawPresenceBadge(int x, int y, int w, int h, bool detected);
void drawLuxCard(int x, int y, int w, int h, float lux);
void drawCO2Card(int x, int y, int w, int h, int co2);
void drawNotifBar(bool wifi, bool lan, bool mqtt, const char* conn_status,
                  const char* room_name, const char* time_str);
void drawDashboardTopBar(const char* time_str, bool wifi, bool lan, bool bus_ok, bool slave_online, bool show_time = true);
void drawCO2Chip(int x, int y, int co2);
void drawLargeTempWidget(int x, int y, int w, int h, float temp, bool valid, bool large);
void drawAcTargetWidget(int x, int y, int w, int h, float target_temp, bool ac_on);
void drawLargeControlButton(int x, int y, int w, int h, const char* label, bool on, const char* subtext = nullptr);
void drawDashboardEmptyState(const char* title, const char* subtitle);
void drawDashboardEmptyHero(const char* time_str, const char* title, const char* subtitle);

#endif
