# Firmware architecture

The firmware is a native C++17 Pico SDK application built by PlatformIO for the
Raspberry Pi Pico W. Device configuration, networking, time synchronization,
controls, and hardware access are separated so UI and API changes do not become
timing-sensitive display or network code.

## Current startup flow

1. Load and validate configuration from the final 4 KiB flash sector.
2. Initialize the LED display scanner and DS3231 real-time clock over I2C1.
3. Render the RTC time as four uniform digits, a six-dot seconds indicator,
   the current weekday icon, and the AM or PM icon.
4. Try the saved Wi-Fi network for up to 20 seconds.
5. If connection succeeds, serve the setup page on the station IP address.
6. Otherwise, start the open `greenpico` access point and DHCP server.
7. Accept Wi-Fi, hostname/domain, NTP server, IANA timezone, and display schedule settings at
   `http://192.168.4.1/`.
8. Display the Pico W Wi-Fi MAC address on the settings page.
9. Save submitted settings atomically to flash and reboot.
10. Explicitly remove the setup AP before joining the saved network; restore it
    only if station connection fails. Send the configured station DHCP hostname
    while retaining `greenpico` as the recovery AP name.
11. In station mode, synchronize the DS3231 from NTP at startup and every six
   hours, retrying failures after five minutes.
12. Convert NTP time through the configured timezone's DST rule before writing
    the DS3231. During the configured display-off interval, blank the LEDs and
    select CYW43 aggressive power management until schedule or button wake.

## Components

| Component | Responsibility |
| --- | --- |
| `ConfigStore` | Validate and persist configuration in reserved flash |
| `WifiManager` | Select station or setup-access-point mode |
| `DhcpServer` | Give a setup client an address on `192.168.4.0/24` |
| `HttpServer` | Stream embedded web assets and serve the versioned JSON API using a fixed pool over lwIP raw TCP |
| `HttpRequest` | Match exact method/path request lines without duplicated route lengths |
| Next.js web app | Build a static Status, Configuration, and Wi-Fi interface |
| `NtpClient` | Resolve the configured host and query it using lwIP raw UDP |
| `Timezone` | Validate supported IANA names and apply DST-aware UTC conversion |
| `DisplaySchedule` | Validate `HH:MM` values and evaluate daily off intervals |
| `Ds3231` | Read/write calendar time and read the RTC temperature |
| `ButtonDebouncer` | Convert noisy active-low samples into one press/release event |
| `FrontButtons` | Read the SET, UP, and DOWN GPIO inputs |
| `Buzzer` | Drive the active-high onboard buzzer on GPIO 14 without blocking display refresh |
| `ClockControl` | Apply shared API/button actions, display overrides, and recovery gestures |
| `PowerSaveController` | Coordinate scheduled/test display blanking and timed/button wake |
| `ClockFace` | Render testable 24x8 numeric and scrolling-text framebuffers |
| `LedDisplay` | Scan framebuffer rows through the clock's LED drivers |
| `board.hpp` | Central Pico Clock Green GPIO mapping |

## Display geometry

The display is an 8x24 LED matrix driven by two SM16106 column drivers and an
SM5166P row controller. Physical columns 0 and 1 are reserved for the status
bar. The clock uses Waveshare's
uniform 4x7 numeric glyphs with one blank column inside each hour and minute
pair. Six pixels in the middle form a 2x3 seconds-progress indicator:

```text
status  digit  gap  digit  progress  digit  gap  digit
  0-1    2-5    6    7-10    12-13    15-18  19   20-23
```

The progress dots are ordered left-to-right, top-to-bottom. The active dot
blinks once per second for seconds 1 through 9 and becomes solid on the tenth
second. The next dot then repeats that pattern. At the next minute, all six
dots clear and the sequence starts again.

Display brightness uses hardware PWM on the active-low output-enable signal at
approximately 10.6 kHz. The row scanner temporarily forces output-enable high
while shifting and latching data, preventing ghost pixels without dropping
whole display frames or introducing visible low-frequency flicker.

