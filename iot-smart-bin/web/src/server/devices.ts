import { createServerFn } from "@tanstack/react-start";
import { eq } from "drizzle-orm";
import z from "zod";
import { db } from "@/db";
import { devices, readings } from "@/db/schema";
import { authMiddleware } from "@/server/auth";

export const getAllDevices = createServerFn({ method: "GET" }).handler(
  async () => {
    return await db.select().from(devices).all();
  },
);

export const toggleDeviceDeployed = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .inputValidator(
    z.object({
      id: z.string(),
      deployed: z.boolean(),
    }),
  )
  .handler(async ({ data }) => {
    await db
      .update(devices)
      .set({ deployed: data.deployed })
      .where(eq(devices.id, data.id));
    return { success: true };
  });

export const clearDeviceReadings = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .inputValidator(z.object({ id: z.string() }))
  .handler(async ({ data }) => {
    await db.delete(readings).where(eq(readings.deviceId, data.id));
    return { success: true };
  });
