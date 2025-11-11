import mqtt from "mqtt"
import type { MqttClient } from "mqtt"
import { env } from "@/env"

let client: MqttClient | null = null
let connectionStatus = "Disconnected"
const messages: string[] = []

const connect = () => {
  // If client exists and is connected/reconnecting, do nothing.
  if (client && (client.connected || client.reconnecting)) {
    return client
  }

  console.log("Attempting to connect to MQTT broker...")
  const brokerUrl = env.MQTT_BROKER_URL
  const options = {
    username: env.MQTT_USERNAME,
    password: env.MQTT_PASSWORD,
    reconnectPeriod: 1000, // Try to reconnect every 1 second
  }

  client = mqtt.connect(brokerUrl, options)

  client.on("connect", () => {
    connectionStatus = "Connected"
    console.log("MQTT client connected")
    client?.subscribe("test/topic", (err) => {
      if (err) {
        console.error("MQTT subscription error:", err)
        messages.push(`Subscription failed: ${err.message}`)
      } else {
        messages.push("Subscribed to test/topic")
      }
    })
  })

  client.on("error", (err) => {
    connectionStatus = `Error: ${err.message}`
    console.error("MQTT connection error:", err)
  })

  client.on("reconnect", () => {
    connectionStatus = "Reconnecting..."
    console.log("MQTT client reconnecting...")
  })

  client.on("close", () => {
    connectionStatus = "Disconnected"
    console.log("MQTT client disconnected")
  })

  client.on("message", (topic, payload) => {
    const message = `[${topic}]: ${payload.toString()}`
    console.log(`MQTT message received: ${message}`)
    messages.push(message)
  })

  return client
}

/**
 * Lazily initializes and returns the MQTT client instance.
 * Ensures the connection logic only runs on the server when needed.
 */
export const getMqttClient = () => {
  if (!client) {
    connect()
  }
  return client
}

/**
 * Returns the current state of the MQTT connection.
 */
export const getMqttState = () => ({
  status: connectionStatus,
  messages: [...messages].reverse(),
})

/**
 * Adds a message to the server-side message log.
 */
export const addMqttMessage = (message: string) => {
  messages.push(message)
}
