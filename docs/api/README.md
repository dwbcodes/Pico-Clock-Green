# greenpico HTTP API

The source-of-truth contract is [openapi.yaml](openapi.yaml). Load that file in
Swagger Editor or Swagger UI to explore the API and generate clients.

All endpoints are versioned under `/api/v1`. JSON configuration reads redact
the Wi-Fi password. Omitting `password` from a write preserves the existing
secret; `clearPassword: true` explicitly configures an open network.

Configuration writes return HTTP `202 Accepted`, save to the reserved flash
sector, and restart the clock. Invalid input uses
`application/problem+json`, following RFC 9457's problem-details shape.

`hostname` and `domainName` are persisted network identity settings. The
hostname is sent in the station DHCP request; the API reports the resulting
FQDN as `hostname.domainName`. The local router or DNS server must own that
domain and register DHCP clients—selecting an arbitrary domain on the clock
does not create a public or authoritative DNS record. The setup AP name stays
`greenpico` so recovery is predictable.

The supported `make flash` workflow reads this persisted identity back from
`GET /config` after restart, verifies it against `GET /status`, and writes the
hostname, domain, and derived device URL to the ignored `web/.env` development
file. This keeps live browser tests aligned with the flashed device.

The embedded timezone table deliberately supports a bounded set of canonical
IANA identifiers documented in the OpenAPI enum. US and European daylight
saving transitions are calculated at each NTP synchronization. Unsupported
identifiers are rejected rather than falling back to a potentially wrong
offset.

The API and browser form are currently unauthenticated. They are appropriate
only for a trusted local network; authentication is required before exposing
the device through port forwarding or an untrusted VLAN.

## Runtime controls

Runtime control is intentionally separate from persistent configuration:

- `GET /status` reports device identity and Pico runtime metrics: uptime, CPU
  frequency, static and heap RAM allocation, firmware flash use, and reset cause.
- `GET /time` reports the current local date and time read from the DS3231,
  along with its configured timezone and latest NTP synchronization state.
- `GET /display` reports the active information page, exact 24×8 LED
  framebuffer, display override, brightness bias, ambient light, actual
  brightness, RTC temperature, network signal, last successful NTP sync, and
  the RTC skew measured immediately before that correction.
- `POST /display/actions` changes pages, brightness, units, wake, or sleep.
- `POST /power-saving/test` blanks the display and selects aggressive CYW43
  Wi-Fi power management for 5, 10, or 15 seconds, or until a physical button
  press when `durationSeconds` is zero.
- `POST /wifi/scan` starts an asynchronous access-point scan.
- `GET /wifi/networks` returns scan state and up to twelve unique SSIDs,
  ordered by signal strength.
- `POST /time/sync` requests an immediate NTP update.
- `POST /network/provisioning` temporarily replaces station mode with the
  `greenpico` setup AP.
- `POST /device/factory-reset` erases configuration only when the request body
  contains `{"confirmation":"erase-all-settings"}`.

Action requests return `202 Accepted` because the networking callback queues
them for the main clock loop. Physical button gestures enter that same command
path, so API behavior and front-panel behavior cannot diverge.

When the configured display schedule is enabled and its off period is active,
the display output is disabled and the CYW43 changes from its default
performance PM policy to `CYW43_AGGRESSIVE_PM`. The station remains associated,
so NTP and the local API continue to work. The DS3231 keeps authoritative time,
and the normal low-power main loop checks it for the scheduled wake. This board
does not route the DS3231 INT/SQW alarm output to a Pico GPIO, so firmware does
not enter RP2040 dormant mode: doing so could not wake reliably at the on time
or from the existing polled buttons. Any front-panel button temporarily wakes
a scheduled-off display and immediately restores normal CYW43 power management.

```json
{"durationSeconds":5}
```

The allowed values are `0`, `5`, `10`, and `15`; zero is deliberately
indefinite and intended to verify physical-button wake-up.

Temperature units use explicit, idempotent `select-fahrenheit` and
`select-celsius` actions. On the physical Temperature page, UP selects F and
DOWN selects C; the matching dedicated status-bar icon confirms the selection.
The Auto Light status-bar icon is lit while `automaticBrightness` is true.

The fourth information page is Status. Its Wi-Fi, NTP, and Brightness items can be selected
with the idempotent page action plus directional status actions:

```json
{"action":"show-status"}
{"action":"next-status"}
{"action":"previous-status"}
```

`GET /display` reports the selected `statusView`, Wi-Fi SSID and strength, NTP
peer and synchronization state, last successful synchronization time, RTC skew,
ambient-light percentage, and actual display brightness percentage used by the
physical scrolling display. RTC skew is reported as RTC minus NTP seconds, so a
positive value means the RTC was ahead before it was corrected.

Persistent brightness policy belongs to `GET/PUT /config`:
`automaticBrightness` selects the ambient-light sensor, while
`manualBrightnessPercent` supplies the 10–100% fallback used when automatic
control is disabled. Existing configuration clients may omit both fields to
preserve their saved values.

`chimeInterval` configures the onboard buzzer using `never`, `15min`, `30min`,
or `1hr`. Chimes align to wall-clock boundaries and emit one short beep. The
printed Chime status icon is lit whenever an interval is enabled.

The device itself serves a compact human-readable reference at `/docs` and a
machine-readable discovery response at `/api/v1`. The setup page links to both.
