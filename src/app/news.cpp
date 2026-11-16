// news.cpp - see news.h
#include "news.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

const char *news_default_url() { return "https://api.github.com/repos/richvam/NBA-JAMs-Recomp/releases/latest"; }

// ---------------------------------------------------------------------------
// the small part of JSON that is needed: the string value of a top-level key
// ---------------------------------------------------------------------------
static void put_utf8(std::string &s, unsigned cp)
{
	if (cp < 0x80) s += char(cp);
	else if (cp < 0x800) { s += char(0xc0 | (cp >> 6)); s += char(0x80 | (cp & 0x3f)); }
	else if (cp < 0x10000) { s += char(0xe0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 0x3f)); s += char(0x80 | (cp & 0x3f)); }
	else { s += char(0xf0 | (cp >> 18)); s += char(0x80 | ((cp >> 12) & 0x3f)); s += char(0x80 | ((cp >> 6) & 0x3f)); s += char(0x80 | (cp & 0x3f)); }
}

// parse the JSON string that starts at s[i] == '"'; returns the index after it, or npos
static size_t read_string(const std::string &s, size_t i, std::string *out)
{
	if (i >= s.size() || s[i] != '"') return std::string::npos;
	for (i++; i < s.size(); i++)
	{
		char c = s[i];
		if (c == '"') return i + 1;
		if (c != '\\') { if (out) *out += c; continue; }
		if (++i >= s.size()) break;
		switch (s[i])
		{
		case 'n': if (out) *out += '\n'; break;
		case 't': if (out) *out += ' '; break;
		case 'r': break;
		case 'b': case 'f': break;
		case 'u':
		{
			if (i + 4 >= s.size()) return std::string::npos;
			unsigned cp = unsigned(strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16));
			i += 4;
			if (cp >= 0xd800 && cp < 0xdc00 && i + 6 < s.size() && s[i + 1] == '\\' && s[i + 2] == 'u')
			{
				unsigned lo = unsigned(strtoul(s.substr(i + 3, 4).c_str(), nullptr, 16));
				cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
				i += 6;
			}
			if (out) put_utf8(*out, cp);
			break;
		}
		default: if (out) *out += s[i];
		}
	}
	return std::string::npos;
}

// value of "key" at nesting depth 1 of the object in s (nested objects such as "author" are skipped)
static bool top_level_string(const std::string &s, const char *key, std::string &out)
{
	int depth = 0;
	for (size_t i = 0; i < s.size();)
	{
		char c = s[i];
		if (c == '{' || c == '[') { depth++; i++; }
		else if (c == '}' || c == ']') { depth--; i++; }
		else if (c == '"')
		{
			std::string k;
			size_t e = read_string(s, i, &k);
			if (e == std::string::npos) return false;
			size_t j = e;
			while (j < s.size() && (s[j] == ' ' || s[j] == '\n' || s[j] == '\r' || s[j] == '\t')) j++;
			if (depth == 1 && j < s.size() && s[j] == ':' && k == key)
			{
				j++;
				while (j < s.size() && (s[j] == ' ' || s[j] == '\n' || s[j] == '\r' || s[j] == '\t')) j++;
				out.clear();
				return read_string(s, j, &out) != std::string::npos;
			}
			i = e;
		}
		else i++;
	}
	return false;
}

bool parse_release_json(const std::string &json, news_item &out)
{
	news_item n;
	if (!top_level_string(json, "tag_name", n.tag) || n.tag.empty()) return false;
	top_level_string(json, "name", n.name);
	top_level_string(json, "body", n.body);
	top_level_string(json, "html_url", n.url);
	std::string when;
	if (top_level_string(json, "published_at", when)) n.date = when.substr(0, 10);
	if (n.name.empty()) n.name = n.tag;
	n.ok = true;
	out = n;
	return true;
}

