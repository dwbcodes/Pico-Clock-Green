import { spawn } from "node:child_process";
import { readFileSync, unlinkSync } from "node:fs";
import { resolve } from "node:path";

const children = [];
const environment = { ...process.env };
const webPort = environment.PICO_CLOCK_DEV_PORT || "3000";
const mockPort = environment.PICO_CLOCK_MOCK_PORT || "3001";
if (environment.PICO_CLOCK_API_URL) {
  environment.PICO_CLOCK_DEFAULT_TARGET = environment.PICO_CLOCK_API_URL;
}
environment.PICO_CLOCK_API_URL = `http://127.0.0.1:${mockPort}`;

// Next.js leaves this advisory lock behind when a development process is
// interrupted abruptly. Remove it only when its recorded process no longer
// exists; a live process must retain ownership and let Next report the clash.
const nextDistDirectory = environment.PICO_CLOCK_NEXT_DIST_DIR || ".next";
const nextLock = resolve(process.cwd(), "web", nextDistDirectory, "dev", "lock");
try {
  const { pid } = JSON.parse(readFileSync(nextLock, "utf8"));
  try {
    process.kill(Number(pid), 0);
  } catch (error) {
    if (error?.code !== "ESRCH") throw error;
    unlinkSync(nextLock);
    process.stdout.write(`Removed stale Next.js development lock: ${nextLock}\n`);
  }
} catch (error) {
  if (error?.code !== "ENOENT") throw error;
}

children.push(spawn(process.execPath, ["scripts/dev-api.mjs"], {
  cwd: process.cwd(), env: environment, stdio: "inherit",
}));

const web = spawn("npm", ["--prefix", "web", "run", "dev", "--", "-p", webPort], {
  cwd: process.cwd(), env: environment, stdio: "inherit",
});
children.push(web);

function stop(signal = "SIGTERM") {
  for (const child of children) if (!child.killed) child.kill(signal);
}

process.on("SIGINT", () => stop("SIGINT"));
process.on("SIGTERM", () => stop("SIGTERM"));
web.on("exit", (code, signal) => {
  stop();
  process.exitCode = code ?? (signal ? 1 : 0);
});
