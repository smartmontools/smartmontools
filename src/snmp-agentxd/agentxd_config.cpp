// agentxd_config.cpp — configuration file parser

#include "agentxd_config.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <syslog.h>

int g_verbosity = 0;

bool agentxd_config_load(const char *path, AgentxConfig &out)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        syslog(LOG_ERR, "Cannot open config file '%s': %s", path, strerror(errno));
        return false;
    }

    char line[1024];
    int lineno = 0;
    bool ok = true;

    while (fgets(line, sizeof(line), f)) {
        ++lineno;

        // Strip trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        // Skip blank lines and comments
        const char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '#')
            continue;

        // Parse "key value"
        char key[64], value[960];
        if (sscanf(p, "%63s %959[^\n]", key, value) < 2) {
            syslog(LOG_WARNING, "%s:%d: ignoring malformed line", path, lineno);
            continue;
        }

        if (strcmp(key, "state_dir") == 0) {
            out.state_dir = value;
        } else if (strcmp(key, "agentx_socket") == 0) {
            out.agentx_socket = value;
        } else if (strcmp(key, "cache_timeout") == 0) {
            char *end;
            long v = strtol(value, &end, 10);
            if (*end != '\0' || v <= 0) {
                syslog(LOG_ERR, "%s:%d: cache_timeout must be a positive integer",
                       path, lineno);
                ok = false;
            } else {
                out.cache_timeout = static_cast<unsigned>(v);
            }
        } else {
            syslog(LOG_WARNING, "%s:%d: unknown option '%s'", path, lineno, key);
        }
    }

    fclose(f);

    if (out.state_dir.empty()) {
        syslog(LOG_ERR,
               "%s: 'state_dir' is required but not set. "
               "Configure smartd with '--jsonstate <dir>' and set "
               "'state_dir <dir>' in %s.",
               path, path);
        ok = false;
    }

    return ok;
}
