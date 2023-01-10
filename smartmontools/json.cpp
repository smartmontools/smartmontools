/*
 * json.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2017-22 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include "json.h"

const char * json_cvsid = "$Id$"
  JSON_H_CVSID;

#include "sg_unaligned.h"
#include "utility.h" // regular_expression, uint128_*()

#include <inttypes.h>
#include <stdexcept>

static void jassert_failed(int line, const char * expr)
{
  char msg[128];
  // Avoid __FILE__ as it may break reproducible builds
  snprintf(msg, sizeof(msg), "json.cpp(%d): Assertion failed: %s", line, expr);
  throw std::logic_error(msg);
}

#define jassert(expr) (!(expr) ? jassert_failed(__LINE__, #expr) : (void)0)

std::string json::str2key(const char * str)
{
  std::string key = str;
  for (char & c : key) {
    if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || c == '_')
      continue;
    if ('A' <= c && c <= 'Z')
      c += 'a' - 'A';
    else
      c = '_';
  }
  return key;
}

json::ref::ref(json & js)
: m_js(js)
{
}

json::ref::ref(json & js, const char * keystr)
: m_js(js)
{
  jassert(keystr && *keystr);
  m_path.push_back(node_info(keystr));
}

json::ref::ref(const ref & base, const char * keystr)
: m_js(base.m_js), m_path(base.m_path)
{
  jassert(keystr && *keystr);
  m_path.push_back(node_info(keystr));
}

json::ref::ref(const ref & base, int index)
: m_js(base.m_js), m_path(base.m_path)
{
  jassert(0 <= index && index < 10000); // Limit: large arrays not supported
  m_path.push_back(node_info(index));
}

json::ref::ref(const ref & base, const char * /*dummy*/, const char * key_suffix)
: m_js(base.m_js), m_path(base.m_path)
{
  int n = (int)m_path.size(), i;
  for (i = n; --i >= 0; ) {
    std::string & base_key = m_path[i].key;
    if (base_key.empty())
      continue; // skip array
    base_key += key_suffix;
    break;
  }
  jassert(i >= 0); // Limit: top level element must be an object
}

json::ref::~ref()
{
}

void json::ref::operator=(bool value)
{
  m_js.set_bool(m_path, value);
}

void json::ref::operator=(long long value)
{
  m_js.set_int64(m_path, (int64_t)value);
}

void json::ref::operator=(unsigned long long value)
{
  m_js.set_uint64(m_path, (uint64_t)value);
}

void json::ref::operator=(int value)
{
  operator=((long long)value);
}

void json::ref::operator=(unsigned value)
{
  operator=((unsigned long long)value);
}

void json::ref::operator=(long value)
{
  operator=((long long)value);
}

void json::ref::operator=(unsigned long value)
{
  operator=((unsigned long long)value);
}

void json::ref::operator=(const char * value)
{
  m_js.set_cstring(m_path, value);
}

void json::ref::operator=(const std::string & value)
{
  m_js.set_string(m_path, value);
}

void json::ref::set_uint128(uint64_t value_hi, uint64_t value_lo)
{
  if (!value_hi)
    operator=((unsigned long long)value_lo);
  else
    m_js.set_uint128(m_path, value_hi, value_lo);
}

bool json::ref::set_if_safe_uint64(uint64_t value)
{
  if (!is_safe_uint(value))
    return false;
  operator=((unsigned long long)value);
  return true;
}

bool json::ref::set_if_safe_uint128(uint64_t value_hi, uint64_t value_lo)
{
  if (value_hi)
    return false;
  return set_if_safe_uint64(value_lo);
}

bool json::ref::set_if_safe_le128(const void * pvalue)
{
  return set_if_safe_uint128(sg_get_unaligned_le64((const uint8_t *)pvalue + 8),
                             sg_get_unaligned_le64(                 pvalue    ));
}

