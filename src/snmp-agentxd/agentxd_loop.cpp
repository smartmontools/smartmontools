// agentxd_loop.cpp — AgentX subagent init, MIB registration, select loop

#include "agentxd_loop.h"
#include "agentxd_config.h"
#include "agentxd_cache.h"
#include "agentxd_datasrc.h"

#include "snmp_common_mib.h"
#include "snmp_nvme_mib.h"
#include "snmp_sata_mib.h"
#include "snmp_sas_mib.h"
#include "snmp_sensor_mib.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <syslog.h>
#include <unistd.h>
#include <sys/select.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

// ---------------------------------------------------------------------------
// AgentX init
// ---------------------------------------------------------------------------

bool agentxd_loop_init(const AgentxConfig &cfg) {
    // Tell net-snmp we are a subagent, not a master agent
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID,
                           NETSNMP_DS_AGENT_ROLE, 1 /* subagent */);

    // Set AgentX socket path
    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
                          NETSNMP_DS_AGENT_X_SOCKET,
                          cfg.agentx_socket.c_str());

    // Suppress net-snmp's own logging (we use syslog directly)
    snmp_disable_log();

    if (init_agent("smartmon-snmp-agentxd") != 0) {
        syslog(LOG_ERR, "init_agent failed — cannot connect to snmpd AgentX socket %s",
               cfg.agentx_socket.c_str());
        return false;
    }

    // Register all MIB table handlers
    register_common_mib();
    register_nvme_mib();
    register_sata_mib();
    register_sas_mib();
    register_sensor_mib();

    init_snmp("smartmon-snmp-agentxd");

    syslog(LOG_INFO, "AgentX subagent registered on %s",
           cfg.agentx_socket.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Select loop
// ---------------------------------------------------------------------------

void agentxd_loop_run(volatile sig_atomic_t *exit_flag,
                      volatile sig_atomic_t *reload_flag,
                      const AgentxConfig &cfg) {
    time_t last_staleness = time(nullptr);

    while (!*exit_flag) {
        if (*reload_flag) {
            *reload_flag = 0;
            syslog(LOG_INFO, "SIGHUP — rescanning %s", cfg.state_dir.c_str());
            // Re-scan: existing devices stay; new/updated files are re-parsed.
            // agentxd_datasrc_handle_events picks up inotify, but on SIGHUP we
            // do a full directory sweep to catch any files written while we were
            // not watching.
            agentxd_datasrc_shutdown();
            agentxd_datasrc_init(cfg.state_dir);
        }

        fd_set fdset;
        FD_ZERO(&fdset);
        int maxfd = -1;

        // Add inotify fd if available
        int ifd = agentxd_datasrc_fd();
        if (ifd >= 0) {
            FD_SET(ifd, &fdset);
            maxfd = std::max(maxfd, ifd);
        }

        // Let net-snmp add its own fds (AgentX socket, etc.)
        struct timeval timeout;
        timeout.tv_sec  = 1;   // check signals at least every second
        timeout.tv_usec = 0;
        int block = 0;
        snmp_select_info(&maxfd, &fdset, &timeout, &block);

        int n = select(maxfd + 1, &fdset, nullptr, nullptr,
                       block ? nullptr : &timeout);
        if (n < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "select: %s", strerror(errno));
            break;
        }

        if (n > 0) {
            // Handle inotify events (new/updated JSON state files)
            if (ifd >= 0 && FD_ISSET(ifd, &fdset)) {
                FD_CLR(ifd, &fdset);
                agentxd_datasrc_handle_events();
            }
            // Let net-snmp process AgentX traffic (GET, GETNEXT, keepalive)
            snmp_read(&fdset);
        }

        // Drive net-snmp timers (retransmits, keepalive ping to master)
        snmp_timeout();
        run_alarms();
        netsnmp_check_outstanding_agent_requests();

        // Periodic staleness check (every ~60 s)
        time_t now = time(nullptr);
        if (now - last_staleness >= 60) {
            agentxd_datasrc_check_staleness(cfg.cache_timeout);
            last_staleness = now;
        }
    }
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void agentxd_loop_shutdown() {
    snmp_shutdown("smartmon-snmp-agentxd");
}
