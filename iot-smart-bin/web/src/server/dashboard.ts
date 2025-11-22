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
import { devices, readings, systemSettings } from "@/db/schema";

export const getDashboardData = createServerFn({ method: "GET" }).handler(
  async () => {
    getMqttClient();

    const settingsRows = await db.select().from(systemSettings);
    const getSetting = (key: string, def: number) =>
      settingsRows.find((s) => s.key === key)?.value ?? def;

    const STANDARD_TIMEOUT = getSetting("TIMEOUT_STANDARD_MS", 900000);
    const TILTED_TIMEOUT = getSetting("TIMEOUT_TILTED_MS", 300000);

    const dbDevices = await db
      .select()
      .from(devices)
      .where(eq(devices.deployed, true));

    const liveState = getLiveDeviceState();

    const mergedDevices = await Promise.all(
      dbDevices.map(async (dbDev) => {
        const live = liveState[dbDev.id];
        const now = Date.now();

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

        const TIMEOUT_MS = isTilted ? TILTED_TIMEOUT : STANDARD_TIMEOUT;

        const lastSeenTime = live?.lastSeen ?? dbDev.lastSeen?.getTime() ?? 0;
        const isTimedOut = now - lastSeenTime > TIMEOUT_MS;

        const isOnline =
          (live?.status === "online" || dbDev.status === "online") &&
          !isTimedOut;

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