void json::ref::set_unsafe_uint64(uint64_t value)
{
  // Output as number "KEY"
  operator=((unsigned long long)value);
  if (!m_js.m_verbose && is_safe_uint(value))
    return;
  // Output as string "KEY_s"
  char s[32];
  snprintf(s, sizeof(s), "%" PRIu64, value);
  with_suffix("_s") = s;
}

void json::ref::set_unsafe_uint128(uint64_t value_hi, uint64_t value_lo)
{
  if (!m_js.m_verbose && !value_hi)
    set_unsafe_uint64(value_lo);
  else {
    // Output as number "KEY", string "KEY_s" and LE byte array "KEY_le[]"
    m_js.m_uint128_output = true;
    set_uint128(value_hi, value_lo);
    char s[64];
    with_suffix("_s") = uint128_hilo_to_str(s, value_hi, value_lo);
    ref le = with_suffix("_le");
    for (int i = 0; i < 8; i++) {
      uint64_t v = (value_lo >> (i << 3));
      if (!v && !value_hi)
        break;
      le[i] = v & 0xff;
    }
    for (int i = 0; i < 8; i++) {
      uint64_t v = value_hi >> (i << 3);
      if (!v)
        break;
      le[8 + i] = v & 0xff;
    }
  }
}

void json::ref::set_unsafe_le128(const void * pvalue)
{
  set_unsafe_uint128(sg_get_unaligned_le64((const uint8_t *)pvalue + 8),
                     sg_get_unaligned_le64(                 pvalue    ));
}

void json::ref::operator+=(std::initializer_list<initlist_key_value_pair> ilist)
{
  for (const initlist_key_value_pair & kv : ilist) {
    jassert(kv.keystr && *kv.keystr);
    switch (kv.value.type) {
      default: operator[](kv.keystr) = kv.value; break;
      case nt_object: operator[](kv.keystr) += kv.object; break;
      case nt_array: operator[](kv.keystr) += kv.array; break;
    }
  }
}

void json::ref::operator+=(std::initializer_list<initlist_value> ilist)
{
  int i = 0;
  for (const initlist_value & v : ilist)
    operator[](i++) = v;
}

json::node::node()
{
}

json::node::node(const std::string & key_)
: key(key_)
{
}

json::node::~node()
{
}

json::node::const_iterator::const_iterator(const json::node * node_p, bool sorted)
: m_node_p(node_p),
  m_use_map(sorted && node_p->type == nt_object)
{
  if (m_use_map)
    m_key_iter = node_p->key2index.begin();
}

bool json::node::const_iterator::at_end() const
{
  if (m_use_map)
    return (m_key_iter == m_node_p->key2index.end());
  else
    return (m_child_idx >= m_node_p->childs.size());
}

unsigned json::node::const_iterator::array_index() const
{
  jassert(m_node_p->type == nt_array);
  return m_child_idx;
}

void json::node::const_iterator::operator++()
{
  if (m_use_map)
    ++m_key_iter;
  else
    ++m_child_idx;
}

const json::node * json::node::const_iterator::operator*() const
{
  if (m_use_map)
    return m_node_p->childs[m_key_iter->second].get();
  else
    return m_node_p->childs[m_child_idx].get();
}

