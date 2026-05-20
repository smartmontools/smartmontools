// agentxd_main.cpp — entry point for smartmon-snmp-agentxd

#include "agentxd_config.h"
#include "agentxd_datasrc.h"
#include "agentxd_loop.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef AGENTXD_SYSCONFDIR
#define AGENTXD_SYSCONFDIR "/etc/smartmontools"
#endif

static const char *default_config_path =
    AGENTXD_SYSCONFDIR "/snmp-agentxd.conf";

static volatile sig_atomic_t g_exit_signal = 0;
static volatile sig_atomic_t g_reload_signal = 0;

static void handle_sigterm(int) { g_exit_signal = 1; }
static void handle_sighup(int)  { g_reload_signal = 1; }

static void install_signals()
{
    struct sigaction sa{};
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);

    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, nullptr);

    // Ignore SIGPIPE — net-snmp may write to a closed AgentX socket
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
}

static void daemonise()
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(EXIT_FAILURE); }
    if (pid > 0) exit(EXIT_SUCCESS);   // parent exits

    if (setsid() < 0) { perror("setsid"); exit(EXIT_FAILURE); }

    // Second fork prevents re-acquiring a controlling terminal
    pid = fork();
    if (pid < 0) { perror("fork"); exit(EXIT_FAILURE); }
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    (void)chdir("/");

    // Close and redirect standard fds
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -c FILE   Config file (default: %s)\n"
        "  -f        Run in foreground (do not daemonise)\n"
        "  -h        Show this help\n",
        prog, default_config_path);
}

int main(int argc, char *argv[])
{
    const char *config_path = default_config_path;
    bool foreground = false;

    int opt;
    while ((opt = getopt(argc, argv, "c:fh")) != -1) {
        switch (opt) {
        case 'c': config_path = optarg; break;
        case 'f': foreground = true;    break;
        case 'h': usage(argv[0]); return EXIT_SUCCESS;
        default:  usage(argv[0]); return EXIT_FAILURE;
        }
    }

    openlog("smartmon-snmp-agentxd", LOG_PID | LOG_CONS, LOG_DAEMON);

    AgentxConfig cfg;
    cfg.foreground = foreground;

    if (!agentxd_config_load(config_path, cfg)) {
        syslog(LOG_ERR, "Configuration error — exiting.");
        return EXIT_FAILURE;
    }

    if (!foreground)
        daemonise();

    install_signals();

    syslog(LOG_INFO, "Starting smartmon-snmp-agentxd, state_dir='%s', "
           "agentx_socket='%s', cache_timeout=%us",
           cfg.state_dir.c_str(), cfg.agentx_socket.c_str(), cfg.cache_timeout);

    // Validate smartd configuration and set up inotify watcher
    if (!agentxd_datasrc_init(cfg.state_dir)) {
        syslog(LOG_ERR, "Data source initialisation failed — exiting.");
        return EXIT_FAILURE;
    }

    // Initialise AgentX connection and register MIB tables
    if (!agentxd_loop_init(cfg)) {
        syslog(LOG_ERR, "AgentX initialisation failed — exiting.");
        agentxd_datasrc_shutdown();
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "AgentX registered, entering main loop.");

    // Main select loop — runs until SIGTERM/SIGINT
    agentxd_loop_run(&g_exit_signal, &g_reload_signal, cfg);

    syslog(LOG_INFO, "Shutting down.");
    agentxd_loop_shutdown();
    agentxd_datasrc_shutdown();
    closelog();
    return EXIT_SUCCESS;
}