The top-row weekday icons use paired LEDs in the clock's printed order. The
mapping from Pico SDK `datetime_t.dotw` is:

| Day (`dotw`) | Top-row columns |
| --- | --- |
| Sunday (0) | 21-22 |
| Monday (1) | 3-4 |
| Tuesday (2) | 6-7 |
| Wednesday (3) | 9-10 |
| Thursday (4) | 12-13 |
| Friday (5) | 15-16 |
| Saturday (6) | 18-19 |

AM is column 0, row 4; PM is column 1, row 4. The hour digits use 12-hour
format to match those indicators: midnight and noon render as 12, and
single-digit hours retain a leading zero so all four digit cells remain used.
The dedicated Fahrenheit and Celsius indicators are columns 0 and 1 on row 3,
respectively. The two LEDs at columns 0-1 on row 7 form the Auto Light icon;
both are lit while automatic brightness is enabled. These coordinates follow
Waveshare's reference status-bar definitions rather than reusing the AM/PM
indicators.

The Date page renders the month as a three-letter 4x7 abbreviation in columns
2-15 (`JAN` through `DEC`). The day remains visible as two compact 3x5 digits
in columns 17-23, producing a fixed `MON DD` layout without scrolling.

## Time authority and button input

NTP is the sole authority for setting the DS3231. The firmware does not provide
a manual hour or minute editing mode. The three active-low, 40 ms debounced
front-panel buttons feed `ClockControl`, the same state machine used by HTTP API
commands. It provides time, date, temperature, and status pages;
ambient-light brightness bias; schedule overrides; immediate NTP sync; a
temporary provisioning AP; and guarded factory reset.

On the temperature page, significant digits are centered without leading
zeroes, followed by a compact 2x2 degree mark and a 3x5 F or C glyph. The
dedicated left-side F/C indicator remains synchronized with that glyph. UP
explicitly selects Fahrenheit and DOWN explicitly selects Celsius. The
corresponding API actions are also explicit and idempotent
(`select-fahrenheit` and `select-celsius`).

The Status page has Wi-Fi, NTP, and Brightness views selected with UP and DOWN. It remains
open until NEXT is pressed so the marquee can show an entire long SSID or NTP
hostname. Wi-Fi shows the active SSID and signal percentage; NTP shows the
configured peer and whether the latest synchronization completed successfully.
Brightness shows the normalized ambient-light ADC reading, actual PWM output,
and whether the sensor or manual configuration controls the base level.
The same `show-status`, `next-status`, and `previous-status` operations are
available through the display-actions API.

Each successful NTP update records the local synchronization timestamp and the
RTC skew immediately before correction. Skew is defined as RTC minus NTP in
seconds, so positive values mean the RTC was ahead. These diagnostics are
included in the display-status response already used by the browser; the web
status page does not continuously request the separate clock-time endpoint.

The persisted chime interval may be disabled or aligned to 15-minute,
30-minute, or hourly wall-clock boundaries. A boundary is keyed by the full
local date, hour, and minute so the 200 ms refresh loop triggers exactly one
120 ms beep. The onboard active-high buzzer uses GPIO 14, matching Waveshare's
reference firmware, and the printed Chime status icon is enabled with it.

The networking code uses the Pico SDK's threadsafe-background CYW43
architecture. Raw lwIP calls made outside callbacks are surrounded by the SDK's
lwIP lock. Station RSSI is sampled once every five seconds; reading it in the
20 ms application loop would add needless CYW43 traffic without improving the
human-facing percentage.

## Power-saving behavior

The main loop evaluates `PowerSaveController` after debounced button input.
When the display would be off because of its schedule (or a power-saving test),
`LedDisplay` disables output and `WifiManager` selects
`CYW43_AGGRESSIVE_PM`. The radio stays associated so HTTP and NTP remain
available. The scheduled on transition or any physical button restores the
SDK's default `CYW43_PERFORMANCE_PM` policy; a button also uses the existing
30-second temporary display wake.

