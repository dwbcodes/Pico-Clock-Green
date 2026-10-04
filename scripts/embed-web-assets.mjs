import { mkdir, readFile, stat, writeFile } from "node:fs/promises";
import { join, normalize, resolve } from "node:path";
import { gzipSync } from "node:zlib";

const root = resolve(import.meta.dirname, "..");
const output = join(root, "web", "out");
const destination = join(root, "include", "generated_web_assets.hpp");
const preview = join(root, ".pio", "web", "embedded-index.html");

function exportedPath(url) {
  if (!url.startsWith("/") || url.includes("..")) {
    throw new Error(`Unsafe exported asset path: ${url}`);
  }
  return normalize(join(output, url));
}

async function requiredFile(url) {
  const path = exportedPath(url);
  const details = await stat(path).catch(() => null);
  if (!details?.isFile()) throw new Error(`Next.js export references missing asset: ${url}`);
  return readFile(path, "utf8");
}

function inlineSafeScript(source, url) {
  const currentScriptMarker = '"object"==typeof document?document.currentScript:void 0';
  if (source.includes(currentScriptMarker)) {
    if (!url.startsWith("/_next/")) {
      throw new Error(`Cannot derive Turbopack chunk identity from: ${url}`);
    }
    // Turbopack normally derives this identity from currentScript.src. Inline
    // scripts have no src, so bake in the exported chunk path that the runtime
    // would otherwise derive. This retains zero asset requests without
    // breaking chunk registration and React hydration.
    source = source.replace(
      currentScriptMarker,
      JSON.stringify(url.slice("/_next/".length)),
    );
  }
  if (source.includes("can't infer type of chunk from URL")) {
    const chunkLoaderMarker = "function H(e,t){let r=I(t);";
    if (!source.includes(chunkLoaderMarker)) {
      throw new Error("Cannot locate the Turbopack dynamic chunk loader");
    }
    // Every exported chunk is embedded and registered synchronously. Resolve
    // dynamic-load requests from that registry instead of opening redundant
    // HTTP connections back to the Pico for files it intentionally omits.
    source = source.replace(
      chunkLoaderMarker,
      `${chunkLoaderMarker}if("loading"===document.readyState)document.addEventListener("DOMContentLoaded",()=>r.resolve(),{once:!0});else r.resolve();return r.promise;`,
    );
  }
  return source.replaceAll(/<\/script/gi, "<\\/script");
}

let html = await readFile(join(output, "index.html"), "utf8");
const stylesheetTags = [...html.matchAll(
  /<link\b(?=[^>]*\brel="stylesheet")(?=[^>]*\bhref="([^"]+)")[^>]*\/?\s*>/g,
)];
const scriptTags = [...html.matchAll(
  /<script\b([^>]*?)\bsrc="([^"]+)"([^>]*)><\/script>/g,
)];

if (scriptTags.length === 0) {
  throw new Error("Next.js export contains no compiled client JavaScript");
}

// Load every referenced build artifact before rewriting the document. A single
// replace pass is important: compiled Next.js chunks can themselves contain
// strings that look like script tags, and sequential replacements could match
// one of those strings instead of the original HTML tag.
const stylesheets = new Map(await Promise.all(stylesheetTags.map(async (match) => [
  match[1],
  await requiredFile(match[1]),
])));
const scripts = new Map(await Promise.all(scriptTags.map(async (match) => [
  match[2],
  inlineSafeScript(await requiredFile(match[2]), match[2]),
])));

html = html.replace(
  /<link\b(?=[^>]*\brel="stylesheet")(?=[^>]*\bhref="([^"]+)")[^>]*\/?\s*>/g,
  (_tag, url) => `<style data-greenpico-compiled>${stylesheets.get(url)}</style>`,
);
html = html.replace(
  /<script\b([^>]*?)\bsrc="([^"]+)"([^>]*)><\/script>/g,
  (_tag, before, url, after) => {
    const javascript = scripts.get(url);
    const attributes = `${before} ${after}`
      .replace(/\s+(?:src="[^"]+"|async=""|defer="")/g, "")
      .trim();
    return `<script${attributes ? ` ${attributes}` : ""} data-greenpico-compiled>${javascript}</script>`;
  },
);

