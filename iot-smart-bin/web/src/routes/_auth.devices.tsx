import { createFileRoute } from "@tanstack/react-router";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { toast } from "sonner";
import { Loader2, Save } from "lucide-react";
import { useEffect, useState } from "react";
import { createColumns } from "./devices/-components/columns";
import { DataTable } from "./devices/-components/data-table";
import {
  clearDeviceReadings,
  getAllDevices,
  updateDeviceSettings,
} from "@/server/devices";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Label } from "@/components/ui/label";
import { Input } from "@/components/ui/input";
import { Button } from "@/components/ui/button";
import { getSystemSettings, updateSystemSettings } from "@/server/settings";

export const Route = createFileRoute("/_auth/devices")({
  component: DevicesPage,
});

function TimeoutSettings() {
  const queryClient = useQueryClient();
  const [stdSeconds, setStdSeconds] = useState<string>("30");
  const [tiltedSeconds, setTiltedSeconds] = useState<string>("300");

  const { data: settings, isLoading } = useQuery({
    queryKey: ["system-settings"],
    queryFn: () => getSystemSettings(),
  });

  // Sync local state when data arrives
  useEffect(() => {
    if (settings) {
      setStdSeconds((settings.standardTimeout / 1000).toString());
      setTiltedSeconds((settings.tiltedTimeout / 1000).toString());
    }
  }, [settings]);

  const mutation = useMutation({
    mutationFn: (data: { standardTimeout: number; tiltedTimeout: number }) =>
      updateSystemSettings({ data }),
    onSuccess: () => {
      toast.success("Timeout settings updated");
      queryClient.invalidateQueries({ queryKey: ["system-settings"] });
      queryClient.invalidateQueries({ queryKey: ["dashboard-data"] }); // Refresh dashboard status immediately
    },
    onError: () => toast.error("Failed to update settings"),
  });

  const handleSave = () => {
    const std = parseInt(stdSeconds) * 1000;
    const tilted = parseInt(tiltedSeconds) * 1000;

    if (isNaN(std) || isNaN(tilted)) {
      toast.error("Please enter valid numbers");
      return;
    }

    mutation.mutate({ standardTimeout: std, tiltedTimeout: tilted });
  };

  if (isLoading) return null;

  return (
    <Card className="mb-6">
      <CardHeader>
        <CardTitle className="text-lg font-medium">
          Connectivity Thresholds
        </CardTitle>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6 items-center">
          <div className="space-y-2">
            <Label htmlFor="std-timeout">Standard Timeout (Seconds)</Label>
            <Input
              id="std-timeout"
              type="number"
              value={stdSeconds}
              onChange={(e) => setStdSeconds(e.target.value)}
              placeholder="30"
            />
            <p className="text-xs text-slate-500">
              Time before an upright bin is marked offline.
            </p>
          </div>
          <div className="space-y-2">
            <Label htmlFor="tilted-timeout">Tilted Timeout (Seconds)</Label>
            <Input
              id="tilted-timeout"
              type="number"
              value={tiltedSeconds}
              onChange={(e) => setTiltedSeconds(e.target.value)}
              placeholder="300"
            />
            <p className="text-xs text-slate-500">
              Extended time allowed for bins being emptied.
            </p>
          </div>
          <Button
            onClick={handleSave}
            disabled={mutation.isPending}
            className="bg-blue-600 hover:bg-blue-700 text-white"
          >
            {mutation.isPending ? (
              <Loader2 className="mr-2 h-4 w-4 animate-spin" />
            ) : (
              <Save className="mr-2 h-4 w-4" />
            )}
            Save Settings
          </Button>
        </div>
      </CardContent>
    </Card>
  );
}

function DevicesPage() {
  const queryClient = useQueryClient();

  // Use the existing dashboard query to get list or create a specific one for all devices including non-deployed
  const { data: devices = [], isLoading } = useQuery({
    queryKey: ["all-devices"],
    queryFn: () => getAllDevices(),
  });

  const updateMutation = useMutation({
    mutationFn: (data: any) => updateDeviceSettings({ data }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ["all-devices"] });
      queryClient.invalidateQueries({ queryKey: ["dashboard-data"] });
      toast.success("Device updated successfully");
    },
    onError: () => toast.error("Failed to update device"),
  });

  const clearReadingsMutation = useMutation({
    mutationFn: (id: string) => clearDeviceReadings({ data: { id } }),
    onSuccess: () => {
      toast.success("Readings cleared successfully");
    },
    onError: () => toast.error("Failed to clear readings"),
  });

  const handleUpdate = (id: string, data: any) => {
    updateMutation.mutate({ id, ...data });
  };

  const handleClearReadings = (id: string) => {
    clearReadingsMutation.mutate(id);
  };

  const columns = createColumns(handleUpdate, handleClearReadings);

  if (isLoading) {
    return (
      <div className="p-10 text-center flex flex-col items-center">
        <div className="animate-spin h-8 w-8 border-4 border-emerald-500 border-t-transparent rounded-full mb-4" />
        <span className="text-slate-500">Loading device inventory...</span>
      </div>
    );
  }

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-8">
        <h1 className="text-3xl font-bold text-slate-900 dark:text-white">
          Device Inventory
        </h1>
        <p className="text-slate-500 mt-2">
          Manage Smart Bin deployment status, locations, and sensor thresholds.
          Click on Location or Threshold cells to edit them.
        </p>
      </div>

      <TimeoutSettings />

      <Card>
        <CardHeader>
          <CardTitle>Fleet Overview</CardTitle>
        </CardHeader>
        <CardContent>
          <DataTable columns={columns} data={devices} />
        </CardContent>
      </Card>
    </div>
  );
}
