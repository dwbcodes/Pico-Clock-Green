import { expect, test, type Page } from "@playwright/test";

const target = process.env.PLAYWRIGHT_TARGET ?? "local-mock";
const physicalTarget = target === "pico" || target === "local-live";
const embeddedTarget = target === "pico" || target === "embedded-local";

type NetworkIdentity = { device: string; domainName: string; fqdn: string };
type NetworkConfig = { hostname: string; domainName: string };

async function networkIdentity(page: Page) {
  const response = await page.request.get("/api/v1/status");
  expect(response.ok()).toBeTruthy();
  return response.json() as Promise<NetworkIdentity>;
}

async function networkConfig(page: Page) {
  const response = await page.request.get("/api/v1/config");
  expect(response.ok()).toBeTruthy();
  return response.json() as Promise<NetworkConfig>;
}

function captureBrowserFailures(page: Page) {
  const failures: string[] = [];
  page.on("pageerror", (error) => failures.push(`page error: ${error.message}`));
  page.on("requestfailed", (request) => {
    const url = new URL(request.url());
    if (url.pathname === "/favicon.ico") return;
    if (embeddedTarget && url.pathname.startsWith("/_next/static/chunks/") &&
        request.failure()?.errorText === "net::ERR_ABORTED") return;
    failures.push(`request failed: ${request.method()} ${url.pathname}: ${request.failure()?.errorText}`);
  });
  return failures;
}

