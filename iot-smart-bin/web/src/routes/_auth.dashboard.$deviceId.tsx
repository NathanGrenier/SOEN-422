import { createFileRoute } from "@tanstack/react-router";
import { DeviceDetailSheet } from "@/routes/dashboard/-components/device-detail-sheet";

export const Route = createFileRoute("/_auth/dashboard/$deviceId")({
  component: DeviceDetailSheet,
});
