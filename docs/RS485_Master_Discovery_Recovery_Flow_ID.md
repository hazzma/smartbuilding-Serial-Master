# Dokumentasi Flow RS485 Master: Boot, Recovery, Polling, dan Discover

Dokumen ini menjelaskan logic RS485 master berdasarkan kode aktif di `src/rs485_manager.cpp` dan `src/rs485_manager.h`.

Fokusnya:

- Saat master pertama kali hidup, dia mencari apa.
- Kapan master kirim ke address `247`.
- Kapan master kirim ke address tersimpan seperti `0x02`, `0x03`, dst.
- Timing scan, timeout, retry, delay, dan kapan master berhenti.
- Bedanya mode normal boot/polling dengan mode Discover.

## Ringkasan Jawaban Cepat

Address penting:

- `0x01`: address master secara konsep.
- `0x02..0xF6` atau `2..246`: address normal slave setelah dipair.
- `0xF7` atau `247`: address default/pairing/recovery slave.
- `0x00`: broadcast, tidak dipakai untuk read normal.

Timing penting dari kode:

| Parameter | Nilai | Arti |
|---|---:|---|
| Modbus response timeout | `100 ms` | Waktu tunggu tiap attempt Modbus |
| Retry count | `1` | Total attempt = 2 kali: attempt `0` dan `1` |
| Poll interval normal | `1000 ms` | Master memproses satu slave per tick polling |
| Discover scan interval | `700 ms` | Master melakukan scan saat pairing/discover aktif |
| Discover known-address window | `4000 ms` | Di 4 detik awal Discover, master juga cek address lama |
| Identity sync interval | `10000 ms` | Identity slave di-refresh tiap 10 detik |
| Capability sync interval | `5000 ms` | Capability/assignment di-refresh tiap 5 detik |
| Offline timeout | `5000 ms` | Jika last seen terlalu lama, slave dianggap offline |
| Discover timeout | `30000 ms` | Discover berhenti otomatis setelah 30 detik |
| Auto recovery interval | `10000 ms` | Recovery known slave dicoba ulang maksimal tiap 10 detik per slave |

Jawaban paling penting:

1. Saat boot, master tidak langsung melakukan Discover unknown device.
2. Jika ada slave tersimpan dengan MAC dan address lama, master mencoba recovery via `247` dulu pada kondisi boot awal.
3. Jika recovery berhasil, master konfirmasi ke address tersimpan, misalnya `0x03`.
4. Jika tidak ada slave tersimpan, master tidak terus-terusan scan `247`; dia menunggu user klik Discover.
5. Discover harus diklik untuk mencari device baru/unknown di address `247`.
6. Saat Discover, polling normal dipause, lalu master scan `247` setiap sekitar `700 ms`.
7. Di 4 detik awal Discover, master juga mengecek address lama yang tersimpan, supaya device lama yang tidak reboot tetap bisa dikenali.
8. Jika Discover tidak menemukan candidate sampai `30 detik`, master timeout, keluar dari Discover, dan restore polling normal.

## Flow Chat Boot Pertama Kali

```text
MASTER BOOT
  |
  v
RS485 init
  - UART: RS485_UART_NUM
  - baudrate: 19200
  - timeout Modbus: 100 ms
  - poll_enabled = true
  |
  v
Load registry RS485 dari NVS/preferences
  - slave_count
  - address tersimpan
  - MAC/UID tersimpan
  - profile/device type
  - enabled slot sensor
  |
  v
Masuk loop RS485
  |
  v
Setiap 1000 ms, pilih 1 slave dari registry
  |
  +--> Tidak ada slave tersimpan?
  |       |
  |       v
  |     Tidak scan 247 otomatis untuk device baru.
  |     Master diam di normal loop.
  |     User perlu klik DISCOVER kalau mau cari device baru.
  |
  +--> Ada slave tersimpan tapi MAC = 0?
  |       |
  |       v
  |     Jalur legacy/fallback:
  |     master bisa coba sync identity/capability ke address tersimpan.
  |
  +--> Ada slave tersimpan dengan MAC dan address valid 2..246?
          |
          v
        Apakah ini boot awal?
        Kondisi kode:
        - last_seen == 0
        - online == false
        - mac != 0
          |
          +--> YA
          |     |
          |     v
          |   AUTO RECOVERY FIRST
          |     - master tulis recovery ke address 247
          |     - register 0x00F4..0x00F7
          |     - isi: MAC saved + address saved
          |     |
          |     v
          |   Confirm ke address saved
          |     - read identity 0x0000..0x0004
          |     - contoh dst=0x03
          |     |
          |     +--> MAC cocok
          |     |     slave online, last_seen update, lanjut polling normal
          |     |
          |     +--> gagal / timeout
          |           status recovery failed
          |           retry recovery paling cepat 10 detik lagi untuk slave itu
          |
          +--> TIDAK
                |
                v
              Poll normal ke address tersimpan
                - identity sync kalau due
                - capability sync kalau due
                - sensor block poll
```

