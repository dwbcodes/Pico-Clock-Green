import { readFile, rename, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const environmentPath = resolve(
  root, process.env.PICO_CLOCK_ENV_PATH || "web/.env",
);
const timeoutMs = Number(process.env.PICO_CLOCK_SYNC_TIMEOUT_MS || 60_000);
const retryMs = 1_000;

function parseEnvironment(contents) {
  const values = {};
  for (const line of contents.split(/\r?\n/)) {
    const match = line.match(/^([A-Z][A-Z0-9_]*)=(.*)$/);
    if (!match) continue;
    values[match[1]] = match[2].replace(/^(?:"(.*)"|'(.*)')$/, "$1$2");
  }
  return values;
}

function origin(value) {
  if (!value) return "";
  const candidate = /^https?:\/\//i.test(value) ? value : `http://${value}`;
  const url = new URL(candidate);
  if (url.protocol !== "http:" || url.username || url.password ||
      (url.pathname !== "/" && url.pathname !== "") || url.search || url.hash) {
    throw new Error(`Invalid clock address: ${value}`);
  }
  return url.origin;
}

function validIdentity(status) {
  if (!status || typeof status.device !== "string" ||
      !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/.test(status.device) ||
      typeof status.domainName !== "string" ||
      !/^[A-Za-z0-9.-]*$/.test(status.domainName) ||
      typeof status.fqdn !== "string") return false;
  const expected = status.domainName
    ? `${status.device}.${status.domainName}` : status.device;
  return status.fqdn === expected;
}

async function readExistingEnvironment() {
  try {
    const contents = await readFile(environmentPath, "utf8");
    return { contents, values: parseEnvironment(contents) };
  } catch (error) {
    if (error?.code === "ENOENT") return { contents: "", values: {} };
    throw error;
  }
}

async function readIdentity(candidate) {
  const request = (path) => fetch(`${candidate}${path}`, {
    signal: AbortSignal.timeout(2_500), headers: { Accept: "application/json" },
  });
  const [configResponse, statusResponse] = await Promise.all([
    request("/api/v1/config"), request("/api/v1/status"),
  ]);
  if (!configResponse.ok) {
    throw new Error(`/config: ${configResponse.status} ${configResponse.statusText}`);
  }
  if (!statusResponse.ok) {
    throw new Error(`/status: ${statusResponse.status} ${statusResponse.statusText}`);
  }
  const config = await configResponse.json();
  const status = await statusResponse.json();
  const identity = {
    device: config.hostname,
    domainName: config.domainName,
    fqdn: config.domainName ? `${config.hostname}.${config.domainName}` : config.hostname,
  };
  if (!validIdentity(identity) || !validIdentity(status) ||
      identity.device !== status.device ||
      identity.domainName !== status.domainName ||
      identity.fqdn !== status.fqdn) {
    throw new Error("configuration and status network identities do not match");
  }
  return identity;
}

const existing = await readExistingEnvironment();
const savedIdentity = existing.values.PICO_HOSTNAME
  ? `${existing.values.PICO_HOSTNAME}${existing.values.PICO_DOMAIN_NAME ? `.${existing.values.PICO_DOMAIN_NAME}` : ""}`
  : "";
const candidates = [...new Set([
  process.env.PICO_CLOCK_API_URL,
  existing.values.PICO_CLOCK_API_URL,
  savedIdentity,
  "greenpico.internal",
  "greenpico",
].filter(Boolean).map(origin))];

const deadline = Date.now() + timeoutMs;
let identity;
let lastError;
while (!identity && Date.now() < deadline) {
  for (const candidate of candidates) {
    try {
      identity = await readIdentity(candidate);
      break;
    } catch (error) {
      lastError = `${candidate}: ${error instanceof Error ? error.message : error}`;
    }
  }
  if (!identity && Date.now() < deadline) {
    await new Promise((resolveRetry) => setTimeout(resolveRetry, retryMs));
  }
}

if (!identity) {
  throw new Error(
    `Clock did not become reachable within ${Math.round(timeoutMs / 1000)} seconds. ` +
    `Set PICO_CLOCK_API_URL to its current hostname or IP address. Last error: ${lastError}`,
  );
}

const managedNames = new Set([
  "PICO_HOSTNAME", "PICO_DOMAIN_NAME", "PICO_CLOCK_API_URL",
]);
const retained = existing.contents.split(/\r?\n/).filter((line) => {
  const match = line.match(/^([A-Z][A-Z0-9_]*)=/);
  return !match || !managedNames.has(match[1]);
});
while (retained.length && retained[retained.length - 1] === "") retained.pop();
const output = [
  ...retained,
  ...(retained.length ? [""] : []),
  "# Generated from the Pico's persisted configuration after make flash.",
  `PICO_HOSTNAME=${identity.device}`,
  `PICO_DOMAIN_NAME=${identity.domainName}`,
  `PICO_CLOCK_API_URL=http://${identity.fqdn}`,
  "",
].join("\n");
const temporaryPath = `${environmentPath}.tmp`;
await writeFile(temporaryPath, output, "utf8");
await rename(temporaryPath, environmentPath);
process.stdout.write(`Updated web/.env for ${identity.fqdn}\n`);
