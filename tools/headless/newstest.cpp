// newstest.cpp - tests of the release news reader (src/app/news.cpp). No ROM, no SDL, no network.
// usage: newstest        (exit code 0 = all passed)
#include "news.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

static int g_fail = 0, g_run = 0;
#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; printf("FAIL  %s  (%s:%d)\n", msg, __FILE__, __LINE__); } else printf("ok    %s\n", msg); } while (0)

// shaped like the answer of api.github.com/repos/<owner>/<repo>/releases/latest
static const char *kJson = R"({
  "url": "https://api.github.com/repos/o/r/releases/1",
  "html_url": "https://github.com/o/r/releases/tag/v1.2.0",
  "author": { "login": "someone", "name": "not the release name", "tag_name": "nested" },
  "tag_name": "v1.2.0",
  "name": "NBA JAM Recomp 1.2.0",
  "draft": false,
  "published_at": "2026-12-01T10:20:30Z",
  "assets": [ { "name": "NBAJamRecomp-v1.2.0-windows-x64.zip", "body": "nested too" } ],
  "body": "# NBA JAM Recomp 1.2.0\r\n\r\nBig \"summary\" — with a dash.\r\n\r\n## Added\r\n- **Bold** thing with `code`\r\n- A [link](https://example.com/x) here\r\n\r\n| System | File |\r\n|---|---|\r\n| Windows | `a.zip` |\r\n"
})";

int main()
{
	news_item n;
	CHECK(parse_release_json(kJson, n) && n.ok, "a release answer is read");
	CHECK(n.tag == "v1.2.0" && n.name == "NBA JAM Recomp 1.2.0", "tag and name come from the top level, not from nested objects");
	CHECK(n.date == "2026-12-01" && n.url == "https://github.com/o/r/releases/tag/v1.2.0", "date and page address");
	CHECK(n.body.find("Big \"summary\"") != std::string::npos, "escaped quotes in the notes");

	news_item bad;
	CHECK(!parse_release_json("", bad) && !bad.ok, "empty answer (offline) is not news");
	CHECK(!parse_release_json("{\"message\":\"Not Found\"}", bad), "\"Not Found\" (no release yet) is not news");
	CHECK(!parse_release_json("<html>captive portal</html>", bad), "a web page instead of the feed is not news");
	CHECK(!parse_release_json("{\"tag_name\":\"v1", bad), "a cut-off answer is not news");

	auto lines = news_plain_lines(n.body);
	auto has = [&](const std::string &t) { for (auto &l : lines) if (l == t) return true; return false; };
	CHECK(has("NBA JAM RECOMP 1.2.0") && has("ADDED"), "headings become upper-case lines");
	CHECK(has("Big \"summary\" - with a dash."), "typographic dash becomes ASCII");
	CHECK(has("- Bold thing with code"), "bold and code marks are removed");
	CHECK(has("- A link here"), "links keep their text only");
	CHECK(has("Windows  a.zip") && !has("|---|---|"), "tables become plain rows");
	bool ascii = true;
	for (auto &l : lines) for (char c : l) ascii = ascii && static_cast<unsigned char>(c) < 0x80 && static_cast<unsigned char>(c) >= 0x20;
	CHECK(ascii, "every line is printable ASCII for the menu font");

	CHECK(version_is_newer("v1.2.0", "1.1.9"), "1.2.0 is newer than 1.1.9");
	CHECK(version_is_newer("v1.10.0", "1.9.0"), "numbers are compared as numbers (1.10 > 1.9)");
	CHECK(!version_is_newer("v1.2.0", "1.2.0"), "the same version is not new");
	CHECK(!version_is_newer("v1.2.0", "1.2.0-5-gabc1234"), "a build made after the release is not told to update to it");
	CHECK(version_is_newer("v1.2.1", "1.2.0-5-gabc1234"), "but it is told about the next release");
	CHECK(!version_is_newer("v1.2.0", "1.3.0"), "an older release is not new");
	CHECK(version_is_newer("v2026.12.01", "2026.11.06-39-g5fb654c"), "date-style versions compare too");
	CHECK(!version_is_newer("v1.2.0", "dev") && !version_is_newer("v1.2.0", "dev-abc1234"), "a dev build is never told to update");
	CHECK(!version_is_newer("nightly", "1.0.0"), "a tag that is not a version is ignored");

	// the background request, fed from a file instead of the network
	namespace fs = std::filesystem;
	fs::path f = fs::temp_directory_path() / "nbajam_newstest.json";
	{ std::ofstream(f, std::ios::binary) << kJson; }
#ifdef _WIN32
	_putenv_s("NBAJAM_NEWS_FILE", f.string().c_str());
#else
	setenv("NBAJAM_NEWS_FILE", f.string().c_str(), 1);
#endif
	news_feed feed;
	auto t0 = std::chrono::steady_clock::now();
	feed.start();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
	CHECK(ms < 100, "start() returns at once");
	news_item got;
	bool arrived = false;
	for (int i = 0; i < 200 && !arrived; i++) { arrived = feed.poll(got); if (!arrived) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
	CHECK(arrived && got.tag == "v1.2.0", "the answer arrives through poll()");
	CHECK(!feed.poll(got), "and is reported once");

	fs::remove(f);
	news_feed none;                     // the file is gone: same as no network
	none.start();
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	CHECK(!none.poll(got), "no answer: no news and no error");

	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
