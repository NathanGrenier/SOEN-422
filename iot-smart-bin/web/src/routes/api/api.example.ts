import { createFileRoute } from "@tanstack/react-router";
import { json } from "@tanstack/react-start";

export const Route = createFileRoute("/api/api/example")({
  server: {
    handlers: {
      GET: async ({ request, params }) => {
        console.info(`Fetching by id=${params.id}... @`, request.url);
        try {
          const res = await fetch(
            `https://jsonplaceholder.typicode.com/users/${params.id}`,
          );
          return json({
            id: res.data.id,
            name: res.data.name,
            email: res.data.email,
          });
        } catch (e) {
          console.error(e);
          return json({ error: "User not found" }, { status: 404 });
        }
      },
    },
  },
});