The external DS3231 remains the time authority, but its INT/SQW alarm signal is
not routed to a Pico GPIO in the board wiring. The firmware therefore keeps the
timer-driven, low-power main loop running and polls the RTC schedule instead of
entering RP2040 dormant mode, which would have no reliable scheduled or
front-button wake source on this hardware. The test endpoint queues the same
state-machine command as the Configuration page. Durations of 5, 10, and 15
seconds wake automatically; zero waits indefinitely for a physical button.

## Embedded web application

The `web/` project uses the Next.js App Router with `output: "export"`. Its
build produces static HTML, CSS, and compiled JavaScript. No Next.js server or
Node.js runtime is present on the Pico. `scripts/embed-web-assets.mjs` verifies
all references, rejects uncompiled TypeScript/JSX, inlines the production CSS
and JavaScript, and gzip-compresses the result into one generated firmware
asset. This avoids the browser opening many simultaneous TCP connections to a
memory-constrained device. The HTTP server streams immutable flash data in
bounded zero-copy lwIP chunks while keeping the root HTML uncached.

The interface reads and writes only the versioned API. Its Status menu combines
device and display telemetry, Configuration manages NTP, timezone, and the
display schedule, and Wi-Fi supports both CYW43 access-point scans and manual
SSID entry. Scans run asynchronously so display refresh and HTTP callbacks are
not blocked.

Shared API state uses a Pico critical section rather than a blocking mutex.
Raw lwIP callbacks run from the CYW43 background IRQ, where blocking on a mutex
held by the interrupted main loop would deadlock the clock. Protected copies
are kept short so display and network work resume immediately.

HTTP JSON formatting uses a server-owned static workspace instead of large
automatic arrays on the main/IRQ stack. Two fixed connection slots own their
request/response buffers for the server lifetime; requests do not allocate and
free multi-kilobyte objects on the embedded heap. The compiled browser client
also serializes all API calls and suppresses overlapping status polls. Loading
or polling the interface therefore adds at most one API connection alongside
the page stream instead of opening a burst of concurrent connections.

Response bodies are retained in their connection until lwIP acknowledges every
byte; the server never aborts a PCB merely because a FIN could not be queued
immediately. HTTP lengths and decimal sensor values use RP2040-compatible
integer formatting because the size-optimized Pico `printf` omits `%zu` and
floating-point conversions.

## Persistent storage

The Pico W has 2 MiB of flash. PlatformIO limits the linked application below
the reserved tail of flash, and `PICO_CLOCK_CONFIG_FLASH_OFFSET` places the
configuration at offset `0x1ff000`. A magic value, schema version, structure
size, and FNV-1a checksum reject erased, corrupt, or incompatible records.
The current schema is v5. The migration path preserves v1 fixed-offset time
settings, v2 timezone/schedule settings, v3 automatic/manual brightness, and v4
chime settings; v5 adds hostname and domain name. Missing newer fields receive
the current safe defaults.

Settings are written only after form or API submission. This avoids periodic flash
writes and leaves room to add wear-aware settings persistence later. The 4 KiB
erase/program scratch sector has static storage because placing it on the Pico's
normal application stack risks overflow.

## Security boundary

The current interface is intended for a trusted local network and setup use.
The setup access point is open, and HTTP is not encrypted or authenticated.
A versioned local JSON API exposes status, redacted configuration, configuration
replacement, display controls, provisioning and reset actions, and
immediate NTP synchronization. It is unauthenticated;
authentication and stronger request limits are required before adding remote
messages or exposing it beyond a trusted LAN.

## Extension boundaries

Display scanning, UI state, and timezone handling remain outside network
callbacks. The HTTP API communicates with the clock application through small
validated commands so slow clients cannot interfere with display refresh. See
[roadmap.md](roadmap.md) for the remaining planned work.
