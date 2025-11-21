import { Outlet, createFileRoute, redirect } from "@tanstack/react-router";
import { getAuthSession } from "@/server/auth";

export const Route = createFileRoute("/_auth")({
  beforeLoad: async ({ location }) => {
    const session = await getAuthSession();

    if (!session) {
      throw redirect({
        to: "/login",
        search: {
          redirect: location.href,
        },
      });
    }

    return { session };
  },
  component: AuthLayout,
});

function AuthLayout() {
  return <Outlet />;
}
