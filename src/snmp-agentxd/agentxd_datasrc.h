// agentxd_datasrc.h — smartd JSON state file watcher

#pragma once

#include <string>

// Validate state_dir and set up inotify/kqueue watcher.
// Logs and returns false on any configuration error.
bool agentxd_datasrc_init(const std::string &state_dir);

// Called from the main select loop when the inotify fd is readable.
void agentxd_datasrc_handle_events();

// Returns the inotify fd to add to the select set (-1 if not available).
int agentxd_datasrc_fd();

// Periodic staleness check; call every ~60 s.
void agentxd_datasrc_check_staleness(unsigned cache_timeout);

void agentxd_datasrc_shutdown();
