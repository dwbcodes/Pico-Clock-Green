# Development

This project targets a Raspberry Pi Pico W with an RP2040. The attached firmware was built with Raspberry Pi Pico SDK 2.1.1 for the `pico_w` board. See [microprocessor.md](microprocessor.md) for the detected hardware and firmware metadata.

## Development-tool responsibilities

PlatformIO can manage most of the firmware development workflow, but it cannot directly claim a Windows USB device from WSL.

| Responsibility | Tool |
| --- | --- |
| Share and attach the Windows USB device to WSL | `make usb-attach` / `usbipd-win` |
| Select the Pico W and Pico SDK | `platformio.ini` |
| Resolve the toolchain and libraries | PlatformIO |
| Build firmware | `make build` / PlatformIO |
| Upload firmware from application or BOOTSEL mode | `make flash` using `picotool` as WSL root |
| Discover serial devices | `pio device list` |
| Read USB serial output | `make monitor` / PlatformIO |
| Source-level debugging | PlatformIO plus an external SWD probe |
| Configure the Pico W's runtime Wi-Fi connection | Application firmware |

The USB attachment remains separate because PlatformIO runs inside Linux, while sharing and attaching the physical device is performed by the Windows `usbipd` service. Once `/dev/ttyACM0` or the RP2 BOOTSEL device is visible inside WSL, PlatformIO can use it.

## Prerequisites

Install GNU Make, Node.js/npm, and PlatformIO Core 6.2 or newer in Linux or WSL.
The current environment was verified with:

```text
PlatformIO Core 6.2.0
```

Confirm the tools from a normal terminal:

```bash
pio --version
node --version
npm --version
make --version
```

In constrained environments set
`PLATFORMIO_CORE_DIR=/tmp/pico-clock-platformio` so downloaded packages and
state are written to a known writable directory.

## Project configuration

The project uses the community `maxgerhardt/platform-raspberrypi` fork because
it provides Pico W definitions and native Pico SDK integration. Its revision is
pinned in [`platformio.ini`](../platformio.ini) so dependency resolution is
reproducible.

The environment selects C++17, USB CDC logging, `picotool` uploads, and the
project lwIP configuration. It also reserves the final flash sector for runtime
configuration:

```ini
[platformio]
default_envs = pico_w

[env:pico_w]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git#5d4561a05e3b212660ac6fdd3fbfb328d1988aa1
board = rpipicow
framework = picosdk
build_type = release

board_upload.maximum_size = 2093056
upload_protocol = picotool
monitor_port = /dev/ttyACM0
monitor_speed = 115200

build_unflags =
    -std=gnu++11

build_flags =
    -std=gnu++17
    -DNDEBUG
    -include $PROJECT_DIR/include/lwipopts.h
    -DPIO_STDIO_USB=1
    -DPICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
    -DPICO_CLOCK_CONFIG_FLASH_OFFSET=2093056
```

The fork's `rpipicow` board manifest selects an RP2040, 2 MiB of flash, Pico W
wireless support, and both the `picosdk` framework and `picotool` upload
protocol. PlatformIO also reserves an additional sector for its own EEPROM
layout; the application does not currently use that sector.

## USB and serial workflow

Attach the board to WSL:

```bash
make usb-attach
```

In normal application mode, verify that PlatformIO can see the serial interface:

```bash
pio device list
ls -l /dev/ttyACM0
```

Start the serial monitor:

```bash
make monitor
```

The baud rate is retained for compatibility with UART-style tools. USB CDC itself does not depend on the physical UART baud rate.

## Building

Run the web interface against the local stateful clock simulator:

```bash
make dev
```

It is served at `http://localhost:3000`. Use the Development API selector on
the Clock page to switch between the simulator and a physical greenpico. To
choose a physical device at startup instead, run
`PICO_CLOCK_API_URL=http://greenpico make dev`.

