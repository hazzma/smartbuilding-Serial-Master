# Data Agent

## Role
Bertanggung jawab sebagai *Single Source of Truth* untuk seluruh data sistem. Mengelola state global dan memastikan keamanan data saat diakses oleh berbagai task (Thread-Safety).

## Skills
- **Skill: State Persistence**
  - Menyimpan data sensor, status jaringan, dan flag UI dalam struktur data terpusat.
- **Skill: Mutex Guarding**
  - Melindungi akses ke global state menggunakan Semaphore/Mutex untuk mencegah *race conditions*.
- **Skill: Data Validation & Cleanup**
  - Secara otomatis mereset data sensor jika tidak ada pembaruan selama lebih dari 10 detik (Data Staleness detection).
- **Skill: Notification Dispatcher**
  - Mengatur flag `ui_needs_update` agar UI Agent tahu kapan harus me-render ulang layar.
