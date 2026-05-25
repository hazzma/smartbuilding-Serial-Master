# Display Agent

## Role
Mengelola abstraksi hardware layar (TFT) dan input sentuh (Touch). Bertindak sebagai interface antara library grafis (LovyanGFX) dan hardware fisik.

## Skills
- **Skill: Hardware Initialization**
  - Konfigurasi bus SPI (TFT) dan I2C (Touch) dengan timing yang optimal.
- **Skill: Driver Management (LovyanGFX)**
  - Mengelola instansi `LGFX` untuk abstraksi driver ILI9488 dan GT911.
- **Skill: Backlight Orchestration**
  - Kontrol intensitas cahaya layar menggunakan PWM (fading effect).
- **Skill: Canvas/Sprite Management**
  - Penyediaan buffer rendering (PSRAM) untuk meminimalkan *flicker* saat update layar.
