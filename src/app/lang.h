// lang.h - user interface language (Spanish / English)
#pragma once

// false = Spanish (castellano), true = English. Chosen in the main menu and saved in nbajam_recomp.ini.
inline bool g_lang_en = false;

// tr("texto", "text"): returns the string for the current language
inline const char *tr(const char *es, const char *en) { return g_lang_en ? en : es; }
