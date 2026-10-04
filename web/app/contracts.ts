export type View = "clock" | "status" | "config" | "wifi";

export type DeviceStatus = {
  device: string;
  domainName: string;
  fqdn: string;
  firmwareVersion: string;
  networkMode: "station" | "provisioning";
  macAddress: string;
  configurationComplete: boolean;
  metrics?: {
    uptimeSeconds: number;
    cpuFrequencyHz: number;
    ramTotalBytes: number;
    ramStaticBytes: number;
    ramAllocatedBytes: number;
    heapUsedBytes: number;
    heapCapacityBytes: number;
    flashTotalBytes: number;
    firmwareBytes: number;
    resetReason: "watchdog" | "power-on-or-external";
  };
};

export type DisplayStatus = {
  page: "time" | "date" | "temperature" | "status";
  statusView: "wifi" | "ntp" | "brightness";
  powerOverride: string;
  automaticBrightness: boolean;
  brightnessBias: number;
  temperatureUnit: "celsius" | "fahrenheit";
  temperatureCelsius: number;
  networkConnected: boolean;
  wifiSsid: string;
  signalPercent: number;
  ntpPeer: string;
  ntpSynchronized: boolean;
  lastNtpSync: string | null;
  rtcSkewSeconds: number | null;
  ambientLightPercent: number;
  brightnessPercent: number;
  powerSaving: boolean;
  powerSaveTestActive: boolean;
  powerSaveTestDurationSeconds: number;
  powerSaveTestSecondsRemaining: number;
  frame: string[];
};

export type Config = {
  ssid: string;
  passwordConfigured: boolean;
  hostname: string;
  domainName: string;
  ntpServer: string;
  timezone: string;
  displaySchedule: { enabled: boolean; off: string; on: string };
  automaticBrightness: boolean;
  manualBrightnessPercent: number;
  chimeInterval: "never" | "15min" | "30min" | "1hr";
};

export type Network = {
  ssid: string;
  rssi: number;
  signalPercent: number;
  channel: number;
  security: "open" | "secured";
};

export type Scan = {
  state: "idle" | "scanning" | "complete" | "failed";
  networks: Network[];
};

export const timezones = [
  "UTC", "America/Los_Angeles", "America/Denver", "America/Chicago",
  "America/New_York", "America/Phoenix", "Europe/London", "Europe/Berlin",
  "Asia/Tokyo",
];

export function configurationWrite(config: Config, ssid = config.ssid) {
  return {
    ssid,
    hostname: config.hostname,
    domainName: config.domainName,
    ntpServer: config.ntpServer,
    timezone: config.timezone,
    displayScheduleEnabled: config.displaySchedule.enabled,
    displayOff: config.displaySchedule.off,
    displayOn: config.displaySchedule.on,
    automaticBrightness: config.automaticBrightness,
    manualBrightnessPercent: config.manualBrightnessPercent,
    chimeInterval: config.chimeInterval,
  };
}
