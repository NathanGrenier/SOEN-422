import { config } from "dotenv"

import { drizzle } from "drizzle-orm/better-sqlite3"
import Database from "better-sqlite3"

import * as schema from "./schema.ts"

config({
  path: process.env.NODE_ENV === "production" ? ".env.production" : ".env",
})

if (!process.env.DATABASE_URL) {
  throw new Error("DATABASE_URL environment variable is not set")
}

const sqlite = new Database(process.env.DATABASE_URL)
export const db = drizzle(sqlite, { schema })
