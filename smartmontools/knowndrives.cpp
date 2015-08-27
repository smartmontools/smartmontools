/*
 * knowndrives.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003-11 Philip Williams, Bruce Allen
 * Copyright (C) 2008-12 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "int64.h"
#include <stdio.h>
#include "atacmds.h"
#include "knowndrives.h"
#include "utility.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h> // access()
#endif

#include <stdexcept>

const char * knowndrives_cpp_cvsid = "$Id$"
                                     KNOWNDRIVES_H_CVSID;

#define MODEL_STRING_LENGTH                         40
#define FIRMWARE_STRING_LENGTH                       8
#define TABLEPRINTWIDTH                             19


// Builtin table of known drives.
// Used as a default if not read from
// "/usr/{,/local}share/smartmontools/drivedb.h"
// or any other file specified by '-B' option,
// see read_default_drive_databases() below.
// The drive_settings structure is described in drivedb.h.
const drive_settings builtin_knowndrives[] = {
#include "drivedb.h"
};


/// Drive database class. Stores custom entries read from file.
/// Provides transparent access to concatenation of custom and
/// default table.
class drive_database
{
public:
  drive_database();

  ~drive_database();

  /// Get total number of entries.
  unsigned size() const
    { return m_custom_tab.size() + m_builtin_size; }

  /// Get number of custom entries.
  unsigned custom_size() const
    { return m_custom_tab.size(); }

  /// Array access.
  const drive_settings & operator[](unsigned i);

  /// Append new custom entry.
  void push_back(const drive_settings & src);

  /// Append builtin table.
  void append(const drive_settings * builtin_tab, unsigned builtin_size)
    { m_builtin_tab = builtin_tab; m_builtin_size = builtin_size; }

private:
  const drive_settings * m_builtin_tab;
  unsigned m_builtin_size;

  std::vector<drive_settings> m_custom_tab;
  std::vector<char *> m_custom_strings;

  const char * copy_string(const char * str);

  drive_database(const drive_database &);
  void operator=(const drive_database &);
};

drive_database::drive_database()
: m_builtin_tab(0), m_builtin_size(0)
{
}

drive_database::~drive_database()
{
  for (unsigned i = 0; i < m_custom_strings.size(); i++)
    delete [] m_custom_strings[i];
}

const drive_settings & drive_database::operator[](unsigned i)
{
  return (i < m_custom_tab.size() ? m_custom_tab[i]
          : m_builtin_tab[i - m_custom_tab.size()] );
}

void drive_database::push_back(const drive_settings & src)
{
  drive_settings dest;
  dest.modelfamily    = copy_string(src.modelfamily);
  dest.modelregexp    = copy_string(src.modelregexp);
  dest.firmwareregexp = copy_string(src.firmwareregexp);
  dest.warningmsg     = copy_string(src.warningmsg);
  dest.presets        = copy_string(src.presets);
  m_custom_tab.push_back(dest);
}

const char * drive_database::copy_string(const char * src)
{
  size_t len = strlen(src);
  char * dest = new char[len+1];
  memcpy(dest, src, len+1);
  try {
    m_custom_strings.push_back(dest);
  }
  catch (...) {
    delete [] dest; throw;
  }
  return dest;
}


/// The drive database.
static drive_database knowndrives;


// Return true if modelfamily string describes entry for USB ID
static bool is_usb_modelfamily(const char * modelfamily)
{
  return !strncmp(modelfamily, "USB:", 4);
}

// Return true if entry for USB ID
static inline bool is_usb_entry(const drive_settings * dbentry)
{
  return is_usb_modelfamily(dbentry->modelfamily);
}

// Compile regular expression, print message on failure.
static bool compile(regular_expression & regex, const char *pattern)
{
  if (!regex.compile(pattern, REG_EXTENDED)) {
    pout("Internal error: unable to compile regular expression \"%s\": %s\n"
         "Please inform smartmontools developers at " PACKAGE_BUGREPORT "\n",
      pattern, regex.get_errmsg());
    return false;
  }
  return true;
}

// Compile & match a regular expression.
static bool match(const char * pattern, const char * str)
{
  regular_expression regex;
  if (!compile(regex, pattern))
    return false;
  return regex.full_match(str);
}

// Searches knowndrives[] for a drive with the given model number and firmware
// string.  If either the drive's model or firmware strings are not set by the
// manufacturer then values of NULL may be used.  Returns the entry of the
// first match in knowndrives[] or 0 if no match if found.
static const drive_settings * lookup_drive(const char * model, const char * firmware)
{
  if (!model)
    model = "";
  if (!firmware)
    firmware = "";

  for (unsigned i = 0; i < knowndrives.size(); i++) {
    // Skip USB entries
    if (is_usb_entry(&knowndrives[i]))
      continue;

    // Check whether model matches the regular expression in knowndrives[i].
    if (!match(knowndrives[i].modelregexp, model))
      continue;

    // Model matches, now check firmware. "" matches always.
    if (!(  !*knowndrives[i].firmwareregexp
          || match(knowndrives[i].firmwareregexp, firmware)))
      continue;

    // Found
    return &knowndrives[i];
  }

  // Not found
  return 0;
}


// Parse drive or USB options in preset string, return false on error.
static bool parse_db_presets(const char * presets, ata_vendor_attr_defs * defs,
                             firmwarebug_defs * firmwarebugs, std::string * type)
{
  for (int i = 0; ; ) {
    i += strspn(presets+i, " \t");
    if (!presets[i])
      break;
    char opt, arg[80+1+13]; int len = -1;
    if (!(sscanf(presets+i, "-%c %80[^ ]%n", &opt, arg, &len) >= 2 && len > 0))
      return false;
    if (opt == 'v' && defs) {
      // Parse "-v N,format[,name]"
      if (!parse_attribute_def(arg, *defs, PRIOR_DATABASE))
        return false;
    }
    else if (opt == 'F' && firmwarebugs) {
      firmwarebug_defs bug;
      if (!parse_firmwarebug_def(arg, bug))
        return false;
      // Don't set if user specified '-F none'.
      if (!firmwarebugs->is_set(BUG_NONE))
        firmwarebugs->set(bug);
    }
    else if (opt == 'd' && type) {
        // TODO: Check valid types
        *type = arg;
    }
    else
      return false;

    i += len;
  }
  return true;
}

// Parse '-v' and '-F' options in preset string, return false on error.
static inline bool parse_presets(const char * presets,
                                 ata_vendor_attr_defs & defs,
                                 firmwarebug_defs & firmwarebugs)
{
  return parse_db_presets(presets, &defs, &firmwarebugs, 0);
}

// Parse '-d' option in preset string, return false on error.
static inline bool parse_usb_type(const char * presets, std::string & type)
{
  return parse_db_presets(presets, 0, 0, &type);
}

// Parse "USB: [DEVICE] ; [BRIDGE]" string
static void parse_usb_names(const char * names, usb_dev_info & info)
{
  int n1 = -1, n2 = -1, n3 = -1;
  sscanf(names, "USB: %n%*[^;]%n; %n", &n1, &n2, &n3);
  if (0 < n1 && n1 < n2)
    info.usb_device.assign(names+n1, n2-n1);
  else
    sscanf(names, "USB: ; %n", &n3);
  if (0 < n3)
    info.usb_bridge = names+n3;
}

// Search drivedb for USB device with vendor:product ID.
int lookup_usb_device(int vendor_id, int product_id, int bcd_device,
                      usb_dev_info & info, usb_dev_info & info2)
{
  // Format strings to match
  char usb_id_str[16], bcd_dev_str[16];
  snprintf(usb_id_str, sizeof(usb_id_str), "0x%04x:0x%04x", vendor_id, product_id);
  if (bcd_device >= 0)
    snprintf(bcd_dev_str, sizeof(bcd_dev_str), "0x%04x", bcd_device);
  else
    bcd_dev_str[0] = 0;

  int found = 0;
  for (unsigned i = 0; i < knowndrives.size(); i++) {
    const drive_settings & dbentry = knowndrives[i];

    // Skip drive entries
    if (!is_usb_entry(&dbentry))
      continue;

    // Check whether USB vendor:product ID matches
    if (!match(dbentry.modelregexp, usb_id_str))
      continue;

    // Parse '-d type'
    usb_dev_info d;
    if (!parse_usb_type(dbentry.presets, d.usb_type))
      return 0; // Syntax error
    parse_usb_names(dbentry.modelfamily, d);

    // If two entries with same vendor:product ID have different
    // types, use bcd_device (if provided by OS) to select entry.
    if (  *dbentry.firmwareregexp && *bcd_dev_str
        && match(dbentry.firmwareregexp, bcd_dev_str)) {
      // Exact match including bcd_device
      info = d; found = 1;
      break;
    }
    else if (!found) {
      // First match without bcd_device
      info = d; found = 1;
    }
    else if (info.usb_type != d.usb_type) {
      // Another possible match with different type
      info2 = d; found = 2;
      break;
    }

    // Stop search at first matching entry with empty bcd_device
    if (!*dbentry.firmwareregexp)
      break;
  }

  return found;
}

// Shows one entry of knowndrives[], returns #errors.
static int showonepreset(const drive_settings * dbentry)
{
  // Basic error check
  if (!(   dbentry
        && dbentry->modelfamily
        && dbentry->modelregexp && *dbentry->modelregexp
        && dbentry->firmwareregexp
        && dbentry->warningmsg
        && dbentry->presets                             )) {
    pout("Invalid drive database entry. Please report\n"
         "this error to smartmontools developers at " PACKAGE_BUGREPORT ".\n");
    return 1;
  }

  bool usb = is_usb_entry(dbentry);

  // print and check model and firmware regular expressions
  int errcnt = 0;
  regular_expression regex;
  pout("%-*s %s\n", TABLEPRINTWIDTH, (!usb ? "MODEL REGEXP:" : "USB Vendor:Product:"),
       dbentry->modelregexp);
  if (!compile(regex, dbentry->modelregexp))
    errcnt++;

  pout("%-*s %s\n", TABLEPRINTWIDTH, (!usb ? "FIRMWARE REGEXP:" : "USB bcdDevice:"),
       *dbentry->firmwareregexp ? dbentry->firmwareregexp : ".*"); // preserve old output (TODO: Change)
  if (*dbentry->firmwareregexp && !compile(regex, dbentry->firmwareregexp))
    errcnt++;

  if (!usb) {
    pout("%-*s %s\n", TABLEPRINTWIDTH, "MODEL FAMILY:", dbentry->modelfamily);

    // if there are any presets, then show them
    firmwarebug_defs firmwarebugs;
    bool first_preset = true;
    if (*dbentry->presets) {
      ata_vendor_attr_defs defs;
      if (!parse_presets(dbentry->presets, defs, firmwarebugs)) {
        pout("Syntax error in preset option string \"%s\"\n", dbentry->presets);
        errcnt++;
      }
      for (int i = 0; i < MAX_ATTRIBUTE_NUM; i++) {
        if (defs[i].priority != PRIOR_DEFAULT) {
          std::string name = ata_get_smart_attr_name(i, defs);
          // Use leading zeros instead of spaces so that everything lines up.
          pout("%-*s %03d %s\n", TABLEPRINTWIDTH, first_preset ? "ATTRIBUTE OPTIONS:" : "",
               i, name.c_str());
          // Check max name length suitable for smartctl -A output
          const unsigned maxlen = 23;
          if (name.size() > maxlen) {
            pout("%*s\n", TABLEPRINTWIDTH+6+maxlen, "Error: Attribute name too long ------^");
            errcnt++;
          }
          first_preset = false;
        }
      }
    }
    if (first_preset)
      pout("%-*s %s\n", TABLEPRINTWIDTH, "ATTRIBUTE OPTIONS:", "None preset; no -v options are required.");

    // describe firmwarefix
    for (int b = BUG_NOLOGDIR; b <= BUG_XERRORLBA; b++) {
      if (!firmwarebugs.is_set((firmwarebug_t)b))
        continue;
      const char * fixdesc;
      switch ((firmwarebug_t)b) {
        case BUG_NOLOGDIR:
          fixdesc = "Avoids reading GP/SMART Log Directories (same as -F nologdir)";
          break;
        case BUG_SAMSUNG:
          fixdesc = "Fixes byte order in some SMART data (same as -F samsung)";
          break;
        case BUG_SAMSUNG2:
          fixdesc = "Fixes byte order in some SMART data (same as -F samsung2)";
          break;
        case BUG_SAMSUNG3:
          fixdesc = "Fixes completed self-test reported as in progress (same as -F samsung3)";
          break;
        case BUG_XERRORLBA:
          fixdesc = "Fixes LBA byte ordering in Ext. Comprehensive SMART error log (same as -F xerrorlba)";
          break;
        default:
          fixdesc = "UNKNOWN"; errcnt++;
          break;
      }
      pout("%-*s %s\n", TABLEPRINTWIDTH, "OTHER PRESETS:", fixdesc);
    }
  }
  else {
    // Print USB info
    usb_dev_info info; parse_usb_names(dbentry->modelfamily, info);
    pout("%-*s %s\n", TABLEPRINTWIDTH, "USB Device:",
      (!info.usb_device.empty() ? info.usb_device.c_str() : "[unknown]"));
    pout("%-*s %s\n", TABLEPRINTWIDTH, "USB Bridge:",
      (!info.usb_bridge.empty() ? info.usb_bridge.c_str() : "[unknown]"));

    if (*dbentry->presets && !parse_usb_type(dbentry->presets, info.usb_type)) {
      pout("Syntax error in USB type string \"%s\"\n", dbentry->presets);
      errcnt++;
    }
    pout("%-*s %s\n", TABLEPRINTWIDTH, "USB Type",
      (!info.usb_type.empty() ? info.usb_type.c_str() : "[unsupported]"));
  }

  // Print any special warnings
  if (*dbentry->warningmsg)
    pout("%-*s %s\n", TABLEPRINTWIDTH, "WARNINGS:", dbentry->warningmsg);
  return errcnt;
}

// Shows all presets for drives in knowndrives[].
// Returns #syntax errors.
int showallpresets()
{
  // loop over all entries in the knowndrives[] table, printing them
  // out in a nice format
  int errcnt = 0;
  for (unsigned i = 0; i < knowndrives.size(); i++) {
    errcnt += showonepreset(&knowndrives[i]);
    pout("\n");
  }

  pout("Total number of entries  :%5u\n"
       "Entries read from file(s):%5u\n\n",
    knowndrives.size(), knowndrives.custom_size());

  pout("For information about adding a drive to the database see the FAQ on the\n");
  pout("smartmontools home page: " PACKAGE_HOMEPAGE "\n");

  if (errcnt > 0)
    pout("\nFound %d syntax error(s) in database.\n"
         "Please inform smartmontools developers at " PACKAGE_BUGREPORT "\n", errcnt);
  return errcnt;
}

// Shows all matching presets for a drive in knowndrives[].
// Returns # matching entries.
int showmatchingpresets(const char *model, const char *firmware)
{
  int cnt = 0;
  const char * firmwaremsg = (firmware ? firmware : "(any)");

  for (unsigned i = 0; i < knowndrives.size(); i++) {
    if (!match(knowndrives[i].modelregexp, model))
      continue;
    if (   firmware && *knowndrives[i].firmwareregexp
        && !match(knowndrives[i].firmwareregexp, firmware))
        continue;
    // Found
    if (++cnt == 1)
      pout("Drive found in smartmontools Database.  Drive identity strings:\n"
           "%-*s %s\n"
           "%-*s %s\n"
           "match smartmontools Drive Database entry:\n",
           TABLEPRINTWIDTH, "MODEL:", model, TABLEPRINTWIDTH, "FIRMWARE:", firmwaremsg);
    else if (cnt == 2)
      pout("and match these additional entries:\n");
    showonepreset(&knowndrives[i]);
    pout("\n");
  }
  if (cnt == 0)
    pout("No presets are defined for this drive.  Its identity strings:\n"
         "MODEL:    %s\n"
         "FIRMWARE: %s\n"
         "do not match any of the known regular expressions.\n",
         model, firmwaremsg);
  return cnt;
}

// Shows the presets (if any) that are available for the given drive.
void show_presets(const ata_identify_device * drive)
{
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];

  // get the drive's model/firmware strings
  ata_format_id_string(model, drive->model, sizeof(model)-1);
  ata_format_id_string(firmware, drive->fw_rev, sizeof(firmware)-1);

  // and search to see if they match values in the table
  const drive_settings * dbentry = lookup_drive(model, firmware);
  if (!dbentry) {
    // no matches found
    pout("No presets are defined for this drive.  Its identity strings:\n"
         "MODEL:    %s\n"
         "FIRMWARE: %s\n"
         "do not match any of the known regular expressions.\n"
         "Use -P showall to list all known regular expressions.\n",
         model, firmware);
    return;
  }
  
  // We found a matching drive.  Print out all information about it.
  pout("Drive found in smartmontools Database.  Drive identity strings:\n"
       "%-*s %s\n"
       "%-*s %s\n"
       "match smartmontools Drive Database entry:\n",
       TABLEPRINTWIDTH, "MODEL:", model, TABLEPRINTWIDTH, "FIRMWARE:", firmware);
  showonepreset(dbentry);
}

// Searches drive database and sets preset vendor attribute
// options in defs and firmwarebugs.
// Values that have already been set will not be changed.
// Returns pointer to database entry or nullptr if none found
const drive_settings * lookup_drive_apply_presets(
  const ata_identify_device * drive, ata_vendor_attr_defs & defs,
  firmwarebug_defs & firmwarebugs)
{
  // get the drive's model/firmware strings
  char model[MODEL_STRING_LENGTH+1], firmware[FIRMWARE_STRING_LENGTH+1];
  ata_format_id_string(model, drive->model, sizeof(model)-1);
  ata_format_id_string(firmware, drive->fw_rev, sizeof(firmware)-1);

  // Look up the drive in knowndrives[].
  const drive_settings * dbentry = lookup_drive(model, firmware);
  if (!dbentry)
    return 0;

  if (*dbentry->presets) {
    // Apply presets
    if (!parse_presets(dbentry->presets, defs, firmwarebugs))
      pout("Syntax error in preset option string \"%s\"\n", dbentry->presets);
  }
  return dbentry;
}


/////////////////////////////////////////////////////////////////////////////
// Parser for drive database files

// Abstract pointer to read file input.
// Operations supported: c = *p; c = p[1]; ++p;
class stdin_iterator
{
public:
  explicit stdin_iterator(FILE * f)
    : m_f(f) { get(); get(); }

  stdin_iterator & operator++()
    { get(); return *this; }

  char operator*() const
    { return m_c; }

  char operator[](int i) const
    {
      if (i != 1)
        fail();
      return m_next;
    }

private:
  FILE * m_f;
  char m_c, m_next;
  void get();
  void fail() const;
};

void stdin_iterator::get()
{
  m_c = m_next;
  int ch = getc(m_f);
  m_next = (ch != EOF ? ch : 0);
}

void stdin_iterator::fail() const
{
  throw std::runtime_error("stdin_iterator: wrong usage");
}


// Use above as parser input 'pointer'. Can easily be changed later
// to e.g. 'const char *' if above is too slow.
typedef stdin_iterator parse_ptr;

// Skip whitespace and comments.
static parse_ptr skip_white(parse_ptr src, const char * path, int & line)
{
  for ( ; ; ++src) switch (*src) {
    case ' ': case '\t':
      continue;

    case '\n':
      ++line;
      continue;

    case '/':
      switch (src[1]) {
        case '/':
          // skip '// comment'
          ++src; ++src;
          while (*src && *src != '\n')
            ++src;
          if (*src)
            ++line;
          break;
        case '*':
          // skip '/* comment */'
          ++src; ++src;
          for (;;) {
            if (!*src) {
              pout("%s(%d): Missing '*/'\n", path, line);
              return src;
            }
            char c = *src; ++src;
            if (c == '\n')
              ++line;
            else if (c == '*' && *src == '/')
              break;
          }
          break;
        default:
          return src;
      }
      continue;

    default:
      return src;
  }
}

