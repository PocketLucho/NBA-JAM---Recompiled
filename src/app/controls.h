// controls.h - remappable game controller bindings, saved as named profiles.
// The same nine cabinet actions are used by every game in the collection, so a
// profile works for all of them. Each player slot (P1..P4) picks a profile.
// Profiles are plain text files in controller_profiles/ (one per profile) and
// can be exported to controller_profiles/export/ or imported from
// controller_profiles/import/ (or by dropping a file onto the window).
#pragma once
#include <SDL.h>
#include <string>
#include <vector>

// 9 cabinet buttons, then shortcuts: one pad input that holds several cabinet
// buttons at once (e.g. shove = turbo + pass, the same for every game)
enum { kNumButtons = 9, kNumCombos = 4, kNumActions = kNumButtons + kNumCombos };
enum { A_UP, A_DOWN, A_LEFT, A_RIGHT, A_SHOOT, A_PASS, A_TURBO, A_START, A_COIN, A_SHOVE, A_TURBO_SHOT, A_CUSTOM1, A_CUSTOM2 };
extern const char *const kActionNames[kNumActions];
std::string combo_mask_text(unsigned mask);                  // "turbo+pass"

// one physical input on a pad: a button, or one direction of an axis
struct pad_input
{
	bool axis = false;
	int code = 0;            // SDL_GameControllerButton or SDL_GameControllerAxis
	int sign = 1;            // axis direction: +1 or -1
	bool operator==(const pad_input &o) const { return axis == o.axis && code == o.code && (!axis || sign == o.sign); }
};

std::string pad_input_name(const pad_input &in);           // "a", "dpup", "-leftx", "+righttrigger"
bool pad_input_parse(const std::string &s, pad_input &out);

struct pad_profile
{
	std::string name = "Default";
	int deadzone = 12000;                                    // axis threshold (0..32767)
	std::vector<pad_input> bind[kNumActions];
	unsigned combo[kNumCombos] = { (1u << A_TURBO) | (1u << A_PASS), (1u << A_TURBO) | (1u << A_SHOOT), 0, 0 };   // cabinet buttons per shortcut

	static pad_profile defaults();                           // the original fixed layout
	bool pressed(SDL_GameController *c, int action) const;
	unsigned buttons(SDL_GameController *c) const;            // cabinet buttons held (bit = action), shortcuts expanded
	std::string action_text(int action) const;               // "a, y" or "-" when unbound
	void add(int action, const pad_input &in);               // adds (a second press of the same input removes it)
	std::string serialize() const;
	bool parse(const std::string &text, std::string &err);
};

bool valid_profile_name(const std::string &n);

// a "game setup" file: the profiles one game uses for each player, bundled together
struct import_result
{
	std::vector<std::string> names;                          // profiles added (after renaming on clashes)
	std::string game;                                        // setup bundle: game id ("" = all games)
	std::string players[4];                                  // setup bundle: (renamed) profile per player
	bool setup = false;
};

class profile_store
{
public:
	void init(const std::string &base);                      // loads controller_profiles/*.ini
	std::vector<pad_profile> &list() { return m_list; }
	int find(const std::string &name) const;                 // -1 if missing
	const pad_profile &get(const std::string &name) const;   // falls back to Default
	bool save(const pad_profile &p);
	bool remove(const std::string &name);
	bool export_profile(const pad_profile &p, std::string &path_out);
	// one file with the player->profile assignment of a game (or "" = all games) and those profiles
	bool export_setup(const std::string &game, const std::string players[4], std::string &path_out);
	// imports every file in controller_profiles/import/ (moved to import/done/ afterwards)
	int import_folder(std::string &msg, std::vector<import_result> &setups);
	bool import_file(const std::string &path, std::string &msg, import_result &res);
	std::string dir() const { return m_dir; }
private:
	std::string unique_name(const std::string &n) const;
	std::string file_for(const std::string &name) const;
	std::string m_dir;
	std::vector<pad_profile> m_list;
};
