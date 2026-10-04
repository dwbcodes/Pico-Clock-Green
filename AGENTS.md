# Repository agent instructions

## Persistence and problem resolution

Own the requested outcome, not merely the first command attempt. When an
operation fails, continue working within the available capabilities instead of
immediately handing the failure back to the user.

For each failure:

1. Read the complete error and identify the layer that produced it.
2. Reproduce it with the repository's supported command.
3. Inspect relevant local state, logs, configuration, permissions, and device
   state using safe read-only checks.
4. Try safe, in-scope alternatives that preserve the user's requested outcome.
5. Fix repository code, scripts, configuration, or documentation when they are
   the cause, then rerun the original command.
6. Add regression coverage when the failure exposes testable behavior.

Do not ask the user to run a command that the agent can run in its current
environment. Escalate only after practical in-scope alternatives have been
exhausted and an external permission, physical action, credential, or sandbox
boundary is proven to be required. When escalation is unavoidable, state the
exact failing command and boundary, leave the project in a working state, and
request only the smallest action needed to continue. Resume verification as
soon as that action is complete.

Never evade or weaken a security sandbox. Treat sandbox policy as an external
capability boundary while still fixing any independent repository issues found
during diagnosis.

## Use the Makefile as the project interface

Use a repository Make target whenever one exists. Do not invoke PlatformIO,
`picotool`, `usbipd`, or the PowerShell attachment script directly when the
Makefile already exposes that operation.

Run `make` to discover the supported workflow. The primary commands are:

- `make test` — run all native unit tests.
- `make build` — build the release Pico W firmware.
- `make firmware-info` — inspect metadata in the built ELF.
- `make usb-attach` — find, share, and attach the Pico from Windows to WSL.
- `make flash` — the canonical device workflow: run native tests, build, attach,
  upload, wait for reboot, synchronize the web test hostname, and run the
  read-only physical Playwright suite.
- `make monitor` — open `/dev/ttyACM0` at 115200 baud.
- `make clean` — remove PlatformIO build output.

If a repeatable project operation has no Make target, add the target before
using the operation. Keep implementation details in `scripts/` when they are
too substantial for a Make recipe.

## Firmware change verification

For every firmware behavior change:

1. Add or update tests for hardware-independent behavior.
2. Run `make test`.
3. Run `make build`.
4. Inspect warnings and memory usage; do not treat a linker-only success as
   sufficient validation.

Run `make flash` only when the user explicitly authorizes replacing firmware.
After flashing, reattach the application-mode USB device with
`make usb-attach` before using `make monitor`.

## Windows and WSL USB boundary

The device has two identities:

- Application USB CDC: `2e8a:000a`, normally `/dev/ttyACM0`.
- RP2 BOOTSEL: `2e8a:0003`, used for `picotool` upload.

USB re-enumeration can drop the WSL attachment. Always use `make usb-attach`
again after a reboot or upload. The target may request Windows administrator
approval when a device has not previously been shared.

If the execution environment blocks Windows interoperability or privileged
USB/IP attachment, first confirm the device state, try the supported Make
workflow, and fix any independent workflow defects. Only then report the exact
failed Make target and error. Do not bypass the repository workflow with ad-hoc
USB commands or attempt to weaken the execution sandbox.

## Source and licensing policy

Use external clock projects as behavior and hardware references only unless
their licences are compatible and attribution requirements are preserved.
Do not copy code from a repository that declares no licence. Keep third-party
copyright and SPDX notices with adapted source.

## Project orientation

This repository contains firmware for the Waveshare Pico-Clock-Green using a
Raspberry Pi Pico W, Pico SDK, and C++17. Firmware version 0.14.0 is the current
baseline. Read these sources before changing behavior:

- `README.md` is the human product, setup, build, and test guide.
- `docs/architecture.md` records implementation constraints and hardware behavior.
- `docs/api/openapi.yaml` is the authoritative HTTP API contract.
- `docs/development.md` documents the toolchain and Windows/WSL USB boundary.
- `docs/README.md` links the Waveshare documentation and locally retained
  DS3231, SM5166P, and SM16106SC datasheets.
- `docs/roadmap.md` lists only work that remains after the current baseline.

When new durable knowledge is discovered, update the appropriate file in
`docs/`; do not leave it only in chat history or add speculative requirements.

## Product invariants

- NTP is the sole authority for setting time. Do not add manual time-setting
  flows. Successful synchronization writes timezone-adjusted local time to the
  DS3231; normal display operation then reads the RTC.
- The display is 24x8. Columns 0-1 are status icons. Four 4x7 time glyphs and a
  six-dot seconds indicator fill the remaining clock face. Preserve the tested
  mappings in `ClockFace` and `docs/architecture.md`.
