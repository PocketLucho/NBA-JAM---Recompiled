// version.cpp - see version.h. Kept in its own file so only this one is rebuilt per commit.
#include "version.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif
#ifndef APP_COMMIT
#define APP_COMMIT "unknown"
#endif

const char *app_version() { return APP_VERSION; }
const char *app_commit() { return APP_COMMIT; }

const char *app_build_date()
{
#ifdef APP_DATE
	return APP_DATE;
#else
	// no date from the build system: use the one the compiler gives ("Mmm dd yyyy")
	static char b[16];
	if (!b[0])
	{
		static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
		const char *d = __DATE__;
		char mon[4] = { d[0], d[1], d[2], 0 };
		const char *m = strstr(months, mon);
		snprintf(b, sizeof(b), "%04d-%02d-%02d", atoi(d + 7), m ? int((m - months) / 3) + 1 : 1, atoi(d + 4));
	}
	return b;
#endif
}

std::string app_version_line()
{
	return std::string("NBA JAM Recomp ") + app_version() + " (" + app_commit() + ", " + app_build_date() + ")";
}
