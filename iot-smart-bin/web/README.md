# Getting Started (Web)

To run this application:

1. Set Up Environment Variables: Create a `.env` file from the `.env.example` template. Fill in your database credentials and any API keys.
2. Install Dependencies: `npm install`
3. Run the Database (using Docker): `docker-compose up -d`
4. Run database migrations: `npm run db:push`
5. Start the Server: `npm run dev`
6. Access the Application: Open your browser and go to http://localhost:3000 (or your configured port).

## Building For Production

To build this application for production:

```bash
npm run build
```

## Testing

This project uses [Vitest](https://vitest.dev/) for testing. You can run the tests with:

```bash
npm run test
```

## Linting & Formatting

This project uses [eslint](https://eslint.org/) and [prettier](https://prettier.io/) for linting and formatting. Eslint is configured using [tanstack/eslint-config](https://tanstack.com/config/latest/docs/eslint). The following scripts are available:

```bash
npm run lint
npm run format
```

## Shadcn

Add components using the latest version of [Shadcn](https://ui.shadcn.com/).

```bash
pnpx shadcn@latest add button
```

# Getting Started (Mosquitto)

## Generating a Password

In order to connect, you'll need to generate a password file. You can use the following command to do so as long as you have Docker installed.

```sh
docker run -it --rm \
  -v "$(pwd)/mosquitto/config/passwd:/mosquitto/config/passwd" \
  eclipse-mosquitto:latest \
  mosquitto_passwd /mosquitto/config/passwd YOUR_USER_NAME
```

You might also need to use this command to properly load the `passwd` file after generation: `chmod 0700 ./mosquitto/config/passwd`

## MQTT.js

Using [this library](https://github.com/mqttjs/MQTT.js) for MQTT communication from the website to the broker. 