## Logic Boot Dalam Bahasa Sederhana

Saat master baru hidup, dia sudah punya registry dari storage master. Registry itu berisi MAC slave dan address yang pernah diberikan, misalnya MAC `AA:BB:CC:DD:EE:FF` dulu dikasih address `0x03`.

Karena slave firmware bersifat RAM-only, setelah slave mati/hidup lagi dia balik ke address `247`. Jadi saat boot awal, master tidak langsung menganggap slave masih di `0x03`. Untuk known slave yang punya MAC tersimpan, master mencoba recovery dulu:

1. Master kirim recovery write ke `247`.
2. Isi recovery adalah MAC target dan address lama yang harus dipakai lagi.
3. Slave yang MAC-nya cocok menerima recovery itu dan pindah ke address lama.
4. Master lalu read identity ke address lama.
5. Kalau MAC hasil read cocok, slave dianggap recovered dan online.

Ini yang bikin boot cocok untuk skenario PoE atau power bareng: master dan slave sama-sama mati, lalu slave kembali ke `247`, lalu master restore lagi ke address saved.

## Apakah Boot Nyari `247` atau Address Lama?

Jawabannya: tergantung kondisi slave registry.

Untuk known slave yang punya MAC tersimpan dan belum pernah online pada sesi boot ini:

```text
Pertama: master kirim recovery ke 247
Kedua: master konfirmasi ke address lama, misalnya 0x03
```

Untuk known slave yang sudah online pada sesi berjalan:

```text
Master polling address lama, misalnya 0x03
```

Untuk unknown/new slave:

```text
Master tidak auto-discover 247 saat boot.
Harus klik DISCOVER.
```

Untuk data lama yang address tersimpan ada tapi MAC kosong:

```text
Master bisa masuk jalur sync address lama.
Ini fallback/legacy, bukan flow utama v2.1.
```

## Timing Boot dan Recovery

Polling normal berjalan setiap `1000 ms`.

Di setiap tick polling, master hanya ambil 1 slave berdasarkan `poll_index`. Jadi kalau ada 3 saved slave, kira-kira:

```text
t+1s  proses slave index 0
t+2s  proses slave index 1
t+3s  proses slave index 2
t+4s  balik slave index 0
```

Satu request Modbus punya:

```text
timeout per attempt = 100 ms
retry count = 1
total attempt = 2
worst-case timeout satu operasi = kira-kira 200 ms + overhead frame
```

Auto recovery satu slave terdiri dari:

```text
1. Write recovery ke 247:0x00F4 length 4
   - response write diabaikan secara logic
   - tapi library Modbus tetap bisa menunggu timeout/response

2. Read identity ke address saved
   - register 0x0000..0x0004
   - 2 attempt, 100 ms per attempt
```

Kalau recovery gagal:

```text
Slave tidak langsung dianggap selesai permanen.
Master boleh coba recovery lagi setelah 10 detik untuk slave yang sama.
```

Jadi untuk known slave, master tidak benar-benar menyerah total. Dia hanya memberi jeda supaya bus tidak penuh recovery spam.

## Kapan Master Menyerah dan Nunggu Discover?

Ada dua kategori:

### Known Slave

Known slave adalah slave yang sudah ada di registry master, punya MAC dan address saved.

Untuk known slave:

```text
Master tidak menyerah permanen.
Kalau offline/lost, master retry auto recovery tiap 10 detik per slave.
```

Known slave bisa offline di UI kalau:

- consecutive fail sudah mencapai threshold,
- atau `last_seen` lebih tua dari `5000 ms`,
- atau recovery belum berhasil.

Tapi logic recovery masih bisa dicoba lagi.

### Unknown/New Slave

Unknown slave adalah device baru yang belum ada MAC-nya di registry master.

Untuk unknown slave:

```text
Master tidak mencari otomatis saat boot.
Master menunggu user klik DISCOVER.
```

Ini sengaja supaya master tidak auto-pair device asing/tidak sengaja masuk bus.

## Flow Chat Normal Polling

