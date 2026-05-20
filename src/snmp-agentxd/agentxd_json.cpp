// agentxd_json.cpp — minimal recursive-descent JSON parser

#include "agentxd_json.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

// ---------------------------------------------------------------------------
// Parser state
// ---------------------------------------------------------------------------

struct Parser {
    const char *p;
    const char *end;
    std::string error;

    explicit Parser(const char *text, std::size_t len)
        : p(text), end(text + len) {}

    bool ok()  const { return error.empty(); }
    bool eof() const { return p >= end; }
    char peek() const { return eof() ? '\0' : *p; }

    void skip_ws() {
        while (!eof() && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            ++p;
    }

    bool expect(char c) {
        skip_ws();
        if (eof() || *p != c) {
            error = std::string("expected '") + c + "', got '" +
                    (eof() ? "EOF" : std::string(1, *p)) + "'";
            return false;
        }
        ++p;
        return true;
    }

    // Parse a string token (p is just past the opening '"')
    std::string parse_string_body() {
        std::string result;
        while (!eof() && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (eof()) { error = "unterminated string escape"; return {}; }
                switch (*p) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    // decode \uXXXX — only handle BMP as UTF-8
                    if (end - p < 5) { error = "short \\u escape"; return {}; }
                    char hex[5] = { p[1], p[2], p[3], p[4], '\0' };
                    unsigned long cp = strtoul(hex, nullptr, 16);
                    p += 4;
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: result += *p; break;
                }
            } else {
                result += *p;
            }
            ++p;
        }
        if (eof()) { error = "unterminated string"; return {}; }
        ++p; // consume closing '"'
        return result;
    }

    JVal parse_value();

    JVal parse_string() {
        ++p; // consume '"'
        JVal v;
        v.type = JVal::J_STRING;
        v.sval = parse_string_body();
        return v;
    }

    JVal parse_number() {
        const char *start = p;
        bool is_neg = false;
        if (*p == '-') { is_neg = true; ++p; }

        bool is_float = false;
        while (!eof() && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E'
                          || *p == '+' || *p == '-')) {
            if (*p == '.' || *p == 'e' || *p == 'E') is_float = true;
            ++p;
        }

        JVal v;
        if (is_float) {
            v.type = JVal::J_FLOAT;
            v.fval = strtod(start, nullptr);
        } else if (is_neg) {
            v.type = JVal::J_INT;
            v.ival = (int64_t)strtoll(start, nullptr, 10);
        } else {
            // Prefer uint for non-negative to handle large values like 2^64-1
            unsigned long long u = strtoull(start, nullptr, 10);
            if (u <= (unsigned long long)INT64_MAX) {
                v.type = JVal::J_INT;
                v.ival = (int64_t)u;
            } else {
                v.type = JVal::J_UINT;
                v.uval = u;
            }
        }
        return v;
    }

    JVal parse_array() {
        ++p; // consume '['
        JVal v;
        v.type = JVal::J_ARRAY;
        skip_ws();
        if (!eof() && *p == ']') { ++p; return v; }
        for (;;) {
            v.arr.push_back(parse_value());
            if (!ok()) return {};
            skip_ws();
            if (eof()) { error = "unterminated array"; return {}; }
            if (*p == ']') { ++p; break; }
            if (!expect(',')) return {};
        }
        return v;
    }

    JVal parse_object() {
        ++p; // consume '{'
        JVal v;
        v.type = JVal::J_OBJECT;
        skip_ws();
        if (!eof() && *p == '}') { ++p; return v; }
        for (;;) {
            skip_ws();
            if (eof() || *p != '"') { error = "expected object key string"; return {}; }
            ++p;
            std::string key = parse_string_body();
            if (!ok()) return {};
            if (!expect(':')) return {};
            JVal val = parse_value();
            if (!ok()) return {};
            std::size_t idx = v.arr.size();
            v.obj_keys[key] = idx;
            v.arr.push_back(std::move(val));
            skip_ws();
            if (eof()) { error = "unterminated object"; return {}; }
            if (*p == '}') { ++p; break; }
            if (!expect(',')) return {};
        }
        return v;
    }
};

JVal Parser::parse_value() {
    skip_ws();
    if (eof()) { error = "unexpected end of input"; return {}; }

    char c = *p;
    if (c == '"') return parse_string();
    if (c == '[') return parse_array();
    if (c == '{') return parse_object();
    if (c == 't') {
        if (end - p >= 4 && strncmp(p, "true", 4) == 0) {
            p += 4;
            JVal v; v.type = JVal::J_BOOL; v.bval = true; return v;
        }
        error = "invalid literal near 'true'"; return {};
    }
    if (c == 'f') {
        if (end - p >= 5 && strncmp(p, "false", 5) == 0) {
            p += 5;
            JVal v; v.type = JVal::J_BOOL; v.bval = false; return v;
        }
        error = "invalid literal near 'false'"; return {};
    }
    if (c == 'n') {
        if (end - p >= 4 && strncmp(p, "null", 4) == 0) {
            p += 4;
            return JVal{};
        }
        error = "invalid literal near 'null'"; return {};
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number();

    error = std::string("unexpected character '") + c + "'";
    return {};
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

JVal json_parse(const char *text, std::string &error) {
    std::size_t len = strlen(text);
    Parser pr(text, len);
    JVal root = pr.parse_value();
    if (!pr.ok()) { error = pr.error; return {}; }
    pr.skip_ws();
    if (!pr.eof()) {
        error = std::string("trailing content after JSON value: '") + pr.peek() + "'";
        return {};
    }
    return root;
}

JVal json_load_file(const std::string &path, std::string &error) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        error = path + ": " + strerror(errno);
        return {};
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) {
        fclose(f);
        error = path + ": empty file";
        return {};
    }
    std::string buf(static_cast<std::size_t>(sz), '\0');
    if (fread(&buf[0], 1, static_cast<std::size_t>(sz), f) != static_cast<std::size_t>(sz)) {
        fclose(f);
        error = path + ": read error";
        return {};
    }
    fclose(f);
    return json_parse(buf.c_str(), error);
}
