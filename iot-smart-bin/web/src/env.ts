import { createEnv } from "@t3-oss/env-core";
import { z } from "zod";

export const env = createEnv({
  clientPrefix: "VITE_",
  server: {
    DATABASE_URL: z.string().min(1),
    NODE_ENV: z.enum(["development", "production"]).default("development"),
    MQTT_BROKER_URL: z.string().min(1),
    MQTT_USERNAME: z.string().min(1),
    MQTT_PASSWORD: z.string().min(1),
  },
  client: {
    VITE_API_BASE_URL: z.string().min(1),
  },

  runtimeEnv: {
    ...import.meta.env,
    ...process.env,
  },

  emptyStringAsUndefined: true,
});
