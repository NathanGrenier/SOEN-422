#!/bin/sh

# Exit immediately if a command exits with a non-zero status.
set -e

# Exit immediately if a command exits with a non-zero status.
set -e

# Verify database migrations
echo "Verifying database migrations..."
npm run db:check

# Run database migrations
echo "Running database migrations..."
npm run db:migrate

# Start the application
echo "Starting the application..."
exec npm run start