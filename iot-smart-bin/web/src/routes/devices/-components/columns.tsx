"use client";

import { ArrowUpDown, MoreHorizontal, Trash2 } from "lucide-react";
import { useState } from "react";
import { toast } from "sonner";
import type { ColumnDef } from "@tanstack/react-table";
import { Button } from "@/components/ui/button";
import { Checkbox } from "@/components/ui/checkbox";
import { Switch } from "@/components/ui/switch";
import { Input } from "@/components/ui/input";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuLabel,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from "@/components/ui/alert-dialog";

export type Device = {
  id: string;
  name: string | null;
  location: string | null;
  threshold: number;
  deployed: boolean | null;
  lastSeen: Date | null;
  status: string | null;
};

const EditableCell = ({
  value,
  onSave,
  type = "text",
}: {
  value: string | number;
  onSave: (val: string | number) => void;
  type?: "text" | "number";
}) => {
  const [isEditing, setIsEditing] = useState(false);
  const [currentValue, setCurrentValue] = useState(value);

  const handleBlur = () => {
    setIsEditing(false);
    if (currentValue !== value) {
      onSave(currentValue);
    }
  };

  if (isEditing) {
    return (
      <Input
        autoFocus
        type={type}
        value={currentValue}
        onChange={(e) =>
          setCurrentValue(
            type === "number" ? Number(e.target.value) : e.target.value,
          )
        }
        onBlur={handleBlur}
        onKeyDown={(e) => {
          if (e.key === "Enter") handleBlur();
        }}
        className="h-8 w-full"
      />
    );
  }

  return (
    <div
      onClick={() => setIsEditing(true)}
      className="cursor-pointer hover:bg-slate-100 dark:hover:bg-slate-800 p-1 rounded border border-transparent hover:border-slate-200 min-h-6"
    >
      {value}
    </div>
  );
};

const ActionCell = ({
  device,
  onClear,
}: {
  device: Device;
  onClear: (id: string) => void;
}) => {
  const [open, setOpen] = useState(false);

  return (
    <>
      <DropdownMenu>
        <DropdownMenuTrigger asChild>
          <Button variant="ghost" className="h-8 w-8 p-0">
            <span className="sr-only">Open menu</span>
            <MoreHorizontal className="h-4 w-4" />
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="end">
          <DropdownMenuLabel>Actions</DropdownMenuLabel>
          <DropdownMenuItem
            onClick={() => {
              navigator.clipboard.writeText(device.id);
              toast.success("Device ID copied");
            }}
          >
            Copy Device ID
          </DropdownMenuItem>
          <DropdownMenuSeparator />
          <DropdownMenuItem
            className="text-red-600 focus:text-red-600"
            onClick={() => setOpen(true)}
          >
            <Trash2 className="mr-2 h-4 w-4" />
            Clear Readings History
          </DropdownMenuItem>
        </DropdownMenuContent>
      </DropdownMenu>

      <AlertDialog open={open} onOpenChange={setOpen}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Are you absolutely sure?</AlertDialogTitle>
            <AlertDialogDescription>
              This will permanently delete all historical sensor readings for
              device <strong>{device.id}</strong>. This action cannot be undone.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction
              className="bg-red-600 hover:bg-red-700"
              onClick={() => {
                onClear(device.id);
                setOpen(false);
              }}
            >
              Delete Readings
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>
    </>
  );
};

export const createColumns = (
  onUpdate: (id: string, data: Partial<Device>) => void,
  onClearReadings: (id: string) => void,
): ColumnDef<Device>[] => [
  {
    id: "select",
    header: ({ table }) => (
      <Checkbox
        checked={
          table.getIsAllPageRowsSelected() ||
          (table.getIsSomePageRowsSelected() && "indeterminate")
        }
        onCheckedChange={(value) => table.toggleAllPageRowsSelected(!!value)}
        aria-label="Select all"
      />
    ),
    cell: ({ row }) => (
      <Checkbox
        checked={row.getIsSelected()}
        onCheckedChange={(value) => row.toggleSelected(!!value)}
        aria-label="Select row"
      />
    ),
    enableSorting: false,
    enableHiding: false,
  },
  {
    accessorKey: "id",
    header: "Device ID",
    cell: ({ row }) => (
      <div className="font-mono font-medium">{row.getValue("id")}</div>
    ),
  },
  {
    accessorKey: "location",
    header: ({ column }) => {
      return (
        <Button
          variant="ghost"
          onClick={() => column.toggleSorting(column.getIsSorted() === "asc")}
        >
          Location
          <ArrowUpDown className="ml-2 h-4 w-4" />
        </Button>
      );
    },
    cell: ({ row }) => (
      <EditableCell
        value={row.getValue("location") || "Unknown"}
        onSave={(val) => onUpdate(row.original.id, { location: String(val) })}
      />
    ),
  },
  {
    accessorKey: "threshold",
    header: "Threshold (%)",
    cell: ({ row }) => (
      <div className="w-20">
        <EditableCell
          type="number"
          value={row.getValue("threshold")}
          onSave={(val) => {
            const num = Number(val);

            if (isNaN(num) || num < 0 || num > 100) {
              toast.error("Threshold must be between 0% and 100%");
              return;
            }

            onUpdate(row.original.id, { threshold: num });
          }}
        />
      </div>
    ),
  },
  {
    accessorKey: "deployed",
    header: "Deployed",
    cell: ({ row }) => (
      <Switch
        checked={row.getValue("deployed")}
        onCheckedChange={(checked) =>
          onUpdate(row.original.id, { deployed: checked })
        }
      />
    ),
  },
  {
    accessorKey: "lastSeen",
    header: "Last Seen",
    cell: ({ row }) => {
      const date = row.getValue("lastSeen");
      if (!date) return <div className="text-slate-400">Never</div>;
      return (
        <div className="text-slate-600">
          {new Date(date as string | number | Date).toLocaleString()}
        </div>
      );
    },
  },
  {
    id: "actions",
    cell: ({ row }) => (
      <ActionCell device={row.original} onClear={onClearReadings} />
    ),
  },
];
