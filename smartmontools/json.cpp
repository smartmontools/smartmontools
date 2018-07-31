/*
 * json.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2017-18 Christian Franke
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
#include "json.h"

const char * json_cvsid = "$Id$"
  JSON_H_CVSID;

#include "sg_unaligned.h"

#include <stdexcept>

static void jassert_failed(int line, const char * expr)
{
  char msg[128];
  // Avoid __FILE__ as it may break reproducible builds
  snprintf(msg, sizeof(msg), "json.cpp(%d): Assertion failed: %s", line, expr);
  throw std::logic_error(msg);
}

#define jassert(expr) (!(expr) ? jassert_failed(__LINE__, #expr) : (void)0)

static void check_key(const char * key)
{
  // Limit: object keys should be valid identifiers (lowercase only)
  char c = key[0];
  jassert('a' <= c && c <= 'z');
  for (int i = 1; (c = key[i]); i++)
    jassert(('a' <= c && c <= 'z') || ('0' <= c && c <= '9') || (c == '_'));
}

json::ref::ref(json & js, const char * key)
: m_js(js)
{
  check_key(key);
  m_path.push_back(node_info(key));
}

json::ref::ref(const ref & base, const char * key)
: m_js(base.m_js), m_path(base.m_path)
{
  check_key(key);
  m_path.push_back(node_info(key));
}

json::ref::ref(const ref & base, int index)
: m_js(base.m_js), m_path(base.m_path)
{
  jassert(0 <= index && index < 10000); // Limit: large arrays not supported
  m_path.push_back(node_info(index));
}

json::ref & json::ref::operator=(bool value)
{
  m_js.set_bool(m_path, value);
  return *this;
}

json::ref & json::ref::operator=(long long value)
{
  m_js.set_int(m_path, value);
  return *this;
}

json::ref & json::ref::operator=(unsigned long long value)
{
  m_js.set_uint(m_path, value);
  return *this;
}

json::ref & json::ref::operator=(int value)
{
  return operator=((long long)value);
}

json::ref & json::ref::operator=(unsigned value)
{
  return operator=((unsigned long long)value);
}

json::ref & json::ref::operator=(long value)
{
  return operator=((long long)value);
}

json::ref & json::ref::operator=(unsigned long value)
{
  return operator=((unsigned long long)value);
}

json::ref & json::ref::operator=(const std::string & value)
{
  m_js.set_string(m_path, value);
  return *this;
}

json::ref & json::ref::operator=(const char * value)
{
  jassert(value); // Limit: null not supported
  return operator=(std::string(value));
}

void json::ref::set_uint128(uint64_t value_hi, uint64_t value_lo)
{
  if (!value_hi)
    operator=((unsigned long long)value_lo);
  else
    m_js.set_uint128(m_path, value_hi, value_lo);
}

void json::ref::set_unsafe_uint64(uint64_t value)
{
  // Output as number and string
  operator[]("n") = (unsigned long long)value;
  char s[32];
  snprintf(s, sizeof(s), "%llu", (unsigned long long)value);
  operator[]("s") = s;
}

static const char * uint128_to_str(char (& str)[64], uint64_t hi, uint64_t lo)
{
  snprintf(str, sizeof(str), "%.0f", hi * (0xffffffffffffffffULL + 1.0) + lo);
  return str;
}

void json::ref::set_unsafe_uint128(uint64_t value_hi, uint64_t value_lo)
{
  if (!value_hi)
    set_unsafe_uint64(value_lo);
  else {
    // Output as number, string and LE byte array
    operator[]("n").set_uint128(value_hi, value_lo);
    char s[64];
    operator[]("s") = uint128_to_str(s, value_hi, value_lo);

    ref le = operator[]("le");
    for (int i = 0; i < 8; i++)
      le[i] = (value_lo >> (i << 3)) & 0xff;
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

json::node::node()
: type(nt_unset),
  intval(0),
  intval_hi(0)
{
}

json::node::node(const std::string & key_)
: type(nt_unset),
  intval(0),
  intval_hi(0),
  key(key_)
{
}

json::node::~node()
{
  for (size_t i = 0; i < childs.size(); i++)
    delete childs[i];
}

json::node::const_iterator::const_iterator(const json::node * node_p, bool sorted)
: m_node_p(node_p),
  m_use_map(sorted && node_p->type == nt_object),
  m_child_idx(0)
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
    return m_node_p->childs[m_key_iter->second];
  else
    return m_node_p->childs[m_child_idx];
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
        p2 = p->childs[ni->second];
      }
      else {
        // Create new object element
        p->key2index[pi.key] = (unsigned)p->childs.size();
        p2 = new node(pi.key);
        p->childs.push_back(p2);
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
        p2 = p->childs[pi.index];
        if (!p2) // Already created ?
          p->childs[pi.index] = p2 = new node;
      }
      else {
        // Grow array, fill gap, create new element
        p->childs.resize(pi.index + 1);
        p->childs[pi.index] = p2 = new node;
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

json::json()
: m_enabled(false)
{
}

void json::set_bool(const node_path & path, bool value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_bool)->intval = (value ? 1 : 0);
}

void json::set_int(const node_path & path, long long value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_int)->intval = value;
}

void json::set_uint(const node_path & path, unsigned long long value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_uint)->intval = (long long)value;
}

void json::set_uint128(const node_path & path, uint64_t value_hi, uint64_t value_lo)
{
  if (!m_enabled)
    return;
  node * p = find_or_create_node(path, nt_uint128);
  p->intval_hi = value_hi;
  p->intval = (long long)value_lo;
}

void json::set_string(const node_path & path, const std::string & value)
{
  if (!m_enabled)
    return;
  find_or_create_node(path, nt_string)->strval = value;
}

static void print_string(FILE * f, const char * s)
{
  putc('"', f);
  for (int i = 0; s[i]; i++) {
    char c = s[i];
    if (c == '"' || c == '\\')
      putc('\\', f);
    else if (c == '\t') {
      putc('\\', f); c = 't';
    }
    else if (!(' ' <= c && c <= '~'))
      c = '?'; // TODO: UTF-8 characters?
    putc(c, f);
  }
  putc('"', f);
}

void json::print_json(FILE * f, bool sorted, const node * p, int level)
{
  if (!p->key.empty())
    fprintf(f, "\"%s\" : ", p->key.c_str());

  switch (p->type) {
    case nt_object:
    case nt_array:
      putc((p->type == nt_object ? '{' : '['), f);
      if (!p->childs.empty()) {
        const char * delim = "";
        for (node::const_iterator it(p, sorted); !it.at_end(); ++it) {
          fprintf(f, "%s\n%*s", delim, (level + 1) * 2, "");
          const node * p2 = *it;
          if (!p2) {
            // Unset element of sparse array
            jassert(p->type == nt_array);
            fputs("null", f);
          }
          else {
            // Recurse
            print_json(f, sorted, p2, level + 1);
          }
          delim = ",";
        }
        fprintf(f, "\n%*s", level * 2, "");
      }
      putc((p->type == nt_object ? '}' : ']'), f);
      break;

    case nt_bool:
      fputs((p->intval ? "true" : "false"), f);
      break;

    case nt_int:
      fprintf(f, "%lld", p->intval);
      break;

    case nt_uint:
      fprintf(f, "%llu", (unsigned long long)p->intval);
      break;

    case nt_uint128:
      {
        char buf[64];
        fputs(uint128_to_str(buf, p->intval_hi, (uint64_t)p->intval), f);
      }
      break;

    case nt_string:
      print_string(f, p->strval.c_str());
      break;

    default: jassert(false);
  }
}

void json::print_flat(FILE * f, bool sorted, const node * p, std::string & path)
{
  switch (p->type) {
    case nt_object:
    case nt_array:
      fprintf(f, "%s = %s;\n", path.c_str(), (p->type == nt_object ? "{}" : "[]"));
      if (!p->childs.empty()) {
        unsigned len = path.size();
        for (node::const_iterator it(p, sorted); !it.at_end(); ++it) {
          const node * p2 = *it;
          if (p->type == nt_array) {
            char buf[10]; snprintf(buf, sizeof(buf), "[%u]", it.array_index());
            path += buf;
          }
          else {
            path += '.'; path += p2->key;
          }
          if (!p2) {
            // Unset element of sparse array
            jassert(p->type == nt_array);
            fprintf(f, "%s = null;\n", path.c_str());
          }
          else {
            // Recurse
            print_flat(f, sorted, p2, path);
          }
          path.erase(len);
        }
      }
      break;

    case nt_bool:
      fprintf(f, "%s = %s;\n", path.c_str(), (p->intval ? "true" : "false"));
      break;

    case nt_int:
      fprintf(f, "%s = %lld;\n", path.c_str(), p->intval);
      break;

    case nt_uint:
      fprintf(f, "%s = %llu;\n", path.c_str(), (unsigned long long)p->intval);
      break;

    case nt_uint128:
      {
        char buf[64];
        fprintf(f, "%s = %s;\n", path.c_str(),
                uint128_to_str(buf, p->intval_hi, (uint64_t)p->intval));
      }
      break;

    case nt_string:
      fprintf(f, "%s = ", path.c_str());
      print_string(f, p->strval.c_str());
      fputs(";\n", f);
      break;

    default: jassert(false);
  }
}

void json::print(FILE * f, bool sorted, bool flat) const
{
  if (m_root_node.type == nt_unset)
    return;
  jassert(m_root_node.type == nt_object);

  if (!flat) {
    print_json(f, sorted, &m_root_node, 0);
    putc('\n', f);
  }
  else {
    std::string path("json");
    print_flat(f, sorted, &m_root_node, path);
  }
}
