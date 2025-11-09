# Start from a Node.js base image
FROM node:18-alpine

# Set the working directory
WORKDIR /app

# Copy package files and install dependencies
COPY package*.json ./
RUN npm install

# Copy the rest of your app code
COPY . .

# Build your TanStack Start app
RUN npm run build

# Create the directory where the database file will live
# This is important for permissions
RUN mkdir -p /app/data

# Set permissions (optional but good practice)
RUN chown -R node:node /app/data

# Switch to a non-root user (good security practice)
USER node

# Expose the port your app runs on
EXPOSE 3000

# The command to start your app
CMD ["npm", "run", "start"]