- Setup mode is an open `greenpico` AP only while configuration is absent,
  station connection has failed, or provisioning is explicitly requested.
  Successful station mode must remove the AP.
- `greenpico.internal` is the default identity, not guaranteed DNS. Resolution
  depends on DHCP/DNS behavior on the user's LAN.
- The embedded Next.js application is a static build. JavaScript executes in
  the browser; no Node.js runtime or server-side rendering exists on the Pico.
- Browser API calls stay serialized. The raw-lwIP server has a fixed pool of two
  connection buffers, so avoid parallel fetch bursts and per-request heap use.
- Raw lwIP callbacks can run in the CYW43 background context. Cross-context API
  state uses short Pico critical sections, never a blocking mutex that an IRQ
  could acquire while interrupting its owner.
- Avoid large automatic buffers on RP2040 stacks. The flash-sector scratch
  buffer and HTTP workspaces intentionally have static storage.
- The final 4 KiB flash sector at `0x1ff000` is reserved for configuration.
  Preserve atomic erase/program behavior, checksums, validation, and migrations.
- The DS3231 INT/SQW pin is not routed to a Pico GPIO on this board. Scheduled
  display-off mode therefore cannot use RP2040 dormant sleep: it blanks the
  display, keeps the schedule loop and API alive, and selects
  `CYW43_AGGRESSIVE_PM` until schedule or button wake.
- Alarms, countdown timers, and manual clock setting are intentionally outside
  current product scope.

## API-first change policy

Treat the API as the boundary between firmware behavior and every interface.
For a changed or new capability, update all applicable layers together:

1. `docs/api/openapi.yaml` and `docs/api/README.md`.
2. Firmware parsing, command/state handling, and responses.
3. `scripts/dev-api.mjs`, which is the deterministic development simulator.
4. `web/app/contracts.ts`, `web/app/api-client.ts`, and the relevant view.
5. Native unit tests and Playwright scenarios.
6. Human documentation when observable behavior changes.

Physical button operations and HTTP operations must converge on `ClockControl`
or the appropriate shared state machine instead of implementing two behaviors.

## Source map

- `src/main.cpp` owns initialization and the nonblocking device loop.
- `ClockControl`, `ClockFace`, and `PowerSaveController` contain testable device
  behavior independent of networking.
- `WifiManager`, `NtpClient`, `DhcpServer`, and `HttpServer` contain Pico/lwIP
  integration. RSSI is sampled on a five-second cadence, not every loop.
- `ConfigStore` validates and stores schema-versioned configuration.
- `web/app/` contains the static client UI; keep API contracts in `contracts.ts`
  and request scheduling in `api-client.ts` rather than duplicating either in views.
- `scripts/embed-web-assets.mjs` validates and packages the static export.
- `scripts/dev-api.mjs` and `scripts/preview-embedded-web.mjs` provide local test
  backends; `web/tests/e2e/` contains Playwright coverage.
- `test/test_*` contains native Unity tests. Hardware-independent logic belongs
  in these testable components rather than `main.cpp`.

## Generated and local files

- `include/generated_web_assets.hpp` is generated by `make web-build`; never
  hand-edit it. Commit it because firmware compilation consumes it.
- `web/out/`, `web/.next/`, `.pio/`, and `web/test-results/` are build output.
- `web/.env` is generated from a connected clock after flashing and is local.
- `docs/images/web-*.png` are reproducible README screenshots generated by
  `make web-screenshots`; commit them when the interface materially changes.

## Required verification

Use `PLATFORMIO_CORE_DIR=/tmp/pico-clock-platformio` in constrained or automated
environments so PlatformIO state is writable and reproducible.

- Firmware logic: `make test`, then `make build`; inspect all warnings and the
  RAM/flash report.
- Frontend behavior: `make web-test`.
- Embedded asset or client boot changes: also run `make web-test-embedded`.
- API changes: exercise the native parser tests, both local browser modes, and
  keep the simulator and OpenAPI contract synchronized.
- Physical tests (`make web-test-live`, `make web-test-pico`) are read-only but
  require a reachable clock. Never substitute them for the local deterministic
  suites.

At the 0.14.0 baseline there are 19 native test programs containing 87 cases.
Do not hard-code that count in scripts; it is supplied here only as an audit
reference.

## Releases and versioning

The public version appears in `include/version.hpp`, `web/package.json`,
`web/package-lock.json`, and `docs/api/openapi.yaml`. Update all four together.
Build the static web export before release so `include/generated_web_assets.hpp`
matches the tagged source. `.github/workflows/firmware.yml` verifies that a
release tag is exactly `v<firmware-version>`, builds through `make test build`,
and publishes the versioned UF2 and checksum. Do not flash a physical clock
unless the user explicitly authorizes it.