json::node * json::find_or_create_node(const json::node_path & path, node_type type)
{
  node * p = &m_root_node;
  for (unsigned i = 0; i < path.size(); i++) {
    const node_info & pi = path[i];
    if (!pi.key.empty()) {
      // Object
      if (p->type == nt_unset)
        p->type = nt_object;
      else
        jassert(p->type == nt_object); // Limit: type change not supported
      // Existing or new object element?
      node::keymap::iterator ni = p->key2index.find(pi.key);
      node * p2;
      if (ni != p->key2index.end()) {
        // Object element exists
        p2 = p->childs[ni->second].get();
      }
      else {
        // Create new object element
        p->key2index[pi.key] = (unsigned)p->childs.size();
        p->childs.push_back(std::unique_ptr<node>(p2 = new node(pi.key)));
      }
      jassert(p2 && p2->key == pi.key);
      p = p2;
    }

    else {
      // Array
      if (p->type == nt_unset)
        p->type = nt_array;
      else
        jassert(p->type == nt_array); // Limit: type change not supported
      node * p2;
      // Existing or new array element?
      if (pi.index < (int)p->childs.size()) {
        // Array index exists
        p2 = p->childs[pi.index].get();
        if (!p2) // Already created ?
          p->childs[pi.index].reset(p2 = new node);
      }
      else {
        // Grow array, fill gap, create new element
        p->childs.resize(pi.index + 1);
        p->childs[pi.index].reset(p2 = new node);
      }
      jassert(p2 && p2->key.empty());
      p = p2;
    }
  }

  if (   p->type == nt_unset
      || (   nt_int <= p->type && p->type <= nt_uint128
          && nt_int <=    type &&    type <= nt_uint128))
    p->type = type;
  else
    jassert(p->type == type); // Limit: type change not supported
  return p;
}

void json::set_bool(const node_path & path, bool value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_bool)->intval = (value ? 1 : 0);
}

void json::set_int64(const node_path & path, int64_t value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_int)->intval = (uint64_t)value;
}

void json::set_uint64(const node_path & path, uint64_t value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_uint)->intval = value;
}

void json::set_uint128(const node_path & path, uint64_t value_hi, uint64_t value_lo)
{
  if (!m_enabled)
    return;
  node * p = find_or_create_node(path, nt_uint128);
  p->intval_hi = value_hi;
  p->intval = value_lo;
}

void json::set_cstring(const node_path & path, const char * value)
{
  if (!m_enabled)
    return;
  jassert(value != nullptr); // Limit: nullptr not supported
  find_or_create_node(path, nt_string)->strval = value;
}

void json::set_string(const node_path & path, const std::string & value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_string)->strval = value;
}

void json::set_initlist_value(const node_path & path, const initlist_value & val)
{
  if (!m_enabled)
    return;
  node * p = find_or_create_node(path, val.type);
  switch (p->type) {
    case nt_bool: case nt_int: case nt_uint: p->intval = val.intval; break;
    case nt_string: p->strval = val.strval; break;
    default: jassert(false);
  }
}

// Return -1 if all UTF-8 sequences are valid, else return index of first invalid char
static int check_utf8(const char * s)
{
  int state = 0, i;
  for (i = 0; s[i]; i++) {
    unsigned char c = s[i];
    // 0xb... (C++14) not used to preserve C++11 compatibility
    if ((c & 0xc0) == 0x80) {                    // 0xb10xxxxx
      if (--state < 0)
        return i;
    }
    else {
      if (state != 0)
        return i;
      if (!(c & 0x80))                           // 0xb0xxxxxxx
        ;
      else if ((c & 0xe0) == 0xc0 && (c & 0x1f)) // 0xb110xxxxx
        state = 1;
      else if ((c & 0xf0) == 0xe0 && (c & 0x0f)) // 0xb1110xxxx
        state = 2;
      else if ((c & 0xf8) == 0xf0 && (c & 0x07)) // 0xb11110xxx
        state = 3;
      else
        return i;
    }
  }
  if (state != 0)
    return i;
  return -1;
}

static void print_quoted_string(FILE * f, const char * s)
{
  int utf8_rc = -2;
  putc('"', f);
  for (int i = 0; s[i]; i++) {
    char c = s[i];
    if (c == '"' || c == '\\')
      putc('\\', f);
    else if (c == '\t') {
      putc('\\', f); c = 't';
    }
    // Print as UTF-8 unless the string contains any invalid sequences
    // "\uXXXX" is not used because it is not valid for YAML
    if (   (' ' <= c && c <= '~')
        || ((c & 0x80) && (utf8_rc >= -1 ? utf8_rc : (utf8_rc = check_utf8(s + i))) == -1))
      putc(c, f);
    else
      // Print informal hex string for unexpected chars:
      // Control chars (except TAB), DEL(0x7f), bit 7 set and no valid UTF-8
      fprintf(f, "\\\\x%02x", (unsigned char)c);
  }
  putc('"', f);
}

