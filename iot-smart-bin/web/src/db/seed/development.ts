import { eq } from "drizzle-orm";
import { db } from "@/db";
import * as schema from "@/db/schema";
import { auth } from "@/lib/auth";
import { env } from "@/env";

export async function seedDevelopment() {
  // --- Seed Devices ---
  const dummyDevices = [
    {
      id: "LIBRARY_QUAD_02",
      name: "Library Main Entrance",
      location: "Library",
      threshold: 90,
      deployed: true,
      status: "offline",
      lastSeen: new Date(),
    },
    {
      id: "CAFETERIA_ZONE_A",
      name: "Cafeteria North",
      location: "Cafeteria",
      threshold: 80,
      deployed: true,
      status: "offline",
      lastSeen: new Date(),
    },
    {
      id: "GYM_LOBBY_01",
      name: "Gymnasium Lobby",
      location: "Gym",
      threshold: 85,
      deployed: false, // Not deployed yet
      status: "offline",
      lastSeen: new Date(),
    },
  ];

  for (const device of dummyDevices) {
    await db.insert(schema.devices).values(device).onConflictDoUpdate({
      target: schema.devices.id,
      set: device,
    });

    // --- Generate Realistic Dummy Readings (Past 24 Hours) ---
    if (device.deployed) {
      const readings: (typeof schema.readings.$inferInsert)[] = [];
      const now = new Date().getTime();
      const oneDayAgo = now - 24 * 60 * 60 * 1000;

      // Simulation state
      let currentFill = Math.floor(Math.random() * 30); // Start relatively empty
      let currentBattery = 98.0 + Math.random() * 2; // Start near 100%

      // Generate a reading every 30 minutes
      for (let t = oneDayAgo; t <= now; t += 30 * 60 * 1000) {
        // 1. Fill Level Logic
        // Add random amount (0-10%)
        const fillChange = Math.random() * 10;
        currentFill += fillChange;

        // If it gets too full, simulate "emptying" the bin
        if (currentFill > 98) {
          currentFill = Math.random() * 5; // Reset to near 0
        }

        // Base drain per interval
        let drain = 0.1 + Math.random() * 0.2;

        // If fill level changed significantly (sensor worked hard), drain more
        if (fillChange > 8) {
          drain += 0.3;
        }

        currentBattery -= drain;
        if (currentBattery < 0) currentBattery = 0;

        // 4.2V is max, 3.3V is cutoff.
        // Add some noise to voltage to simulate temperature fluctuation
        const voltageNoise = (Math.random() - 0.5) * 0.05;
        const voltage = 3.3 + (currentBattery / 100) * 0.9 + voltageNoise;

        readings.push({
          deviceId: device.id,
          fillLevel: parseFloat(currentFill.toFixed(1)),
          batteryPercentage: parseFloat(currentBattery.toFixed(1)),
          voltage: parseFloat(voltage.toFixed(2)),
          isTilted: false,
          createdAt: new Date(t),
        });
      }

      // Batch insert readings
      if (readings.length > 0) {
        // Clear existing readings for this device to avoid duplicates on re-seed
        await db
          .delete(schema.readings)
          .where(eq(schema.readings.deviceId, device.id));
        await db.insert(schema.readings).values(readings);
      }
    }
  }

  // --- Seed Admin User ---
  const adminEmail = env.ADMIN_EMAIL;
  const adminPassword = env.ADMIN_PASSWORD;

  const existingUser = await db.query.user.findFirst({
    where: eq(schema.user.email, adminEmail),
  });

  if (!existingUser) {
    console.log("Creating admin user...");
    await auth.api.signUpEmail({
      body: {
        email: adminEmail,
        password: adminPassword,
        name: "Admin User",
      },
    });
    console.log(`User created: ${adminEmail} / ${adminPassword}`);
  } else {
    console.log("Admin user already exists.");
  }

  console.log("✅ Seeding complete!");
}
