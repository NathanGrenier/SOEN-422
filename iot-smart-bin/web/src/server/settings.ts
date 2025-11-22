import { createServerFn } from "@tanstack/react-start";
import z from "zod";
import { db } from "@/db";
import { systemSettings } from "@/db/schema";
import { authMiddleware } from "@/server/auth";

export const getSystemSettings = createServerFn({ method: "GET" }).handler(
  async () => {
    const settings = await db.select().from(systemSettings);

    // Helper to find value or return default
    const getValue = (key: string, defaultValue: number) =>
      settings.find((s) => s.key === key)?.value ?? defaultValue;

    return {
      standardTimeout: getValue("TIMEOUT_STANDARD_MS", 900000), // Default 15m
      tiltedTimeout: getValue("TIMEOUT_TILTED_MS", 300000), // Default 5m
    };
  },
);

export const updateSystemSettings = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .inputValidator(
    z.object({
      standardTimeout: z.number().min(1000), // Min 1 second
      tiltedTimeout: z.number().min(1000),
    }),
  )
  .handler(async ({ data }) => {
    await db
      .insert(systemSettings)
      .values({
        key: "TIMEOUT_STANDARD_MS",
        value: data.standardTimeout,
        description: "Connection timeout for upright devices",
      })
      .onConflictDoUpdate({
        target: systemSettings.key,
        set: { value: data.standardTimeout },
      });

    await db
      .insert(systemSettings)
      .values({
        key: "TIMEOUT_TILTED_MS",
        value: data.tiltedTimeout,
        description: "Connection timeout for tilted devices",
      })
      .onConflictDoUpdate({
        target: systemSettings.key,
        set: { value: data.tiltedTimeout },
      });

    return { success: true };
  });
