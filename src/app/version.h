// version.h - what this build is. The values come from the build (Makefile: APP_VERSION,
// APP_COMMIT, APP_DATE; the version is taken from the nearest v* git tag). This is the
// *program* version; the online protocol version lives in netplay.cpp and changes on its own.
#pragma once
#include <string>

const char *app_version();      // "1.2.0" on a tagged build, "1.2.0-5-gabc1234" after it, "dev" outside git
const char *app_commit();       // short commit hash, or "unknown"
const char *app_build_date();   // YYYY-MM-DD (UTC)
std::string app_version_line(); // "NBA JAM Recomp 1.2.0 (abc1234, 2026-11-15)"
