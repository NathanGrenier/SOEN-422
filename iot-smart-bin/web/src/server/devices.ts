import { createServerFn } from "@tanstack/react-start";
import { eq } from "drizzle-orm";
import z from "zod";
import { db } from "@/db";
import { devices, readings } from "@/db/schema";
import { authMiddleware } from "@/server/auth";
import { getMqttClient } from "@/server/mqtt-client"; // Import the MQTT client

export const getAllDevices = createServerFn({ method: "GET" }).handler(
  async () => {
    return await db.select().from(devices).all();
  },
);

export const clearDeviceReadings = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .inputValidator(z.object({ id: z.string() }))
  .handler(async ({ data }) => {
    await db.delete(readings).where(eq(readings.deviceId, data.id));
    return { success: true };
  });

export const updateDeviceSettings = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .inputValidator(
    z.object({
      id: z.string(),
      location: z.string().optional(),
      threshold: z.number().optional(),
      deployed: z.boolean().optional(),
    }),
  )
  .handler(async ({ data }) => {
    await db
      .update(devices)
      .set({
        ...(data.location !== undefined && { location: data.location }),
        ...(data.threshold !== undefined && { threshold: data.threshold }),
        ...(data.deployed !== undefined && { deployed: data.deployed }),
      })
      .where(eq(devices.id, data.id));

    if (data.threshold !== undefined) {
      const client = getMqttClient();

      if (client) {
        const topic = `bins/${data.id}/config`;
        const payload = JSON.stringify({ threshold: data.threshold });

        try {
          await new Promise<void>((resolve, reject) => {
            client.publish(topic, payload, { qos: 1, retain: true }, (err) => {
              if (err) reject(err);
              else resolve();
            });
          });
          console.log(`[MQTT] Successfully sent threshold: ${data.threshold}%`);
        } catch (err) {
          console.error(`[MQTT] Publish failed:`, err);
        }
      } else {
        console.warn(`[MQTT] Client not connected. Could not send update.`);
      }
    }

    return { success: true };
  });
