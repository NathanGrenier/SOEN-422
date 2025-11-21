/* eslint-disable @typescript-eslint/no-unnecessary-condition */
/* eslint-disable @typescript-eslint/require-await */
import { createServerFn } from "@tanstack/react-start";
import z from "zod"; // Ensure zod is imported
import { eq } from "drizzle-orm";
import {
  getBrokerStatus,
  getLiveDeviceState,
  getMqttClient,
} from "./mqtt-client";
import { db } from "@/db";
import { devices } from "@/db/schema";

// --- Dashboard Data ---
export const getDashboardData = createServerFn({ method: "GET" }).handler(
  async () => {
    getMqttClient();

    const dbDevices = await db
      .select()
      .from(devices)
      .where(eq(devices.deployed, true));

    const liveState = getLiveDeviceState();

    const mergedDevices = dbDevices.map((dbDev) => {
      const live = liveState[dbDev.id];
      const now = Date.now();
      const TIMEOUT_MS = 30000;

      // Determine if online based on recent live activity
      // If we have live data, use its timestamp, otherwise use DB timestamp
      const lastSeenTime = live?.lastSeen ?? dbDev.lastSeen?.getTime() ?? 0;
      const isTimedOut = now - lastSeenTime > TIMEOUT_MS;

      // Valid if MQTT says online AND it hasn't timed out
      const isOnline =
        (live?.status === "online" || dbDev.status === "online") && !isTimedOut;

      return {
        id: dbDev.id,
        name: dbDev.name ?? dbDev.id,
        location: dbDev.location ?? "Unknown",
        threshold: dbDev.threshold,
        fillLevel: live?.fillLevel ?? 0, // Default to 0 if no live data
        batteryPercentage: live?.batteryPercentage ?? 0,
        lastSeen: lastSeenTime,
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-type-assertion
        status: (isOnline ? "online" : "offline") as "online" | "offline",
        isOnline: isOnline,
        deployed: dbDev.deployed,
      };
    });

    return {
      brokerStatus: getBrokerStatus(),
      devices: mergedDevices,
    };
  },
);

// --- Device Management ---
export const updateDeviceSettings = createServerFn({ method: "POST" })
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

    return { success: true };
  });

export const pingDevice = createServerFn({ method: "POST" })
  .inputValidator(z.object({ id: z.string() }))
  .handler(async ({ data }) => {
    const client = getMqttClient();

    if (client && client.connected) {
      console.log(`Pinging device: cmd/ping/${data.id}`);
      client.publish(`cmd/ping/${data.id}`, "PING");
      return { success: true };
    }

    throw new Error("MQTT Broker not connected");
  });
