import {
  AlertTriangle,
  Battery,
  CircleX,
  MapPin,
  Radio,
  RadioTower,
  Wifi,
  WifiOff,
} from "lucide-react";
import { createFileRoute } from "@tanstack/react-router";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";

export const Route = createFileRoute("/dashboard")({
  component: Dashboard,
});

type ConnectionStatus = "Connected" | "Disconnected";
type HealthStatus = "Healthy" | "Warning" | "Error";

type Device = {
  id: string;
  location: string;
  fillLevel: number;
  threshold: number;
  connectionStatus: ConnectionStatus;
  healthStatus: HealthStatus;
  batteryPercentage: number;
};

const devicesData: Device[] = [
  {
    id: "BIN-001",
    location: "Main St Plaza",
    fillLevel: 85,
    threshold: 90,
    connectionStatus: "Connected",
    healthStatus: "Healthy",
    batteryPercentage: 75,
  },
  {
    id: "BIN-002",
    location: "Parkside Cafeteria",
    fillLevel: 95,
    threshold: 90,
    connectionStatus: "Connected",
    healthStatus: "Healthy",
    batteryPercentage: 50,
  },
  {
    id: "BIN-003",
    location: "City Hall - 3rd Floor",
    fillLevel: 60,
    threshold: 90,
    connectionStatus: "Disconnected",
    healthStatus: "Warning",
    batteryPercentage: 20,
  },
  {
    id: "BIN-004",
    location: "Boardwalk",
    fillLevel: 15,
    threshold: 90,
    connectionStatus: "Connected",
    healthStatus: "Healthy",
    batteryPercentage: 98,
  },
  {
    id: "BIN-005",
    location: "South End",
    fillLevel: 92,
    threshold: 90,
    connectionStatus: "Connected",
    healthStatus: "Warning",
    batteryPercentage: 35,
  },
  {
    id: "BIN-006",
    location: "Central Station",
    fillLevel: 70,
    threshold: 90,
    connectionStatus: "Connected",
    healthStatus: "Error",
    batteryPercentage: 0,
  },
];

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
  const statusIcons: Record<ConnectionStatus, React.ReactNode> = {
    Connected: <Wifi className="h-4 w-4 text-green-600" />,
    Disconnected: <WifiOff className="h-4 w-4 text-red-600" />,
  };

  const statusBadgeVariant: Record<
    HealthStatus,
    "secondary" | "outline" | "destructive"
  > = {
    Healthy: "secondary",
    Warning: "outline",
    Error: "destructive",
  };

  const statusBadgeIcon: Record<HealthStatus, React.ReactNode | null> = {
    Healthy: null,
    Warning: <AlertTriangle className="mr-1 h-3 w-3 text-yellow-600" />,
    Error: <CircleX className="mr-1 h-3 w-3 text-white-600" />,
  };

  const getBatteryColor = (percentage: number) => {
    if (percentage <= 15) {
      return "text-red-600";
    }
    if (percentage <= 30) {
      return "text-yellow-600";
    }
    return "text-grey-600";
  };

  const batteryColorClass = getBatteryColor(device.batteryPercentage);

  return (
    <Card>
      <CardHeader>
        <div className="flex justify-between items-start">
          <CardTitle>{device.id}</CardTitle>
          <Badge variant={statusBadgeVariant[device.healthStatus]}>
            {statusBadgeIcon[device.healthStatus]}
            {device.healthStatus}
          </Badge>
        </div>
        <div className="flex items-center text-sm text-gray-500 pt-1">
          <MapPin className="mr-1.5 h-4 w-4" />
          <span>{device.location}</span>
        </div>
      </CardHeader>
      <CardContent>
        <FillLevelBar level={device.fillLevel} threshold={device.threshold} />

        <div className="mt-4 flex justify-between items-center text-sm text-gray-600">
          <div className={`flex items-center space-x-2 ${batteryColorClass}`}>
            <Battery className="h-5 w-5" />
            <span>{device.batteryPercentage}%</span>
          </div>
          <div className="flex items-center space-x-2">
            {statusIcons[device.connectionStatus] || (
              <WifiOff className="h-4 w-4 text-gray-400" />
            )}
            <span>{device.connectionStatus}</span>
          </div>
        </div>
      </CardContent>
      <CardFooter className="flex justify-between">
        <a
          href={`/devices/${device.id}`}
          onClick={(e) => e.preventDefault()}
          className="text-sm font-medium text-blue-600 hover:underline"
        >
          View Details
        </a>
        <Button variant="outline" size="sm">
          <Radio className="mr-1.5 h-4 w-4" />
          Ping
        </Button>
      </CardFooter>
    </Card>
  );
};

function Dashboard() {
  return (
    <div className="p-6 md:p-10">
      <header className="mb-6 flex flex-col md:flex-row md:items-center md:justify-between">
        <h1 className="text-3xl font-bold text-gray-900 mb-4 md:mb-0">
          Smart Bin Dashboard
        </h1>
        <Button variant="default" size="lg">
          <RadioTower className="mr-2 h-5 w-5" />
          Ping All Devices
        </Button>
      </header>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {devicesData.map((device) => (
          <DeviceCard key={device.id} device={device} />
        ))}
      </div>
    </div>
  );
}

export default Dashboard;
