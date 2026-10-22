/*
 * dev_scsi_trace.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2026 smartmontools contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <smartmon/dev_interface.h>
#include <smartmon/scsicmds.h>
#include <smartmon/utility.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace smartmon {

/////////////////////////////////////////////////////////////////////////////
// Pseudo-device to replay a recorded SCSI command trace and simulate a
// SCSI device (or a USB bridge behind which SAT or NVMe tunnels are used)
// with the same behaviour.
//
// Trace format (one block per command, '#' starts a comment line):
//
//   CDB 4d 00 72 00 00 00 00 00 fc 00
//   ALLOC 252
//   STATUS 0             (SCSI status, or "-1 ERROR n" if pass-through failed)
//   SENSE                (sense data if status != 0)
//   DATA b2 00 00 2d ... (data-in returned by the device)
//   END
//
// Replay rules:
// - The allocation length field of well known commands is ignored when
//   matching a CDB. A recorded response is truncated to the requested
//   length or returned with a residual count.
// - An exact match (including allocation length) is preferred, otherwise
//   the response recorded with the largest allocation length is used.
// - Unrecorded CDBs fail with ILLEGAL REQUEST: INVALID FIELD IN CDB if the
//   opcode was recorded at least once, INVALID COMMAND OPERATION CODE else.
// - Data-out commands are never accepted.

namespace {

class trace_scsi_device
: public /*implements*/ scsi_device
{
public:
  trace_scsi_device(smart_interface * intf, const char * dev_name,
                    const char * trace_text);

  virtual bool is_open() const override;

  virtual bool open() override;

  virtual bool close() override;

  virtual bool scsi_pass_through(scsi_cmnd_io * iop) override;

private:
  struct trace_cmd
  {
    std::vector<uint8_t> cdb;
    unsigned alloc = 0;
    int status = 0;
    int error = 0;
    std::vector<uint8_t> sense;
    std::vector<uint8_t> data;
  };

  bool parse(const std::string & text, const char * src);

  bool set_check_condition(scsi_cmnd_io * iop, uint8_t asc);

  bool m_has_text;
  std::string m_text;
  std::vector<trace_cmd> m_cmds;
  bool m_open;
};

trace_scsi_device::trace_scsi_device(smart_interface * intf, const char * dev_name,
                                     const char * trace_text)
: smart_device(intf, dev_name, "scsi", "scsi"),
  m_has_text(!!trace_text),
  m_text(trace_text ? trace_text : ""),
  m_open(false)
{
  set_info().info_name = strprintf("%s [SCSI trace]", dev_name);
}

bool trace_scsi_device::is_open() const
{
  return m_open;
}

bool trace_scsi_device::close()
{
  m_open = false;
  return true;
}

// Read whole file or stdin
static bool read_text(FILE * f, std::string & text)
{
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    text.append(buf, n);
  return !ferror(f);
}

bool trace_scsi_device::open()
{
  m_cmds.clear();
  const char * src = get_dev_name();
  std::string text;
  if (m_has_text) {
    text = m_text;
    src = "<text>";
  }
  else if (!strcmp(src, "-")) {
    src = "<stdin>";
    if (!read_text(stdin, text))
      return set_err(EIO, "%s: read error", src);
  }
  else {
    FILE * f = fopen(src, "rb");
    if (!f)
      return set_err(ENOENT, "%s: cannot open trace file", src);
    bool ok = read_text(f, text);
    fclose(f);
    if (!ok)
      return set_err(EIO, "%s: read error", src);
  }
  if (!parse(text, src))
    return false;
  m_open = true;
  return true;
}

// Parse hex bytes "xx xx ...", return false on syntax error
static bool parse_hex(const char * s, std::vector<uint8_t> & out)
{
  out.clear();
  for (;;) {
    while (*s == ' ' || *s == '\t')
      s++;
    if (!*s)
      return true;
    char * end;
    unsigned long v = strtoul(s, &end, 16);
    if (end - s != 2 || !(!*end || *end == ' ' || *end == '\t'))
      return false;
    out.push_back((uint8_t)v);
    s = end;
  }
}

