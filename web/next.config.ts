import type { NextConfig } from "next";

const development = process.env.NODE_ENV === "development";
const config: NextConfig = development
  ? {
      poweredByHeader: false,
      agentRules: false,
      allowedDevOrigins: ["127.0.0.1"],
      distDir: process.env.PICO_CLOCK_NEXT_DIST_DIR ?? ".next",
      async rewrites() {
        const api = process.env.PICO_CLOCK_API_URL ?? "http://127.0.0.1:3001";
        return [{ source: "/api/:path*", destination: `${api}/api/:path*` }];
      },
    }
  : { output: "export", poweredByHeader: false, agentRules: false };

export default config;
