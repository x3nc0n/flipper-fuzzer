#pragma once

// Starts a UART REPL console exposing an "ffcfg" command so tunables
// (rotation interval, jitter, per-radio enable flags, beacon count, etc.)
// can be inspected, changed live, and persisted to NVS in the field --
// without reflashing. Spawns its own task internally; call once from
// app_main() after fluckflock_config_load().
void console_cfg_start(void);