// Parse decimal number without sign and trailing characters
static bool parse_uint(const char * s, unsigned & v)
{
  if (!('0' <= *s && *s <= '9'))
    return false;
  char * end;
  errno = 0;
  unsigned long x = strtoul(s, &end, 10);
  if (*end || errno || x > 0xffffffffUL)
    return false;
  v = (unsigned)x;
  return true;
}

// Parse "STATUS" argument: SCSI status 0-255 or "-1 ERROR n"
static bool parse_status(const char * s, int & status, int & error)
{
  unsigned v;
  if (!strncmp(s, "-1 ERROR ", 9)) {
    if (!parse_uint(s + 9, v) || v > 0x7fffffff)
      return false;
    status = -1;
    error = (int)v;
    return true;
  }
  if (!parse_uint(s, v) || v > 0xff)
    return false;
  status = (int)v;
  error = 0;
  return true;
}

bool trace_scsi_device::parse(const std::string & text, const char * src)
{
  trace_cmd cmd;
  bool in_cmd = false, has_alloc = false, has_status = false;
  int lineno = 0;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string::npos)
      eol = text.size();
    std::string line = text.substr(pos, eol - pos);
    pos = eol + 1;
    lineno++;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;

    size_t sp = line.find(' ');
    std::string key = line.substr(0, sp);
    const char * arg = (sp != std::string::npos ? line.c_str() + sp + 1 : "");
    bool ok = true;
    if (key == "CDB") {
      cmd = trace_cmd();
      ok = (!in_cmd && parse_hex(arg, cmd.cdb) && !cmd.cdb.empty());
      in_cmd = true;
      has_alloc = has_status = false;
    }
    else if (key == "ALLOC")
      ok = has_alloc = (in_cmd && parse_uint(arg, cmd.alloc));
    else if (key == "STATUS")
      ok = has_status = (in_cmd && parse_status(arg, cmd.status, cmd.error));
    else if (key == "SENSE")
      ok = (in_cmd && parse_hex(arg, cmd.sense));
    else if (key == "DATA")
      ok = (in_cmd && parse_hex(arg, cmd.data));
    else if (key == "END") {
      if (in_cmd && !(has_alloc && has_status))
        return set_err(EINVAL, "%s(%d): ALLOC or STATUS missing", src, lineno);
      ok = in_cmd;
      if (ok)
        m_cmds.push_back(cmd);
      in_cmd = false;
    }
    else
      ok = false;
    if (!ok)
      return set_err(EINVAL, "%s(%d): syntax error: %s", src, lineno, line.c_str());
  }
  if (in_cmd)
    return set_err(EINVAL, "%s(%d): missing END", src, lineno);
  if (m_cmds.empty())
    return set_err(EINVAL, "%s: no commands found", src);
  return true;
}

// Location of the allocation length field of commands which return
// a (truncated) response depending on this length
static bool get_alloc_field(const uint8_t * cdb, unsigned cdb_len,
                            unsigned & offset, unsigned & size)
{
  switch (cdb[0]) {
    case REQUEST_SENSE:    offset = 4; size = 1; break;
    case INQUIRY:          offset = 3; size = 2; break;
    case MODE_SENSE_6:     offset = 4; size = 1; break;
    case MODE_SENSE_10:    offset = 7; size = 2; break;
    case LOG_SENSE:        offset = 7; size = 2; break;
    case RECEIVE_DIAGNOSTIC: offset = 3; size = 2; break;
    case READ_DEFECT_10:   offset = 7; size = 2; break;
    case READ_DEFECT_12:   offset = 6; size = 4; break;
    case REPORT_LUNS:      offset = 6; size = 4; break;
    case MAINTENANCE_IN_12: offset = 6; size = 4; break;
    case SERVICE_ACTION_IN_16: offset = 10; size = 4; break;
    default:
      return false;
  }
  return (offset + size <= cdb_len);
}

