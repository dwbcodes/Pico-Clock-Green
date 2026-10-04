# Remaining roadmap

The core clock, NTP/RTC synchronization, display pages, buttons, brightness,
chime, Wi-Fi provisioning, browser interface, and versioned API are implemented.
This list contains only work that remains useful after the 1.0.0 baseline.

## Hardware validation

- Exercise first-time provisioning, failed-network fallback, reboot persistence,
  timezone transitions, and RTC updates on production hardware.
- Run an overnight display schedule and verify scheduled and button wake,
  CYW43 power-policy transitions, NTP renewal, and long-term display stability.
- Validate all chime intervals and the ambient-light curve across real room
  lighting conditions.

## Performance and reliability

- Evaluate moving the CPU-timer LED scan loop to PIO/DMA. Preserve the tested
  framebuffer and PWM behavior if this is implemented.
- Add long-running soak tests for HTTP connection reuse, repeated page loads,
  Wi-Fi loss/recovery, and NTP failure.
- Consider generating TypeScript API types from the OpenAPI contract to reduce
  the remaining manual contract synchronization.

## Product hardening

- Add authentication, request throttling, and a secure deployment model before
  exposing the device outside a trusted LAN.
- Design a firmware update and recovery mechanism with rollback protection.
- Expand the bounded timezone table only when a required region is identified,
  or evaluate a compact POSIX-TZ representation.
- Extend responsive and accessibility browser coverage for additional screen
  sizes and assistive technology.

Alarms, countdown timers, and manual time setting are intentionally outside the
current product scope. NTP remains the sole time-setting authority.
