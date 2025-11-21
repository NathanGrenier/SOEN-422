import { config } from "dotenv";
import { seedDevelopment } from "@/db/seed/development";
import { seedProduction } from "@/db/seed/production";

config({ path: ".env" });

if (!process.env.DATABASE_URL) {
  throw new Error("DATABASE_URL is not set");
}

async function main() {
  console.log(`[${process.env.NODE_ENV}] 🌱 Seeding database...`);
  switch (process.env.NODE_ENV) {
    case "development":
      await seedDevelopment();
      break;
    case "production":
      await seedProduction();
      break;
    default:
      console.log("Not a valid environment. Skipping seed.");
      break;
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