static char yaml_string_needs_quotes(const char * s)
{
  if (!*s)
    return '\''; // empty string
  bool need = false; char quotes = '\'';
  for (int i = 0; s[i]; i++) {
    char c = s[i];
    if (!(' ' <= c && c <= '~'))
      return '"'; // non-ascii char
    if (   ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z') || c == '_'              )
      continue;
    if (c == '\'') // if string contains any single quote
      quotes = '"'; // use double quotes
    if (i == 0)
      need = true; // space or special char except '_' at BOL
    else if (c == ' ' && (s[i-1] == ':' || s[i+1] == '#'))
      need = true; // ": " or " #"
    else if ((c == ' ' || c == ':') && !s[i+1])
      need = true; // space or : at EOL
  }
  if (need)
    return quotes;

  static const regular_expression special(
    "[0-9]+[,0-9]*(\\.[0-9]*)?([eE][-+]?[0-9]+)?|" // decimal ('^[-+.]' handled above)
    "0x[0-7A-Fa-f]+|" // hex
    "[Ff][Aa][Ll][Ss][Ee]|[Tt][Rr][Uu][Ee]|[Nn][Oo]|[Yy][Ee][Ss]|" // boolean
    "[Nn][Uu][Ll][Ll]" // null
  );
  if (special.full_match(s))
    return quotes; // special token
  return 0; // none of the above
}

void json::print_json(FILE * f, bool pretty, bool sorted, const node * p, int level)
{
  bool is_obj = (p->type == nt_object);
  switch (p->type) {
    case nt_object:
    case nt_array:
      putc((is_obj ? '{' : '['), f);
      if (!p->childs.empty()) {
        bool first = true;
        for (node::const_iterator it(p, sorted); !it.at_end(); ++it) {
          if (!first)
            putc(',', f);
          if (pretty)
            fprintf(f, "\n%*s", (level + 1) * 2, "");
          const node * p2 = *it;
          if (!p2) {
            // Unset element of sparse array
            jassert(!is_obj);
            fputs("null", f);
          }
          else {
            jassert(is_obj == !p2->key.empty());
            if (is_obj)
              fprintf(f, "\"%s\":%s", p2->key.c_str(), (pretty ? " " : ""));
            // Recurse
            print_json(f, pretty, sorted, p2, level + 1);
          }
          first = false;
        }
        if (pretty)
          fprintf(f, "\n%*s", level * 2, "");
      }
      putc((is_obj ? '}' : ']'), f);
      break;

    case nt_bool:
      fputs((p->intval ? "true" : "false"), f);
      break;

    case nt_int:
      fprintf(f, "%" PRId64, (int64_t)p->intval);
      break;

    case nt_uint:
      fprintf(f, "%" PRIu64, p->intval);
      break;

    case nt_uint128:
      {
        char buf[64];
        fputs(uint128_hilo_to_str(buf, p->intval_hi, p->intval), f);
      }
      break;

    case nt_string:
      print_quoted_string(f, p->strval.c_str());
      break;

    default: jassert(false);
  }
}

void json::print_yaml(FILE * f, bool pretty, bool sorted, const node * p, int level_o,
                      int level_a, bool cont)
{
  bool is_obj = (p->type == nt_object);
  switch (p->type) {
    case nt_object:
    case nt_array:
      if (!p->childs.empty()) {
        if (!cont)
          fputs("\n", f);
        for (node::const_iterator it(p, sorted); !it.at_end(); ++it) {
          int spaces = (cont ? 1 : (is_obj ? level_o : level_a) * 2);
          if (spaces > 0)
            fprintf(f, "%*s", spaces, "");
          const node * p2 = *it;
          if (!p2) {
            // Unset element of sparse array
            jassert(!is_obj);
            fputs("-" /*" null"*/ "\n", f);
          }
          else {
            jassert(is_obj == !p2->key.empty());
            if (is_obj)
              fprintf(f, "%s:", p2->key.c_str());
            else
              putc('-', f);
            // Recurse
            print_yaml(f, pretty, sorted, p2, (is_obj ? level_o : level_a) + 1,
                       (is_obj ? level_o + (pretty ? 1 : 0) : level_a + 1), !is_obj);
          }
          cont = false;
        }
      }
      else {
        fputs((is_obj ? "{}\n" : "[]\n"), f);
      }
      break;

    case nt_bool:
      fputs((p->intval ? " true\n" : " false\n"), f);
      break;

    case nt_int:
      fprintf(f, " %" PRId64 "\n", (int64_t)p->intval);
      break;

    case nt_uint:
      fprintf(f, " %" PRIu64 "\n", p->intval);
      break;

    case nt_uint128:
      {
        char buf[64];
        fprintf(f, " %s\n", uint128_hilo_to_str(buf, p->intval_hi, p->intval));
      }
      break;

    case nt_string:
      putc(' ', f);
      switch (yaml_string_needs_quotes(p->strval.c_str())) {
        default:   print_quoted_string(f, p->strval.c_str()); break;
        case '\'': fprintf(f, "'%s'", p->strval.c_str()); break;
        case 0:    fputs(p->strval.c_str(), f); break;
      }
      putc('\n', f);
      break;

    default: jassert(false);
  }
}

void json::print_flat(FILE * f, const char * assign, bool sorted, const node * p,
                      std::string & path)
{
  bool is_obj = (p->type == nt_object);
  switch (p->type) {
    case nt_object:
    case nt_array:
      fprintf(f, "%s%s%s;\n", path.c_str(), assign, (is_obj ? "{}" : "[]"));
      if (!p->childs.empty()) {
        unsigned len = path.size();
        for (node::const_iterator it(p, sorted); !it.at_end(); ++it) {
          const node * p2 = *it;
          if (!is_obj) {
            char buf[10]; snprintf(buf, sizeof(buf), "[%u]", it.array_index());
            path += buf;
          }
          else {
            path += '.'; path += p2->key;
          }
          if (!p2) {
            // Unset element of sparse array
            jassert(!is_obj);
            fprintf(f, "%s%snull;\n", path.c_str(), assign);
          }
          else {
            // Recurse
            print_flat(f, assign, sorted, p2, path);
          }
          path.erase(len);
        }
      }
      break;

    case nt_bool:
      fprintf(f, "%s%s%s;\n", path.c_str(), assign, (p->intval ? "true" : "false"));
      break;

    case nt_int:
      fprintf(f, "%s%s%" PRId64 ";\n", path.c_str(), assign, (int64_t)p->intval);
      break;

    case nt_uint:
      fprintf(f, "%s%s%" PRIu64 ";\n", path.c_str(), assign, p->intval);
      break;

    case nt_uint128:
      {
        char buf[64];
        fprintf(f, "%s%s%s;\n", path.c_str(), assign,
                uint128_hilo_to_str(buf, p->intval_hi, p->intval));
      }
      break;

    case nt_string:
      fprintf(f, "%s%s", path.c_str(), assign);
      print_quoted_string(f, p->strval.c_str());
      fputs(";\n", f);
      break;

    default: jassert(false);
  }
}

void json::print(FILE * f, const print_options & options) const
{
  if (m_root_node.type == nt_unset)
    return;
  jassert(m_root_node.type == nt_object);

  switch (options.format) {
    default:
      print_json(f, options.pretty, options.sorted, &m_root_node, 0);
      if (options.pretty)
        putc('\n', f);
      break;
    case 'y':
      fputs("---", f);
      print_yaml(f, options.pretty, options.sorted, &m_root_node, 0, 0, false);
      break;
    case 'g': {
        std::string path("json");
        print_flat(f, (options.pretty ? " = " : "="), options.sorted, &m_root_node, path);
      }
      break;
  }
}
