import { eq } from "drizzle-orm";
import { db } from "@/db";
import * as schema from "@/db/schema";
import { auth } from "@/lib/auth";
import { env } from "@/env";

export async function seedProduction() {
  console.log("🌱 Seeding production database...");

  // --- Seed Devices ---
  const dummyDevices = [
    {
      id: "LIBRARY_QUAD_02",
      name: "Library Main Entrance",
      location: "Library",
      threshold: 90,
      deployed: true,
      status: "online",
      lastSeen: new Date(),
    },
    {
      id: "CAFETERIA_ZONE_A",
      name: "Cafeteria North",
      location: "Cafeteria",
      threshold: 80,
      deployed: true,
      status: "online",
      lastSeen: new Date(),
    },
    {
      id: "GYM_LOBBY_01",
      name: "Gymnasium Lobby",
      location: "Gym",
      threshold: 85,
      deployed: false,
      status: "offline",
      lastSeen: new Date(),
    },
  ];

  for (const device of dummyDevices) {
    await db.insert(schema.devices).values(device).onConflictDoUpdate({
      target: schema.devices.id,
      set: device,
    });

    // --- Generate Readings for Production Demo Purposes ---
    if (device.deployed) {
      const readings: (typeof schema.readings.$inferInsert)[] = [];
      const now = new Date().getTime();
      const oneDayAgo = now - 24 * 60 * 60 * 1000;

      let currentFill = Math.floor(Math.random() * 30);
      let currentBattery = 100.0;

      for (let t = oneDayAgo; t <= now; t += 30 * 60 * 1000) {
        // Fill logic
        const change = Math.random() * 12;
        currentFill += change;
        if (currentFill > 95) currentFill = 2;

        const drainFactor = change > 5 ? 0.4 : 0.1;
        const randomDrain = Math.random() * 0.2;
        currentBattery -= drainFactor + randomDrain;

        if (currentBattery < 0) currentBattery = 0;

        readings.push({
          deviceId: device.id,
          fillLevel: parseFloat(currentFill.toFixed(1)),
          batteryPercentage: parseFloat(currentBattery.toFixed(1)),
          voltage: 4.0,
          isTilted: false,
          createdAt: new Date(t),
        });
      }

      if (readings.length > 0) {
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
    console.log("Admin User created");
  } else {
    console.log("Admin user already exists.");
  }

  console.log("✅ Seeding complete!");
}
