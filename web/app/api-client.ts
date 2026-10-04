const developmentTargetKey = "greenpico-development-target";
const localDevelopment = process.env.NODE_ENV === "development";
let apiQueue: Promise<void> = Promise.resolve();

export { developmentTargetKey, localDevelopment };

export function api<T = unknown>(path: string, init?: RequestInit): Promise<T> {
  // The Pico is a small raw-lwIP server. Serialize browser requests so page
  // hydration, polling, and user actions cannot create a connection burst.
  const request = apiQueue.then(async () => {
    const headers = new Headers(init?.headers);
    if (init?.body) headers.set("Content-Type", "application/json");
    if (localDevelopment && typeof window !== "undefined") {
      const target = window.localStorage.getItem(developmentTargetKey);
      if (target) headers.set("X-Greenpico-Target", target);
    }
    const response = await fetch(path, { ...init, cache: "no-store", headers });
    if (!response.ok) {
      const problem = await response.json().catch(() => ({}));
      throw new Error(problem.detail || `${response.status} ${response.statusText}`);
    }
    return response.json() as Promise<T>;
  });
  apiQueue = request.then(() => undefined, () => undefined);
  return request;
}
