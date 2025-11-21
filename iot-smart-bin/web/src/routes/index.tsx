import { Link, createFileRoute } from "@tanstack/react-router";
import { ArrowRight, BarChart3, BatteryCharging, Signal } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";

export const Route = createFileRoute("/")({ component: LandingPage });

function LandingPage() {
  return (
    <div className="flex flex-col min-h-[calc(100vh-4rem)]">
      {/* Hero Section */}
      <section className="flex-1 flex flex-col items-center justify-center py-20 text-center space-y-8 max-w-4xl mx-auto px-4">
        <h1 className="text-4xl md:text-6xl font-extrabold tracking-tight text-slate-900 dark:text-white">
          Data-Driven <span className="text-emerald-600">Waste Management</span>
        </h1>

        <p className="text-xl text-slate-600 dark:text-slate-400 max-w-2xl mx-auto leading-relaxed">
          Eliminating the inefficiencies of static collection schedules. We use
          IoT sensors to provide real-time fill levels, optimizing labor and
          ensuring campus sanitation.
        </p>

        <div className="flex flex-col sm:flex-row gap-4 w-full justify-center pt-4">
          <Link to="/dashboard">
            <Button
              size="lg"
              className="w-full sm:w-auto bg-emerald-600 hover:bg-emerald-700 text-white text-lg px-8 h-12"
            >
              Launch Dashboard
              <ArrowRight className="ml-2 h-5 w-5" />
            </Button>
          </Link>
          <Button
            variant="outline"
            size="lg"
            className="w-full sm:w-auto h-12 text-lg"
            asChild
          >
            <a
              href="https://github.com/NathanGrenier/SOEN-422"
              target="_blank"
              rel="noopener noreferrer"
            >
              View Documentation
            </a>
          </Button>
        </div>
      </section>

      {/* Features Grid */}
      <section className="py-16 bg-slate-50 dark:bg-slate-900/50 rounded-3xl mb-10">
        <div className="max-w-6xl mx-auto px-4">
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            <Card className="border-none shadow-md bg-white dark:bg-slate-800">
              <CardHeader>
                <Signal className="h-10 w-10 text-emerald-600 mb-2" />
                <CardTitle>Real-Time Telemetry</CardTitle>
              </CardHeader>
              <CardContent className="text-slate-600 dark:text-slate-400">
                Utilizing ESP32 microcontrollers and ultrasonic sensors to
                transmit fill-level data instantly via MQTT over WiFi.
              </CardContent>
            </Card>

            <Card className="border-none shadow-md bg-white dark:bg-slate-800">
              <CardHeader>
                <BarChart3 className="h-10 w-10 text-blue-600 mb-2" />
                <CardTitle>Intelligent Analytics</CardTitle>
              </CardHeader>
              <CardContent className="text-slate-600 dark:text-slate-400">
                A central dashboard aggregates data to identify overflowing bins
                (85%+) and optimize collection routes.
              </CardContent>
            </Card>

            <Card className="border-none shadow-md bg-white dark:bg-slate-800">
              <CardHeader>
                <BatteryCharging className="h-10 w-10 text-amber-500 mb-2" />
                <CardTitle>Low Power Design</CardTitle>
              </CardHeader>
              <CardContent className="text-slate-600 dark:text-slate-400">
                Embedded Deep-Sleep protocols ensure longevity, waking only on
                periodic timers or interrupt triggers.
              </CardContent>
            </Card>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer className="py-8 text-center text-sm text-slate-500 border-t">
        <p>© 2025 Concordia University - SOEN 422 Semester Project.</p>
      </footer>
    </div>
  );
}