// ---------------------------------------------------------------------------
// versions
// ---------------------------------------------------------------------------
// "v1.2.0-5-gabc" -> {1,2,0}, ahead = 5 (commits after that release). False if it does not start with a number.
static bool split_version(std::string v, std::vector<long> &nums, long &ahead)
{
	if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(0, 1);
	nums.clear();
	ahead = 0;
	size_t i = 0;
	while (i < v.size() && isdigit(static_cast<unsigned char>(v[i])))
	{
		size_t j = i;
		while (j < v.size() && isdigit(static_cast<unsigned char>(v[j]))) j++;
		nums.push_back(atol(v.substr(i, j - i).c_str()));
		i = j;
		if (i < v.size() && v[i] == '.' && i + 1 < v.size() && isdigit(static_cast<unsigned char>(v[i + 1]))) i++;
		else break;
	}
	if (nums.empty()) return false;
	// git describe: -<commits>-g<hash>
	size_t g = v.find("-g", i);
	if (i < v.size() && v[i] == '-' && g != std::string::npos && g > i + 1)
	{
		std::string n = v.substr(i + 1, g - i - 1);
		bool digits = !n.empty();
		for (char c : n) digits = digits && isdigit(static_cast<unsigned char>(c));
		if (digits) ahead = atol(n.c_str());
	}
	return true;
}

bool version_is_newer(const std::string &latest_tag, const std::string &running)
{
	std::vector<long> a, b;
	long aa, ba;
	if (!split_version(latest_tag, a, aa) || !split_version(running, b, ba)) return false;
	a.resize(std::max(a.size(), b.size()), 0);
	b.resize(a.size(), 0);
	if (a != b) return a > b;
	return aa > ba;     // same release: a build made after it is not older than it
}

// ---------------------------------------------------------------------------
// notes for the menu font
// ---------------------------------------------------------------------------
std::vector<std::string> news_plain_lines(const std::string &body)
{
	std::vector<std::string> out;
	std::string line;
	bool in_table = false, last_was_heading = false;
	auto flush = [&]()
	{
		// trim
		size_t a = line.find_first_not_of(" \t"), b = line.find_last_not_of(" \t");
		std::string t = a == std::string::npos ? std::string() : line.substr(a, b - a + 1);
		line.clear();
		bool heading = false;
		if (!t.empty() && t[0] == '#') { heading = true; t.erase(0, t.find_first_not_of("# ")); }
		if (!t.empty() && t[0] == '|')
		{
			// table: skip the |---| line, show the cells of the others separated by two spaces
			if (t.find("---") != std::string::npos) return;
			std::string row, cell;
			for (char c : t.substr(1) + "|")
			{
				if (c != '|') { cell += c; continue; }
				size_t ca = cell.find_first_not_of(" \t"), cb = cell.find_last_not_of(" \t");
				if (ca != std::string::npos) row += (row.empty() ? "" : "  ") + cell.substr(ca, cb - ca + 1);
				cell.clear();
			}
			t = row;
			in_table = true;
		}
		else in_table = false;
		if (t.size() > 1 && (t[0] == '*' || t[0] == '-') && t[1] == ' ') t = "- " + t.substr(2);
		std::string p;
		for (size_t i = 0; i < t.size(); i++)
		{
			unsigned char c = static_cast<unsigned char>(t[i]);
			if (c == '`' || c == '*') continue;                       // code and bold marks
			if (c == '[')                                             // [text](link) -> text
			{
				size_t e = t.find("](", i), f = e == std::string::npos ? e : t.find(')', e);
				if (f != std::string::npos) { p += t.substr(i + 1, e - i - 1); i = f; continue; }
			}
			if (c >= 0x80)
			{
				// one '?' per character, not per byte; common typographic marks become ASCII
				size_t n = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
				std::string ch = t.substr(i, n);
				i += n - 1;
				if (ch == "\xe2\x80\x99" || ch == "\xe2\x80\x98") p += '\'';
				else if (ch == "\xe2\x80\x9c" || ch == "\xe2\x80\x9d") p += '"';
				else if (ch == "\xe2\x80\x93" || ch == "\xe2\x80\x94") p += '-';
				else if (ch == "\xe2\x80\xa6") p += "...";
				else p += '?';
				continue;
			}
			if (c < 0x20) continue;
			p += char(c);
		}
		while (!p.empty() && p.back() == ' ') p.pop_back();
		// a line that continues the one before (changelogs are wrapped at 90 columns, and the
		// continuation of a list item is indented): join it, the screen wraps on its own
		bool starts_item = p.rfind("- ", 0) == 0;
		if (!heading && !in_table && !starts_item && !p.empty() && !out.empty() && !out.back().empty() && !last_was_heading)
		{
			out.back() += " " + p;
			return;
		}
		last_was_heading = heading;
		if (heading) { for (char &c : p) c = char(toupper(static_cast<unsigned char>(c))); if (!out.empty() && !out.back().empty()) out.push_back(""); }
		if (p.empty() && (out.empty() || out.back().empty())) return;    // no leading or doubled blank lines
		out.push_back(p);
	};
	for (char c : body)
	{
		if (c == '\n') flush();
		else if (c != '\r') line += c;
	}
	flush();
	(void)in_table;
	while (!out.empty() && out.back().empty()) out.pop_back();
	return out;
}

