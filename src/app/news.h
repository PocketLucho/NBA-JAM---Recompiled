// news.h - latest release notes and the "new version available" notice (roadmap R2.3).
//
// The program asks the public GitHub Releases feed for the latest published release, once,
// on a background thread, when it starts. It only informs: nothing is ever downloaded or
// installed. No answer (offline, feed down, no release yet) simply means no news: no error
// is shown and nothing waits for it. Switched off with news=0 in nbajam_recomp.ini or in
// the main menu.
//
// Test hooks (environment): NBAJAM_NEWS_FILE = read the feed answer from this file instead
// of the network; NBAJAM_NEWS_URL = ask this address instead.
#pragma once
#include <memory>
#include <string>
#include <vector>

struct news_item
{
	bool ok = false;
	std::string tag;      // "v1.2.0"
	std::string name;     // release title
	std::string date;     // YYYY-MM-DD
	std::string body;     // release notes (Markdown)
	std::string url;      // release page
};

// The address asked by default.
const char *news_default_url();

// Reads the answer of GET /repos/<owner>/<repo>/releases/latest. False if it is not one.
bool parse_release_json(const std::string &json, news_item &out);

// Is `latest_tag` a newer version than the one running? Compares the dotted numbers
// ("v1.2.0" against "1.1.3", or "1.2.0-5-gabc1234" for a build made after a release).
// A "dev" build, or anything that cannot be compared, is never told to update.
bool version_is_newer(const std::string &latest_tag, const std::string &running);

// Release notes as plain lines for the 8x8 menu font: Markdown marks removed, list items
// kept as "- text", non-ASCII characters replaced, long lines left for the caller to wrap.
std::vector<std::string> news_plain_lines(const std::string &markdown_body);

// One HTTPS GET with a short time limit. Empty string on any failure.
std::string news_http_get(const std::string &url, int timeout_ms);

class news_feed
{
public:
	// Starts the one request in the background. Returns at once.
	void start();
	// True once, when the answer has arrived and holds a release; fills `out`.
	bool poll(news_item &out);
	bool started() const { return bool(m_state); }

private:
	struct state;
	std::shared_ptr<state> m_state;   // shared with the thread, which may outlive the app
};
