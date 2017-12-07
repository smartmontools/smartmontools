/*
 * json.h
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

#ifndef JSON_H_CVSID
#define JSON_H_CVSID "$Id$"

#include <stdio.h>
#include <string>
#include <vector>
#include <map>

/// Create and print JSON output.
class json
{
private:
  struct node_info
  {
    std::string key;
    int index;

    node_info()
      : index(0) { }
    explicit node_info(const char * key_)
      : key(key_), index(0) { }
    explicit node_info(int index_)
      : index(index_) { }
  };

  typedef std::vector<node_info> node_path;

public:
  json();

  /// Reference to a JSON element.
  class ref
  {
  public:
    /// Return reference to object element.
    ref operator[](const char * key) const
      { return ref(*this, key); };

    /// Return reference to array element.
    ref operator[](int index) const
      { return ref(*this, index); };

    // Assignment operators create or change element.
    ref & operator=(bool value);

    ref & operator=(int value);
    ref & operator=(unsigned value);
    ref & operator=(long value);
    ref & operator=(unsigned long value);
    ref & operator=(long long  value);
    ref & operator=(unsigned long long value);

    ref & operator=(const char * value);
    ref & operator=(const std::string & value);

  private:
    friend class json;
    ref(json & js, const char * key);
    ref(const ref & base, const char * key);
    ref(const ref & base, int index);

    json & m_js;
    node_path m_path;
  };

  /// Return reference to element of top level object.
  ref operator[](const char * key)
    { return ref(*this, key); };

  /// Enable/disable JSON output.
  void enable(bool yes = true)
    { m_enabled = yes; }

  /// Return true if enabled.
  bool is_enabled() const
    { return m_enabled; }

  /// Print JSON tree to a file.
  void print(FILE * f, bool sorted, bool flat) const;

private:
  enum node_type { nt_unset, nt_object, nt_array, nt_bool, nt_int, nt_string };

  struct node
  {
    node();
    explicit node(const std::string & key_);
    ~node();

    node_type type;

    long long intval;
    std::string strval;

    std::string key;
    std::vector<node *> childs;
    typedef std::map<std::string, unsigned> keymap;
    keymap key2index;

    class const_iterator
    {
    public:
      const_iterator(const node * node_p, bool sorted);
      bool at_end() const;
      unsigned array_index() const;
      void operator++();
      const node * operator*() const;

    private:
      const node * m_node_p;
      bool m_use_map;
      unsigned m_child_idx;
      keymap::const_iterator m_key_iter;
    };

#if __cplusplus >= 201103
    node(const node &) = delete;
    void operator=(const node &) = delete;
#else
    private: node(const node &); void operator=(const node &);
#endif
  };

  bool m_enabled;
  node m_root_node;

  node * find_or_create_node(const node_path & path, node_type type);

  void set_bool(const node_path & path, bool value);
  void set_int(const node_path & path, long long value);
  void set_string(const node_path & path, const std::string & value);

  static void print_json(FILE * f, bool sorted, const node * p, int level);
  static void print_flat(FILE * f, bool sorted, const node * p, std::string & path);
};

#endif // JSON_H_CVSID