// ---------------------------------------------------------------------------
// HTTPS GET
// ---------------------------------------------------------------------------
#ifdef _WIN32
static std::wstring widen(const std::string &s)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0);
	std::wstring w(size_t(n), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), w.data(), n);
	return w;
}

std::string news_http_get(const std::string &url, int timeout_ms)
{
	std::string result;
	std::wstring wurl = widen(url);
	URL_COMPONENTS uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256], path[2048];
	uc.lpszHostName = host; uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return result;
	HINTERNET ses = WinHttpOpen(L"NBAJamRecomp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ses) return result;
	WinHttpSetTimeouts(ses, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
	HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
	HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
	                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
	if (req && WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
	    WinHttpReceiveResponse(req, nullptr))
	{
		DWORD status = 0, len = sizeof(status);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
		if (status == 200)
		{
			char buf[4096];
			DWORD got = 0;
			while (WinHttpReadData(req, buf, sizeof(buf), &got) && got > 0 && result.size() < 1000000) result.append(buf, got);
		}
	}
	if (req) WinHttpCloseHandle(req);
	if (con) WinHttpCloseHandle(con);
	WinHttpCloseHandle(ses);
	return result;
}
#else
// No TLS library is linked into the program; the system's curl does the request. If curl is
// not installed there is simply no news.
std::string news_http_get(const std::string &url, int timeout_ms)
{
	std::string result;
	for (char c : url)
		if (!(isalnum(static_cast<unsigned char>(c)) || strchr(":/._-?=&%~+", c))) return result;   // nothing a shell could act on
	char cmd[2600];
	snprintf(cmd, sizeof(cmd), "curl -fsSL --max-time %d -H 'Accept: application/vnd.github+json' -A NBAJamRecomp '%s' 2>/dev/null",
	         std::max(1, timeout_ms / 1000), url.c_str());
	FILE *p = popen(cmd, "r");
	if (!p) return result;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), p)) > 0 && result.size() < 1000000) result.append(buf, n);
	if (pclose(p) != 0) result.clear();
	return result;
}
#endif

// ---------------------------------------------------------------------------
// the one background request
// ---------------------------------------------------------------------------
struct news_feed::state
{
	std::mutex lock;
	bool done = false, taken = false;
	news_item item;
};

void news_feed::start()
{
	if (m_state) return;
	m_state = std::make_shared<state>();
	std::shared_ptr<state> st = m_state;
	const char *file = getenv("NBAJAM_NEWS_FILE"), *env_url = getenv("NBAJAM_NEWS_URL");
	std::string from_file = file ? file : "", url = env_url && *env_url ? env_url : news_default_url();
	std::thread([st, from_file, url]()
	{
		std::string json;
		if (!from_file.empty())
		{
			std::ifstream f(from_file, std::ios::binary);
			json.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		}
		else json = news_http_get(url, 5000);
		news_item n;
		parse_release_json(json, n);
		std::lock_guard<std::mutex> g(st->lock);
		st->item = n;
		st->done = true;
	}).detach();       // never joined: quitting does not wait for a slow network
}

bool news_feed::poll(news_item &out)
{
	if (!m_state) return false;
	std::lock_guard<std::mutex> g(m_state->lock);
	if (!m_state->done || m_state->taken) return false;
	m_state->taken = true;
	if (!m_state->item.ok) return false;
	out = m_state->item;
	return true;
}