// Info about a token.
struct token_info
{
  char type;
  int line;
  std::string value;

  token_info() : type(0), line(0) { }
};

// Get next token.
static parse_ptr get_token(parse_ptr src, token_info & token, const char * path, int & line)
{
  src = skip_white(src, path, line);
  switch (*src) {
    case '{': case '}': case ',':
      // Simple token
      token.type = *src; token.line = line;
      ++src;
      break;

    case '"':
      // String constant
      token.type = '"'; token.line = line;
      token.value = "";
      do {
        for (++src; *src != '"'; ++src) {
          char c = *src;
          if (!c || c == '\n' || (c == '\\' && !src[1])) {
            pout("%s(%d): Missing terminating '\"'\n", path, line);
            token.type = '?'; token.line = line;
            return src;
          }
          if (c == '\\') {
            c = *++src;
            switch (c) {
              case 'n' : c = '\n'; break;
              case '\n': ++line; break;
              case '\\': case '"': break;
              default:
                pout("%s(%d): Unknown escape sequence '\\%c'\n", path, line, c);
                token.type = '?'; token.line = line;
                continue;
            }
          }
          token.value += c;
        }
        // Lookahead to detect string constant concatentation
        src = skip_white(++src, path, line);
      } while (*src == '"');
      break;

    case 0:
      // EOF
      token.type = 0; token.line = line;
      break;

    default:
      pout("%s(%d): Syntax error, invalid char '%c'\n", path, line, *src);
      token.type = '?'; token.line = line;
      while (*src && *src != '\n')
        ++src;
      break;
  }

  return src;
}

