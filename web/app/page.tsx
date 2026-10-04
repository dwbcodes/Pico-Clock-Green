"use client";

import { FormEvent, useCallback, useEffect, useRef, useState } from "react";
import { api, developmentTargetKey, localDevelopment } from "./api-client";
import { ClockSimulator } from "./clock-simulator";
import {
  configurationWrite,
  timezones,
  type Config,
  type DeviceStatus,
  type DisplayStatus,
  type Scan,
  type View,
} from "./contracts";

function formatBytes(bytes: number) {
  if (!Number.isFinite(bytes)) return "—";
  return bytes >= 1024 * 1024
    ? `${(bytes / (1024 * 1024)).toFixed(2)} MiB`
    : `${(bytes / 1024).toFixed(1)} KiB`;
}

function formatUptime(seconds: number) {
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return [days ? `${days}d` : "", hours || days ? `${hours}h` : "", `${minutes}m`]
    .filter(Boolean).join(" ");
}

function formatUsage(used: number, total: number) {
  const percent = total > 0 ? Math.round(used * 100 / total) : 0;
  return `${formatBytes(used)} / ${formatBytes(total)} (${percent}%)`;
}

export default function Home() {
  const [view, setView] = useState<View>("clock");
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [display, setDisplay] = useState<DisplayStatus | null>(null);
  const [config, setConfig] = useState<Config | null>(null);
  const [scan, setScan] = useState<Scan>({ state: "idle", networks: [] });
  const [selectedSsid, setSelectedSsid] = useState("");
  const [manual, setManual] = useState(false);
  const [password, setPassword] = useState("");
  const [openNetwork, setOpenNetwork] = useState(false);
  const [notice, setNotice] = useState("");
  const [error, setError] = useState("");
  const [controlBusy, setControlBusy] = useState(false);
  const [powerSaveTestSeconds, setPowerSaveTestSeconds] = useState(5);
  const [powerSaveTestBusy, setPowerSaveTestBusy] = useState(false);
  const [developmentTarget, setDevelopmentTarget] = useState("");
  const [developmentTargetDraft, setDevelopmentTargetDraft] = useState("");
  const clockPollPending = useRef(false);

  const loadStatus = useCallback(async () => {
    try {
      const [device, screen] = await Promise.all([
        api<DeviceStatus>("/api/v1/status"),
        api<DisplayStatus>("/api/v1/display"),
      ]);
      setStatus(device);
      setDisplay(screen);
      setError("");
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Unable to load status");
    }
  }, []);

  const loadClock = useCallback(async () => {
    const screen = await api<DisplayStatus>("/api/v1/display");
    setDisplay(screen);
  }, []);

  const loadConfig = useCallback(async () => {
    try {
      const current = await api<Config>("/api/v1/config");
      setConfig(current);
      setSelectedSsid(current.ssid);
      setError("");
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Unable to load configuration");
    }
  }, []);

  useEffect(() => {
    if (localDevelopment) {
      const savedTarget = window.localStorage.getItem(developmentTargetKey) ?? "";
      setDevelopmentTarget(savedTarget);
      setDevelopmentTargetDraft(savedTarget);
    }
    void loadStatus();
    void loadConfig();
  }, [loadConfig, loadStatus]);
  useEffect(() => {
    const pollInterval = view === "clock" ? 1000 : view === "status" ? 5000 : 0;
    if (pollInterval === 0) return;
    const handle = window.setInterval(() => {
      if (clockPollPending.current) return;
      clockPollPending.current = true;
      void loadClock().catch(() => undefined).finally(() => {
        clockPollPending.current = false;
      });
    }, pollInterval);
    return () => window.clearInterval(handle);
  }, [loadClock, view]);
  useEffect(() => {
    if (scan.state !== "scanning") return;
    const handle = window.setInterval(async () => {
      const latest = await api<Scan>("/api/v1/wifi/networks").catch(() => null);
      if (latest) setScan(latest);
    }, 800);
    return () => window.clearInterval(handle);
  }, [scan.state]);

  async function startScan() {
    setError("");
    setScan({ state: "scanning", networks: [] });
    try {
      await api("/api/v1/wifi/scan", { method: "POST" });
    } catch (reason) {
      setScan({ state: "failed", networks: [] });
      setError(reason instanceof Error ? reason.message : "Scan failed");
    }
  }

  async function pressFrontButton(button: "next" | "up" | "down") {
    if (!display || controlBusy) return;
    const action = button === "next"
      ? "next-page"
      : button === "up"
        ? display.page === "temperature" ? "select-fahrenheit"
          : display.page === "status" ? "next-status" : "brightness-up"
        : display.page === "temperature" ? "select-celsius"
          : display.page === "status" ? "previous-status" : "brightness-down";
    setControlBusy(true);
    setError("");
    try {
      await api("/api/v1/display/actions", {
        method: "POST",
        body: JSON.stringify({ action }),
      });
      window.setTimeout(() => { void loadClock().catch(() => undefined); }, 120);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Button action failed");
    } finally {
      setControlBusy(false);
    }
  }

  const hostname = status?.device || config?.hostname || "greenpico";
  const hostnameTarget = localDevelopment && developmentTarget
    ? developmentTarget
    : status?.fqdn || hostname;
  const hostnameUrl = `http://${hostnameTarget}/`;
  const temperatureFahrenheit = display
    ? display.temperatureCelsius * 9 / 5 + 32
    : null;

  async function selectDevelopmentTarget(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const target = developmentTargetDraft.trim()
      .replace(/^http:\/\//i, "").replace(/\/$/, "");
    if (target) window.localStorage.setItem(developmentTargetKey, target);
    else window.localStorage.removeItem(developmentTargetKey);
    setDevelopmentTarget(target);
    setDevelopmentTargetDraft(target);
    setError("");
    setNotice(target ? `Connecting to greenpico at ${target}…` : "Using the local greenpico simulator.");
    try {
      if (target) await api<DeviceStatus>("/api/v1/status");
      await Promise.all([loadStatus(), loadConfig()]);
      setNotice(target ? `Connected to greenpico at ${target}.` : "Using the local greenpico simulator.");
    } catch (reason) {
      setNotice("");
      setError(reason instanceof Error ? reason.message : "Unable to reach greenpico");
    }
  }

  async function useDevelopmentSimulator() {
    window.localStorage.removeItem(developmentTargetKey);
    setDevelopmentTarget("");
    setDevelopmentTargetDraft("");
    setNotice("Using the local greenpico simulator.");
    await Promise.all([loadStatus(), loadConfig()]);
  }

  async function saveConfiguration(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!config) return;
    setNotice("Saving configuration…");
    try {
      await api("/api/v1/config", {
        method: "PUT",
        body: JSON.stringify(configurationWrite(config)),
      });
      setNotice("Saved. The clock is restarting.");
    } catch (reason) {
      setNotice("");
      setError(reason instanceof Error ? reason.message : "Save failed");
    }
  }

  async function testPowerSaving() {
    setPowerSaveTestBusy(true);
    setError("");
    setNotice("Starting power-saving test…");
    try {
      await api("/api/v1/power-saving/test", {
        method: "POST",
        body: JSON.stringify({ durationSeconds: powerSaveTestSeconds }),
      });
      setNotice(powerSaveTestSeconds === 0
        ? "Power saving is active until a physical clock button is pressed."
        : `Power saving is active for ${powerSaveTestSeconds} seconds.`);
      window.setTimeout(() => { void loadClock().catch(() => undefined); }, 120);
    } catch (reason) {
      setNotice("");
      setError(reason instanceof Error ? reason.message : "Power-saving test failed");
    } finally {
      setPowerSaveTestBusy(false);
    }
  }

  async function saveWifi(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!config || !selectedSsid.trim()) return;
    if (selectedSsid !== config.ssid && !openNetwork && !password) {
      setError("Enter the password for the selected network.");
      return;
    }
    const body: Record<string, unknown> = configurationWrite(
      config, selectedSsid.trim());
    if (openNetwork) body.clearPassword = true;
    else if (password) body.password = password;
    setNotice("Saving Wi-Fi settings…");
    try {
      await api("/api/v1/config", { method: "PUT", body: JSON.stringify(body) });
      setNotice("Saved. Reconnect after the clock restarts.");
    } catch (reason) {
      setNotice("");
      setError(reason instanceof Error ? reason.message : "Wi-Fi save failed");
    }
  }

  return (
    <main>
      <header>
        <div className="brand-mark" aria-hidden="true"><span /><span /><span /></div>
        <div><p className="eyebrow">Pico Clock Green</p><h1><a className="hostname-link" href={hostnameUrl}>{hostname}</a></h1></div>
        <span className={`connection ${status?.networkMode ?? "unknown"}`}>
          {status?.networkMode ?? "connecting"}
        </span>
      </header>

      <nav aria-label="Control sections">
        {(["clock", "status", "config", "wifi"] as View[]).map((item) => (
          <button key={item} className={view === item ? "active" : ""} onClick={() => setView(item)}>
            {item === "config" ? "Configuration" : item === "wifi" ? "Wi-Fi" : item === "clock" ? "Clock" : "Status"}
          </button>
        ))}
        <a href="/docs">API docs</a>
      </nav>

      {error && <div className="message error" role="alert">{error}</div>}
      {notice && <div className="message success" role="status">{notice}</div>}

      {view === "clock" && (
        <section className="panel clock-page">
          <ClockSimulator display={display} busy={controlBusy} onButton={(button) => void pressFrontButton(button)} />
          {localDevelopment && (
            <form className="development-api" onSubmit={selectDevelopmentTarget}>
              <div>
                <p className="eyebrow">Development API</p>
                <strong>{developmentTarget ? `Physical greenpico · ${developmentTarget}` : "Local simulator"}</strong>
              </div>
              <label>Hostname or IP
                <input value={developmentTargetDraft} onChange={(event) => setDevelopmentTargetDraft(event.target.value)} placeholder="greenpico or 192.168.1.42" inputMode="url" />
              </label>
              <button className="primary" type="submit">Connect</button>
              {developmentTarget && <button className="secondary" type="button" onClick={() => void useDevelopmentSimulator()}>Use simulator</button>}
            </form>
          )}
        </section>
      )}

      {view === "status" && (
        <section className="panel status-page">
          <div className="section-heading"><div><p className="eyebrow">Read-only live telemetry</p><h2>Status</h2></div><span className="live-chip">Live</span></div>
          <div className="status-groups">
            <section className="status-group">
              <h3>Device</h3>
              <dl>
                <div><dt>Hostname</dt><dd><a href={hostnameUrl}>{hostname}</a></dd></div>
                <div><dt>Domain name</dt><dd>{status?.domainName || "—"}</dd></div>
                <div><dt>FQDN</dt><dd>{status?.fqdn ?? hostname}</dd></div>
                <div><dt>Device URL</dt><dd><a href={hostnameUrl}>{hostnameUrl}</a></dd></div>
                <div><dt>Firmware</dt><dd>{status?.firmwareVersion ?? "—"}</dd></div>
                <div><dt>MAC address</dt><dd>{status?.macAddress ?? "—"}</dd></div>
                <div><dt>Configured</dt><dd>{status ? (status.configurationComplete ? "Yes" : "No") : "—"}</dd></div>
              </dl>
            </section>
            <section className="status-group">
              <h3>Pico</h3>
              <dl>
                <div><dt>Uptime</dt><dd>{status?.metrics ? formatUptime(status.metrics.uptimeSeconds) : "—"}</dd></div>
                <div><dt>CPU clock</dt><dd>{status?.metrics ? `${(status.metrics.cpuFrequencyHz / 1_000_000).toFixed(0)} MHz` : "—"}</dd></div>
                <div><dt>RAM allocated</dt><dd>{status?.metrics ? formatUsage(status.metrics.ramAllocatedBytes, status.metrics.ramTotalBytes) : "—"}</dd></div>
                <div><dt>Static RAM</dt><dd>{status?.metrics ? formatBytes(status.metrics.ramStaticBytes) : "—"}</dd></div>
                <div><dt>Heap allocations</dt><dd>{status?.metrics ? formatUsage(status.metrics.heapUsedBytes, status.metrics.heapCapacityBytes) : "—"}</dd></div>
                <div><dt>Firmware flash</dt><dd>{status?.metrics ? formatUsage(status.metrics.firmwareBytes, status.metrics.flashTotalBytes) : "—"}</dd></div>
                <div><dt>Last reset</dt><dd>{status?.metrics ? (status.metrics.resetReason === "watchdog" ? "Watchdog / software restart" : "Power-on or external reset") : "—"}</dd></div>
              </dl>
            </section>
            <section className="status-group">
              <h3>Network</h3>
              <dl>
                <div><dt>Mode</dt><dd>{status?.networkMode ?? "—"}</dd></div>
                <div><dt>Connection</dt><dd>{display ? (display.networkConnected ? "Connected" : "Disconnected") : "—"}</dd></div>
                <div><dt>Wi-Fi network</dt><dd>{display?.wifiSsid || "—"}</dd></div>
                <div><dt>Signal</dt><dd>{display ? `${display.signalPercent}%` : "—"}</dd></div>
                <div><dt>NTP server</dt><dd>{display?.ntpPeer ?? "—"}</dd></div>
                <div><dt>NTP state</dt><dd>{display ? (display.ntpSynchronized ? "Synchronized" : "Not synchronized") : "—"}</dd></div>
              </dl>
            </section>
            <section className="status-group">
              <h3>Clock</h3>
              <dl>
                <div><dt>Timezone</dt><dd>{config?.timezone ?? "—"}</dd></div>
                <div><dt>Last NTP sync</dt><dd>{display?.lastNtpSync ? display.lastNtpSync.replace("T", " ") : "Never"}</dd></div>
                <div><dt>RTC skew</dt><dd>{display?.rtcSkewSeconds === null || display?.rtcSkewSeconds === undefined ? "Not measured" : display.rtcSkewSeconds === 0 ? "0 s" : display.rtcSkewSeconds > 0 ? `+${display.rtcSkewSeconds} s ahead` : `${display.rtcSkewSeconds} s behind`}</dd></div>
                <div><dt>Temperature</dt><dd>{display && temperatureFahrenheit !== null ? `${display.temperatureCelsius.toFixed(1)}°C / ${temperatureFahrenheit.toFixed(1)}°F` : "—"}</dd></div>
                <div><dt>Chime</dt><dd>{config?.chimeInterval === "15min" ? "Every 15 minutes" : config?.chimeInterval === "30min" ? "Every 30 minutes" : config?.chimeInterval === "1hr" ? "Every hour" : "Never"}</dd></div>
              </dl>
            </section>
            <section className="status-group">
              <h3>Display</h3>
              <dl>
                <div><dt>Page</dt><dd>{display?.page ?? "—"}</dd></div>
                <div><dt>Status view</dt><dd>{display?.statusView ?? "—"}</dd></div>
                <div><dt>Power mode</dt><dd>{display?.powerOverride ?? "—"}</dd></div>
                <div><dt>Power saving</dt><dd>{display ? (display.powerSaving ? "Aggressive" : "Normal") : "—"}</dd></div>
                <div><dt>Brightness</dt><dd>{display ? `${display.brightnessPercent}%` : "—"}</dd></div>
                <div><dt>Ambient light</dt><dd>{display ? `${display.ambientLightPercent}%` : "—"}</dd></div>
                <div><dt>Brightness control</dt><dd>{display ? (display.automaticBrightness ? "Automatic" : "Manual") : "—"}</dd></div>
                <div><dt>Button adjustment</dt><dd>{display ? `${display.brightnessBias > 0 ? "+" : ""}${display.brightnessBias}` : "—"}</dd></div>
                <div><dt>Temperature unit</dt><dd>{display?.temperatureUnit ?? "—"}</dd></div>
              </dl>
            </section>
          </div>
        </section>
      )}

      {view === "config" && config && (
        <section className="panel">
          <div className="section-heading"><div><p className="eyebrow">Clock behavior</p><h2>Configuration</h2></div></div>
          <form onSubmit={saveConfiguration}>
            <label>NTP server<input value={config.ntpServer} onChange={(e) => setConfig({ ...config, ntpServer: e.target.value })} required maxLength={63} /></label>
            <label>Timezone<select value={config.timezone} onChange={(e) => setConfig({ ...config, timezone: e.target.value })}>{timezones.map((zone) => <option key={zone}>{zone}</option>)}</select></label>
            <label>Chime interval<select value={config.chimeInterval} onChange={(e) => setConfig({ ...config, chimeInterval: e.target.value as Config["chimeInterval"] })}><option value="never">Never</option><option value="15min">15min</option><option value="30min">30min</option><option value="1hr">1hr</option></select></label>
            <label className="check"><input type="checkbox" checked={config.displaySchedule.enabled} onChange={(e) => setConfig({ ...config, displaySchedule: { ...config.displaySchedule, enabled: e.target.checked } })} /> Enable display schedule</label>
            <div className="row"><label>Display off<input type="time" value={config.displaySchedule.off} onChange={(e) => setConfig({ ...config, displaySchedule: { ...config.displaySchedule, off: e.target.value } })} /></label><label>Display on<input type="time" value={config.displaySchedule.on} onChange={(e) => setConfig({ ...config, displaySchedule: { ...config.displaySchedule, on: e.target.value } })} /></label></div>
            <label className="check"><input type="checkbox" checked={config.automaticBrightness} onChange={(e) => setConfig({ ...config, automaticBrightness: e.target.checked })} /> Adjust brightness automatically using the ambient-light sensor</label>
            <label>Manual brightness: {config.manualBrightnessPercent}%<input type="range" min="10" max="100" step="10" value={config.manualBrightnessPercent} onChange={(e) => setConfig({ ...config, automaticBrightness: false, manualBrightnessPercent: Number(e.target.value) })} /></label>
            <div className="row">
              <label>Power-saving test
                <select aria-label="Power-saving test duration" value={powerSaveTestSeconds} onChange={(e) => setPowerSaveTestSeconds(Number(e.target.value))}>
                  <option value={0}>Until physical button press</option>
                  <option value={5}>5 seconds</option>
                  <option value={10}>10 seconds</option>
                  <option value={15}>15 seconds</option>
                </select>
              </label>
              <button className="secondary" type="button" disabled={powerSaveTestBusy} onClick={() => void testPowerSaving()}>
                {powerSaveTestBusy ? "Starting…" : "Test power saving"}
              </button>
            </div>
            <p className="clock-hint">The display turns off and Wi-Fi enters aggressive power management. A physical front-panel button wakes it immediately.</p>
            <button className="primary" type="submit">Save and restart</button>
          </form>
        </section>
      )}

      {view === "wifi" && config && (
        <section className="panel">
          <div className="section-heading"><div><p className="eyebrow">Connectivity</p><h2>Wi-Fi</h2></div><button onClick={() => void startScan()} disabled={scan.state === "scanning"}>{scan.state === "scanning" ? "Scanning…" : "Scan networks"}</button></div>
          <form onSubmit={saveWifi}>
            <div className="row">
              <label>Hostname<input value={config.hostname} onChange={(e) => setConfig({ ...config, hostname: e.target.value })} required minLength={1} maxLength={63} pattern="[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?" placeholder="greenpico" /></label>
              <label>Domain name<input value={config.domainName} onChange={(e) => setConfig({ ...config, domainName: e.target.value })} maxLength={253} pattern="[A-Za-z0-9.-]*" placeholder="internal" /></label>
            </div>
            <div className="choice-row"><button type="button" className={!manual ? "choice active" : "choice"} onClick={() => setManual(false)}>Available networks</button><button type="button" className={manual ? "choice active" : "choice"} onClick={() => setManual(true)}>Enter manually</button></div>
            {!manual ? (
              <div className="network-list">
                {scan.networks.map((network) => <button type="button" key={network.ssid} className={selectedSsid === network.ssid ? "network selected" : "network"} onClick={() => { setSelectedSsid(network.ssid); setOpenNetwork(network.security === "open"); }}><span><strong>{network.ssid}</strong><small>Channel {network.channel} · {network.security}</small></span><b>{network.signalPercent}%</b></button>)}
                {scan.state === "complete" && scan.networks.length === 0 && <p className="empty">No networks found. Try manual entry.</p>}
                {scan.state === "idle" && <p className="empty">Scan to discover nearby access points.</p>}
              </div>
            ) : <label>Network name (SSID)<input value={selectedSsid} onChange={(e) => setSelectedSsid(e.target.value)} required maxLength={32} /></label>}
            <label>Password<input type="password" value={password} onChange={(e) => setPassword(e.target.value)} maxLength={63} placeholder={selectedSsid === config.ssid && config.passwordConfigured ? "Leave blank to keep saved password" : "Network password"} disabled={openNetwork} /></label>
            <label className="check"><input type="checkbox" checked={openNetwork} onChange={(e) => setOpenNetwork(e.target.checked)} /> Open network without a password</label>
            <button className="primary" type="submit">Save Wi-Fi and restart</button>
          </form>
        </section>
      )}

      <footer>Local device interface · <a href="/api/v1">API v1</a></footer>
    </main>
  );
}
