/* eslint-disable @typescript-eslint/no-unnecessary-condition */
import mqtt from "mqtt";
import z from "zod";
import { eq } from "drizzle-orm";
import type { MqttClient } from "mqtt";
import { env } from "@/env";
import { db } from "@/db"; // Import DB
import { devices, readings } from "@/db/schema";

// --- Types ---
const BinDataSchema = z.object({
  deviceId: z.string(),
  fillLevel: z.number(),
  batteryPercentage: z.number(),
  voltage: z.number(),
  isTilted: z.boolean(),
});
export type BinData = z.infer<typeof BinDataSchema>;

export interface Device {
  id: string;
  fillLevel: number;
  threshold: number;
  lastSeen: number;
  status: "online" | "offline";
  isOnline: boolean;
  location: string;
  batteryPercentage: number;
  voltage: number;
  isTilted: boolean;
}

// --- State and Client ---
let client: MqttClient | null = null;
let brokerConnectionStatus:
  | "Connected"
  | "Disconnected"
  | "Connecting"
  | "Reconnecting"
  | "Error" = "Disconnected";

// In-memory cache for the dashboard (fast access)
const deviceStore: Record<
  string,
  {
    fillLevel: number;
    lastSeen: number;
    status: string;
    batteryPercentage: number;
    voltage: number;
    isTilted: boolean;
  }
> = {};

const connect = () => {
  if (client && (client.connected || client.reconnecting)) {
    return client;
  }
  console.log("Attempting to connect to MQTT broker...");
  brokerConnectionStatus = "Connecting";

  const options = {
    username: env.MQTT_USERNAME,
    password: env.MQTT_PASSWORD,
    reconnectPeriod: 1000,
  };

  client = mqtt.connect(env.MQTT_BROKER_URL, options);

  client.on("connect", () => {
    brokerConnectionStatus = "Connected";
    console.log("MQTT Broker Connected");
    client?.subscribe(["bins/+/data", "bins/+/status"], (err) => {
      if (err) console.error("Subscription failed:", err);
      else console.log("Subscribed to bins/+/data and bins/+/status");
    });
  });

  client.on("error", (err) => {
    brokerConnectionStatus = "Error";
    console.error("MQTT Error: " + err.message);
  });

  client.on("reconnect", () => {
    brokerConnectionStatus = "Reconnecting";
    console.log("MQTT client reconnecting...");
  });

  client.on("close", () => {
    brokerConnectionStatus = "Disconnected";
    console.log("MQTT Connection Closed");
  });

  client.on("message", async (topic, payload) => {
    const parts = topic.split("/");
    const binId = parts[1];
    const type = parts[2];
    const msgString = payload.toString();

    if (!binId) return;

    // Ensure device exists in DB (Upsert)
    try {
      await db
        .insert(devices)
        .values({
          id: binId,
          lastSeen: new Date(),
          deployed: true,
        })
        .onConflictDoUpdate({
          target: devices.id,
          set: { lastSeen: new Date() },
        });
    } catch (e) {
      console.error("DB Error creating device:", e);
    }

    if (type === "status") {
      const status = msgString === "online" ? "online" : "offline";

      // Update In-Memory Store
      const existing = deviceStore[binId] || {
        fillLevel: 0,
        batteryPercentage: 100,
        voltage: 5.0,
      };
      deviceStore[binId] = {
        ...existing,
        status,
        lastSeen: Date.now(),
      };

      await db.update(devices).set({ status }).where(eq(devices.id, binId));
    } else if (type === "data") {
      try {
        const json = JSON.parse(msgString);
        const result = BinDataSchema.safeParse(json);

        if (result.success) {
          const { deviceId, fillLevel, batteryPercentage, voltage, isTilted } =
            result.data;

          // Update In-Memory Store
          deviceStore[deviceId] = {
            fillLevel,
            batteryPercentage,
            voltage,
            isTilted,
            lastSeen: Date.now(),
            status: "online",
          };

          // Update DB Device Status
          await db
            .update(devices)
            .set({
              status: "online",
              lastSeen: new Date(),
              batteryPercentage: batteryPercentage,
              voltage: voltage,
              isTilted: isTilted,
            })
            .where(eq(devices.id, deviceId));

          await db.insert(readings).values({
            deviceId,
            fillLevel,
            batteryPercentage,
            voltage,
            isTilted,
            createdAt: new Date(),
          });
        }
      } catch (e) {
        console.error("Failed to parse JSON or DB error:", e);
      }
    }
  });

  return client;
};

export const getMqttClient = () => {
  if (!client) {
    connect();
  }
  return client;
};

export const getBrokerStatus = () => brokerConnectionStatus;

export const getLiveDeviceState = () => deviceStore;

export const getDevices = (): Device[] => {
  const now = Date.now();
  const TIMEOUT_MS = 30000;

  return Object.entries(deviceStore).map(([id, data]) => {
    const isTimedOut = now - data.lastSeen > TIMEOUT_MS;
    const isOnline = data.status === "online" && !isTimedOut;

    return {
      id,
      fillLevel: data.fillLevel,
      threshold: 85,
      lastSeen: data.lastSeen,
      status: data.status as "online" | "offline",
      isOnline,
      location: "Unknown",
      batteryPercentage: data.batteryPercentage,
      voltage: data.voltage || 0,
      isTilted: data.isTilted || false,
    };
  });
};