// Parse drive database from abstract input pointer.
static bool parse_drive_database(parse_ptr src, drive_database & db, const char * path)
{
  int state = 0, field = 0;
  std::string values[5];
  bool ok = true;

  token_info token; int line = 1;
  src = get_token(src, token, path, line);
  for (;;) {
    // EOF is ok after '}', trailing ',' is also allowed.
    if (!token.type && (state == 0 || state == 4))
      break;

    // Check expected token
    const char expect[] = "{\",},";
    if (token.type != expect[state]) {
      if (token.type != '?')
        pout("%s(%d): Syntax error, '%c' expected\n", path, token.line, expect[state]);
      ok = false;
      // Skip to next entry
      while (token.type && token.type != '{')
        src = get_token(src, token, path, line);
      state = 0;
      if (token.type)
        continue;
      break;
    }

    // Interpret parser state
    switch (state) {
      case 0: // ... ^{...}
        state = 1; field = 0;
        break;
      case 1: // {... ^"..." ...}
        switch (field) {
          case 1: case 2:
            if (!token.value.empty()) {
              regular_expression regex;
              if (!regex.compile(token.value.c_str(), REG_EXTENDED)) {
                pout("%s(%d): Error in regular expression: %s\n", path, token.line, regex.get_errmsg());
                ok = false;
              }
            }
            else if (field == 1) {
              pout("%s(%d): Missing regular expression for drive model\n", path, token.line);
              ok = false;
            }
            break;
          case 4:
            if (!token.value.empty()) {
              if (!is_usb_modelfamily(values[0].c_str())) {
                ata_vendor_attr_defs defs; firmwarebug_defs fix;
                if (!parse_presets(token.value.c_str(), defs, fix)) {
                  pout("%s(%d): Syntax error in preset option string\n", path, token.line);
                  ok = false;
                }
              }
              else {
                std::string type;
                if (!parse_usb_type(token.value.c_str(), type)) {
                  pout("%s(%d): Syntax error in USB type string\n", path, token.line);
                  ok = false;
                }
              }
            }
            break;
        }
        values[field] = token.value;
        state = (++field < 5 ? 2 : 3);
        break;
      case 2: // {... "..."^, ...}
        state = 1;
        break;
      case 3: // {...^}, ...
        {
          drive_settings entry;
          entry.modelfamily    = values[0].c_str();
          entry.modelregexp    = values[1].c_str();
          entry.firmwareregexp = values[2].c_str();
          entry.warningmsg     = values[3].c_str();
          entry.presets        = values[4].c_str();
          db.push_back(entry);
        }
        state = 4;
        break;
      case 4: // {...}^, ...
        state = 0;
        break;
      default:
        pout("Bad state %d\n", state);
        return false;
    }
    src = get_token(src, token, path, line);
  }
  return ok;
}

