// utf8.h - file paths as UTF-8 strings on every platform (#35).
// TI Revive keeps every path in a std::string holding UTF-8. On Windows, std::filesystem::path(std::string),
// path::string(), fopen() and getenv() use the ANSI code page instead: a name it cannot represent (Japanese on a
// Western system, a combining accent copied from a Mac, an emoji) made path::string() throw std::system_error, and
// load_banks() in initialize() crashed the host. These helpers convert between UTF-8 and the wide Windows APIs at the
// edge; on macOS and Linux paths are UTF-8 already and they pass the bytes through. None of them throws.
// Implemented in utf8.cpp (C++17, vticore): path::u8string() returns another type under C++20/23 (the AU target).
#pragma once
#include <cstdio>
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace vti {

// a path from UTF-8 (invalid UTF-8 becomes U+FFFD on Windows)
std::filesystem::path u8path(const std::string &utf8);
// the path as UTF-8, native separators (a name that is not valid UTF-16 on Windows gets U+FFFD instead of throwing)
std::string u8str(const std::filesystem::path &p);
// the path as UTF-8 with '/' separators
std::string u8generic(const std::filesystem::path &p);
// fopen() for a UTF-8 path (_wfopen on Windows)
FILE *u8fopen(const std::string &utf8, const char *mode);
// an environment variable as UTF-8 (_wgetenv on Windows); empty when it is not set
std::string u8env(const char *name);
// the whole file (UTF-8 path) into `out`; false if it cannot be opened or read, or is larger than `cap` bytes
bool u8read(const std::string &utf8, std::vector<uint8_t> &out, size_t cap = (size_t)512 << 20);
// writes `data` to `path` whole or not at all: a temp file next to it, then a rename over the old one (another
// instance reading meanwhile sees the old or the new file). false and no temp file left on an error, `error` says why.
bool u8write_atomic(const std::string &path, const std::vector<uint8_t> &data, std::string *error = nullptr);
// that temp file's name: "<path>.<process>-<n>.tmp", new for every call (review B9: two instances saving at once
// used one "<path>.tmp" and wrote into each other's)
std::string u8temp_name(const std::string &path);
// `s` with ASCII A..Z lowered (file extensions, resource names); other bytes as they are
std::string ascii_lower(std::string s);
// for the editor's bitmap font (ASCII 32..127): one character per code point, Latin letters without their accents,
// combining marks dropped, everything else '?' (also a byte that is not UTF-8)
std::string ascii_fold(const std::string &utf8);

#ifdef _WIN32
std::wstring widen(const std::string &utf8);
std::string narrow(const std::wstring &wide);
#endif

} // namespace vti