test("hydrates and navigates through every frontend section", async ({ page }) => {
  const failures = captureBrowserFailures(page);
  const identity = await networkIdentity(page);
  await page.goto("/");

  await expect(page.getByRole("heading", { name: identity.device, exact: true })).toBeVisible();
  await page.waitForTimeout(500);
  expect(failures).toEqual([]);
  await expect(page.getByRole("link", { name: identity.device, exact: true }))
    .toHaveAttribute("href", `http://${identity.fqdn}/`);
  await expect(page.getByRole("button", { name: "Clock", exact: true })).toHaveClass(/active/);
  await expect(page.getByRole("button", { name: /NEXT Next page/ })).toBeVisible();

  await page.getByRole("button", { name: "Status", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Status", exact: true })).toBeVisible();

  await page.getByRole("button", { name: "Configuration", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Configuration" })).toBeVisible();

  await page.getByRole("button", { name: "Wi-Fi", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Wi-Fi", exact: true })).toBeVisible();

  await page.getByRole("button", { name: "Clock", exact: true }).click();
  await expect(page.locator("section.clock-page")).toBeVisible();
  expect(failures).toEqual([]);
});

test("renders live status and configuration returned by the API", async ({ page }) => {
  const failures = captureBrowserFailures(page);
  await page.goto("/");

  await page.getByRole("button", { name: "Status", exact: true }).click();
  await expect(page.getByText("Firmware", { exact: true })).toBeVisible();
  await expect(page.getByText("MAC address", { exact: true })).toBeVisible();
  await expect(page.getByText("RAM allocated", { exact: true })).toBeVisible();
  await expect(page.getByText("Heap allocations", { exact: true })).toBeVisible();
  await expect(page.getByText("Firmware flash", { exact: true })).toBeVisible();
  await expect(page.getByText("Wi-Fi network", { exact: true })).toBeVisible();
  await expect(page.getByText("Last NTP sync", { exact: true })).toBeVisible();
  await expect(page.getByText("RTC skew", { exact: true })).toBeVisible();
  await expect(page.getByText("RTC time", { exact: true })).toHaveCount(0);
  await expect(page.getByText("Ambient light", { exact: true })).toBeVisible();
  await expect(page.locator("section.status-page button")).toHaveCount(0);

  await page.getByRole("button", { name: "Configuration", exact: true }).click();
  await expect(page.getByLabel("NTP server")).not.toHaveValue("");
  await expect(page.getByLabel("Timezone").locator("option")).toHaveCount(9);
  await expect(page.getByLabel("Chime interval").locator("option")).toHaveCount(4);
  await expect(page.getByLabel("Power-saving test duration").locator("option")).toHaveCount(4);
  await expect(page.getByLabel(/Adjust brightness automatically/)).toBeVisible();
  const brightness = page.getByLabel(/Manual brightness/);
  await brightness.fill("70");
  await expect(brightness).toHaveValue("70");
  await expect(page.getByLabel(/Adjust brightness automatically/)).not.toBeChecked();
  expect(failures).toEqual([]);
});

test("starts a timed power-saving test through the API", async ({ page }) => {
  test.skip(physicalTarget, "Live-device suites are intentionally read-only");
  await page.goto("/");
  await page.getByRole("button", { name: "Configuration", exact: true }).click();
  await page.getByLabel("Power-saving test duration").selectOption("5");
  const responsePromise = page.waitForResponse((response) =>
    new URL(response.url()).pathname === "/api/v1/power-saving/test" &&
    response.request().method() === "POST");
  await page.getByRole("button", { name: "Test power saving" }).click();
  expect((await responsePromise).ok()).toBe(true);
  await expect(page.getByRole("status")).toContainText("active for 5 seconds");
});

test("supports both discovered-network and manual Wi-Fi forms", async ({ page }) => {
  const failures = captureBrowserFailures(page);
  const config = await networkConfig(page);
  await page.goto("/");
  await page.getByRole("button", { name: "Wi-Fi", exact: true }).click();

  await expect(page.getByLabel("Hostname", { exact: true })).toHaveValue(config.hostname);
  await expect(page.getByLabel("Domain name", { exact: true })).toHaveValue(config.domainName);
  await expect(page.getByText("Scan to discover nearby access points.")).toBeVisible();
  await page.getByRole("button", { name: "Enter manually" }).click();
  await expect(page.getByLabel("Network name (SSID)")).toBeVisible();
  await expect(page.getByLabel("Password", { exact: true })).toHaveAttribute("type", "password");
  await page.getByRole("button", { name: "Available networks" }).click();
  await expect(page.getByText("Scan to discover nearby access points.")).toBeVisible();
  expect(failures).toEqual([]);
});

test("continues polling without breaking navigation", async ({ page }) => {
  const failures = captureBrowserFailures(page);
  let displayResponses = 0;
  let timeRequests = 0;
  page.on("request", (request) => {
    if (new URL(request.url()).pathname === "/api/v1/time") timeRequests += 1;
  });
  page.on("response", (response) => {
    const path = new URL(response.url()).pathname;
    if (response.ok() && path === "/api/v1/display") displayResponses += 1;
  });

  await page.goto("/");
  await expect.poll(() => displayResponses, { timeout: 8_000 }).toBeGreaterThanOrEqual(2);
  expect(timeRequests).toBe(0);
  await page.getByRole("button", { name: "Status", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Status", exact: true })).toBeVisible();
  expect(failures).toEqual([]);
});

test("read-only API endpoints satisfy the frontend contract", async ({ request }) => {
  const statusResponse = await request.get("/api/v1/status");
  expect(statusResponse.ok()).toBeTruthy();
  const status = await statusResponse.json();
  expect(status).toEqual(expect.objectContaining({
    device: expect.any(String),
    domainName: expect.any(String),
    fqdn: expect.any(String),
    firmwareVersion: expect.any(String),
    networkMode: expect.stringMatching(/^(station|provisioning)$/),
    configurationComplete: expect.any(Boolean),
    metrics: expect.objectContaining({
      uptimeSeconds: expect.any(Number),
      cpuFrequencyHz: expect.any(Number),
      ramTotalBytes: expect.any(Number),
      ramAllocatedBytes: expect.any(Number),
      heapUsedBytes: expect.any(Number),
      heapCapacityBytes: expect.any(Number),
      flashTotalBytes: expect.any(Number),
      firmwareBytes: expect.any(Number),
      resetReason: expect.stringMatching(/^(watchdog|power-on-or-external)$/),
    }),
  }));

  const displayResponse = await request.get("/api/v1/display");
  expect(displayResponse.ok()).toBeTruthy();
  const display = await displayResponse.json();
  expect(display).toEqual(expect.objectContaining({
    page: expect.stringMatching(/^(time|date|temperature|status)$/),
    automaticBrightness: expect.any(Boolean),
    brightnessPercent: expect.any(Number),
    powerSaving: expect.any(Boolean),
    powerSaveTestActive: expect.any(Boolean),
    powerSaveTestDurationSeconds: expect.any(Number),
    powerSaveTestSecondsRemaining: expect.any(Number),
    temperatureCelsius: expect.any(Number),
    lastNtpSync: expect.any(String),
    rtcSkewSeconds: expect.any(Number),
    frame: expect.any(Array),
  }));
  expect(display.frame).toHaveLength(8);

  const timeResponse = await request.get("/api/v1/time");
  expect(timeResponse.ok()).toBeTruthy();
  expect(await timeResponse.json()).toEqual(expect.objectContaining({
    valid: expect.any(Boolean),
    timezone: expect.any(String),
    ntpSynchronized: expect.any(Boolean),
  }));

  const configResponse = await request.get("/api/v1/config");
  expect(configResponse.ok()).toBeTruthy();
  const config = await configResponse.json();
  expect(config).toEqual(expect.objectContaining({
    ssid: expect.any(String),
    hostname: expect.any(String),
    domainName: expect.any(String),
    ntpServer: expect.any(String),
    timezone: expect.any(String),
    automaticBrightness: expect.any(Boolean),
    manualBrightnessPercent: expect.any(Number),
    chimeInterval: expect.stringMatching(/^(never|15min|30min|1hr)$/),
  }));
  expect(status.device).toBe(config.hostname);
  expect(status.domainName).toBe(config.domainName);
  expect(status.fqdn).toBe(config.domainName
    ? `${config.hostname}.${config.domainName}`
    : config.hostname);
});

test("embedded Pico page is self-contained", async ({ page }) => {
  test.skip(!embeddedTarget, "Only applies to the embedded frontend artifact");
  const fetchedAssets: string[] = [];
  page.on("response", (response) => {
    const path = new URL(response.url()).pathname;
    if (path.startsWith("/_next/") || /\.(?:js|css)$/.test(path)) fetchedAssets.push(path);
  });

  const response = await page.goto("/");
  expect(response?.ok()).toBeTruthy();
  await expect(page.getByRole("button", { name: "Status", exact: true })).toBeVisible();
  expect(fetchedAssets).toEqual([]);
});

test("physical Pico serves its API index and documentation", async ({ request }) => {
  test.skip(target !== "pico", "Only the firmware server owns the API documentation route");

  const indexResponse = await request.get("/api/v1");
  expect(indexResponse.ok()).toBeTruthy();
  expect(await indexResponse.json()).toEqual(expect.objectContaining({
    title: "greenpico API",
    version: "v1",
    documentation: "/docs",
  }));

  const docsResponse = await request.get("/docs");
  expect(docsResponse.ok()).toBeTruthy();
  expect(docsResponse.headers()["content-type"]).toContain("text/html");
  expect(await docsResponse.text()).toContain("greenpico API v1");
});

test("physical-device modes never expose development controls", async ({ page }) => {
  test.skip(!physicalTarget || target === "local-live", "The local development UI intentionally exposes its API selector");
  await page.goto("/");
  await expect(page.getByText("Development API", { exact: true })).toHaveCount(0);
});
