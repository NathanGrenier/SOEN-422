import { createFileRoute } from "@tanstack/react-router";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { toast } from "sonner";
import { createColumns } from "./devices/-components/columns";
import { DataTable } from "./devices/-components/data-table";
import { updateDeviceSettings } from "@/server/mqtt";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { clearDeviceReadings, getAllDevices } from "@/server/devices";

export const Route = createFileRoute("/_auth/devices")({
  component: DevicesPage,
});

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
