/*
 * json.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2017 Christian Franke
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

json::ref & json::ref::operator=(int value)
{
  return operator=((long long)value);
}

json::ref & json::ref::operator=(unsigned value)
{
  return operator=((long long)value);
}

json::ref & json::ref::operator=(long value)
{
  return operator=((long long)value);
}

json::ref & json::ref::operator=(unsigned long value)
{
  return operator=((long long)value);
}

json::ref & json::ref::operator=(unsigned long long value)
{
  return operator=((long long)value);
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

json::node::~node()
{
  for (size_t i = 0; i < childs.size(); i++)
    delete childs[i];
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

  if (p->type == nt_unset)
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

void json::print_worker(FILE * f, const node * p, int level) const
{
  if (!p->key.empty())
    fprintf(f, "\"%s\" : ", p->key.c_str());

  switch (p->type) {
    case nt_object:
    case nt_array:
      putc((p->type == nt_object ? '{' : '['), f);
      if (!p->childs.empty()) {
        for (unsigned i = 0; i < p->childs.size(); i++) {
          fprintf(f, "%s\n%*s", (i > 0 ? "," : ""), (level + 1) * 2, "");
          node * p2 = p->childs[i];
          if (!p2) {
            // Unset element of sparse array
            jassert(p->type == nt_array);
            fputs("null", f);
          }
          else {
            // Recurse
            print_worker(f, p2, level + 1);
          }
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

    case nt_string:
      print_string(f, p->strval.c_str());
      break;

    default: jassert(false);
  }
}

void json::print(FILE * f) const
{
  if (m_root_node.type == nt_unset)
    return;
  jassert(m_root_node.type == nt_object);
  print_worker(f, &m_root_node, 0);
  putc('\n', f);
}
