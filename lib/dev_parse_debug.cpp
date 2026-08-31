/*
 * dev_parse_debug.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2007 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "dev_ata_cmd_set.h"

namespace smartmon {

const char * const commandstrings[] = {
  "SMART ENABLE",
  "SMART DISABLE",
  "SMART AUTOMATIC ATTRIBUTE SAVE",
  "SMART IMMEDIATE OFFLINE",
  "SMART AUTO OFFLINE",
  "SMART STATUS",
  "SMART STATUS CHECK",
  "SMART READ ATTRIBUTE VALUES",
  "SMART READ ATTRIBUTE THRESHOLDS",
  "SMART READ LOG",
  "IDENTIFY DEVICE",
  "IDENTIFY PACKET DEVICE",
  "CHECK POWER MODE",
  "SMART WRITE LOG",
  "WARNING (UNDEFINED COMMAND -- CONTACT DEVELOPERS AT " PACKAGE_BUGREPORT ")\n"
};

/////////////////////////////////////////////////////////////////////////////
// Pseudo-device to parse "smartctl -r ataioctl,2 ..." output and simulate
// an ATA device with same behaviour

namespace {

class parsed_ata_device
: public /*implements*/ ata_device_with_command_set
{
public:
  parsed_ata_device(smart_interface * intf, const char * dev_name);

  virtual ~parsed_ata_device();

  virtual bool is_open() const override;

  virtual bool open() override;

  virtual bool close() override;

  virtual bool ata_identify_is_cached() const override;

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data) override;

private:
  // Table of parsed commands, return value, data
  struct parsed_ata_command
  {
    smart_command_set command;
    int select;
    int retval, errval;
    char * data;
  };

  enum { max_num_commands = 32 };
  parsed_ata_command m_command_table[max_num_commands];

  int m_num_commands;
  int m_next_replay_command;
  bool m_replay_out_of_sync;
  bool m_ata_identify_is_cached;
};

static const char * nextline(const char * s, int & lineno)
{
  for (s += strcspn(s, "\r\n"); *s == '\r' || *s == '\n'; s++) {
    if (*s == '\r' && s[1] == '\n')
      s++;
    lineno++;
  }
  return s;
}

static int name2command(const char * s)
{
  for (int i = 0; i < (int)(sizeof(commandstrings)/sizeof(commandstrings[0])); i++) {
    if (!strcmp(s, commandstrings[i]))
      return i;
  }
  return -1;
}

static bool matchcpy(char * dest, size_t size, const char * src,
  const regular_expression::match_range & srcmatch)
{
  if (srcmatch.rm_so < 0)
    return false;
  size_t n = srcmatch.rm_eo - srcmatch.rm_so;
  if (n >= size)
    n = size-1;
  memcpy(dest, src + srcmatch.rm_so, n);
  dest[n] = 0;
  return true;
}

static inline int matchtoi(const char * src, const regular_expression::match_range & srcmatch, int defval)
{
  if (srcmatch.rm_so < 0)
    return defval;
  return atoi(src + srcmatch.rm_so);
}

parsed_ata_device::parsed_ata_device(smart_interface * intf, const char * dev_name)
: smart_device(intf, dev_name, "ata", ""),
  m_num_commands(0),
  m_next_replay_command(0),
  m_replay_out_of_sync(false),
  m_ata_identify_is_cached(false)
{
  memset(m_command_table, 0, sizeof(m_command_table));
}

parsed_ata_device::~parsed_ata_device()
{
  parsed_ata_device::close();
}

bool parsed_ata_device::is_open() const
{
  return (m_num_commands > 0);
}

