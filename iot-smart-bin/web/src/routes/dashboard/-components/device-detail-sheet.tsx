import { useQuery } from "@tanstack/react-query";
import { useParams, useRouter } from "@tanstack/react-router";
import {
  Area,
  AreaChart,
  CartesianGrid,
  Label,
  Line,
  LineChart,
  ReferenceLine,
  XAxis,
  YAxis,
} from "recharts";
import { Battery, Clock, Trash2, Zap } from "lucide-react";
import type { ChartConfig } from "@/components/ui/chart";
import {
  Sheet,
  SheetContent,
  SheetDescription,
  SheetHeader,
  SheetTitle,
} from "@/components/ui/sheet";
import {
  ChartContainer,
  ChartTooltip,
  ChartTooltipContent,
} from "@/components/ui/chart";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { getDeviceReadings } from "@/server/dashboard-chart";
import { useIsMobile } from "@/lib/utils";

const fillChartConfig = {
  fillLevel: {
    label: "Fill Level",
    color: "hsl(var(--chart-1))",
  },
} satisfies ChartConfig;

const batteryChartConfig = {
  batteryPercentage: {
    label: "Battery",
    color: "hsl(var(--chart-2))",
  },
} satisfies ChartConfig;

// Helper interface for Recharts Dot props
interface CustomDotProps {
  cx: number;
  cy: number;
  payload: {
    fillLevel: number;
  };
}

