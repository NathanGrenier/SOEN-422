# --- Build Stage ---
FROM node:22-alpine AS build
WORKDIR /app

# Install build tools needed for native modules
RUN apk add --no-cache python3 build-base sqlite-dev

# Copy package files and install all dependencies (including dev)
COPY package*.json ./
RUN npm ci

# Copy the rest of your app code
COPY . .

# Build your TanStack Start app
RUN npm run build

# Remove devDependencies to keep node_modules small for runtime
RUN npm prune --production

# --- Production Stage ---
FROM node:22-alpine AS production
WORKDIR /app

# Copy only necessary files from the build stage
COPY --from=build /app/.output ./.output
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/package.json ./package.json
COPY --from=build /app/drizzle.config.ts ./drizzle.config.ts
COPY --from=build /app/drizzle ./drizzle
# Files needed for seeding
COPY --from=build /app/tsconfig.json ./tsconfig.json
COPY --from=build /app/src/db ./src/db
COPY --from=build /app/src/lib ./src/lib
COPY --from=build /app/src/env.ts ./src/env.ts

# Copy the entrypoint script and make it executable
COPY docker-entrypoint.sh .
RUN chmod +x docker-entrypoint.sh

ENV NODE_ENV=production

EXPOSE 3000

# Use the entrypoint script to start the container
ENTRYPOINT ["/app/docker-entrypoint.sh"]