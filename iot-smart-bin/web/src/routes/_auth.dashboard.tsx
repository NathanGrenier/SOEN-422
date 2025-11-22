import { useCallback, useEffect, useMemo, useState } from "react";
import { toast } from "sonner";
import {
  Battery,
  GripVertical,
  Loader2,
  MapPin,
  Radio,
  RadioTower,
  RotateCw,
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
import { getDashboardData, pingDevice } from "@/server/dashboard";

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

const DeviceCard = ({
  device,
  onDragStart,
  onDrop,
  isDragging,
}: {
  device: Device;
  onDragStart: (e: React.DragEvent, id: string) => void;
  onDrop: (e: React.DragEvent, id: string) => void;
  isDragging: boolean;
}) => {
  const [isPinging, setIsPinging] = useState(false);
  const isOnline = device.isOnline;
  const isTipped = device.isTilted;

  let batteryColorClass = "text-gray-600";
  if (device.batteryPercentage <= 15) batteryColorClass = "text-red-600";
  else if (device.batteryPercentage <= 30)
    batteryColorClass = "text-yellow-600";

  const handlePing = async () => {
    if (isPinging) return;
    setIsPinging(true);

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
    <div
      draggable="true"
      onDragStart={(e) => onDragStart(e, device.id)}
      onDragOver={(e) => e.preventDefault()}
      onDrop={(e) => onDrop(e, device.id)}
      className={`transition-opacity duration-200 ${isDragging ? "opacity-40" : "opacity-100"}`}
    >
      <Card
        className={`${!isOnline ? `opacity-60 grayscale` : ``} ${isTipped ? "border-red-500 border-2" : ""}`}
      >
        <CardHeader>
          <div className="flex justify-between items-start">
            <div className="flex items-center gap-2">
              <div className="cursor-grab active:cursor-grabbing p-1 rounded hover:bg-slate-100 dark:hover:bg-slate-800">
                <GripVertical className="h-4 w-4 text-slate-400" />
              </div>
              <CardTitle>{device.id}</CardTitle>
            </div>
            {isTipped && (
              <div className="flex items-center text-red-600 font-bold animate-pulse">
                <RotateCw className="h-5 w-5 mr-1" />
                <span>TIPPED</span>
              </div>
            )}
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
          <div className="flex items-center text-sm text-gray-500 pt-1 pl-1">
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
    </div>
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

  // --- Drag and Drop Logic ---
  const STORAGE_KEY = "dashboard-device-order";
  const [orderedIds, setOrderedIds] = useState<string[]>([]);
  const [draggedId, setDraggedId] = useState<string | null>(null);

  // Load order from local storage on mount
  useEffect(() => {
    const savedOrder = localStorage.getItem(STORAGE_KEY);
    if (savedOrder) {
      try {
        setOrderedIds(JSON.parse(savedOrder));
      } catch (e) {
        console.error("Failed to parse saved device order", e);
      }
    }
  }, []);

  // Derived state: Sort the fetched devices based on the saved order
  const sortedDevices = useMemo(() => {
    if (!devices.length) return [];

    const deviceMap = new Map(devices.map((d) => [d.id, d]));
    const result: Device[] = [];

    // 1. Add devices in the order specified by state
    orderedIds.forEach((id) => {
      if (deviceMap.has(id)) {
        result.push(deviceMap.get(id)!);
        deviceMap.delete(id);
      }
    });

    // 2. Add any remaining devices (new ones not in saved order) to the end
    deviceMap.forEach((device) => {
      result.push(device);
    });

    return result;
  }, [devices, orderedIds]);

  const handleDragStart = (e: React.DragEvent, id: string) => {
    setDraggedId(id);
    // required for firefox
    e.dataTransfer.effectAllowed = "move";
  };

  const handleDrop = useCallback(
    (e: React.DragEvent, targetId: string) => {
      e.preventDefault();

      if (!draggedId || draggedId === targetId) return;

      // Get the current valid order list (including any new devices not yet in state)
      const currentOrder = sortedDevices.map((d) => d.id);
      const fromIndex = currentOrder.indexOf(draggedId);
      const toIndex = currentOrder.indexOf(targetId);

      if (fromIndex === -1 || toIndex === -1) return;

      const newOrder = [...currentOrder];
      newOrder.splice(fromIndex, 1);
      newOrder.splice(toIndex, 0, draggedId);

      setOrderedIds(newOrder);
      setDraggedId(null);
      localStorage.setItem(STORAGE_KEY, JSON.stringify(newOrder));
    },
    [draggedId, sortedDevices],
  );

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

      {!isLoading && sortedDevices.length === 0 && (
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
        {sortedDevices.map((device) => (
          <DeviceCard
            key={device.id}
            device={device}
            onDragStart={handleDragStart}
            onDrop={handleDrop}
            isDragging={draggedId === device.id}
          />
        ))}
      </div>
    </div>
  );
}

export default Dashboard;