```text
NORMAL LOOP
  |
  v
Setiap 1000 ms
  |
  v
Pilih 1 slave dari registry
  |
  +--> pairing_active == true?
  |       |
  |       v
  |     Skip polling normal
  |
  +--> address kosong?
  |       |
  |       v
  |     Skip
  |
  +--> boot recovery first?
  |       |
  |       v
  |     Recovery via 247 lalu confirm address saved
  |
  +--> identity due?
  |       |
  |       v
  |     Read identity 0x0000..0x0004
  |
  +--> capability due?
  |       |
  |       v
  |     Read capability/assignment 0x0010..0x0017
  |
  +--> selain itu
          |
          v
        Read sensor block 0x0100..0x010E
```

Identity due:

```text
Jika identity belum synced, atau sudah lewat 10 detik dari sync terakhir.
```

Capability due:

```text
Jika capability belum synced, atau sudah lewat 5 detik dari sync terakhir.
```

Sensor poll:

```text
Read 15 register dari 0x0100 sampai 0x010E.
1 register = 2 byte.
```

## Flow Chat Saat User Klik Discover

```text
USER KLIK DISCOVER
  |
  v
rs485_request_pairing()
  |
  v
Loop RS485 menerima pairing request
  |
  v
pairing_active = true
pairing_started_ms = millis()
pairing_timeout_ms = 30000 ms
pairing_known_scan_index = 0
pairing_restore_poll_enabled = poll_enabled sebelumnya
poll_enabled = false
  |
  v
Masuk mode Discover selama maksimal 30 detik
```

Saat Discover aktif:

```text
Setiap 700 ms:
  |
  +--> 4 detik awal sejak Discover?
  |       |
  |       v
  |     Cek 1 known slave address dari registry
  |       - read identity ke address lama
  |       - label: PAIR_KNOWN_IDENTITY
  |       - kalau MAC cocok:
  |           update slave sebagai known alive
  |           sync capability jika perlu
  |           pada kode saat ini, interval scan itu selesai di sini
  |           scan 247 lanjut pada interval berikutnya / setelah known scan tidak hit
  |
  +--> Scan address 247
          |
          v
        Read identity 0x0000..0x0004 di 247
          |
          +--> timeout
          |     tunggu interval 700 ms berikutnya
          |
          +--> identity OK
                |
                v
              Cek MAC-nya ada di registry?
                |
                +--> MAC sudah dikenal
                |     |
                |     v
                |   Recovery known pairing slave
                |     - write recovery 247:0x00F4..0x00F7
                |     - confirm ke saved address
                |     - pairing selesai
                |     - polling restore
                |
                +--> MAC belum dikenal
                      |
                      v
                    Read capability/assignment 0x0010..0x0017 di 247
                      |
                      v
                    pairing_candidate_ready = true
                    UI tampilkan UNPAIRED DEVICE
                    Scan berhenti menunggu user assign/pair
```

## Apakah Discover Harus Diklik Untuk Cek Address Lama?

Untuk cek address lama ada dua jalur:

### Jalur otomatis normal

Kalau bukan mode Discover, master polling/recovery known slave otomatis dari registry.

```text
Tidak perlu klik Discover untuk known slave yang sudah ada di registry.
```

### Jalur tambahan saat Discover

Saat Discover diklik, master juga cek address lama pada 4 detik awal.

Tujuannya:

- Kalau slave lama tidak ikut reboot dan masih hidup di address lama, master tetap bisa melihat dia alive.
- Ini berguna saat testing pakai USB/adaptor terpisah.
- Ini juga membantu kasus user klik Discover sambil device lama masih aktif.

Namun address lama hanya dicek di window awal `4000 ms`. Setelah itu Discover fokus ke `247`.

## Apakah Discover Harus Diklik Untuk Scan `247`?

Ya, untuk device baru/unknown, user harus klik Discover.

Di luar Discover, `247` hanya dipakai untuk auto recovery known slave. Master tidak melakukan scan unknown device di `247` saat boot normal.

Flow singkat:

```text
Unknown device baru colok
  |
  v
Slave boot di 247
  |
  v
Master tidak otomatis pair
  |
  v
User klik DISCOVER
  |
  v
Master scan 247
  |
  v
Jika MAC belum dikenal, UI tampilkan UNPAIRED DEVICE
  |
  v
User tekan PAIR DEVICE / ASSIGN AUTO
```

## Flow Assign / Pair Device Baru

