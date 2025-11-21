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
  }

  // --- Seed Admin User ---
  const adminEmail = env.ADMIN_EMAIL;
  const adminPassword = env.ADMIN_PASSWORD;

  const existingUser = await db.query.user.findFirst({
    where: (u, { eq }) => eq(u.email, adminEmail),
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
