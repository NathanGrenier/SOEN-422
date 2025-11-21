import { createAuthClient } from "better-auth/react";

const baseURL =
  import.meta.env.VITE_API_BASE_URL || process.env.VITE_API_BASE_URL || "/api";

export const authClient = createAuthClient({
  baseURL: `${baseURL}/auth`,
});
