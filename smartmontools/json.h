/*
 * json.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2017-18 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JSON_H_CVSID
#define JSON_H_CVSID "$Id$"

#include <stdint.h>
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
  /// Return true if value is a safe JSON integer.
  /// Same as Number.isSafeInteger(value) in JavaScript.
  static bool is_safe_uint(unsigned long long value)
    { return (value < (1ULL << 53)); }

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

  private:
    friend class json;
    ref(json & js, const char * key);
    ref(const ref & base, const char * key);
    ref(const ref & base, int index);
    ref(const ref & base, const char * /*dummy*/, const char * key_suffix);

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

  /// Enable/disable extra string output for safe integers also.
  void set_verbose(bool yes = true)
    { m_verbose = yes; }

  /// Return true if any 128-bit value has been output.
  bool has_uint128_output() const
    { return m_uint128_output; }

  /// Print JSON tree to a file.
  void print(FILE * f, bool sorted, bool flat) const;

private:
  enum node_type {
    nt_unset, nt_object, nt_array,
    nt_bool, nt_int, nt_uint, nt_uint128, nt_string
  };

  struct node
  {
    node();
    explicit node(const std::string & key_);
    ~node();

    node_type type;

    uint64_t intval, intval_hi;
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
  bool m_verbose;
  bool m_uint128_output;

  node m_root_node;

  node * find_or_create_node(const node_path & path, node_type type);

  void set_bool(const node_path & path, bool value);
  void set_int64(const node_path & path, int64_t value);
  void set_uint64(const node_path & path, uint64_t value);
  void set_uint128(const node_path & path, uint64_t value_hi, uint64_t value_lo);
  void set_string(const node_path & path, const std::string & value);

  static void print_json(FILE * f, bool sorted, const node * p, int level);
  static void print_flat(FILE * f, bool sorted, const node * p, std::string & path);
};

#endif // JSON_H_CVSID
