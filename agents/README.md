# General Master Agent (ASTER)

## Overview
ASTER adalah General Master Agent yang mengoordinasikan seluruh sub-agent dalam ekosistem **Smart Building Master S3 (Serial Edition)**. ASTER memastikan sinkronisasi antara hardware, jaringan, dan antarmuka pengguna berjalan harmonis.

## Sub-Agents & Capability Map
1. **[Data Agent](./DataAgent/README.md)**: State management & Thread-safety.
2. **[Display Agent](./DisplayAgent/README.md)**: Hardware driver (LovyanGFX) & Touch.
3. **[UI Agent](./UIAgent/README.md)**: Screen logic & SmartBuildingUI implementation.
4. **[WiFi Agent](./WiFiAgent/README.md)**: Wireless connectivity & Scanning.
5. **[LAN Agent](./LANAgent/README.md)**: Ethernet (W5500) connectivity.
6. **[Comm Agent](./CommAgent/README.md)**: RS485 industrial communication.

## Operational Philosophy
- **Modular**: Setiap agent bersifat independen dan memiliki tanggung jawab yang jelas.
- **Skill-Based**: Interaksi antar agent dilakukan melalui pemanggilan "Skill" yang tersedia.
- **Fail-Safe**: ASTER mengawasi kesehatan setiap agent (misal: reconnecting network jika terputus).
