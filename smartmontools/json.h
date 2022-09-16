/*
 * json.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2017-22 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JSON_H_CVSID
#define JSON_H_CVSID "$Id$"

#include <stdint.h>
#include <stdio.h>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

/// Create and print JSON output.
class json
{
public:
  /// Return true if value is a safe JSON integer.
  /// Same as Number.isSafeInteger(value) in JavaScript.
  static bool is_safe_uint(unsigned long long value)
    { return (value < (1ULL << 53)); }

  /// Replace space and non-alphanumerics with '_', upper to lower case.
  static std::string str2key(const char * str);

  /// Replace space and non-alphanumerics with '_', upper to lower case
  /// (std::string variant).
  static std::string str2key(const std::string & str)
    { return str2key(str.c_str()); }

  enum node_type {
    nt_unset, nt_object, nt_array,
    nt_bool, nt_int, nt_uint, nt_uint128, nt_string
  };

  // initializer_list<> elements.
  struct initlist_value {
    // cppcheck-suppress noExplicitConstructor
    initlist_value(node_type t) : type(t) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(bool v) : type(nt_bool), intval(v ? 1 : 0) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(int v) : initlist_value((long long)v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(unsigned v) : initlist_value((unsigned long long)v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(long v) : initlist_value((long long)v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(unsigned long v) : initlist_value((unsigned long long)v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(long long v) : type(nt_int), intval((uint64_t)(int64_t)v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(unsigned long long v) : type(nt_uint), intval((uint64_t)v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(const char * v) : type(nt_string), strval(v) {}
    // cppcheck-suppress noExplicitConstructor
    initlist_value(const std::string & v) : type(nt_string), strval(v.c_str()) {}
    node_type type;
    uint64_t intval = 0;
    const char * strval = nullptr;
  };

  struct initlist_key_value_pair {
    initlist_key_value_pair(const char * k, const initlist_value & v) : keystr(k), value(v) {}
    initlist_key_value_pair(const std::string & k, const initlist_value & v)
      : keystr(k.c_str()), value(v) {}
    initlist_key_value_pair(const char * k, const std::initializer_list<initlist_key_value_pair> & ilist)
      : keystr(k), value(nt_object), object(ilist) {}
    initlist_key_value_pair(const std::string & k, const std::initializer_list<initlist_key_value_pair> & ilist)
      : keystr(k.c_str()), value(nt_object), object(ilist) {}
    initlist_key_value_pair(const char * k, const std::initializer_list<initlist_value> & ilist)
      : keystr(k), value(nt_array), array(ilist) {}
    initlist_key_value_pair(const std::string & k, const std::initializer_list<initlist_value> & ilist)
      : keystr(k.c_str()), value(nt_array), array(ilist) {}
    const char * keystr;
    initlist_value value;
    std::initializer_list<initlist_key_value_pair> object;
    std::initializer_list<initlist_value> array;
  };

private:
  struct node_info
  {
    std::string key;
    int index = 0;

    node_info() = default;
    explicit node_info(const char * keystr) : key(str2key(keystr)) { }
    explicit node_info(int index_) : index(index_) { }
  };

  typedef std::vector<node_info> node_path;

public:
  /// Reference to a JSON element.
  class ref
  {
  public:
    ~ref();

    /// Return reference to object element.
    ref operator[](const char * keystr) const
      { return ref(*this, keystr); }

    /// Return reference to object element (std::string variant).
    ref operator[](const std::string & keystr) const
      { return ref(*this, keystr.c_str()); }

    /// Return reference to array element.
    ref operator[](int index) const
      { return ref(*this, index); }

    // Assignment operators create or change element.
    void operator=(bool value);

    void operator=(int value);
    void operator=(unsigned value);
    void operator=(long value);
    void operator=(unsigned long value);
    void operator=(long long value);
    void operator=(unsigned long long value);

    void operator=(const char * value);
    void operator=(const std::string & value);

    /// Return reference to element with KEY_SUFFIX appended to last key.
    ref with_suffix(const char * key_suffix) const
      { return ref(*this, "", key_suffix); }

    void set_uint128(uint64_t value_hi, uint64_t value_lo);

    // Output only if safe integer.
    bool set_if_safe_uint64(uint64_t value);
    bool set_if_safe_uint128(uint64_t value_hi, uint64_t value_lo);
    bool set_if_safe_le128(const void * pvalue);

    // If unsafe integer, output also as string with key "NUMBER_s".
    void set_unsafe_uint64(uint64_t value);
    void set_unsafe_uint128(uint64_t value_hi, uint64_t value_lo);
    void set_unsafe_le128(const void * pvalue);

    /// Braced-init-list support for nested objects.
    void operator+=(std::initializer_list<initlist_key_value_pair> ilist);
    /// Braced-init-list support for simple arrays.
    void operator+=(std::initializer_list<initlist_value> ilist);

  private:
    friend class json;
    explicit ref(json & js);
    ref(json & js, const char * keystr);
    ref(const ref & base, const char * keystr);
    ref(const ref & base, int index);
    ref(const ref & base, const char * /*dummy*/, const char * key_suffix);

    void operator=(const initlist_value & value)
      { m_js.set_initlist_value(m_path, value); }

    json & m_js;
    node_path m_path;
  };

  /// Return reference to element of top level object.
  ref operator[](const char * keystr)
    { return ref(*this, keystr); }

  /// Return reference to element of top level object (std::string variant).
  ref operator[](const std::string & keystr)
    { return ref(*this, keystr.c_str()); }

  /// Braced-init-list support for top level object.
  void operator+=(std::initializer_list<initlist_key_value_pair> ilist)
    { ref(*this) += ilist; }

  /// Enable/disable JSON output.
  void enable(bool yes = true)
    { m_enabled = yes; }

  /// Return true if enabled.
  bool is_enabled() const
    { return m_enabled; }

  /// Enable/disable extra string output for safe integers also.
  void set_verbose(bool yes = true)
    { m_verbose = yes; }

  /// Return true if any 128-bit value has been output.
  bool has_uint128_output() const
    { return m_uint128_output; }

  /// Options for print().
  struct print_options {
    bool pretty = false; //< Pretty-print output.
    bool sorted = false; //< Sort object keys.
    char format = 0; //< 'y': YAML, 'g': flat(grep, gron), other: JSON
  };

  /// Print JSON tree to a file.
  void print(FILE * f, const print_options & options) const;