// Next's client bootstrap also reads document.currentScript.src to determine
// the asset prefix. An inline script has no src, so provide a detached script
// element with the same shape Next expects. Turbopack chunk identities are
// baked in above, which means this stable synthetic URL is only used to derive
// the empty, root-relative asset prefix.
const currentScriptShim = `<script data-greenpico-runtime-shim>(()=>{const script=document.createElement("script");script.src=new URL("/_next/static/chunks/greenpico-inline.js",document.baseURI).href;Object.defineProperty(document,"currentScript",{configurable:true,get:()=>script})})()</script>`;
html = html.replace("<head>", `<head>${currentScriptShim}`);

// Script preloads are redundant once every compiled chunk is inline. Removing
// them guarantees the browser cannot create a burst of asset connections.
html = html.replace(
  /<link\b(?=[^>]*\brel="preload")(?=[^>]*\bas="script")[^>]*\/?\s*>/g,
  "",
);

// The React server-component payload repeats CSS and JavaScript tags so a
// normal Next server can preload them during hydration. Those assets are
// already registered inline above. Neutralize only the element src/href and
// stylesheet preload hint. Keep the module-to-chunk lists intact: Turbopack
// needs their original paths to resolve the already registered inline chunks.
html = html.replace(
  /<script([^>]*)>([\s\S]*?)<\/script>/g,
  (tag, attributes, source) => {
    if (attributes.includes("data-greenpico")) return tag;
    const embeddedSource = source
      .replaceAll(
        /(\\"src\\":\\")\/_next\/static\/chunks\/[a-z0-9_-]+\.js/gi,
        "$1data:text/javascript,void%200//.js",
      )
      .replaceAll(
        /(\\"href\\":\\")\/_next\/static\/chunks\/[a-z0-9_-]+\.css/gi,
        "$1data:text/css,",
      )
      .replaceAll(
        /(:HL\[\\")\/_next\/static\/chunks\/[a-z0-9_-]+\.css/gi,
        "$1data:text/css,",
      );
    return `<script${attributes}>${embeddedSource}</script>`;
  },
);

const documentShell = html
  .replace(/<script\b[^>]*>[\s\S]*?<\/script>/g, "")
  .replace(/<style\b[^>]*>[\s\S]*?<\/style>/g, "");
if (/<script\b[^>]*\bsrc=/.test(documentShell) ||
    /<link\b(?=[^>]*\brel="stylesheet")[^>]*\bhref=/.test(documentShell)) {
  throw new Error("Firmware page still references external JavaScript or CSS");
}
if (/\.(?:tsx?|jsx)(?:[?"'])/i.test(html)) {
  throw new Error("Uncompiled TypeScript or JSX reference found in firmware page");
}
if (html.includes('"object"==typeof document?document.currentScript:void 0')) {
  throw new Error("An inline Turbopack chunk still depends on currentScript.src");
}
if (!html.includes("data-greenpico-runtime-shim")) {
  throw new Error("The inline Next.js runtime shim is missing");
}

const compressed = gzipSync(Buffer.from(html), { level: 9, mtime: 0 });
const bytes = Array.from(
  compressed,
  (byte) => `0x${byte.toString(16).padStart(2, "0")}`,
);
const lines = [
  "// Generated by scripts/embed-web-assets.mjs. Do not edit.",
  "// Next.js static export with all compiled JavaScript and CSS inlined.",
  "#pragma once",
  "#include <cstddef>",
  "#include <cstdint>",
  "#include <cstring>",
  "namespace pico_clock::web_assets {",
  "struct Asset { const char* path; const char* content_type; const std::uint8_t* data; std::size_t size; };",
  `inline constexpr std::uint8_t compiled_client[] = {${bytes.join(",")}};`,
  "inline constexpr Asset assets[] = {",
  "  {\"/\", \"text/html; charset=utf-8\", compiled_client, sizeof(compiled_client)},",
  "  {\"/index.html\", \"text/html; charset=utf-8\", compiled_client, sizeof(compiled_client)},",
  "};",
  "inline const Asset* find(const char* path) {",
  "  for (const auto& asset : assets) if (std::strcmp(asset.path, path) == 0) return &asset;",
  "  return nullptr;",
  "}",
  "}  // namespace pico_clock::web_assets",
  "",
];

await writeFile(destination, lines.join("\n"));
await mkdir(resolve(preview, ".."), { recursive: true });
await writeFile(preview, html);
console.log(
  `Embedded one precompiled client page (${html.length} bytes raw, ${compressed.length} bytes gzip) in ${destination.slice(root.length + 1)}`,
);
