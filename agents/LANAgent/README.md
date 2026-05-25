# LAN Agent

## Role
Mengelola koneksi fisik via Ethernet (W5500). Digunakan sebagai jalur data utama atau cadangan jika WiFi tidak tersedia.

## Skills
- **Skill: Ethernet Initialization**
  - Inisialisasi driver chip W5500 via bus SPI khusus.
- **Skill: Link Status Detection**
  - Mendeteksi apakah kabel LAN terpasang atau terlepas secara real-time.
- **Skill: IP Provisioning**
  - Mengelola permintaan alamat IP via DHCP atau konfigurasi IP Statis.
- **Skill: Network Priority Switching**
  - Melaporkan kesiapan jalur kabel ke Master Agent untuk logika failover.
