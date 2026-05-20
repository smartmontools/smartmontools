// agentxd_json.h — minimal JSON reader for smartd state files

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Represents any JSON value.  Null by default.
struct JVal {
    enum Type { J_NULL, J_BOOL, J_INT, J_UINT, J_FLOAT, J_STRING, J_ARRAY, J_OBJECT };

    Type type { J_NULL };
    bool     bval  { false };
    int64_t  ival  { 0 };
    uint64_t uval  { 0 };
    double   fval  { 0.0 };
    std::string sval;

    std::vector<JVal>                            arr;
    std::unordered_map<std::string, std::size_t> obj_keys; // key → arr index
    // object values stored in arr (preserves insertion order)

    // --- type predicates ---
    bool is_null()   const { return type == J_NULL; }
    bool is_bool()   const { return type == J_BOOL; }
    bool is_int()    const { return type == J_INT;  }
    bool is_uint()   const { return type == J_UINT; }
    bool is_number() const { return type == J_INT || type == J_UINT || type == J_FLOAT; }
    bool is_string() const { return type == J_STRING; }
    bool is_array()  const { return type == J_ARRAY; }
    bool is_object() const { return type == J_OBJECT; }

    // --- value accessors (with safe defaults) ---
    bool        as_bool()   const { return is_bool()   ? bval : (is_int() ? ival != 0 : false); }
    int64_t     as_int64()  const;
    uint64_t    as_uint64() const;
    std::string as_string() const { return is_string() ? sval : ""; }

    // --- array ---
    std::size_t size() const { return arr.size(); }

    const JVal& operator[](std::size_t i) const {
        static const JVal null_val;
        return i < arr.size() ? arr[i] : null_val;
    }

    // --- object ---
    bool has(const std::string &key) const {
        return obj_keys.count(key) != 0;
    }

    const JVal& operator[](const std::string &key) const {
        static const JVal null_val;
        auto it = obj_keys.find(key);
        return it != obj_keys.end() ? arr[it->second] : null_val;
    }

    // Convenience: chain through object keys and array indices.
    // Returns a null JVal if any step is missing.
    const JVal& get(const std::string &key) const { return (*this)[key]; }
};

inline int64_t JVal::as_int64() const {
    if (type == J_INT)   return ival;
    if (type == J_UINT)  return static_cast<int64_t>(uval);
    if (type == J_FLOAT) return static_cast<int64_t>(fval);
    if (type == J_BOOL)  return bval ? 1 : 0;
    return 0;
}

inline uint64_t JVal::as_uint64() const {
    if (type == J_UINT)  return uval;
    if (type == J_INT)   return static_cast<uint64_t>(ival);
    if (type == J_FLOAT) return static_cast<uint64_t>(fval);
    if (type == J_BOOL)  return bval ? 1 : 0;
    return 0;
}

// Parse JSON from a NUL-terminated string.
// Returns the root value; on error, sets error and returns null JVal.
JVal json_parse(const char *text, std::string &error);

// Load and parse a JSON file.  Returns null JVal on I/O or parse error.
JVal json_load_file(const std::string &path, std::string &error);
