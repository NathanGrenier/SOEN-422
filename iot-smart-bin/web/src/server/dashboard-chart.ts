import { createServerFn } from "@tanstack/react-start";
import { desc, eq } from "drizzle-orm";
import z from "zod";
import { authMiddleware } from "./auth";
import { devices, readings } from "@/db/schema";
import { db } from "@/db";

export const getDeviceReadings = createServerFn({ method: "GET" })
  .middleware([authMiddleware])
  .inputValidator(z.object({ id: z.string() }))
  .handler(async ({ data }) => {
    const history = await db
      .select()
      .from(readings)
      .where(eq(readings.deviceId, data.id))
      .orderBy(desc(readings.createdAt))
      .limit(50);

    const deviceSettings = await db
      .select({ threshold: devices.threshold })
      .from(devices)
      .where(eq(devices.id, data.id))
      .limit(1);

    return {
      history: history.reverse(),
      threshold: deviceSettings.length > 0 ? deviceSettings[0].threshold : 85,
    };
  });
