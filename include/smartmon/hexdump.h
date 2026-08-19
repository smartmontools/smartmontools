/*
 * hexdump.h - template class hexdumper and hexdump() function
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2026 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_HEXDUMP_H
#define SMARTMON_HEXDUMP_H

#include <stdint.h>
#include <stdlib.h>
#include <array>

namespace smartmon {

/// Options for 'hexdumper<>' and 'hexdump()'.
// TODO: add zero initialization to members when C++14 is enabled.
struct hexdump_options {
  const char * prefix;     //< Prepend each line with this string.
  uint64_t offset_min;     //< Start value of offset field.
  uint64_t offset_max;     //< Max offset to determine field length: 0=none, 1=auto.
  bool hex_2_columns;      //< Output 16 hex bytes in two 8 byte columns.
  bool add_ascii;          //< Add "|...ASCII...|" block.
  uint8_t skip_identical;  //< Skip identical lines: 0=no, 1=nullbytes only, 2=all.
  bool append_offset;      //< Append next offset after last hex line.
  bool no_trailing_nl;     //< Do not append '\n' to last line.
};

/// Options for format compatible with 'hexdump --canonical' (see hexdump(1)).
constexpr hexdump_options hexdump_options_canonical {
  nullptr,     // prefix
  0,           // offset_min
  0xffffffff,  // offset_max
  true,        // hex_2_columns
  true,        // add_ascii
  2,           // skip_identical
  true,        // append_offset
  false        // no_trailing_nl
};

/// Options for format compatible with 'xxd -r'.
constexpr hexdump_options hexdump_options_xxd_r {
  nullptr,     // prefix
  0,           // offset_min
  0xffffffff,  // offset_max
  false,       // hex_2_columns
  true,        // add_ascii
  1,           // skip_identical
  false,       // append_offset
  false        // no_trailing_nl
};

// Base class for 'hexdumper<>'.
class hexdumper_base
{
public:
  explicit hexdumper_base(const hexdump_options & options);

  virtual ~hexdumper_base() = default;

  /// Add SIZE bytes at PTR to the hexdump.
  void operator()(const void * ptr, size_t size);

  /// Flush pending output and prepare for next hex dump.
  void operator()();

  /// Output function to implement in derived class.
  virtual void output(const char * str) = 0;

private:
  const hexdump_options m_options;
  bool m_started{};
  uint8_t m_offset_len{};
  uint64_t m_offset{};
  static constexpr unsigned s_blocksize = 16;
  std::array<uint8_t, s_blocksize> m_prev_block{};
  uint8_t m_skip_state{};
  std::array<uint8_t, s_blocksize - 1> m_next_bytes{};
  uint8_t m_next_size{};
  bool m_append_offset{};
};

/// Output hexdump using 'void OUT(const char * str)'.
template <typename F>
class hexdumper : public hexdumper_base
{
public:
  /// Initialize hexdumper with 'void OUT(const char * str)' and OPTIONS.
  hexdumper(F out, const hexdump_options & options)
    : hexdumper_base(options), m_out(out) {}

  virtual void output(const char * str) override
    { m_out(str); }

private:
  F m_out;
};

/// Output hex dump of SIZE bytes at PTR using 'void OUT(const char * str)'.
template <typename F>
void hexdump(F out, const void * ptr, size_t size, const hexdump_options & options)
{
  hexdumper<F> hd{out, options};
  hd(ptr, size);
  hd();
}

} // namespace smartmon

#endif // SMARTMON_HEXDUMP_H
