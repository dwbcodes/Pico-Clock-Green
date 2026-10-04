# Microprocessor

The attached controller is a **Raspberry Pi Pico W** based on the **RP2040**. It is not a Pico 2 W, which uses an RP2350.

## Hardware

| Property | Value |
| --- | --- |
| Board | Raspberry Pi Pico W |
| SDK board identifier | `pico_w` |
| Microcontroller | RP2040 |
| Silicon revision | B2 |
| Flash size | 2 MiB (2048 KiB) |
| Flash ID | `0xE66130100F69882B` |
| USB serial device | `2e8a:000a` (`/dev/ttyACM0`) |
| USB BOOTSEL device | `2e8a:0003` (RP2 Boot) |

## Installed firmware

| Property | Value |
| --- | --- |
| Name | greenpico |
| Pico SDK version | 2.1.1 |
| Build date | March 28, 2025 |
| Build type | Release |
| Boot stage | `boot2_w25q080` |
| Binary start | `0x10000000` |
| Binary end | `0x10084698` |
| Standard I/O | UART and USB |

The firmware assigns UART0 TX to GPIO 0 and UART0 RX to GPIO 1.

## Inspecting the device

Attach the device to WSL in either serial or BOOTSEL mode:

```bash
make usb-attach
```

When the board is already in BOOTSEL mode, inspect its firmware and hardware metadata with:

```bash
sudo picotool info -a
```

Return from BOOTSEL to the installed application with:

```bash
sudo picotool reboot --application
```

After rebooting, run `make usb-attach` again to expose the serial interface as `/dev/ttyACM0`.
