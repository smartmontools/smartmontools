// agentxd_config.h — configuration for smartmon-snmp-agentxd

#pragma once

#include <string>

struct AgentxConfig {
    // Required: directory where smartd writes --jsonstate files
    std::string state_dir;

    // Path to AgentX master socket
    std::string agentx_socket { "/var/agentx/master" };

    // net-snmp cache timeout in seconds (also staleness threshold base)
    unsigned cache_timeout { 300 };

    // Run in foreground instead of daemonising (set by -f flag)
    bool foreground { false };
};

// Parse /etc/smartmontools/snmp-agentxd.conf (or path given on command line).
// Returns false and logs an error if the file cannot be read or a required
// option is missing.
bool agentxd_config_load(const char *path, AgentxConfig &out);
