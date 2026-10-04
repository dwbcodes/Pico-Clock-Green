import { spawn } from "node:child_process";
import { mkdir } from "node:fs/promises";
import { resolve } from "node:path";
import { chromium } from "@playwright/test";

const root = resolve(import.meta.dirname, "../..");
const output = resolve(root, "docs/images");
// Use a process-specific pair so documentation generation can run alongside a
// developer server or another test process without stealing its ports.
const port = 34_000 + (process.pid % 10_000) * 2;
const mockPort = port + 1;
const preview = spawn(process.execPath, ["scripts/preview-embedded-web.mjs"], {
  cwd: root,
  env: {
    ...process.env,
    PICO_CLOCK_EMBEDDED_PORT: String(port),
    PICO_CLOCK_MOCK_PORT: String(mockPort),
  },
  stdio: "inherit",
});

async function waitForPreview() {
  const deadline = Date.now() + 30_000;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(`http://127.0.0.1:${port}/`);
      if (response.ok) return;
    } catch {
      // The preview and simulator start asynchronously.
    }
    await new Promise((resolveRetry) => setTimeout(resolveRetry, 200));
  }
  throw new Error("Embedded preview did not start within 30 seconds");
}

let browser;
try {
  await waitForPreview();
  await mkdir(output, { recursive: true });
  browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1280, height: 1000 } });
  await page.goto(`http://127.0.0.1:${port}/`);
  await page.getByRole("button", { name: "Clock", exact: true }).waitFor();
  await page.addStyleTag({ content: "*,*::before,*::after{transition:none!important;animation:none!important}" });

  const capture = async (name) => {
    await page.locator("main").screenshot({ path: resolve(output, name) });
  };

  await capture("web-clock.png");
  await page.getByRole("button", { name: "Status", exact: true }).click();
  await page.getByRole("heading", { name: "Status", exact: true }).waitFor();
  await capture("web-status.png");
  await page.getByRole("button", { name: "Configuration", exact: true }).click();
  await page.getByRole("heading", { name: "Configuration", exact: true }).waitFor();
  await capture("web-configuration.png");
  await page.getByRole("button", { name: "Wi-Fi", exact: true }).click();
  await page.getByRole("button", { name: "Scan networks" }).click();
  await page.getByText("Workshop", { exact: true }).waitFor();
  await capture("web-wifi.png");
  process.stdout.write(`Captured README screenshots in ${output}\n`);
} finally {
  if (browser) await browser.close();
  preview.kill("SIGTERM");
}
