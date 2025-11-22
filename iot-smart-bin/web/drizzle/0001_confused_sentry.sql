ALTER TABLE `devices` ADD `battery_percentage` real DEFAULT 100;--> statement-breakpoint
ALTER TABLE `devices` ADD `voltage` real DEFAULT 5;--> statement-breakpoint
ALTER TABLE `devices` ADD `is_tilted` integer DEFAULT false;--> statement-breakpoint
ALTER TABLE `readings` ADD `voltage` real DEFAULT 0;--> statement-breakpoint
ALTER TABLE `readings` ADD `is_tilted` integer NOT NULL;