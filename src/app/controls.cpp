// controls.cpp - remappable game controller profiles (see controls.h)
#include "controls.h"
#include "romload.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

const char *const kActionNames[kNumActions] = { "up", "down", "left", "right", "shoot", "pass", "turbo", "start", "coin",
                                                 "shove", "turbo_shot", "custom1", "custom2" };

std::string combo_mask_text(unsigned mask)
{
	std::string s;
	for (int i = 0; i < kNumButtons; i++) if (mask & (1u << i)) s += (s.empty() ? "" : "+") + std::string(kActionNames[i]);
	return s.empty() ? "-" : s;
}

static unsigned parse_mask(const std::string &v)
{
	unsigned m = 0;
	std::stringstream ss(v);
	std::string item;
	while (std::getline(ss, item, '+'))
	{
		size_t a = item.find_first_not_of(" \t"), b = item.find_last_not_of(" \t");
		if (a == std::string::npos) continue;
		item = item.substr(a, b - a + 1);
		for (int i = 0; i < kNumButtons; i++) if (item == kActionNames[i]) m |= 1u << i;
	}
	return m;
}

static std::string trim(std::string s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static fs::path P(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }
static std::string S(const fs::path &p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }

std::string pad_input_name(const pad_input &in)
{
	if (in.axis)
	{
		const char *n = SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis(in.code));
		return std::string(in.sign < 0 ? "-" : "+") + (n ? n : "?");
	}
	const char *n = SDL_GameControllerGetStringForButton(SDL_GameControllerButton(in.code));
	return n ? n : "?";
}

bool pad_input_parse(const std::string &s0, pad_input &out)
{
	std::string s = trim(s0);
	if (s.empty()) return false;
	if (s[0] == '+' || s[0] == '-')
	{
		SDL_GameControllerAxis a = SDL_GameControllerGetAxisFromString(s.substr(1).c_str());
		if (a == SDL_CONTROLLER_AXIS_INVALID) return false;
		out = { true, int(a), s[0] == '-' ? -1 : 1 };
		return true;
	}
	SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(s.c_str());
	if (b == SDL_CONTROLLER_BUTTON_INVALID) return false;
	out = { false, int(b), 1 };
	return true;
}

pad_profile pad_profile::defaults()
{
	pad_profile p;
	auto B = [](SDL_GameControllerButton b) { return pad_input{ false, int(b), 1 }; };
	auto A = [](SDL_GameControllerAxis a, int s) { return pad_input{ true, int(a), s }; };
	p.bind[0] = { B(SDL_CONTROLLER_BUTTON_DPAD_UP), A(SDL_CONTROLLER_AXIS_LEFTY, -1) };
	p.bind[1] = { B(SDL_CONTROLLER_BUTTON_DPAD_DOWN), A(SDL_CONTROLLER_AXIS_LEFTY, 1) };
	p.bind[2] = { B(SDL_CONTROLLER_BUTTON_DPAD_LEFT), A(SDL_CONTROLLER_AXIS_LEFTX, -1) };
	p.bind[3] = { B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT), A(SDL_CONTROLLER_AXIS_LEFTX, 1) };
	p.bind[4] = { B(SDL_CONTROLLER_BUTTON_A), B(SDL_CONTROLLER_BUTTON_Y) };
	p.bind[5] = { B(SDL_CONTROLLER_BUTTON_B) };
	p.bind[6] = { B(SDL_CONTROLLER_BUTTON_X), B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
	              A(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1) };
	p.bind[7] = { B(SDL_CONTROLLER_BUTTON_START) };
	p.bind[8] = { B(SDL_CONTROLLER_BUTTON_BACK) };
	return p;
}

bool pad_profile::pressed(SDL_GameController *c, int action) const
{
	for (const pad_input &in : bind[action])
	{
		if (in.axis)
		{
			int v = SDL_GameControllerGetAxis(c, SDL_GameControllerAxis(in.code));
			if (v * in.sign > deadzone) return true;
		}
		else if (SDL_GameControllerGetButton(c, SDL_GameControllerButton(in.code))) return true;
	}
	return false;
}

unsigned pad_profile::buttons(SDL_GameController *c) const
{
	unsigned m = 0;
	for (int a = 0; a < kNumActions; a++)
		if (pressed(c, a)) m |= a < kNumButtons ? 1u << a : combo[a - kNumButtons];
	return m;
}

std::string pad_profile::action_text(int action) const
{
	std::string s;
	for (const pad_input &in : bind[action]) s += (s.empty() ? "" : ", ") + pad_input_name(in);
	return s.empty() ? "-" : s;
}

void pad_profile::add(int action, const pad_input &in)
{
	auto &v = bind[action];
	auto it = std::find(v.begin(), v.end(), in);
	if (it != v.end()) v.erase(it);
	else if (v.size() < 6) v.push_back(in);
}