```text
Candidate unknown ditemukan di 247
  |
  v
User pilih PAIR DEVICE / ASSIGN AUTO
  |
  v
Master pilih address
  - jika user request address tertentu: pakai itu
  - jika address 0/auto: cari address kosong dari 2..246
  |
  v
Master siapkan assignment register
  - temp assignment
  - lux assignment
  - co2 count
  - presence assignment
  - relay assignment
  - projector enable
  - ac1 enable
  - ac2 enable
  |
  v
Master write assignment ke 247:0x0010..0x0017
  |
  v
Master write NODE_ADDRESS ke 247:0x0000
  |
  v
Slave pindah dari 247 ke address baru
  |
  v
Master simpan candidate ke registry
  - address
  - MAC/UID
  - capability/profile
  - nama/room
  |
  v
pairing_active = false
poll_enabled restore ke state sebelum Discover
  |
  v
Polling normal lanjut di address baru
```

## Apa Yang Terjadi Jika Discover Timeout?

Discover punya timeout `30000 ms`.

Kalau selama 30 detik:

- tidak ada candidate ready,
- user tidak assign,
- atau device tidak menjawab,

maka:

```text
pairing_active = false
poll_enabled dikembalikan seperti sebelum Discover
pairing_candidate dikosongkan
status = "RS485 pairing timeout"
```

Setelah itu master kembali ke normal polling. Untuk mencari device baru lagi, user harus klik Discover lagi.

## Apa Yang Terjadi Jika Device Lama dan Device Baru Sama-Sama Ada?

Contoh:

- Device lama sudah paired di `0x03`.
- Device baru baru colok dan masih default di `247`.

Saat normal:

```text
Master polling/recovery device lama dari registry.
Device baru di 247 tidak dicari otomatis.
```

Saat user klik Discover:

```text
4 detik awal:
  master juga cek address lama seperti 0x03.

Selama Discover:
  master scan 247 untuk device baru.
```

Catatan penting dari kode saat ini:

```text
Kalau pada interval 700 ms master berhasil confirm known slave di address lama,
scan 247 untuk interval itu tidak dijalankan.
Scan 247 akan jalan di interval berikutnya, atau setelah window known scan selesai.
```

Jadi device baru tetap bisa dicari, tapi 4 detik awal bisa terasa sedikit ketunda karena master juga menjaga known slave.

## Kondisi Yang Bisa Terlihat Seperti "Semua Slave Hilang"

Saat `pairing_active == true`, UI masuk mode Discover. Di mode ini polling normal dipause dan layar fokus ke panel Discover/candidate.

Artinya:

- slave registry belum tentu hilang,
- device lama belum tentu kehapus,
- tapi tampilan bisa terlihat berbeda karena sedang di mode Discover.

Cara bedain dari serial:

```text
rs485 stats
```

Lihat:

- `slaves=...`
- tiap `slave[i] addr=... online=... last_seen=...`
- `pairing=1` berarti masih di mode Discover
- `pair_remain=...` sisa waktu Discover

## Log Label Yang Penting

Saat baca serial monitor, label ini bisa dipakai buat tahu master sedang apa:

| Label | Arti |
|---|---|
| `AUTO_RECOVERY` | Master mencoba restore known slave lewat `247` |
| `RECOVERY_CONFIRM` | Master confirm recovery ke address saved |
| `PAIR_KNOWN_IDENTITY` | Discover sedang cek address lama |
| `PAIR_IDENTITY` | Discover sedang scan identity di `247` |
| `PAIR_CAPABILITY` | Discover sedang baca capability candidate di `247` |
| `PAIR_SET_ASSIGN` | Master write assignment register ke candidate |
| `PAIR_SET_ADDRESS` | Master write address baru ke candidate |
| `IDENTITY_SYNC` | Polling normal refresh identity address saved |
| `CAPABILITY_SYNC` | Polling normal refresh capability address saved |
| `SENSOR_BLOCK_POLL` | Polling normal baca sensor block |

## Kesimpulan Logic Aktif Sekarang

Logic master sekarang bisa dibaca seperti ini:

```text
Known slave:
  master punya MAC + address saved.
  saat boot/reboot, recovery via 247 dulu.
  setelah berhasil, polling pakai address saved.
  kalau lost, retry recovery tiap 10 detik.

Unknown slave:
  master belum punya MAC di registry.
  tidak discan otomatis saat boot.
  harus user klik Discover.
  Discover scan 247 selama 30 detik.

Discover:
  polling normal dipause.
  scan berjalan tiap 700 ms.
  4 detik awal juga cek known address lama.
  kalau candidate unknown ketemu, tunggu user assign.
  kalau timeout, balik normal.
```

Dengan logic ini, boot tidak asal pair device baru, tapi tetap bisa memulihkan slave lama yang balik ke `247` setelah kehilangan daya.