// Parse stdin and build command table
bool parsed_ata_device::open()
{
  const char * pathname = get_dev_name();
  if (strcmp(pathname, "-"))
    return set_err(EINVAL);
  pathname = "<stdin>";
  // Fill buffer
  char buffer[64*1024];
  int size = 0;
  while (size < (int)sizeof(buffer)) {
    int nr = fread(buffer, 1, sizeof(buffer), stdin);
    if (nr <= 0)
      break;
    size += nr;
  }
  if (size <= 0)
    return set_err(ENOENT, "%s: Unexpected EOF", pathname);
  if (size >= (int)sizeof(buffer))
    return set_err(EIO, "%s: Buffer overflow", pathname);
  buffer[size] = 0;

  // Regex to match output from "-r ataioctl,2"
  static const char pattern[] = "^"
  "(" // (1
    "REPORT-IOCTL: DeviceF?D?=[^ ]+ Command=([A-Z ]*[A-Z])" // (2)
    "(" // (3
      "( InputParameter=([0-9]+))?" // (4 (5))
    "|"
      "( returned (-?[0-9]+)( errno=([0-9]+)[^\r\n]*)?)" // (6 (7) (8 (9)))
    ")" // )
    "[\r\n]" // EOL match necessary to match optional parts above
  "|"
    "===== \\[([A-Z ]*[A-Z])\\] DATA START " // (10)
  "|"
    "    *(En|Dis)abled status cached by OS, " // (11)
  ")"; // )

  // Compile regex
  const regular_expression regex(pattern);

  // Parse buffer
  const char * errmsg = 0;
  int i = -1, state = 0, lineno = 1;
  for (const char * line = buffer; *line; line = nextline(line, lineno)) {
    // Match line
    if (!(line[0] == 'R' || line[0] == '=' || line[0] == ' '))
      continue;
    const int nmatch = 1+11;
    regular_expression::match_range match[nmatch];
    if (!regex.execute(line, match))
      continue;

    char cmdname[40];
    if (matchcpy(cmdname, sizeof(cmdname), line, match[2])) { // "REPORT-IOCTL:... Command=%s ..."
      int nc = name2command(cmdname);
      if (nc < 0) {
        errmsg = "Unknown ATA command name"; break;
      }
      if (match[7].rm_so < 0) { // "returned %d"
        // Start of command
        if (!(state == 0 || state == 2)) {
          errmsg = "Missing REPORT-IOCTL result"; break;
        }
        if (++i >= max_num_commands) {
          errmsg = "Too many ATA commands"; break;
        }
        m_command_table[i].command = (smart_command_set)nc;
        m_command_table[i].select = matchtoi(line, match[5], 0); // "InputParameter=%d"
        state = 1;
      }
      else {
        // End of command
        if (!(state == 1 && (int)m_command_table[i].command == nc)) {
          errmsg = "Missing REPORT-IOCTL start"; break;
        }
        m_command_table[i].retval = matchtoi(line, match[7], -1); // "returned %d"
        m_command_table[i].errval = matchtoi(line, match[9], 0); // "errno=%d"
        state = 2;
      }
    }
    else if (matchcpy(cmdname, sizeof(cmdname), line, match[10])) { // "===== [%s] DATA START "
      // Start of sector hexdump
      int nc = name2command(cmdname);
      if (!(state == (nc == WRITE_LOG ? 1 : 2) && (int)m_command_table[i].command == nc)) {
          errmsg = "Unexpected DATA START"; break;
      }
      line = nextline(line, lineno);
      char * data = (char *)malloc(512);
      unsigned j;
      for (j = 0; j < 32; j++) {
        unsigned b[16];
        unsigned u1, u2; int n1 = -1;
        if (!(sscanf(line, "%3u-%3u: "
                        "%2x %2x %2x %2x %2x %2x %2x %2x "
                        "%2x %2x %2x %2x %2x %2x %2x %2x%n",
                     &u1, &u2,
                     b+ 0, b+ 1, b+ 2, b+ 3, b+ 4, b+ 5, b+ 6, b+ 7,
                     b+ 8, b+ 9, b+10, b+11, b+12, b+13, b+14, b+15, &n1) == 18
              && n1 >= 56 && u1 == j*16 && u2 == j*16+15))
          break;
        for (unsigned k = 0; k < 16; k++)
          data[j*16+k] = b[k];
        line = nextline(line, lineno);
      }
      if (j < 32) {
        free(data);
        errmsg = "Incomplete sector hex dump"; break;
      }
      m_command_table[i].data = data;
      if (nc != WRITE_LOG)
        state = 0;
    }
    else if (match[11].rm_so > 0) { // "(En|Dis)abled status cached by OS"
      m_ata_identify_is_cached = true;
    }
  }

  if (!(state == 0 || state == 2))
    errmsg = "Missing REPORT-IOCTL result";

  if (!errmsg && i < 0)
    errmsg = "No information found";

  m_num_commands = i+1;
  m_next_replay_command = 0;
  m_replay_out_of_sync = false;

  if (errmsg) {
    close();
    return set_err(EIO, "%s(%d): Syntax error: %s", pathname, lineno, errmsg);
  }
  return true;
}

// Report warnings and free command table 
bool parsed_ata_device::close()
{
  if (m_replay_out_of_sync)
      lib_printf("REPLAY-IOCTL: Warning: commands replayed out of sync\n");
  else if (m_next_replay_command != 0)
      lib_printf("REPLAY-IOCTL: Warning: %d command(s) not replayed\n", m_num_commands-m_next_replay_command);

  for (int i = 0; i < m_num_commands; i++) {
    if (m_command_table[i].data) {
      free(m_command_table[i].data); m_command_table[i].data = 0;
    }
  }
  m_num_commands = 0;
  m_next_replay_command = 0;
  m_replay_out_of_sync = false;
  return true;
}


bool parsed_ata_device::ata_identify_is_cached() const
{
  return m_ata_identify_is_cached;
}


// Simulate ATA command from command table
int parsed_ata_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  // Find command, try round-robin if out of sync
  int i = m_next_replay_command;
  for (int j = 0; ; j++) {
    if (j >= m_num_commands) {
      lib_printf("REPLAY-IOCTL: Warning: Command not found\n");
      errno = ENOSYS;
      return -1;
    }
    if (m_command_table[i].command == command && m_command_table[i].select == select)
      break;
    if (!m_replay_out_of_sync) {
      m_replay_out_of_sync = true;
      lib_printf("REPLAY-IOCTL: Warning: Command #%d is out of sync\n", i+1);
    }
    if (++i >= m_num_commands)
      i = 0;
  }
  m_next_replay_command = i;
  if (++m_next_replay_command >= m_num_commands)
    m_next_replay_command = 0;

  // Return command data
  switch (command) {
    case IDENTIFY:
    case PIDENTIFY:
    case READ_VALUES:
    case READ_THRESHOLDS:
    case READ_LOG:
      if (m_command_table[i].data)
        memcpy(data, m_command_table[i].data, 512);
      break;
    case WRITE_LOG:
      if (!(m_command_table[i].data && !memcmp(data, m_command_table[i].data, 512)))
        lib_printf("REPLAY-IOCTL: Warning: WRITE LOG data does not match\n");
      break;
    case CHECK_POWER_MODE:
      data[0] = (char)0xff;
    default:
      break;
  }

  if (m_command_table[i].errval)
    errno = m_command_table[i].errval;
  return m_command_table[i].retval;
}

} // namespace

ata_device * get_parsed_ata_device(smart_interface * intf, const char * dev_name)
{
  return new parsed_ata_device(intf, dev_name);
}

} // namespace smartmon
