import http from "node:http";

const port = Number(process.env.PICO_CLOCK_MOCK_PORT || 3001);
const defaultTarget = process.env.PICO_CLOCK_DEFAULT_TARGET || "";
const pages = ["time", "date", "temperature", "status"];
const statusViews = ["wifi", "ntp", "brightness"];
const state = {
  page: "time",
  statusView: "wifi",
  powerOverride: "automatic",
  automaticBrightness: true,
  brightnessBias: 0,
  temperatureUnit: "fahrenheit",
  timezone: "America/Los_Angeles",
  config: {
    ssid: "greenpico-dev",
    passwordConfigured: true,
    hostname: "greenpico",
    domainName: "internal",
    ntpServer: "pool.ntp.org",
    displaySchedule: { enabled: false, off: "22:00", on: "07:00" },
    automaticBrightness: true,
    manualBrightnessPercent: 50,
    chimeInterval: "never",
  },
  scanState: "idle",
  lastNtpSync: null,
  rtcSkewSeconds: 2,
  powerSaveTestDurationSeconds: 0,
  powerSaveTestUntil: 0,
};

function json(response, status, value) {
  const body = `${JSON.stringify(value)}\n`;
  response.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store",
  });
  response.end(body);
}

async function body(request) {
  let value = "";
  for await (const chunk of request) value += chunk;
  return value ? JSON.parse(value) : {};
}

