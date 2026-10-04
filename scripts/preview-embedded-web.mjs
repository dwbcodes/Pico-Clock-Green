import { spawn } from "node:child_process";
import { readFile } from "node:fs/promises";
import http from "node:http";
import { resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const port = Number(process.env.PICO_CLOCK_EMBEDDED_PORT || 3300);
const mockPort = Number(process.env.PICO_CLOCK_MOCK_PORT || 3301);
const html = await readFile(resolve(root, ".pio/web/embedded-index.html"));
const mock = spawn(process.execPath, ["scripts/dev-api.mjs"], {
  cwd: root,
  env: { ...process.env, PICO_CLOCK_MOCK_PORT: String(mockPort), PICO_CLOCK_DEFAULT_TARGET: "" },
  stdio: "inherit",
});

async function requestBody(request) {
  const chunks = [];
  for await (const chunk of request) chunks.push(chunk);
  return chunks.length ? Buffer.concat(chunks) : undefined;
}

const server = http.createServer(async (request, response) => {
  try {
    const url = new URL(request.url, `http://${request.headers.host}`);
    if (request.method === "GET" && (url.pathname === "/" || url.pathname === "/index.html")) {
      response.writeHead(200, {
        "Content-Type": "text/html; charset=utf-8",
        "Content-Length": html.length,
        "Cache-Control": "no-store",
      });
      response.end(html);
      return;
    }
    if (url.pathname.startsWith("/api/")) {
      const body = await requestBody(request);
      const upstream = await fetch(`http://127.0.0.1:${mockPort}${url.pathname}${url.search}`, {
        method: request.method,
        headers: body ? { "Content-Type": request.headers["content-type"] || "application/json" } : undefined,
        body,
      });
      const result = Buffer.from(await upstream.arrayBuffer());
      response.writeHead(upstream.status, {
        "Content-Type": upstream.headers.get("content-type") || "application/json",
        "Content-Length": result.length,
        "Cache-Control": "no-store",
      });
      response.end(result);
      return;
    }
    response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
    response.end("Not found\n");
  } catch (error) {
    response.writeHead(502, { "Content-Type": "text/plain; charset=utf-8" });
    response.end(`${error instanceof Error ? error.message : "Preview request failed"}\n`);
  }
});

function stop() {
  server.close();
  if (!mock.killed) mock.kill("SIGTERM");
}

process.on("SIGINT", stop);
process.on("SIGTERM", stop);
mock.on("exit", (code) => {
  if (code && code !== 0) process.exitCode = code;
});
server.listen(port, "127.0.0.1", () => {
  process.stdout.write(`Embedded greenpico preview listening on http://127.0.0.1:${port}\n`);
});