// Read drive database from file.
bool read_drive_database(const char * path)
{
  stdio_file f(path, "r"
#ifdef __CYGWIN__ // Allow files with '\r\n'.
                      "t"
#endif
                         );
  if (!f) {
    pout("%s: cannot open drive database file\n", path);
    return false;
  }

  return parse_drive_database(parse_ptr(f), knowndrives, path);
}

// Get path for additional database file
const char * get_drivedb_path_add()
{
#ifndef _WIN32
  return SMARTMONTOOLS_SYSCONFDIR"/smart_drivedb.h";
#else
  static std::string path = get_exe_dir() + "/drivedb-add.h";
  return path.c_str();
#endif
}

#ifdef SMARTMONTOOLS_DRIVEDBDIR

// Get path for default database file
const char * get_drivedb_path_default()
{
#ifndef _WIN32
  return SMARTMONTOOLS_DRIVEDBDIR"/drivedb.h";
#else
  static std::string path = get_exe_dir() + "/drivedb.h";
  return path.c_str();
#endif
}

#endif

// Read drive databases from standard places.
bool read_default_drive_databases()
{
  // Read file for local additions: /{,usr/local/}etc/smart_drivedb.h
  const char * db1 = get_drivedb_path_add();
  if (!access(db1, 0)) {
    if (!read_drive_database(db1))
      return false;
  }

#ifdef SMARTMONTOOLS_DRIVEDBDIR
  // Read file from package: /usr/{,local/}share/smartmontools/drivedb.h
  const char * db2 = get_drivedb_path_default();
  if (!access(db2, 0)) {
    if (!read_drive_database(db2))
      return false;
  }
  else
#endif
  {
    // Append builtin table.
    knowndrives.append(builtin_knowndrives,
      sizeof(builtin_knowndrives)/sizeof(builtin_knowndrives[0]));
  }

  return true;
}
