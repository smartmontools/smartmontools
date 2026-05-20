// agentxd_loop.h — AgentX session lifecycle and main select loop

#pragma once

#include "agentxd_config.h"

#include <csignal>

// Initialise net-snmp as AgentX subagent and register all MIB table handlers.
// Returns false on failure.
bool agentxd_loop_init(const AgentxConfig &cfg);

// Run the select loop until *exit_flag becomes non-zero.
// Handles AgentX keepalive, inotify events, and periodic staleness checks.
void agentxd_loop_run(volatile sig_atomic_t *exit_flag,
                      volatile sig_atomic_t *reload_flag,
                      const AgentxConfig &cfg);

void agentxd_loop_shutdown();
