/* eslint-disable @typescript-eslint/require-await */
import { createServerFn } from "@tanstack/react-start"
import { z } from "zod"
import { addMqttMessage, getMqttClient, getMqttState } from "./mqtt-client"

// --- Server Functions ---
export const getMqttStatus = createServerFn().handler(async () => {
  getMqttClient()
  return getMqttState()
})

const publishInputSchema = z.object({
  topic: z.string().min(1),
  message: z.string(),
})

export const publishMqttMessage = createServerFn({ method: "POST" })
  .inputValidator(publishInputSchema)
  .handler(async ({ data }) => {
    const client = getMqttClient()

    if (client && client.connected) {
      client.publish(data.topic, data.message, (err) => {
        if (err) {
          console.error("MQTT publish error:", err)
          addMqttMessage(`Publish error: ${err.message}`)
        } else {
          addMqttMessage(`Published to ${data.topic}: "${data.message}"`)
        }
      })
      return { success: true }
    } else {
      addMqttMessage("Publish failed: Client not connected")
      return { success: false, error: "Client not connected" }
    }
  })
