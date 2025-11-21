import { useState } from "react";
import { toast } from "sonner";
import {
  Battery,
  Loader2,
  MapPin,
  Radio,
  RadioTower,
  Wifi,
  WifiOff,
} from "lucide-react";
import { createFileRoute } from "@tanstack/react-router";
import { useQuery } from "@tanstack/react-query";
import type { Device } from "@/server/mqtt-client";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { getDashboardData, pingDevice } from "@/server/mqtt";

export const Route = createFileRoute("/_auth/dashboard")({
  component: Dashboard,
});

const FillLevelBar = ({
  level,
  threshold,
}: {
  level: number;
  threshold: number;
}) => {
  const isOverThreshold = level > threshold;
  const barColor = isOverThreshold ? "bg-red-500" : "bg-blue-500";

  return (
    <div className="w-full my-3">
      <div className="flex justify-between items-end mb-1">
        <span className="text-sm font-medium text-gray-600">Fill Level</span>
        <span
          className={`text-lg font-bold ${
            isOverThreshold ? "text-red-600" : "text-gray-900"
          }`}
        >
          {level}%
        </span>
      </div>
      <div className="relative h-4 w-full rounded-full bg-gray-200">
        {/* The fill bar */}
        <div
          className={`h-4 rounded-full transition-all duration-500 ${barColor}`}
          style={{ width: `${level}%` }}
        ></div>
        {/* The threshold line */}
        <div
          className="absolute top-0 h-full w-0.5 border-r-2 border-dashed border-black opacity-70"
          style={{ left: `${threshold}%` }}
          title={`Threshold: ${threshold}%`}
        ></div>
      </div>
    </div>
  );
};

const DeviceCard = ({ device }: { device: Device }) => {
  const isOnline = device.isOnline;
  const [isPinging, setIsPinging] = useState(false);

  let batteryColorClass = "text-gray-600";
  if (device.batteryPercentage <= 15) batteryColorClass = "text-red-600";
  else if (device.batteryPercentage <= 30)
    batteryColorClass = "text-yellow-600";

  const handlePing = async () => {
    if (isPinging) return;
    setIsPinging(true);

    // Create a timeout promise that rejects after 10 seconds
    const timeoutPromise = new Promise((_, reject) =>
      setTimeout(() => reject(new Error("Request timed out")), 10000),
    );

    try {
      await Promise.race([
        pingDevice({ data: { id: device.id } }),
        timeoutPromise,
      ]);

      toast.success(`Ping sent to ${device.id}`);
    } catch (e) {
      console.error(e);
      const errorMessage =
        e instanceof Error && e.message === "Request timed out"
          ? "Ping request timed out"
          : "Failed to send ping";

      toast.error(errorMessage);
    } finally {
      setIsPinging(false);
    }
  };

  return (
    <Card className={`${!isOnline ? `opacity-60 grayscale` : ``}`}>
      <CardHeader>
        <div className="flex justify-between items-start">
          <CardTitle>{device.id}</CardTitle>
          <div className="flex items-center space-x-2">
            {isOnline ? (
              <Wifi className="h-4 w-4 text-green-600" />
            ) : (
              <WifiOff className="h-4 w-4 text-red-600" />
            )}
            <span
              className={
                isOnline
                  ? "text-green-700 font-medium"
                  : "text-red-700 font-medium"
              }
            >
              {isOnline ? "Online" : "Offline"}
            </span>
          </div>
        </div>
        <div className="flex items-center text-sm text-gray-500 pt-1">
          <MapPin className="mr-1.5 h-4 w-4" />
          <span>{device.location}</span>
        </div>
      </CardHeader>
      <CardContent>
        <FillLevelBar level={device.fillLevel} threshold={device.threshold} />
      </CardContent>
      <CardFooter className="flex justify-between">
        <div
          className={`flex items-center space-x-2 text-sm ${batteryColorClass}`}
        >
          <Battery className="h-5 w-5" />
          <span>{device.batteryPercentage}%</span>
        </div>

        <Button
          variant="outline"
          size="sm"
          disabled={!isOnline || isPinging}
          onClick={handlePing}
        >
          {isPinging ? (
            <Loader2 className="mr-1.5 h-4 w-4 animate-spin" />
          ) : (
            <Radio className="mr-1.5 h-4 w-4" />
          )}
          {isPinging ? "Pinging..." : "Ping"}
        </Button>
      </CardFooter>
    </Card>
  );
};

function Dashboard() {
  const { data, isLoading } = useQuery({
    queryKey: ["dashboard-data"],
    queryFn: () => getDashboardData(),
    refetchInterval: 1000,
  });

  const brokerStatus = data?.brokerStatus || "Connecting";
  const devices = data?.devices || [];

  return (
    <div className="p-6 md:p-10">
      <header className="mb-6 flex flex-col md:flex-row md:items-center md:justify-between gap-4">
        <div>
          <h1 className="text-3xl font-bold text-gray-900">
            Smart Bin Dashboard
          </h1>

          <div className="flex items-center gap-2 mt-2 text-sm">
            <span className="text-gray-500">System Status:</span>
            {brokerStatus === "Connected" ? (
              <Badge className="bg-emerald-600 hover:bg-emerald-700">
                MQTT Broker Connected
              </Badge>
            ) : (
              <Badge
                variant="outline"
                className="text-amber-600 border-amber-600 flex gap-1"
              >
                <Loader2 className="h-3 w-3 animate-spin" />
                {brokerStatus}...
              </Badge>
            )}
          </div>
        </div>

        <Button variant="default" size="lg">
          <RadioTower className="mr-2 h-5 w-5" />
          Scan Network
        </Button>
      </header>

      {isLoading && (
        <div className="flex flex-col items-center justify-center py-20">
          <Loader2 className="h-10 w-10 text-blue-500 animate-spin mb-4" />
          <p className="text-gray-500">
            Establishing connection to telemetry server...
          </p>
        </div>
      )}

      {!isLoading && devices.length === 0 && (
        <div className="p-10 border-2 border-dashed rounded-xl text-center text-gray-500 bg-gray-50/50">
          <Wifi className="h-10 w-10 mx-auto mb-3 text-gray-400" />
          <p className="text-lg font-medium">No Devices Detected</p>
          <p className="text-sm">
            Waiting for devices to publish to{" "}
            <code className="bg-gray-200 px-1 rounded">bins/+/data</code>
          </p>
        </div>
      )}

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {devices.map((device) => (
          <DeviceCard key={device.id} device={device} />
        ))}
      </div>
    </div>
  );
}

export default Dashboard;
