import { Link, useNavigate } from "@tanstack/react-router";
import { LogIn, LogOut, Trash2 } from "lucide-react";
import { Button } from "@/components/ui/button";
import { authClient } from "@/lib/auth-client";

export default function Header() {
  const { data: session, isPending } = authClient.useSession();
  const navigate = useNavigate();

  const handleSignOut = async () => {
    await authClient.signOut({
      fetchOptions: {
        onSuccess: () => {
          navigate({ to: "/login" });
        },
      },
    });
  };

  return (
    <header className="sticky top-0 z-50 w-full border-b bg-white/95 backdrop-blur supports-backdrop-filter:bg-white/60 dark:bg-slate-950/95 dark:border-slate-800">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 flex h-16 items-center justify-between">
        {/* Logo Section */}
        <div className="flex items-center gap-2">
          <Link to="/" className="flex items-center gap-2">
            <div className="bg-emerald-600 p-1.5 rounded-md">
              <Trash2 className="h-5 w-5 text-white" />
            </div>
            <span className="hidden sm:inline-block font-bold text-lg tracking-tight text-slate-900 dark:text-white">
              SmartBin<span className="text-emerald-600">IoT</span>
            </span>
          </Link>
        </div>

        {/* Navigation Links - Only show protected routes if logged in */}
        <nav className="flex items-center gap-2">
          {session && (
            <>
              <Link
                to="/dashboard"
                className="px-3 py-2 rounded-md text-sm font-medium text-slate-600 hover:text-emerald-600 transition-colors dark:text-slate-400 dark:hover:text-emerald-500"
                activeProps={{
                  className:
                    "text-emerald-600 dark:text-emerald-500 font-semibold underline underline-offset-4",
                }}
              >
                Dashboard
              </Link>
              <Link
                to="/devices"
                className="px-3 py-2 rounded-md text-sm font-medium text-slate-600 hover:text-emerald-600 transition-colors dark:text-slate-400 dark:hover:text-emerald-500"
                activeProps={{
                  className:
                    "text-emerald-600 dark:text-emerald-500 font-semibold underline underline-offset-4",
                }}
              >
                Devices
              </Link>
            </>
          )}
        </nav>

        {/* Action Buttons */}
        <div className="flex items-center gap-2">
          {!isPending && (
            <>
              {session ? (
                <Button
                  variant="ghost"
                  size="sm"
                  className="flex gap-2"
                  onClick={handleSignOut}
                >
                  <LogOut className="h-4 w-4" />
                  Logout
                </Button>
              ) : (
                <Link to="/login">
                  <Button variant="ghost" size="sm" className="flex gap-2">
                    <LogIn className="h-4 w-4" />
                    Login
                  </Button>
                </Link>
              )}
            </>
          )}
        </div>
      </div>
    </header>
  );
}
