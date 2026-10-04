.DEFAULT_GOAL := help

.PHONY: help check-platformio test dev web-install web-test-install web-test web-test-embedded web-test-live web-test-pico web-build web-screenshots build clean flash sync-web-env monitor usb-attach firmware-info

SHELL := /bin/bash
PLATFORMIO ?= $(shell command -v platformio 2>/dev/null || command -v pio 2>/dev/null || printf '%s' "$$HOME/.platformio/penv/bin/platformio")
POWERSHELL := /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
USB_ATTACH_LOG := .usb-attach.log
WEB_BUILD_LOCK := .pio/web-build.lock
WEB_TEST_LOCK := .pio/web-test.lock
FIRMWARE_ELF := .pio/build/pico_w/firmware.elf
FIRMWARE_UF2 := .pio/build/pico_w/firmware.uf2
FIRMWARE_WSL_PATH := $(abspath $(FIRMWARE_UF2))

help:
	@printf '%s\n' \
		'Available targets:' \
		'  test           Run the native unit-test suite' \
		'  dev            Run the web UI with a simulated local greenpico API' \
		'  web-test       Test the local UI and simulated API in Chromium' \
		'  web-test-embedded  Test the compiled inline firmware page locally' \
		'  web-test-live  Test the local UI against the configured physical clock' \
		'  web-test-pico  Test the frontend served by the configured physical clock' \
		'  web-build      Compile, verify, and embed the client-only Next.js interface' \
		'  web-screenshots  Rebuild the README screenshots from the local simulator' \
		'  build          Build the Pico W firmware with PlatformIO' \
		'  clean          Remove PlatformIO build output' \
		'  flash          Test, build, attach, upload, reboot, and validate' \
		'  sync-web-env   Read the clock network identity into web/.env' \
		'  monitor        Open the USB serial monitor on /dev/ttyACM0' \
		'  usb-attach     Attach the Pico to WSL in serial or BOOTSEL mode' \
		'  firmware-info  Show metadata from the built firmware image'

check-platformio:
	@if [[ -z "$(strip $(PLATFORMIO))" ]] || ! "$(PLATFORMIO)" --version >/dev/null 2>&1; then \
		printf '%s\n' 'PlatformIO Core was not found. Install it or set PLATFORMIO=/path/to/platformio.' >&2; \
		exit 127; \
	fi

test: check-platformio
	$(PLATFORMIO) test --environment native

dev:
	node scripts/dev.mjs

web-install:
	npm_config_cache=/tmp/pico-clock-npm npm --prefix web install

web-test-install: web-install
	npm_config_cache=/tmp/pico-clock-npm npm --prefix web exec -- playwright install chromium

web-test:
	@test -d web/node_modules/@playwright/test || $(MAKE) web-install
	@mkdir -p $(dir $(WEB_TEST_LOCK))
	flock $(WEB_TEST_LOCK) env npm_config_cache=/tmp/pico-clock-npm npm --prefix web run test:e2e

web-test-embedded: web-build
	@test -d web/node_modules/@playwright/test || $(MAKE) web-install
	@mkdir -p $(dir $(WEB_TEST_LOCK))
	flock $(WEB_TEST_LOCK) env PLAYWRIGHT_TARGET=embedded-local \
		npm_config_cache=/tmp/pico-clock-npm npm --prefix web run test:e2e

web-test-live:
	@test -d web/node_modules/@playwright/test || $(MAKE) web-install
	@mkdir -p $(dir $(WEB_TEST_LOCK))
	flock $(WEB_TEST_LOCK) env PLAYWRIGHT_TARGET=local-live \
		npm_config_cache=/tmp/pico-clock-npm npm --prefix web run test:e2e

web-test-pico:
	@test -d web/node_modules/@playwright/test || $(MAKE) web-install
	@mkdir -p $(dir $(WEB_TEST_LOCK))
	flock $(WEB_TEST_LOCK) env PLAYWRIGHT_TARGET=pico \
		npm_config_cache=/tmp/pico-clock-npm npm --prefix web run test:e2e

web-build:
	@test -d web/node_modules || $(MAKE) web-install
	@mkdir -p $(dir $(WEB_BUILD_LOCK))
	flock $(WEB_BUILD_LOCK) env npm_config_cache=/tmp/pico-clock-npm npm --prefix web run build

web-screenshots: web-build
	@test -d web/node_modules/@playwright/test || $(MAKE) web-install
	node web/scripts/capture-screenshots.mjs

build: web-build check-platformio
	$(PLATFORMIO) run

clean: check-platformio
	$(PLATFORMIO) run --target clean

flash:
	$(MAKE) test
	$(MAKE) build
	$(MAKE) usb-attach
	@printf '%s\n' 'Uploading through the Windows/WSL-root picotool wrapper (no PlatformIO port detection).'
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass \
		-File ./scripts/upload-usb.ps1 -FirmwarePath $(FIRMWARE_WSL_PATH)
	$(MAKE) sync-web-env
	$(MAKE) web-test-pico

sync-web-env:
	node scripts/sync-web-env.mjs

monitor: check-platformio
	$(PLATFORMIO) device monitor --port /dev/ttyACM0 --baud 115200

firmware-info: build
	picotool info -a $(FIRMWARE_ELF)

usb-attach:
	@set -o pipefail; \
		$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File ./scripts/attach-usb.ps1 2>&1 \
		| tee $(USB_ATTACH_LOG)
