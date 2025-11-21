import { Link } from "@tanstack/react-router";
import { ArrowLeft, FileQuestion, Home } from "lucide-react";
import { Button } from "@/components/ui/button";

export function NotFound({ children }: { children?: any }) {
  return (
    <div className="flex flex-col items-center justify-center min-h-[60vh] text-center space-y-8 p-6">
      {/* Icon Circle */}
      <div className="bg-slate-100 dark:bg-slate-800 p-8 rounded-full shadow-sm">
        <FileQuestion className="h-16 w-16 text-slate-400 dark:text-slate-500" />
      </div>

      {/* Text Content */}
      <div className="space-y-3">
        <h1 className="text-4xl font-extrabold tracking-tight text-slate-900 dark:text-white">
          Page Not Found
        </h1>
        <div className="text-lg text-slate-600 dark:text-slate-400 max-w-md mx-auto leading-relaxed">
          {children || (
            <p>
              Sorry, we couldn't find the page you're looking for. It might have
              been moved, deleted, or the URL might be incorrect.
            </p>
          )}
        </div>
      </div>

      {/* Actions */}
      <div className="flex flex-col sm:flex-row gap-4 w-full sm:w-auto">
        <Button
          variant="outline"
          size="lg"
          onClick={() => window.history.back()}
          className="gap-2 min-w-[140px]"
        >
          <ArrowLeft className="h-4 w-4" />
          Go Back
        </Button>

        <Link to="/">
          <Button
            size="lg"
            className="gap-2 min-w-[140px] bg-emerald-600 hover:bg-emerald-700 text-white"
          >
            <Home className="h-4 w-4" />
            Return Home
          </Button>
        </Link>
      </div>
    </div>
  );
}