std::string pad_profile::serialize() const
{
	std::ostringstream f;
	f << "# NBA JAM Recomp - controller profile\n"
	  << "# inputs: SDL GameController button names (a b x y back guide start leftstick rightstick\n"
	  << "# leftshoulder rightshoulder dpup dpdown dpleft dpright misc1 paddle1-4 touchpad) or an\n"
	  << "# axis with a direction (+leftx -leftx +lefty -lefty +rightx ... +lefttrigger +righttrigger),\n"
	  << "# several separated by commas. Shortcuts (shove, turbo_shot, custom1, custom2) hold several\n"
	  << "# cabinet buttons at once; <name>_buttons lists them, e.g. custom1_buttons=turbo+pass+shoot\n[profile]\nname=" << name << "\ndeadzone=" << deadzone << "\n";
	for (int i = 0; i < kNumActions; i++)
	{
		f << kActionNames[i] << "=";
		for (size_t j = 0; j < bind[i].size(); j++) f << (j ? "," : "") << pad_input_name(bind[i][j]);
		f << "\n";
	}
	for (int c = 0; c < kNumCombos; c++)
		f << kActionNames[kNumButtons + c] << "_buttons=" << (combo[c] ? combo_mask_text(combo[c]) : "") << "\n";
	return f.str();
}

bool pad_profile::parse(const std::string &text, std::string &err)
{
	*this = pad_profile();
	bool header = false, any = false;
	std::istringstream f(text);
	std::string line;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;
		if (line[0] == '[') { header = header || line == "[profile]"; continue; }
		auto eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
		if (k == "name") name = v;
		else if (k.size() > 8 && k.substr(k.size() - 8) == "_buttons")
		{
			for (int c = 0; c < kNumCombos; c++)
				if (k.substr(0, k.size() - 8) == kActionNames[kNumButtons + c]) combo[c] = parse_mask(v);
		}
		else if (k == "deadzone") deadzone = std::clamp(atoi(v.c_str()), 1000, 32000);
		else
			for (int i = 0; i < kNumActions; i++)
				if (k == kActionNames[i])
				{
					any = true;
					std::stringstream ss(v);
					std::string item;
					while (std::getline(ss, item, ','))
					{
						pad_input in;
						if (pad_input_parse(item, in)) add(i, in);
					}
				}
	}
	if (!header || !any) { err = "not a controller profile"; return false; }
	if (!valid_profile_name(name)) { err = "invalid profile name"; return false; }
	return true;
}

bool valid_profile_name(const std::string &n)
{
	if (n.empty() || n.size() > 24) return false;
	for (char c : n)
		if (!(isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')')) return false;
	return n.find_first_not_of(" .") != std::string::npos;
}

std::string profile_store::file_for(const std::string &name) const
{
	std::string f;
	for (char c : name) f += (c == ' ') ? '_' : c;
	return path_join(m_dir, f + ".ini");
}

static bool read_text(const std::string &path, std::string &out)
{
	std::vector<uint8_t> d;
	if (!read_whole_file(path, d)) return false;
	out.assign(d.begin(), d.end());
	return true;
}

static bool write_text(const std::string &path, const std::string &s)
{
	return write_whole_file(path, std::vector<uint8_t>(s.begin(), s.end()));
}

void profile_store::init(const std::string &base)
{
	m_dir = path_join(base, "controller_profiles");
	std::error_code ec;
	fs::create_directories(P(path_join(m_dir, "import")), ec);
	fs::create_directories(P(path_join(m_dir, "export")), ec);
	m_list.clear();
	for (auto &e : fs::directory_iterator(P(m_dir), ec))
	{
		if (!e.is_regular_file() || e.path().extension() != ".ini") continue;
		std::string text, err;
		pad_profile p;
		if (read_text(S(e.path()), text) && p.parse(text, err) && find(p.name) < 0) m_list.push_back(p);
	}
	if (find("Default") < 0) { m_list.insert(m_list.begin(), pad_profile::defaults()); save(m_list.front()); }
	std::stable_sort(m_list.begin(), m_list.end(), [](const pad_profile &a, const pad_profile &b) {
		return (a.name == "Default") > (b.name == "Default") || ((a.name == "Default") == (b.name == "Default") && a.name < b.name);
	});
}

int profile_store::find(const std::string &name) const
{
	for (size_t i = 0; i < m_list.size(); i++) if (m_list[i].name == name) return int(i);
	return -1;
}

const pad_profile &profile_store::get(const std::string &name) const
{
	int i = find(name);
	if (i < 0) i = find("Default");
	if (i < 0) { static const pad_profile d = pad_profile::defaults(); return d; }
	return m_list[size_t(i)];
}

bool profile_store::save(const pad_profile &p)
{
	if (!valid_profile_name(p.name)) return false;
	int i = find(p.name);
	if (i < 0) m_list.push_back(p);
	else if (&m_list[size_t(i)] != &p) m_list[size_t(i)] = p;
	return write_text(file_for(p.name), p.serialize());
}

bool profile_store::remove(const std::string &name)
{
	int i = find(name);
	if (i < 0 || name == "Default") return false;
	std::error_code ec;
	fs::remove(P(file_for(name)), ec);
	m_list.erase(m_list.begin() + i);
	return true;
}

