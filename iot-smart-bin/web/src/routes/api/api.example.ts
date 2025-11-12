import { createFileRoute } from "@tanstack/react-router";
import { json } from "@tanstack/react-start";

export const Route = createFileRoute("/api/api/example")({
  server: {
    handlers: {
      GET: async ({
        request,
        params,
      }: {
        request: Request;
        params: { id: string };
      }) => {
        console.info(`Fetching by id=${params.id}... @`, request.url);
        try {
          const res = await fetch(
            `https://jsonplaceholder.typicode.com/users/${params.id}`,
          );
          const data = await res.json();
          return json({
            id: data.id,
            name: data.name,
            email: data.email,
          });
        } catch (e) {
          console.error(e);
          return json({ error: "User not found" }, { status: 404 });
        }
      },
    },
  },
});
