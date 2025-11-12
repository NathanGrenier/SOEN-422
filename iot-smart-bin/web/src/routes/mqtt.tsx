import { createFileRoute } from "@tanstack/react-router";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { getMqttStatus, publishMqttMessage } from "@/server/mqtt";

export const Route = createFileRoute("/mqtt")({
  component: MqttComponent,
});

function MqttComponent() {
  const queryClient = useQueryClient();

  const { data, error, isLoading } = useQuery({
    queryKey: ["mqttStatus"],
    queryFn: () => getMqttStatus(),
    refetchInterval: 2000, // Poll for updates every 2 seconds
  });

  const mutation = useMutation({
    mutationFn: publishMqttMessage,
    onSuccess: () => {
      // After publishing, invalidate the query to refetch status and messages
      queryClient.invalidateQueries({ queryKey: ["mqttStatus"] });
    },
  });

  const publishTestMessage = () => {
    const testMessage = `Hello from client at ${new Date().toLocaleTimeString()}`;
    mutation.mutate({ data: { topic: "test/topic", message: testMessage } });
  };

  const connectionStatus = data?.status ?? "Loading...";
  const messages = data?.messages ?? [];

  return (
    <div className="p-4">
      <h1 className="text-2xl font-bold">MQTT Connection Test</h1>
      <p className="mt-2">
        Status:{" "}
        <span
          className={`font-semibold ${
            connectionStatus === "Connected" ? "text-green-600" : "text-red-600"
          }`}
        >
          {connectionStatus}
        </span>
      </p>
      <button
        onClick={publishTestMessage}
        disabled={
          isLoading || mutation.isPending || connectionStatus !== "Connected"
        }
        className="mt-4 rounded bg-blue-500 px-4 py-2 font-bold text-white hover:bg-blue-700 disabled:cursor-not-allowed disabled:opacity-50"
      >
        {mutation.isPending ? "Publishing..." : "Publish Test Message"}
      </button>
      <div className="mt-4">
        <h2 className="text-xl font-semibold">Messages</h2>
        {error ? (
          <p className="text-red-500">Error fetching status: {error.message}</p>
        ) : null}
        <ul className="mt-2 list-inside list-disc rounded-md bg-gray-100 p-4 font-mono text-sm">
          {isLoading ? (
            <li>Loading messages...</li>
          ) : (
            messages.map((msg, index) => <li key={index}>{msg}</li>)
          )}
        </ul>
      </div>
    </div>
  );
}
