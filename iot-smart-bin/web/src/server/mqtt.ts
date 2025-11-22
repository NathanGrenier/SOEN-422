/* eslint-disable @typescript-eslint/no-unnecessary-condition */
/* eslint-disable @typescript-eslint/require-await */
import { createServerFn } from "@tanstack/react-start";
import z from "zod";
import { desc, eq } from "drizzle-orm";
import {
  getBrokerStatus,
  getLiveDeviceState,
  getMqttClient,
} from "./mqtt-client";
import { db } from "@/db";
import { devices, readings } from "@/db/schema";

// --- Dashboard Data ---
export const getDashboardData = createServerFn({ method: "GET" }).handler(
  async () => {
    getMqttClient();

    const dbDevices = await db
      .select()
      .from(devices)
      .where(eq(devices.deployed, true));

    const liveState = getLiveDeviceState();

    const mergedDevices = await Promise.all(
      dbDevices.map(async (dbDev) => {
        const live = liveState[dbDev.id];
        const now = Date.now();
        const TIMEOUT_MS = 30000;

        // Determine if online based on recent live activity
        // If we have live data, use its timestamp, otherwise use DB timestamp
        const lastSeenTime = live?.lastSeen ?? dbDev.lastSeen?.getTime() ?? 0;
        const isTimedOut = now - lastSeenTime > TIMEOUT_MS;

        // Valid if MQTT says online AND it hasn't timed out
        const isOnline =
          (live?.status === "online" || dbDev.status === "online") &&
          !isTimedOut;

        let fillLevel = live?.fillLevel;
        let batteryPercentage = live?.batteryPercentage;
        let voltage = live?.voltage;
        let isTilted = live?.isTilted;

        // If live data is missing (e.g. server restart), try to fetch latest from DB
        if (fillLevel === undefined) {
          try {
            const lastReading = await db
              .select()
              .from(readings)
              .where(eq(readings.deviceId, dbDev.id))
              .orderBy(desc(readings.createdAt))
              .limit(1);

            if (lastReading.length > 0) {
              fillLevel = lastReading[0].fillLevel;
              batteryPercentage =
                batteryPercentage ?? lastReading[0].batteryPercentage;
              voltage = voltage ?? lastReading[0].voltage;
              isTilted = isTilted ?? lastReading[0].isTilted;
            }
          } catch (error) {
            console.error(
              `Failed to fetch last reading for ${dbDev.id}:`,
              error,
            );
          }
        }

        if (batteryPercentage === undefined)
          batteryPercentage = dbDev.batteryPercentage ?? 100;
        if (voltage === undefined) voltage = dbDev.voltage ?? 5.0;
        if (isTilted === undefined) isTilted = dbDev.isTilted ?? false;

        return {
          id: dbDev.id,
          name: dbDev.name ?? dbDev.id,
          location: dbDev.location ?? "Unknown",
          threshold: dbDev.threshold,
          fillLevel: fillLevel ?? 0,
          batteryPercentage: batteryPercentage,
          voltage: voltage,
          isTilted: isTilted,
          lastSeen: lastSeenTime,
          // eslint-disable-next-line @typescript-eslint/no-unnecessary-type-assertion
          status: (isOnline ? "online" : "offline") as "online" | "offline",
          isOnline: isOnline,
          deployed: dbDev.deployed,
        };
      }),
    );

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