bool profile_store::export_profile(const pad_profile &p, std::string &path_out)
{
	std::string f;
	for (char c : p.name) f += (c == ' ') ? '_' : c;
	path_out = path_join(path_join(m_dir, "export"), f + ".ini");
	return write_text(path_out, p.serialize());
}

std::string profile_store::unique_name(const std::string &n) const
{
	if (find(n) < 0) return n;
	for (int k = 2; k < 1000; k++)
	{
		std::string c = n.substr(0, 19) + " (" + std::to_string(k) + ")";
		if (find(c) < 0) return c;
	}
	return n;
}

// splits a file into its [profile] sections and the optional [setup] section
static void split_sections(const std::string &text, std::vector<std::string> &profiles, std::string &setup)
{
	std::istringstream f(text);
	std::string line, *cur = nullptr;
	while (std::getline(f, line))
	{
		std::string t = trim(line);
		if (t == "[profile]") { profiles.emplace_back(); cur = &profiles.back(); }
		else if (t == "[setup]") cur = &setup;
		else if (!t.empty() && t[0] == '[') cur = nullptr;
		if (cur) *cur += line + "\n";
	}
}

bool profile_store::import_file(const std::string &path, std::string &msg, import_result &res)
{
	res = import_result();
	std::string text, err, setup;
	if (!read_text(path, text)) { msg = "cannot read " + path; return false; }
	std::vector<std::string> secs;
	split_sections(text, secs, setup);
	std::string fname = S(P(path).filename());
	if (secs.empty()) { msg = fname + ": not a controller profile"; return false; }
	std::vector<std::pair<std::string, std::string>> renamed;   // original -> stored name
	for (auto &sec : secs)
	{
		pad_profile p;
		if (!p.parse(sec, err)) { msg = fname + ": " + err; continue; }
		std::string orig = p.name;
		int ex = find(p.name);
		if (ex >= 0 && m_list[size_t(ex)].serialize() == p.serialize())
		{
			renamed.push_back({ orig, orig });      // identical profile already here: reuse it
			res.names.push_back(orig);
			continue;
		}
		p.name = unique_name(p.name);       // never overwrite a different profile with the same name
		if (!save(p)) { msg = "cannot save " + p.name; continue; }
		renamed.push_back({ orig, p.name });
		res.names.push_back(p.name);
	}
	if (res.names.empty()) return false;
	if (!setup.empty())
	{
		std::istringstream f(setup);
		std::string line;
		while (std::getline(f, line))
		{
			auto eq = line.find('=');
			if (eq == std::string::npos) continue;
			std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
			if (k == "game") res.game = v == "all" ? "" : v;
			else if (k.size() == 2 && k[0] == 'p' && k[1] >= '1' && k[1] <= '4')
			{
				std::string nm = v;
				for (auto &r : renamed) if (r.first == v) nm = r.second;
				if (v.empty() || find(nm) >= 0) res.players[k[1] - '1'] = v.empty() ? "" : nm;
			}
		}
		res.setup = true;
	}
	msg.clear();
	for (auto &n : res.names) msg += (msg.empty() ? "" : ", ") + n;
	return true;
}

bool profile_store::export_setup(const std::string &game, const std::string players[4], std::string &path_out)
{
	std::ostringstream f;
	f << "# NBA JAM Recomp - controller setup\n"
	  << "# game: a game id or all; p1..p4: the profile of each player (empty = same as all games)\n"
	  << "[setup]\ngame=" << (game.empty() ? "all" : game) << "\n";
	std::vector<std::string> done;
	for (int i = 0; i < 4; i++) f << "p" << (i + 1) << "=" << players[i] << "\n";
	for (int i = 0; i < 4; i++)
	{
		if (players[i].empty() || std::find(done.begin(), done.end(), players[i]) != done.end() || find(players[i]) < 0) continue;
		done.push_back(players[i]);
		f << "\n" << get(players[i]).serialize();
	}
	path_out = path_join(path_join(m_dir, "export"), "setup_" + (game.empty() ? std::string("all") : game) + ".ini");
	std::string t = f.str();
	return write_text(path_out, t);
}

int profile_store::import_folder(std::string &msg, std::vector<import_result> &setups)
{
	std::string idir = path_join(m_dir, "import"), done = path_join(idir, "done");
	std::error_code ec;
	int n = 0;
	std::vector<fs::path> files;
	for (auto &e : fs::directory_iterator(P(idir), ec))
		if (e.is_regular_file()) files.push_back(e.path());
	std::sort(files.begin(), files.end());
	msg.clear();
	for (auto &f : files)
	{
		std::string m;
		import_result r;
		bool ok = import_file(S(f), m, r);
		if (ok)
		{
			if (r.setup) setups.push_back(r);
			n++;
			fs::create_directories(P(done), ec);
			fs::rename(f, P(done) / f.filename(), ec);
		}
		msg += (msg.empty() ? "" : "; ") + m;
	}
	return n;
}