export function DeviceDetailSheet() {
  const { deviceId } = useParams({ from: "/_auth/dashboard/$deviceId" });
  const router = useRouter();
  const isMobile = useIsMobile();

  const { data, isLoading } = useQuery({
    queryKey: ["device-history", deviceId],
    queryFn: () => getDeviceReadings({ data: { id: deviceId } }),
    refetchInterval: 5000,
  });

  const history = data?.history;
  const threshold = data?.threshold || 85;

  const handleOpenChange = (open: boolean) => {
    if (!open) {
      router.navigate({ to: "/dashboard" });
    }
  };

  // Calculate gradient offset based on threshold
  // SVG coordinate system: 0 is top (100%), 1 is bottom (0%)
  const gradientOffset = (100 - threshold) / 100;

  if (!deviceId) return null;

  const latest = history?.[history.length - 1];

  return (
    <Sheet open={true} onOpenChange={handleOpenChange}>
      <SheetContent
        side={isMobile ? "bottom" : "right"}
        className={`overflow-y-auto bg-background ${
          isMobile
            ? "max-h-[85vh] rounded-t-2xl border-t"
            : "sm:max-w-2xl border-l"
        }`}
      >
        <SheetHeader className="space-y-4">
          <div className="flex flex-col gap-2">
            <SheetTitle className="text-4xl font-bold tracking-tight">
              {deviceId}
            </SheetTitle>
            <SheetDescription className="text-base font-medium text-muted-foreground/80">
              Real-time telemetry and historical performance data.
            </SheetDescription>
          </div>
        </SheetHeader>

        {isLoading ? (
          <div className="h-96 flex flex-col items-center justify-center text-slate-500 gap-4">
            <div className="animate-spin h-8 w-8 border-4 border-emerald-500 border-t-transparent rounded-full" />
            <p>Loading telemetry...</p>
          </div>
        ) : (
          <div className="space-y-8 pb-8 md:mx-5 mx-3">
            {/* Summary Cards */}
            <div className="grid grid-cols-2 gap-4">
              <Card className="flex flex-col items-center justify-center text-center shadow-sm bg-muted/20 border-muted-foreground/10">
                <CardHeader className="p-4 pb-2 w-full flex items-center justify-center">
                  <CardTitle className="text-sm font-medium text-muted-foreground flex flex-col items-center gap-2 uppercase tracking-wider">
                    <Zap className="h-5 w-5 text-yellow-500 fill-yellow-500" />
                    Voltage
                  </CardTitle>
                </CardHeader>
                <CardContent className="p-4 pt-0">
                  <div className="text-3xl font-bold text-foreground">
                    {latest?.voltage?.toFixed(2) ?? "--"}
                    <span className="text-3xl font-normal text-muted-foreground ml-1">
                      V
                    </span>
                  </div>
                </CardContent>
              </Card>
              <Card className="flex flex-col items-center justify-center text-center shadow-sm bg-muted/20 border-muted-foreground/10">
                <CardHeader className="p-4 pb-2 w-full flex items-center justify-center">
                  <CardTitle className="text-sm font-medium text-muted-foreground flex flex-col items-center gap-2 uppercase tracking-wider">
                    <Clock className="h-5 w-5 text-blue-500" />
                    Last Reading
                  </CardTitle>
                </CardHeader>
                <CardContent className="p-4 pt-0">
                  <div className="text-xl font-bold text-foreground">
                    {latest
                      ? new Date(latest.createdAt).toLocaleTimeString([], {
                          hour: "2-digit",
                          minute: "2-digit",
                        })
                      : "--:--"}
                  </div>
                </CardContent>
              </Card>
            </div>

            {/* --- Fill Level Chart --- */}
            <Card className="shadow-sm border-muted-foreground/10">
              <CardHeader>
                <CardTitle className="text-base font-semibold flex items-center gap-2">
                  <Trash2 className="h-4 w-4 text-emerald-600" />
                  Fill Level History
                </CardTitle>
                <CardDescription>
                  Percentage of capacity used over time.
                </CardDescription>
              </CardHeader>
              <CardContent className="pl-0 pr-4 pt-4">
                <ChartContainer
                  config={fillChartConfig}
                  className="min-h-[250px] w-full"
                >
                  <LineChart
                    data={history}
                    margin={{ top: 10, right: 10, left: 0, bottom: 0 }}
                  >
                    <defs>
                      <linearGradient
                        id="fillGradient"
                        x1="0"
                        y1="0"
                        x2="0"
                        y2="1"
                      >
                        {/* Top (100%): Red */}
                        <stop offset={0} stopColor="#ef4444" stopOpacity={1} />
                        {/* Threshold: Red */}
                        <stop
                          offset={gradientOffset}
                          stopColor="#ef4444"
                          stopOpacity={1}
                        />
                        {/* Bottom (0%): Blue - Interpolates from Threshold (Red) to Bottom (Blue) */}
                        <stop offset={1} stopColor="#3b82f6" stopOpacity={1} />
                      </linearGradient>
                    </defs>
                    <CartesianGrid vertical={false} strokeDasharray="3 3" />
                    <XAxis
                      dataKey="createdAt"
                      tickLine={false}
                      axisLine={false}
                      tickMargin={8}
                      tickFormatter={(value) =>
                        new Date(value).toLocaleTimeString([], {
                          hour: "2-digit",
                          minute: "2-digit",
                        })
                      }
                    />
                    <YAxis
                      domain={[0, 100]}
                      tickLine={false}
                      axisLine={false}
                      width={40}
                      tickFormatter={(value) => `${value}%`}
                    />
                    <ChartTooltip
                      content={
                        <ChartTooltipContent
                          indicator="line"
                          labelFormatter={(value) => {
                            return new Date(value).toLocaleString([], {
                              month: "short",
                              day: "numeric",
                              hour: "numeric",
                              minute: "2-digit",
                            });
                          }}
                        />
                      }
                    />
                    <Line
                      dataKey="fillLevel"
                      type="monotone"
                      stroke="url(#fillGradient)"
                      strokeWidth={3}
                      // Dynamic dot color based on threshold
                      dot={(props: any) => {
                        const typedProps = props as CustomDotProps;
                        const val = typedProps.payload.fillLevel;
                        const isAbove = val >= threshold;
                        const color = isAbove ? "#ef4444" : "#3b82f6";
                        return (
                          <circle
                            cx={typedProps.cx}
                            cy={typedProps.cy}
                            r={4}
                            fill={color}
                            stroke={color}
                            strokeWidth={1}
                          />
                        );
                      }}
                      activeDot={(props: any) => {
                        const typedProps = props as CustomDotProps;
                        const val = typedProps.payload.fillLevel;
                        const isAbove = val >= threshold;
                        const color = isAbove ? "#ef4444" : "#3b82f6";
                        return (
                          <circle
                            cx={typedProps.cx}
                            cy={typedProps.cy}
                            r={6}
                            fill={color}
                            stroke="var(--background)"
                            strokeWidth={2}
                          />
                        );
                      }}
                    />
                    <ReferenceLine
                      y={threshold}
                      stroke="#dc2626"
                      strokeDasharray="5 5"
                      strokeWidth={2}
                      isFront={true}
                    >
                      <Label
                        position="insideTopRight"
                        value="Threshold"
                        fill="#dc2626"
                        className="text-xs font-bold"
                        offset={10}
                      />
                    </ReferenceLine>
                  </LineChart>
                </ChartContainer>
              </CardContent>
            </Card>

            {/* --- Battery Chart --- */}
            <Card className="shadow-sm border-muted-foreground/10">
              <CardHeader>
                <CardTitle className="text-base font-semibold flex items-center gap-2">
                  <Battery className="h-4 w-4 text-amber-600" />
                  Battery Drain
                </CardTitle>
                <CardDescription>
                  Battery percentage levels over time.
                </CardDescription>
              </CardHeader>
              <CardContent className="pl-0 pr-4 pt-4">
                <ChartContainer
                  config={batteryChartConfig}
                  className="min-h-[200px] w-full"
                >
                  <AreaChart
                    data={history}
                    margin={{ top: 10, right: 10, left: 0, bottom: 0 }}
                  >
                    <CartesianGrid vertical={false} strokeDasharray="3 3" />
                    <XAxis
                      dataKey="createdAt"
                      tickLine={false}
                      axisLine={false}
                      tickMargin={8}
                      tickFormatter={(value) =>
                        new Date(value).toLocaleTimeString([], {
                          hour: "2-digit",
                          minute: "2-digit",
                        })
                      }
                    />
                    <YAxis
                      domain={[0, 100]}
                      tickLine={false}
                      axisLine={false}
                      width={40}
                      tickFormatter={(value) => `${value}%`}
                    />
                    <ChartTooltip
                      content={
                        <ChartTooltipContent
                          indicator="dot"
                          // hideLabel removed so timestamp shows as header
                          labelFormatter={(value) => {
                            return new Date(value).toLocaleString([], {
                              month: "short",
                              day: "numeric",
                              hour: "numeric",
                              minute: "2-digit",
                            });
                          }}
                        />
                      }
                    />
                    <Area
                      dataKey="batteryPercentage"
                      type="step"
                      fill="var(--color-batteryPercentage)"
                      fillOpacity={0.2}
                      stroke="var(--color-batteryPercentage)"
                      strokeWidth={2}
                    />
                  </AreaChart>
                </ChartContainer>
              </CardContent>
            </Card>
          </div>
        )}
      </SheetContent>
    </Sheet>
  );
}