static bool same_cdb(const std::vector<uint8_t> & rec, const uint8_t * cdb,
                     unsigned cdb_len, bool & exact)
{
  if (rec.size() != cdb_len)
    return false;
  unsigned offset = 0, size = 0;
  if (!get_alloc_field(cdb, cdb_len, offset, size))
    size = 0;
  exact = true;
  for (unsigned i = 0; i < cdb_len; i++) {
    if (rec[i] == cdb[i])
      continue;
    if (!(offset <= i && i < offset + size))
      return false;
    exact = false;
  }
  return true;
}

bool trace_scsi_device::set_check_condition(scsi_cmnd_io * iop, uint8_t asc)
{
  // Fixed format sense: ILLEGAL REQUEST, ASC, ASCQ=0
  uint8_t sense[18] = {0x70, 0, SCSI_SK_ILLEGAL_REQUEST, 0, 0, 0, 0, 10, };
  sense[12] = asc;
  size_t n = (iop->max_sense_len < sizeof(sense) ? iop->max_sense_len : sizeof(sense));
  if (iop->sensep && n)
    memcpy(iop->sensep, sense, n);
  iop->resp_sense_len = n;
  iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
  iop->resid = (int)iop->dxfer_len;
  return true;
}

bool trace_scsi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  if (!m_open)
    return set_err(EBADF);
  if (!iop->cmnd || !iop->cmnd_len)
    return set_err(EINVAL, "empty CDB");

  if (scsi_debugmode > 0) {
    std::string s;
    for (unsigned i = 0; i < iop->cmnd_len; i++)
      s += strprintf(" %02x", iop->cmnd[i]);
    lib_printf(" [SCSI trace:%s]\n", s.c_str());
  }

  iop->resp_sense_len = 0;
  iop->scsi_status = 0;
  iop->resid = 0;

  const uint8_t opcode = iop->cmnd[0];
  if (iop->dxfer_dir == DXFER_TO_DEVICE)
    // Replay never accepts data-out commands
    return set_check_condition(iop, 0x20 /* INVALID COMMAND OPERATION CODE */);

  const trace_cmd * best = nullptr;
  bool opcode_seen = false;
  for (const trace_cmd & cmd : m_cmds) {
    if (cmd.cdb[0] == opcode)
      opcode_seen = true;
    bool exact = false;
    if (!same_cdb(cmd.cdb, iop->cmnd, iop->cmnd_len, exact))
      continue;
    if (exact && cmd.alloc == iop->dxfer_len) {
      best = &cmd;
      break;
    }
    if (!best || cmd.alloc > best->alloc)
      best = &cmd;
  }

  if (!best)
    return set_check_condition(iop, (opcode_seen ? 0x24 /* INVALID FIELD IN CDB */
                                                 : 0x20 /* INVALID COMMAND OPERATION CODE */));

  if (best->status < 0)
    return set_err(EIO, "SCSI trace: pass-through failed (recorded error %d)", best->error);

  iop->scsi_status = (uint8_t)best->status;
  if (best->status) {
    size_t n = best->sense.size();
    if (n > iop->max_sense_len)
      n = iop->max_sense_len;
    if (iop->sensep && n)
      memcpy(iop->sensep, best->sense.data(), n);
    iop->resp_sense_len = n;
    iop->resid = (int)iop->dxfer_len;
    return true;
  }

  if (iop->dxfer_dir == DXFER_FROM_DEVICE && iop->dxferp) {
    size_t n = best->data.size();
    if (n > iop->dxfer_len)
      n = iop->dxfer_len;
    memcpy(iop->dxferp, best->data.data(), n);
    iop->resid = (int)(iop->dxfer_len - n);
  }
  return true;
}

} // namespace

scsi_device * get_trace_scsi_device(smart_interface * intf, const char * dev_name,
                                    const char * trace_text /* = nullptr */)
{
  return new trace_scsi_device(intf, dev_name, trace_text);
}

} // namespace smartmon