private:
  struct node
  {
    node();
    node(const node &) = delete;
    explicit node(const std::string & key_);
    ~node();
    void operator=(const node &) = delete;

    node_type type = nt_unset;

    uint64_t intval = 0, intval_hi = 0;
    std::string strval;

    std::string key;
    std::vector< std::unique_ptr<node> > childs;
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
      unsigned m_child_idx = 0;
      keymap::const_iterator m_key_iter;
    };
  };

  bool m_enabled = false;
  bool m_verbose = false;
  bool m_uint128_output = false;

  node m_root_node;

  node * find_or_create_node(const node_path & path, node_type type);

  void set_bool(const node_path & path, bool value);
  void set_int64(const node_path & path, int64_t value);
  void set_uint64(const node_path & path, uint64_t value);
  void set_uint128(const node_path & path, uint64_t value_hi, uint64_t value_lo);
  void set_cstring(const node_path & path, const char * value);
  void set_string(const node_path & path, const std::string & value);
  void set_initlist_value(const node_path & path, const initlist_value & value);

  static void print_json(FILE * f, bool pretty, bool sorted, const node * p, int level);
  static void print_yaml(FILE * f, bool pretty, bool sorted, const node * p, int level_o,
                         int level_a, bool cont);
  static void print_flat(FILE * f, const char * assign, bool sorted, const node * p,
                         std::string & path);
};

#endif // JSON_H_CVSID
