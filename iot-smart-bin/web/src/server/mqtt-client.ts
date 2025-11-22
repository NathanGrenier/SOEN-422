/* eslint-disable @typescript-eslint/no-unnecessary-condition */
import mqtt from "mqtt";
import z from "zod";
import { eq } from "drizzle-orm";
import type { MqttClient } from "mqtt";
import { env } from "@/env";
import { db } from "@/db"; // Import DB
import { devices, readings } from "@/db/schema";

const SUBSCRIPTION_TOPICS = [
  "bins/+/data",
  "bins/+/status",
  "bins/+/get-config",
];

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
const globalMqtt = globalThis as unknown as { mqttClient?: MqttClient };
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
  if (
    globalMqtt.mqttClient &&
    (globalMqtt.mqttClient.connected || globalMqtt.mqttClient.reconnecting)
  ) {
    return globalMqtt.mqttClient;
  }
  console.log("Attempting to connect to MQTT broker...");
  brokerConnectionStatus = "Connecting";

  const options = {
    username: env.MQTT_USERNAME,
    password: env.MQTT_PASSWORD,
    reconnectPeriod: 1000,
  };

  const client = mqtt.connect(env.MQTT_BROKER_URL, options);

  client.on("connect", () => {
    brokerConnectionStatus = "Connected";
    console.log("MQTT Broker Connected");
    client?.subscribe(SUBSCRIPTION_TOPICS, (err) => {
      if (err) console.error("Subscription failed:", err);
      else console.log("Subscribed to topics:", SUBSCRIPTION_TOPICS);
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

    switch (type) {
      case "status": {
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
        break;
      }

      case "data": {
        try {
          const json = JSON.parse(msgString);
          const result = BinDataSchema.safeParse(json);

          if (result.success) {
            const {
              deviceId,
              fillLevel,
              batteryPercentage,
              voltage,
              isTilted,
            } = result.data;

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
        break;
      }

      case "get-config": {
        console.log(`[Config] Request received for device: ${binId}`);
        try {
          const deviceRecord = await db
            .select({ threshold: devices.threshold })
            .from(devices)
            .where(eq(devices.id, binId))
            .limit(1);

          const threshold =
            deviceRecord.length > 0 ? deviceRecord[0].threshold : 85;

          const responsePayload = JSON.stringify({ threshold });
          const responseTopic = `bins/${binId}/config`;

          if (client) {
            client.publish(responseTopic, responsePayload, { retain: false });
            console.log(`[Config] Sent to ${binId}: ${responsePayload}`);
          }
        } catch (e) {
          console.error(`[Config] Error handling request for ${binId}:`, e);
        }
        break;
      }

      default: {
        console.warn(`Received message on unknown topic type: ${type}`);
        break;
      }
    }
  });

  globalMqtt.mqttClient = client;
  return client;
};

export const getMqttClient = () => {
  if (!globalMqtt.mqttClient) {
    return connect();
  }
  return globalMqtt.mqttClient;
};

export const getBrokerStatus = () => brokerConnectionStatus;

export const getLiveDeviceState = () => deviceStore;
