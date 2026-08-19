/*
 * hexdump.cpp - class hexdumper_base
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2026 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <smartmon/hexdump.h>

#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace smartmon {

hexdumper_base::hexdumper_base(const hexdump_options & options)
: m_options(options)
{
}

void hexdumper_base::operator()(const void * ptr, size_t size)
{
  char line[40 + 16 + 2 + s_blocksize * 3 + 2 + 1 + s_blocksize + 2 + 1 + 13]{};

  if (!m_started) { // First call?
    m_started = true;
    if (m_options.offset_max) {
      // Set max offset length
      int len = snprintf(line, sizeof(line), "%llx", (unsigned long long)
                         (m_options.offset_max >= m_options.offset_min + size
                          ? m_options.offset_max : m_options.offset_min + size));
      if (len < 0)
        len = 2;
      if (m_options.offset_max == 1 && len < 2)
        len = 2;
      if (len > 16)
        len = 16;
      m_offset_len = (uint8_t)len;
      m_append_offset = m_options.append_offset;
    }

    if (m_options.skip_identical)
      m_skip_state = 1;
  }

  const char * const prefix = (m_options.prefix ? m_options.prefix : "");
  const char eol[] = {(m_options.no_trailing_nl ? '\0' : '\n'), '\0'};

  for (size_t pi = 0; ; ) {
    std::array<uint8_t, s_blocksize> block{};
    size_t sz = m_next_size;
    if (sz) {
      // Start with remaining bytes from previous call
      std::copy_n(m_next_bytes.cbegin(), sz, block.begin());
      m_next_size = 0;
    }
    // Fill block with new bytes
    while (sz < s_blocksize && pi < size)
      block[sz++] = reinterpret_cast<const uint8_t *>(ptr)[pi++];

    if (!sz) {
      if (size)
        break;
      // Finish dump
      if (!(m_append_offset || m_skip_state == 2 || m_skip_state == 3))
        break;
      // Output trailing offset or last identical block
    }
    else if (sz < s_blocksize) {
      if (size) {
        // Keep remaining bytes for next call
        std::copy_n(block.cbegin(), sz, m_next_bytes.begin());
        m_next_size = (uint8_t)sz;
        break;
      }
      // Output remaining bytes
    }

    const char bol[] = {(m_options.no_trailing_nl && m_offset ? '\n' : '\0'), '\0'};

    if (m_skip_state) {
      bool marker = false;
      static const decltype(block) null_block{};
      if (   sz == s_blocksize && m_offset >= s_blocksize && m_prev_block == block
          && (m_options.skip_identical > 1 || block == null_block)) {
        // Skip duplicate line
        m_offset += sz;
        if (m_skip_state == 1) {
          // Start skipping
          if (m_append_offset) {
            m_skip_state = 4; // Don't repeat block if last block ...
            marker = true; // ... and output '*' and continue
          }
          else if (pi < size) {
            m_skip_state = 3; // Repeat block if last block ...
            marker = true; // ... and output '*' and continue
          }
          else {
            m_skip_state = 2; // Repeat block if last block ...
            continue; // ... and delay output of '*'
          }
        }
        else if (m_skip_state == 2) {
          m_skip_state = 3; // Repeat block if last block ...
          marker = true; // ... and output '*' (delayed) and continue
        }
        else {
          continue; // '*' already output
        }
      }
      else if (!sz && !size) {
        if (m_skip_state == 2 || m_skip_state == 3) {
          // Repeat the previous identical block to show total size
          block = m_prev_block;
          sz = s_blocksize;
          m_offset -= sz;
        }
        m_skip_state = 0;
      }
      else {
        // Output this line
        if (m_skip_state == 2)
          marker = true; // Output '*' (delayed from previous block)
        m_skip_state = 1;
        // Save for next duplicate check
        m_prev_block = block;
      }

      if (marker) {
        if (snprintf(line, sizeof(line), "%s%.40s*%s", bol, prefix, eol) > 0)
          output(line);
        if (m_skip_state > 1)
          continue; // Still skipping
      }
    }

    unsigned ci = 0;
    if (bol[0] || *prefix) {
      int len = snprintf(line, sizeof(line) - 40, "%s%.40s", bol, prefix);
      ci += (unsigned)(len >= 0 ? len : 0);
    }

    if (m_offset_len && (sz || m_append_offset)) {
      // Output offset
      int len = snprintf(line + ci, sizeof(line) - ci - 2 - 4, "%0*llx%s", (int)m_offset_len,
                         (unsigned long long)(m_options.offset_min + m_offset), (sz ? "  " : eol));
      if (!sz) {
        output(line);
        m_append_offset = false;
        break;
      }
      ci += (unsigned)(len >= 0 ? len : 0);
    }

    // Output 16 bytes in hex
    unsigned bi;
    for (bi = 0; bi < sz && ci + 4 + 4 < sizeof(line); bi++) {
      int len = snprintf(line + ci, sizeof(line) - ci, "%s%02x",
                         (bi == 0 ? "" : ((m_options.hex_2_columns && bi == 8) ? "  " : " ")),
                         block[bi]                                                            );
      ci += (unsigned)(len >= 0 ? len : 0);
    }

    if (m_options.add_ascii) {
      // Fill gap
      unsigned spc = 2;
      if (bi < s_blocksize)
        spc += (s_blocksize - bi) * 3 + ((m_options.hex_2_columns && bi <= 8) ? 1 : 0);
      for (bi = 0; bi < spc && ci + 1 + 4 < sizeof(line); bi++)
        line[ci++] = ' ';
      // Output "|...ASCII...|"
      line[ci++] = '|';
      for (bi = 0; bi < sz && ci + 1 + 3 < sizeof(line); bi++) {
        char c = (char)block[bi];
        line[ci++] = (' ' <= c && c <= '~' ? c : '.');
      }
      line[ci++] = '|';
    }

    snprintf(line + ci, sizeof(line) - ci, "%s", eol);
    output(line);
    m_offset += sz;
  }

  if (!size) {
    // Prepare for new output
    m_started = false;
    m_offset_len = 0;
    m_offset = 0;
    m_skip_state = 0;
    m_next_size = 0;
    m_append_offset = false;
  }
}

void hexdumper_base::operator()()
{
  operator()((const void *)0, 0);
}

} // namespace smartmon
