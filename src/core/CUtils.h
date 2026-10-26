// CUtils.h - logging, timing and data-path helpers (subset of ProjectY::CUtils).
#pragma once

#include <cstdarg>
#include <string>

namespace py {

class CUtils {
public:
    // printf-style log line (original: __android_log_print + optional file).
    static void log(const char* fmt, ...);
    static void vlog(const char* fmt, va_list ap);
    static void setLogFile(const std::string& path);
    static void setQuiet(bool quiet);

    // Milliseconds since startTimer(), as float. The original computes
    // (sec*1000.0 + nsec/1e6) in double and narrows to float.
    static void startTimer();
    static float getTimer();

    // Directory holding the original game data (assets + map files).
    static void setDataDir(const std::string& dir);
    static const std::string& getDataDir();
    static std::string dataPath(const std::string& file);
};

}  // namespace py
