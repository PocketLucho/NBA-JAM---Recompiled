// logmacro.h - minimame version (logging compiled in, gated by VERBOSE)
#ifndef VERBOSE
#define VERBOSE 0
#endif
#ifndef LOG_GENERAL
#define LOG_GENERAL (1U << 0)
#endif
#undef LOGMASKED
#undef LOG
#define LOGMASKED(mask, ...) do { if (VERBOSE & (mask)) logerror(__VA_ARGS__); } while (false)
#define LOG(...) LOGMASKED(LOG_GENERAL, __VA_ARGS__)
