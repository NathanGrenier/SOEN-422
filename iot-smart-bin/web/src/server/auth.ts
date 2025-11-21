import { createMiddleware, createServerFn } from "@tanstack/react-start";
import { getRequest } from "@tanstack/react-start/server";
import { auth } from "@/lib/auth";

export const authMiddleware = createMiddleware().server(async ({ next }) => {
  const request = getRequest();
  const session = await auth.api.getSession({
    headers: request.headers,
  });

  if (!session) {
    throw new Error("Unauthorized");
  }

  return next({
    context: {
      session,
      user: session.user,
    },
  });
});

export const getAuthSession = createServerFn({ method: "GET" }).handler(
  async () => {
    const request = getRequest();

    const session = await auth.api.getSession({
      headers: request.headers,
    });

    return session;
  },
);