function targetOrigin(value) {
  if (!value) return "";
  const normalized = String(value).trim().replace(/^http:\/\//i, "").replace(/\/$/, "");
  if (!/^[a-z0-9.-]+(?::[0-9]{1,5})?$/i.test(normalized) || normalized.length > 259) {
    throw new Error("Enter a hostname or IPv4 address, optionally followed by a port.");
  }
  return `http://${normalized}`;
}

async function proxyToGreenpico(request, response, target, path) {
  const origin = targetOrigin(target);
  const requestBody = request.method === "GET" || request.method === "HEAD"
    ? undefined : await body(request).then((value) => JSON.stringify(value));
  const upstream = await fetch(`${origin}${path}`, {
    method: request.method,
    headers: requestBody ? { "Content-Type": request.headers["content-type"] || "application/json" } : undefined,
    body: requestBody,
    redirect: "manual",
    signal: AbortSignal.timeout(5000),
  });
  const responseBody = Buffer.from(await upstream.arrayBuffer());
  response.writeHead(upstream.status, {
    "Content-Type": upstream.headers.get("content-type") || "application/json",
    "Content-Length": responseBody.length,
    "Cache-Control": "no-store",
  });
  response.end(responseBody);
}

function currentTime() {
  const parts = Object.fromEntries(new Intl.DateTimeFormat("en-CA", {
    timeZone: state.timezone,
    year: "numeric", month: "2-digit", day: "2-digit",
    hour: "2-digit", minute: "2-digit", second: "2-digit",
    hourCycle: "h23", weekday: "short",
  }).formatToParts(new Date()).map(({ type, value }) => [type, value]));
  const weekdays = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
  return {
    valid: true,
    date: `${parts.year}-${parts.month}-${parts.day}`,
    time: `${parts.hour}:${parts.minute}:${parts.second}`,
    weekday: weekdays.indexOf(parts.weekday),
    timezone: state.timezone,
    ntpSynchronized: true,
  };
}

const digits = [
  [0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110],
  [0b0100, 0b0110, 0b0100, 0b0100, 0b0100, 0b0100, 0b1110],
  [0b0110, 0b1001, 0b1000, 0b0100, 0b0010, 0b0001, 0b1111],
  [0b0110, 0b1001, 0b1000, 0b0110, 0b1000, 0b1001, 0b0110],
  [0b1000, 0b1100, 0b1010, 0b1001, 0b1111, 0b1000, 0b1000],
  [0b1111, 0b0001, 0b0111, 0b1000, 0b1000, 0b1001, 0b0110],
  [0b0100, 0b0010, 0b0001, 0b0111, 0b1001, 0b1001, 0b0110],
  [0b1111, 0b1001, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100],
  [0b0110, 0b1001, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110],
  [0b0110, 0b1001, 0b1001, 0b1110, 0b1000, 0b0100, 0b0010],
];
const dateLetters = {
  A: [0b0110, 0b1001, 0b1001, 0b1111, 0b1001, 0b1001, 0b1001],
  B: [0b0111, 0b1001, 0b1001, 0b0111, 0b1001, 0b1001, 0b0111],
  C: [0b1110, 0b0001, 0b0001, 0b0001, 0b0001, 0b0001, 0b1110],
  D: [0b0111, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0111],
  E: [0b1111, 0b0001, 0b0001, 0b0111, 0b0001, 0b0001, 0b1111],
  F: [0b1111, 0b0001, 0b0001, 0b0111, 0b0001, 0b0001, 0b0001],
  G: [0b1110, 0b0001, 0b0001, 0b1101, 0b1001, 0b1001, 0b1110],
  J: [0b1100, 0b1000, 0b1000, 0b1000, 0b1001, 0b1001, 0b0110],
  L: [0b0001, 0b0001, 0b0001, 0b0001, 0b0001, 0b0001, 0b1111],
  M: [0b1001, 0b1111, 0b1111, 0b1001, 0b1001, 0b1001, 0b1001],
  N: [0b1001, 0b1011, 0b1011, 0b1101, 0b1101, 0b1001, 0b1001],
  O: [0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110],
  P: [0b0111, 0b1001, 0b1001, 0b0111, 0b0001, 0b0001, 0b0001],
  R: [0b0111, 0b1001, 0b1001, 0b0111, 0b0101, 0b1001, 0b1001],
  S: [0b1110, 0b0001, 0b0001, 0b0110, 0b1000, 0b1000, 0b0111],
  T: [0b1111, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100, 0b0100],
  U: [0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110],
  V: [0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110, 0b0110],
  Y: [0b1001, 0b1001, 0b1001, 0b0110, 0b0100, 0b0100, 0b0100],
};
const smallDigits = [
  [0b111, 0b101, 0b101, 0b101, 0b111],
  [0b010, 0b011, 0b010, 0b010, 0b111],
  [0b111, 0b100, 0b111, 0b001, 0b111],
  [0b111, 0b100, 0b111, 0b100, 0b111],
  [0b101, 0b101, 0b111, 0b100, 0b100],
  [0b111, 0b001, 0b111, 0b100, 0b111],
  [0b111, 0b001, 0b111, 0b101, 0b111],
  [0b111, 0b100, 0b100, 0b100, 0b100],
  [0b111, 0b101, 0b111, 0b101, 0b111],
  [0b111, 0b101, 0b111, 0b100, 0b111],
];

function renderFrame() {
  const rows = Array(8).fill(0);
  const set = (x, y) => { rows[y] = (rows[y] | (1 << (23 - x))) >>> 0; };
  const drawDigit = (x, digit) => digits[digit].forEach((bits, row) => {
    for (let column = 0; column < 4; column += 1) if (bits & (1 << column)) set(x + column, row + 1);
  });
  const drawFour = (value) => {
    const bounded = Math.max(0, Math.min(9999, Math.round(value)));
    drawDigit(2, Math.floor(bounded / 1000));
    drawDigit(7, Math.floor(bounded / 100) % 10);
    drawDigit(15, Math.floor(bounded / 10) % 10);
    drawDigit(20, bounded % 10);
  };
  const drawTemperature = (value, fahrenheit) => {
    const bounded = Math.max(0, Math.min(999, Math.round(value)));
    const digitCount = bounded >= 100 ? 3 : bounded >= 10 ? 2 : 1;
    const digitStart = digitCount === 3 ? 2 : digitCount === 2 ? 5 : 7;
    let divisor = digitCount === 3 ? 100 : digitCount === 2 ? 10 : 1;
    for (let index = 0; index < digitCount; index += 1) {
      drawDigit(digitStart + index * 5, Math.floor(bounded / divisor) % 10);
      divisor /= 10;
    }
    const degreeX = digitStart + digitCount * 5;
    [1, 2].forEach((y) => { set(degreeX, y); set(degreeX + 1, y); });
    const unit = fahrenheit
      ? [0b111, 0b001, 0b111, 0b001, 0b001]
      : [0b111, 0b001, 0b001, 0b001, 0b111];
    const unitX = degreeX + 3;
    unit.forEach((bits, row) => {
      for (let column = 0; column < 3; column += 1) {
        if (bits & (1 << column)) set(unitX + column, row + 3);
      }
    });
  };
  const drawDate = (month, day) => {
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    [...months[month - 1]].forEach((letter, index) => {
      dateLetters[letter].forEach((bits, row) => {
        for (let column = 0; column < 4; column += 1) {
          if (bits & (1 << column)) set(2 + index * 5 + column, row + 1);
        }
      });
    });
    [Math.floor(day / 10), day % 10].forEach((digit, index) => {
      smallDigits[digit].forEach((bits, row) => {
        for (let column = 0; column < 3; column += 1) {
          if (bits & (1 << column)) set(17 + index * 4 + column, row + 2);
        }
      });
    });
  };
  const now = currentTime();
  const [hour, minute, second] = now.time.split(":").map(Number);
  const weekdayColumns = [[21, 22], [3, 4], [6, 7], [9, 10], [12, 13], [15, 16], [18, 19]];
  weekdayColumns[now.weekday].forEach((x) => set(x, 0));
  if (state.page === "time") {
    set(hour < 12 ? 0 : 1, 4);
    const displayHour = hour === 0 ? 12 : hour > 12 ? hour - 12 : hour;
    drawDigit(2, Math.floor(displayHour / 10));
    drawDigit(7, displayHour % 10);
    drawDigit(15, Math.floor(minute / 10));
    drawDigit(20, minute % 10);
    const secondDots = [[12, 2], [13, 2], [12, 4], [13, 4], [12, 6], [13, 6]];
    const completedDots = Math.floor(second / 10);
    secondDots.slice(0, completedDots).forEach(([x, y]) => set(x, y));
    const blinkOn = Math.floor(Date.now() / 500) % 2 === 0;
    if (second % 10 !== 0 && blinkOn) {
      const [x, y] = secondDots[completedDots];
      set(x, y);
    }
  } else if (state.page === "date") {
    const month = Number(now.date.slice(5, 7));
    const day = Number(now.date.slice(8, 10));
    drawDate(month, day);
  } else if (state.page === "temperature") {
    const fahrenheit = state.temperatureUnit === "fahrenheit";
    drawTemperature(fahrenheit ? 73 : 23, fahrenheit);
    set(fahrenheit ? 0 : 1, 3);
  } else {
    drawFour(state.statusView === "wifi" ? 84 : state.statusView === "ntp" ? 1 : 60);
  }
  if (state.config.chimeInterval !== "never") { set(0, 6); set(1, 6); }
  if (state.automaticBrightness) { set(0, 7); set(1, 7); }
  return rows.map((row) => row.toString(16).toUpperCase().padStart(6, "0"));
}

function displayStatus() {
  const now = currentTime();
  const powerSaveTestActive = state.powerSaveTestUntil === Number.POSITIVE_INFINITY ||
    state.powerSaveTestUntil > Date.now();
  return {
    page: state.page,
    statusView: state.statusView,
    powerOverride: state.powerOverride,
    automaticBrightness: state.automaticBrightness,
    brightnessBias: state.brightnessBias,
    temperatureUnit: state.temperatureUnit,
    temperatureCelsius: 22.75,
    networkConnected: true,
    wifiSsid: state.config.ssid,
    signalPercent: 84,
    ntpPeer: state.config.ntpServer,
    ntpSynchronized: true,
    lastNtpSync: state.lastNtpSync ?? `${now.date}T${now.time}`,
    rtcSkewSeconds: state.rtcSkewSeconds,
    ambientLightPercent: 63,
    brightnessPercent: Math.max(10, Math.min(100, 60 + state.brightnessBias * 10)),
    powerSaving: powerSaveTestActive,
    powerSaveTestActive,
    powerSaveTestDurationSeconds: state.powerSaveTestDurationSeconds,
    powerSaveTestSecondsRemaining: Number.isFinite(state.powerSaveTestUntil) && powerSaveTestActive
      ? Math.ceil((state.powerSaveTestUntil - Date.now()) / 1000) : 0,
    frame: renderFrame(),
    factoryResetCountdownSeconds: 0,
  };
}

function applyAction(action) {
  if (action === "next-page") state.page = pages[(pages.indexOf(state.page) + 1) % pages.length];
  else if (action.startsWith("show-")) state.page = action.slice(5);
  else if (action === "next-status") state.statusView = statusViews[(statusViews.indexOf(state.statusView) + 1) % statusViews.length];
  else if (action === "previous-status") state.statusView = statusViews[(statusViews.indexOf(state.statusView) + 2) % statusViews.length];
  else if (action === "brightness-up") state.brightnessBias = Math.min(4, state.brightnessBias + 1);
  else if (action === "brightness-down") state.brightnessBias = Math.max(-4, state.brightnessBias - 1);
  else if (action === "automatic-brightness") state.automaticBrightness = true;
  else if (action === "select-celsius") state.temperatureUnit = "celsius";
  else if (action === "select-fahrenheit") state.temperatureUnit = "fahrenheit";
  else if (action === "wake" || action === "maximum-brightness") state.powerOverride = "on-temporarily";
  else if (action === "sleep-until-schedule") state.powerOverride = "off-until-schedule";
}

const server = http.createServer(async (request, response) => {
  try {
    const path = new URL(request.url, `http://${request.headers.host}`).pathname;
    const requestedTarget = request.headers["x-greenpico-target"] || defaultTarget;
    if (requestedTarget) {
      await proxyToGreenpico(request, response, requestedTarget, path);
      return;
    }
    if (request.method === "GET" && path === "/api/v1/status") {
      return json(response, 200, {
        device: state.config.hostname,
        domainName: state.config.domainName,
        fqdn: state.config.domainName
          ? `${state.config.hostname}.${state.config.domainName}`
          : state.config.hostname,
        firmwareVersion: "dev-simulator",
        networkMode: "station",
        macAddress: "02:00:00:00:00:01",
        configurationComplete: true,
        metrics: {
          uptimeSeconds: Math.floor(process.uptime()),
          cpuFrequencyHz: 133_000_000,
          ramTotalBytes: 270_336,
          ramStaticBytes: 74_472,
          ramAllocatedBytes: 82_664,
          heapUsedBytes: 8_192,
          heapCapacityBytes: 187_672,
          flashTotalBytes: 2_097_152,
          firmwareBytes: 526_244,
          resetReason: "power-on-or-external",
        },
      });
    }
    if (request.method === "GET" && path === "/api/v1/time") return json(response, 200, currentTime());
    if (request.method === "GET" && path === "/api/v1/display") return json(response, 200, displayStatus());
    if (request.method === "GET" && path === "/api/v1/config") return json(response, 200, { ...state.config, timezone: state.timezone });
    if (request.method === "GET" && path === "/api/v1/wifi/networks") {
      return json(response, 200, { state: state.scanState, networks: state.scanState === "complete" ? [
        { ssid: "greenpico-dev", rssi: -48, signalPercent: 84, channel: 6, security: "secured" },
        { ssid: "Workshop", rssi: -67, signalPercent: 46, channel: 11, security: "secured" },
      ] : [] });
    }
    if (request.method === "POST" && path === "/api/v1/display/actions") {
      applyAction((await body(request)).action || "");
      return json(response, 202, { status: "accepted" });
    }
    if (request.method === "POST" && path === "/api/v1/power-saving/test") {
      const { durationSeconds } = await body(request);
      if (![0, 5, 10, 15].includes(durationSeconds)) {
        return json(response, 400, {
          type: "about:blank", title: "Invalid duration", status: 400,
          detail: "Use durationSeconds 0, 5, 10, or 15.",
        });
      }
      state.powerSaveTestDurationSeconds = durationSeconds;
      state.powerSaveTestUntil = durationSeconds === 0
        ? Number.POSITIVE_INFINITY : Date.now() + durationSeconds * 1000;
      return json(response, 202, { status: "accepted" });
    }
    if (request.method === "POST" && path === "/api/v1/wifi/scan") {
      state.scanState = "complete";
      return json(response, 202, { status: "accepted" });
    }
    if (request.method === "POST" && path === "/api/v1/time/sync") {
      const now = currentTime();
      state.lastNtpSync = `${now.date}T${now.time}`;
      state.rtcSkewSeconds = 0;
      return json(response, 202, { status: "accepted" });
    }
    if (request.method === "PUT" && path === "/api/v1/config") {
      const update = await body(request);
      state.timezone = update.timezone || state.timezone;
      state.config = {
        ...state.config,
        ssid: update.ssid || state.config.ssid,
        hostname: update.hostname || state.config.hostname,
        domainName: update.domainName ?? state.config.domainName,
        ntpServer: update.ntpServer || state.config.ntpServer,
        displaySchedule: {
          enabled: update.displayScheduleEnabled ?? state.config.displaySchedule.enabled,
          off: update.displayOff || state.config.displaySchedule.off,
          on: update.displayOn || state.config.displaySchedule.on,
        },
        automaticBrightness: update.automaticBrightness ?? state.config.automaticBrightness,
        manualBrightnessPercent: update.manualBrightnessPercent ?? state.config.manualBrightnessPercent,
        chimeInterval: update.chimeInterval || state.config.chimeInterval,
      };
      state.automaticBrightness = state.config.automaticBrightness;
      return json(response, 202, { status: "accepted", restartRequired: true });
    }
    return json(response, 404, { title: "Not found", status: 404, detail: "The simulated API route does not exist." });
  } catch (error) {
    const invalidTarget = error instanceof Error && error.message.startsWith("Enter a hostname");
    return json(response, invalidTarget ? 400 : 502, {
      title: invalidTarget ? "Invalid greenpico address" : "greenpico unavailable",
      status: invalidTarget ? 400 : 502,
      detail: invalidTarget ? error.message : `Could not reach greenpico: ${error instanceof Error ? error.message : "connection failed"}`,
    });
  }
});

server.listen(port, "127.0.0.1", () => {
  process.stdout.write(`greenpico development API gateway listening on http://127.0.0.1:${port}\n`);
  process.stdout.write(defaultTarget
    ? `Defaulting to physical greenpico at ${targetOrigin(defaultTarget)}\n`
    : "Using the built-in simulator until a physical greenpico is selected in the UI.\n");
});