Install and build the Next.js static interface when working on `web/`:

```bash
make web-install
make web-build
```

Install Playwright's Chromium browser once and run the frontend end-to-end
tests locally, through the local UI against the live clock API, or entirely
from the page embedded in the clock:

```bash
make web-test-install
make web-test
make web-test-embedded
make web-test-live
make web-test-pico
make web-screenshots
```

`make web-screenshots` rebuilds the exact embedded page and captures the four
README images against the deterministic simulator. Regenerate and review them
when the interface changes materially.

After `make flash` uploads and restarts the clock, it waits for the device API
and reads `hostname` and `domainName` from the configuration loaded from the
Pico's reserved flash sector. It then updates the generated, ignored
`web/.env` file with `PICO_HOSTNAME`, `PICO_DOMAIN_NAME`, and
`PICO_CLOCK_API_URL`. Unrelated entries in that file are preserved.

Both live-device targets use those generated values. Because an address is
needed before the API can report its own identity, the first flash of an
already-customized clock can be bootstrapped by IP or its previous name:
`PICO_CLOCK_API_URL=http://192.168.1.42 make flash`. The same override can be
used for a single test run, for example
`PICO_CLOCK_API_URL=http://192.168.1.42 make web-test-live`.

`make flash` finishes by running `make web-test-pico` against the rebooted
clock. Flashing is not considered successful until the
embedded page, read-only API contract, repeated polling, self-contained assets,
API index, and `/docs` page pass. Physical-network tests retry once to tolerate
a brief DHCP or Wi-Fi recovery delay while retaining the first failure trace.

`web-test-embedded` runs the compiled, fully inline firmware page against the
simulator and catches embedding-specific hydration failures before upload.
`web-test-live` proxies the local Next.js interface to the generated device
URL. `web-test-pico` loads that URL directly and also
asserts that the firmware page requests no external JavaScript or CSS. Both
live modes restrict the suite to read-only behavior. Playwright retains a
trace, screenshot, and video in `web/test-results/` when a test fails.

The web build performs a production static export, verifies that all browser
JavaScript and CSS is compiled and locally available, then inlines and
gzip-compresses it into `include/generated_web_assets.hpp`. The embedded page
therefore needs one HTTP asset request and executes entirely in the browser.
Normal firmware builds and uploads run this step automatically. Concurrent
Make invocations serialize this step with `.pio/web-build.lock`, so an upload
waits for an existing web build instead of failing Next.js lock acquisition.

`include/generated_web_assets.hpp` is produced by this build and must not be
edited by hand. The frontend output directories and `web/.env` are local build
state; only the generated header and deliberate documentation screenshots are
source artifacts.

Run the native firmware tests:

```bash
make test
```

Build the release firmware:

```bash
make build
```

The UF2 output is `.pio/build/pico_w/firmware.uf2`. Inspect its embedded Pico
metadata with:

```bash
make firmware-info
```

## Automated firmware releases

`.github/workflows/firmware.yml` builds on pull requests, pushes to `main`, tag
pushes, and manual dispatches. It installs the pinned PlatformIO Core and npm
dependencies, runs `make test build`, and uploads a versioned UF2 plus SHA-256
checksum as a 30-day workflow artifact.

Tags matching `v*` also create a GitHub Release and attach those files. The tag
must be exactly `v` followed by the value in `include/version.hpp`; for example,
firmware version `1.0.0` is released with tag `v1.0.0`. Rerunning an existing
tag replaces only its two generated release assets.

Clean generated build output with:

```bash
make clean
```

## Uploading over USB

The Pico uses two USB identities:

- Application serial mode: `2e8a:000a`, exposed as `/dev/ttyACM0`.
- RP2 BOOTSEL mode: `2e8a:0003`, used by `picotool` for flashing.

Changing modes causes USB to re-enumerate. The upload wrapper handles those
transitions and reattaches each USB identity, so the canonical command is:

```bash
make flash
```

The command attaches the current identity, requests BOOTSEL when necessary,
uploads as WSL root, reboots, and reattaches application USB. During each
transition it waits for the exact expected USB VID/PID rather than accepting a
stale entry for the previous identity. It first uses Pico's vendor reset
interface and falls back to the USB CDC 1200-baud reset.

If both software reset methods fail, the currently running firmware is not
servicing USB requests. Hold BOOTSEL while resetting or reconnecting the Pico,
then run `make flash`; the wrapper will detect the already-present
`2e8a:0003` device and continue normally.

For repeated uploads, run USBIPD's reattachment loop in a dedicated Windows PowerShell window:

```powershell
usbipd attach --wsl --busid 1-3 --auto-attach
```

Leave that command running while PlatformIO resets and flashes the board. The bus ID can change if the board is moved to another physical USB port; confirm it with `usbipd list` when necessary.

The upload wrapper invokes `picotool` through `wsl.exe --user root`. USBIP
BOOTSEL nodes are normally root-only, and PlatformIO's automatic serial-port
detection cannot represent an already attached BOOTSEL device. After a
successful upload, the wrapper waits for the application to enumerate and runs
the USB attachment script again.

Keeping `monitor_port = /dev/ttyACM0` is useful because the development machine
has many persisted serial devices.

## Debugging

USB serial and BOOTSEL are sufficient for logging and flashing, but not for breakpoints or stepping through code. The Pico W has no onboard debug probe. Full debugging requires an external CMSIS-DAP, J-Link, or Raspberry Pi SWD probe wired to `SWDIO`, `SWCLK`, and ground.

For a CMSIS-DAP-compatible probe, the eventual PlatformIO environment can include:

```ini
debug_tool = cmsis-dap
upload_protocol = cmsis-dap
```

The debug probe must also be attached to WSL through `usbipd-win` before PlatformIO can access it.

## First-run Wi-Fi and NTP configuration

The firmware never embeds Wi-Fi credentials in `platformio.ini` or source
control. If no valid saved configuration exists, or the saved network cannot be
reached within 20 seconds, it starts an open setup network named `greenpico`.

Connect a phone or computer to that network, browse to
`http://192.168.4.1/`, and enter:

- Wi-Fi network name and password
- DHCP hostname (default `greenpico`)
- local DNS domain (default `internal`)
- NTP hostname (default `pool.ntp.org`)
- IANA timezone (with automatic DST for supported zones)
- optional daily display off/on times

The values are validated, stored in the reserved flash sector, and applied
after an automatic restart. The setup network is explicitly disabled before
joining the saved network; it is restored only when that connection fails.
The device sends the configured hostname in its DHCP request and reports the
configured `hostname.domain` FQDN through the API and web interface. Resolution
depends on local router/DNS support; changing this setting does not register an
arbitrary public DNS name. When station mode succeeds, the serial log prints
both the LAN address and configured identity. The page also displays the
Pico W Wi-Fi MAC address for identifying the device on the local network.
Supported IANA zones apply their daylight-saving transition rules automatically.

## References

- [PlatformIO Raspberry Pi Pico board documentation](https://docs.platformio.org/en/stable/boards/raspberrypi/pico.html)
- [PlatformIO serial monitor configuration](https://docs.platformio.org/en/latest/projectconf/sections/env/options/monitor/monitor_port.html)
- [PlatformIO upload-port configuration](https://docs.platformio.org/en/stable/projectconf/sections/env/options/upload/upload_port.html)
- [Community PlatformIO Raspberry Pi platform fork](https://github.com/maxgerhardt/platform-raspberrypi)
- [Pico SDK PlatformIO example for `rpipicow`](https://github.com/maxgerhardt/platform-raspberrypi/tree/develop/examples/picosdk-blink)
- [Next.js static export guide](https://nextjs.org/docs/app/guides/static-exports)
