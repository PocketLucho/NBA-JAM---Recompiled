// main.cpp - NBA JAM Recomp: one application for the whole NBA JAM saga.
// Game picker at start-up, SDL2 video/audio/input, per-game NVRAM.
#include "games.h"
#include "font.h"
#include "lang.h"
#include "miniz.h"
#include "gl_present.h"
#include "netplay.h"
#include "shaders.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <ctime>
#include <vector>

// ---------------------------------------------------------------------------
// configuration (nbajam_recomp.ini next to the executable)
// ---------------------------------------------------------------------------
struct key_map
{
	SDL_Scancode up, down, left, right, shoot, pass, turbo, start, coin;
};

struct config
{
	int scale = 3;
	bool fullscreen = false;
	bool smooth = false;          // legacy (SDL renderer fallback)
	bool scanlines = false;       // legacy
	int shader = 3;               // 0 sharp, 1 smooth, 2 scanlines, 3 CRT, 4 CRT curved
	bool integer_scale = true;    // pixel perfect vertical scale
	float scan_strength = 0.6f;
	int aspect = 0;               // 0 = 4:3, 1 = 16:9 (stretched), 2 = 16:10, 3 = fill window
	bool use_opengl = true;
	float volume = 0.8f;
	int net_port = 7845;
	int net_delay = 4;
	int lang = -1;                // -1 = system language, 0 = Spanish, 1 = English
	std::string net_host;
	dip_settings dips;
	key_map keys[4] = {
		{ SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_1, SDL_SCANCODE_5 },
		{ SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_V, SDL_SCANCODE_2, SDL_SCANCODE_6 },
		{ SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_3, SDL_SCANCODE_7 },
		{ SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_H, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_U, SDL_SCANCODE_4, SDL_SCANCODE_8 },
	};
};

static std::string trim(std::string s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static const char *kNames[9] = { "up", "down", "left", "right", "shoot", "pass", "turbo", "start", "coin" };

static SDL_Scancode *key_slot(key_map &k, int i)
{
	SDL_Scancode *s[9] = { &k.up, &k.down, &k.left, &k.right, &k.shoot, &k.pass, &k.turbo, &k.start, &k.coin };
	return s[i];
}

static void save_config(const std::string &path, config &c)
{
	std::ofstream f(path);
	f << "# NBA JAM Recomp - configuration / configuracion\n[general]\n# language: es = castellano, en = English (also in the main menu)\nlanguage="
	  << (c.lang == 1 ? "en" : c.lang == 0 ? "es" : "auto") << "\n\n[video]\nscale=" << c.scale << "\nfullscreen=" << c.fullscreen
	  << "\n# shader: 0 nitido, 1 suavizado, 2 scanlines, 3 CRT, 4 CRT curvo\nshader=" << c.shader
	  << "\n# aspect: 0 = 4:3 original, 1 = 16:9 panoramico, 2 = 16:10, 3 = llenar ventana\naspect=" << c.aspect << "\ninteger_scale=" << c.integer_scale << "\nscanline_strength=" << c.scan_strength
	  << "\n# opengl=0 usa el renderizador SDL basico (sin shaders)\nopengl=" << c.use_opengl
	  << "\n\n[audio]\nvolume=" << c.volume
	  << "\n\n[red]\nport=" << c.net_port << "\ninput_delay=" << c.net_delay << "\nlast_host=" << c.net_host
	  << "\n\n[dips]\nfree_play=" << c.dips.free_play << "\nfour_players=" << c.dips.four_players
	  << "\ncountry=" << c.dips.country << "\nvideo_clips=" << c.dips.video_clips << "\n\n[keys]\n"
	  << "# nombres de tecla SDL (https://wiki.libsdl.org/SDL2/SDL_Scancode)\n";
	for (int p = 0; p < 4; p++)
		for (int i = 0; i < 9; i++)
			f << "p" << (p + 1) << "_" << kNames[i] << "=" << SDL_GetScancodeName(*key_slot(c.keys[p], i)) << "\n";
}

static void load_config(const std::string &path, config &c)
{
	std::ifstream f(path);
	if (!f) { save_config(path, c); return; }
	std::string line;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == '[') continue;
		auto eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
		if (k == "language") c.lang = v == "en" ? 1 : v == "es" ? 0 : -1;
		else if (k == "scale") c.scale = std::clamp(atoi(v.c_str()), 1, 8);
		else if (k == "fullscreen") c.fullscreen = atoi(v.c_str()) != 0;
		else if (k == "smooth") c.smooth = atoi(v.c_str()) != 0;
		else if (k == "scanlines") c.scanlines = atoi(v.c_str()) != 0;
		else if (k == "shader") c.shader = std::clamp(atoi(v.c_str()), 0, kNumShaders - 1);
		else if (k == "aspect") c.aspect = std::clamp(atoi(v.c_str()), 0, 3);
		else if (k == "integer_scale") c.integer_scale = atoi(v.c_str()) != 0;
		else if (k == "scanline_strength") c.scan_strength = std::clamp(float(atof(v.c_str())), 0.0f, 1.0f);
		else if (k == "opengl") c.use_opengl = atoi(v.c_str()) != 0;
		else if (k == "port") c.net_port = std::clamp(atoi(v.c_str()), 1024, 65535);
		else if (k == "input_delay") c.net_delay = std::clamp(atoi(v.c_str()), 1, 10);
		else if (k == "last_host") c.net_host = v;
		else if (k == "volume") c.volume = std::clamp(float(atof(v.c_str())), 0.0f, 1.0f);
		else if (k == "free_play") c.dips.free_play = atoi(v.c_str()) != 0;
		else if (k == "four_players") c.dips.four_players = atoi(v.c_str()) != 0;
		else if (k == "country") c.dips.country = std::clamp(atoi(v.c_str()), 0, 2);
		else if (k == "video_clips") c.dips.video_clips = atoi(v.c_str()) != 0;
		else if (k.size() > 3 && k[0] == 'p' && k[2] == '_')
		{
			int p = k[1] - '1';
			if (p < 0 || p > 3) continue;
			for (int i = 0; i < 9; i++)
				if (k.substr(3) == kNames[i])
				{
					SDL_Scancode sc = SDL_GetScancodeFromName(v.c_str());
					if (sc != SDL_SCANCODE_UNKNOWN) *key_slot(c.keys[p], i) = sc;
				}
		}
	}
}

// ---------------------------------------------------------------------------
// small software text renderer
// ---------------------------------------------------------------------------
struct canvas
{
	int w, h;
	std::vector<u32> px;
	canvas(int W, int H) : w(W), h(H), px(size_t(W) * H, 0xff000000) {}
	void fill(int x, int y, int ww, int hh, u32 c)
	{
		for (int j = std::max(0, y); j < std::min(h, y + hh); j++)
			for (int i = std::max(0, x); i < std::min(w, x + ww); i++) px[size_t(j) * w + i] = c;
	}
	void blend(int x, int y, int ww, int hh, u32 c, int alpha)
	{
		for (int j = std::max(0, y); j < std::min(h, y + hh); j++)
			for (int i = std::max(0, x); i < std::min(w, x + ww); i++)
			{
				u32 &d = px[size_t(j) * w + i];
				auto mix = [&](int sh) { return u32((((d >> sh) & 0xff) * (255 - alpha) + ((c >> sh) & 0xff) * alpha) / 255) << sh; };
				d = 0xff000000 | mix(16) | mix(8) | mix(0);
			}
	}
	void text(int x, int y, const std::string &s, u32 c, int scale = 1, bool shadow = true)
	{
		if (shadow) text(x + scale, y + scale, s, 0xff000000, scale, false);
		for (size_t n = 0; n < s.size(); n++)
		{
			unsigned ch = static_cast<unsigned char>(s[n]);
			if (ch > 127) ch = '?';
			for (int row = 0; row < 8; row++)
			{
				unsigned bits = g_font8[ch][row];
				for (int col = 0; col < 8; col++)
					if (bits & (1u << col))
						fill(x + int(n) * 8 * scale + col * scale, y + row * scale, scale, scale, c);
			}
		}
	}
	int text_w(const std::string &s, int scale = 1) const { return int(s.size()) * 8 * scale; }
	// word-wrapped text that never leaves the canvas; returns the y below the last line
	int text_wrap(int x, int y, const std::string &s, u32 c, int line_h = 10, bool shadow = false)
	{
		size_t maxc = size_t(std::max(1, (w - x - 2) / 8));
		std::string rest = s;
		while (!rest.empty())
		{
			std::string ln = rest;
			if (ln.size() > maxc)
			{
				size_t cut = rest.rfind(' ', maxc);
				if (cut == std::string::npos || cut == 0) cut = maxc;
				ln = rest.substr(0, cut);
				rest = rest.substr(cut);
				while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
			}
			else rest.clear();
			text(x, y, ln, c, 1, shadow);
			y += line_h;
		}
		return y;
	}
};

static const char *shader_name(int i)
{
	static const char *es[] = { "Nitido (pixel perfect)", "Suavizado", "Scanlines", "CRT", "CRT curvo" };
	static const char *en[] = { "Sharp (pixel perfect)", "Smooth", "Scanlines", "CRT", "Curved CRT" };
	i = std::clamp(i, 0, kNumShaders - 1);
	return g_lang_en ? en[i] : es[i];
}

// version = build date (YYYY.MM.DD), taken from the compiler
static std::string app_version()
{
	static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
	const char *d = __DATE__;            // "Mmm dd yyyy"
	int mon = int((strstr(months, std::string(d, 3).c_str()) - months) / 3) + 1;
	int day = atoi(d + 4), year = atoi(d + 7);
	char b[32];
	snprintf(b, sizeof(b), "%04d.%02d.%02d", year, mon, day);
	return b;
}

// ---------------------------------------------------------------------------
// application
// ---------------------------------------------------------------------------
class app
{
public:
	int run();
private:
	enum class screen { menu, net_menu, net_lobby, game, about };

	void open_window();
	void present(const u32 *pixels, int w, int h, bool game);
	void menu_frame();
	void net_menu_frame();
	void net_lobby_frame();
	void about_frame();
	bool game_frame();                 // returns true when an emulated frame was run
	bool start_game(int index, bool net_client = false);
	void stop_game();
	void poll_events();
	cabinet_inputs gather_inputs();
	void draw_overlay(canvas &cv);
	void draw_background(canvas &cv);
	float aspect_value() const { static const float a[4] = { 4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f, 0.0f }; return a[std::clamp(m_cfg.aspect, 0, 3)]; }
	void save_screenshot(const u32 *px, int w, int h);
	void refresh_availability();
	void message(const std::string &m, u32 ms = 2500) { m_message = m; m_message_until = SDL_GetTicks() + ms; }
	std::string nv_path(const game_info &g) const { return path_join(path_join(m_base, "saves"), std::string(g.id) + ".nv"); }

	std::string m_base, m_cfgpath;
	config m_cfg;
	SDL_Window *m_win = nullptr;
	SDL_Renderer *m_ren = nullptr;
	SDL_Texture *m_tex = nullptr;
	gl_presenter m_gl;
	int m_texw = 0, m_texh = 0;
	SDL_AudioDeviceID m_audio = 0;
	std::vector<SDL_GameController *> m_pads;
	std::unique_ptr<game_catalog> m_cat;
	std::unique_ptr<arcade_machine> m_mach;
	screen m_screen = screen::menu;
	int m_cur_game = -1;
	int m_sel = 0;
	bool m_quit = false, m_paused = false, m_show_help = false, m_test = false, m_shot = false;
	std::string m_message;
	u32 m_message_until = 0;
	std::vector<bool> m_avail;
	std::vector<std::string> m_why;
	bool m_menu_up = false, m_menu_down = false, m_menu_left = false, m_menu_right = false, m_menu_ok = false, m_menu_back = false, m_menu_net = false;
	std::vector<float> m_abuf;

	// netplay
	netplay m_net;
	int m_net_sel = 0;
	bool m_editing = false;
	std::string m_edit;
	bool m_net_game = false;           // current game runs under netplay
	std::vector<std::string> m_local_ips;
};

void app::refresh_availability()
{
	m_avail.clear();
	m_why.clear();
	for (const auto &g : game_list())
	{
		std::string why;
		m_avail.push_back(m_cat->available(g, why));
		m_why.push_back(why);
	}
}

void app::open_window()
{
	int w = 400 * m_cfg.scale, h = 300 * m_cfg.scale;
	Uint32 fs = m_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
	if (m_cfg.use_opengl)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		m_win = SDL_CreateWindow("NBA JAM Recomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | fs);
		std::string err;
		if (m_win && m_gl.init(m_win, err)) return;
		if (m_win) SDL_DestroyWindow(m_win);
		m_win = nullptr;
		message(tr("OpenGL no disponible: sin shaders (", "OpenGL not available: no shaders (") + err.substr(0, 60) + ")", 8000);
		SDL_Log("OpenGL: %s", err.c_str());
	}
	m_win = SDL_CreateWindow("NBA JAM Recomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE | fs);
	m_ren = SDL_CreateRenderer(m_win, -1, SDL_RENDERER_ACCELERATED);
	if (!m_ren) m_ren = SDL_CreateRenderer(m_win, -1, SDL_RENDERER_SOFTWARE);
}

void app::present(const u32 *pixels, int w, int h, bool game)
{
	int shader = game ? m_cfg.shader : 0;
	if (m_shot && !game) save_screenshot(pixels, w, h);
	if (m_gl.ok())
	{
		m_gl.present(pixels, w, h, shader, game ? m_cfg.integer_scale : false, m_cfg.scan_strength, game ? aspect_value() : 4.0f / 3.0f);
		return;
	}
	bool smooth = shader == 1;
	if (!m_tex || w != m_texw || h != m_texh || m_cfg.smooth != smooth)
	{
		if (m_tex) SDL_DestroyTexture(m_tex);
		m_cfg.smooth = smooth;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "1" : "0");
		m_tex = SDL_CreateTexture(m_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		m_texw = w; m_texh = h;
	}
	SDL_UpdateTexture(m_tex, nullptr, pixels, w * 4);
	int ww, wh;
	SDL_GetRendererOutputSize(m_ren, &ww, &wh);
	SDL_Rect dst;
	output_rect(ww, wh, w, h, game && m_cfg.integer_scale, game ? aspect_value() : 4.0f / 3.0f, dst.x, dst.y, dst.w, dst.h);
	SDL_SetRenderDrawColor(m_ren, 0, 0, 0, 255);
	SDL_RenderClear(m_ren);
	SDL_RenderCopy(m_ren, m_tex, nullptr, &dst);
	if (shader >= 2 && dst.h >= h * 2)
	{
		SDL_SetRenderDrawBlendMode(m_ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(m_ren, 0, 0, 0, Uint8(140 * m_cfg.scan_strength));
		for (int y = 0; y < h; y++)
		{
			int yy = dst.y + (y * dst.h) / h + (dst.h / h) - 1;
			SDL_RenderDrawLine(m_ren, dst.x, yy, dst.x + dst.w, yy);
		}
	}
	SDL_RenderPresent(m_ren);
}

void app::poll_events()
{
	m_menu_up = m_menu_down = m_menu_left = m_menu_right = m_menu_ok = m_menu_back = m_menu_net = false;
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_QUIT: m_quit = true; break;
			case SDL_CONTROLLERDEVICEADDED:
				if (SDL_GameController *c = SDL_GameControllerOpen(e.cdevice.which)) m_pads.push_back(c);
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				for (size_t i = 0; i < m_pads.size(); i++)
					if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_pads[i])) == e.cdevice.which)
					{
						SDL_GameControllerClose(m_pads[i]);
						m_pads.erase(m_pads.begin() + long(i));
						break;
					}
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) m_menu_up = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) m_menu_down = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) m_menu_left = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) m_menu_right = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A || e.cbutton.button == SDL_CONTROLLER_BUTTON_START) m_menu_ok = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE || (m_screen != screen::game && e.cbutton.button == SDL_CONTROLLER_BUTTON_B)) m_menu_back = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_Y) m_menu_net = true;
				break;
			case SDL_TEXTINPUT:
				if (m_editing)
					for (const char *c = e.text.text; *c; c++)
						if ((isalnum((unsigned char)*c) || *c == '.' || *c == '-' || *c == ':') && m_edit.size() < 60) m_edit += *c;
				break;
			case SDL_KEYDOWN:
			{
				SDL_Keycode k = e.key.keysym.sym;
				bool alt = (e.key.keysym.mod & KMOD_ALT) != 0;
				if (m_editing)
				{
					if (k == SDLK_BACKSPACE && !m_edit.empty()) m_edit.pop_back();
					if (k == SDLK_v && (e.key.keysym.mod & KMOD_CTRL))
						if (char *t = SDL_GetClipboardText()) { for (char *c = t; *c && m_edit.size() < 60; c++) if (!isspace((unsigned char)*c)) m_edit += *c; SDL_free(t); }
					if (k == SDLK_RETURN || k == SDLK_KP_ENTER) m_menu_ok = true;
					if (k == SDLK_ESCAPE) m_menu_back = true;
					break;
				}
				if (k == SDLK_UP) m_menu_up = true;
				if (k == SDLK_DOWN) m_menu_down = true;
				if (k == SDLK_LEFT) m_menu_left = true;
				if (k == SDLK_RIGHT) m_menu_right = true;
				if (k == SDLK_RETURN && !alt) m_menu_ok = true;
				if (k == SDLK_ESCAPE) m_menu_back = true;
				if (k == SDLK_n) m_menu_net = true;
				if (k == SDLK_F11 || (k == SDLK_RETURN && alt))
				{
					m_cfg.fullscreen = !m_cfg.fullscreen;
					SDL_SetWindowFullscreen(m_win, m_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
				}
				if (k == SDLK_F1) m_show_help = !m_show_help;
				if (k == SDLK_F12 || k == SDLK_PRINTSCREEN) m_shot = true;
				if (k == SDLK_F4)
				{
					m_cfg.shader = (m_cfg.shader + 1) % kNumShaders;
					message(std::string("Video: ") + shader_name(m_cfg.shader) + (m_gl.ok() ? "" : tr(" (sin OpenGL: aproximado)", " (no OpenGL: approximate)")));
				}
				if (k == SDLK_F5)
				{
					m_cfg.integer_scale = !m_cfg.integer_scale;
					message(m_cfg.integer_scale ? tr("Escalado entero (pixel perfect): SI", "Integer scaling (pixel perfect): ON") : tr("Escalado entero: NO (llenar pantalla)", "Integer scaling: OFF (fill screen)"));
				}
				if (k == SDLK_F7)
				{
					static const char *es[4] = { "4:3 (original)", "16:9 panoramico", "16:10 panoramico", "llenar la ventana" };
					static const char *en[4] = { "4:3 (original)", "16:9 widescreen", "16:10 widescreen", "fill the window" };
					m_cfg.aspect = (m_cfg.aspect + 1) % 4;
					message(std::string(tr("Aspecto: ", "Aspect: ")) + (g_lang_en ? en : es)[m_cfg.aspect]);
				}
				if (k == SDLK_F6)
				{
					m_cfg.scan_strength = m_cfg.scan_strength >= 0.95f ? 0.2f : m_cfg.scan_strength + 0.2f;
					message(tr("Intensidad de scanlines: ", "Scanline strength: ") + std::to_string(int(m_cfg.scan_strength * 100 + 0.5f)) + "%");
				}
				if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { m_cfg.volume = std::max(0.0f, m_cfg.volume - 0.1f); message(tr("Volumen ", "Volume ") + std::to_string(int(m_cfg.volume * 100 + 0.5f)) + "%", 1200); }
				if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { m_cfg.volume = std::min(1.0f, m_cfg.volume + 0.1f); message(tr("Volumen ", "Volume ") + std::to_string(int(m_cfg.volume * 100 + 0.5f)) + "%", 1200); }
				if (m_mach && !m_net_game)
				{
					if (k == SDLK_F2) { m_test = !m_test; message(m_test ? tr("Interruptor TEST: ON (menu de servicio)", "TEST switch: ON (service menu)") : tr("Interruptor TEST: OFF", "TEST switch: OFF"), 2000); }
					if (k == SDLK_F3) { m_mach->reset(); message("Reset", 1500); }
					if (k == SDLK_p || k == SDLK_PAUSE) m_paused = !m_paused;
				}
				break;
			}
		}
	}
}

cabinet_inputs app::gather_inputs()
{
	cabinet_inputs in{};
	const Uint8 *ks = SDL_GetKeyboardState(nullptr);
	for (int p = 0; p < 4; p++)
	{
		const key_map &k = m_cfg.keys[p];
		auto &q = in.p[p];
		q.up = ks[k.up]; q.down = ks[k.down]; q.left = ks[k.left]; q.right = ks[k.right];
		q.shoot = ks[k.shoot]; q.pass = ks[k.pass]; q.turbo = ks[k.turbo]; q.start = ks[k.start]; q.coin = ks[k.coin];
	}
	for (size_t i = 0; i < m_pads.size() && i < 4; i++)
	{
		SDL_GameController *c = m_pads[i];
		auto &q = in.p[i];
		auto b = [&](SDL_GameControllerButton bt) { return SDL_GameControllerGetButton(c, bt) != 0; };
		int ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
		q.up |= b(SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -12000;
		q.down |= b(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > 12000;
		q.left |= b(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -12000;
		q.right |= b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > 12000;
		q.shoot |= b(SDL_CONTROLLER_BUTTON_A) || b(SDL_CONTROLLER_BUTTON_Y);
		q.pass |= b(SDL_CONTROLLER_BUTTON_B);
		q.turbo |= b(SDL_CONTROLLER_BUTTON_X) || b(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || b(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ||
			SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
		q.start |= b(SDL_CONTROLLER_BUTTON_START);
		q.coin |= b(SDL_CONTROLLER_BUTTON_BACK);
	}
	in.test = m_test;
	in.service = ks[SDL_SCANCODE_9] != 0;
	return in;
}

bool app::start_game(int index, bool net_client)
{
	const game_info &g = game_list()[size_t(index)];
	std::string err;
	SDL_SetWindowTitle(m_win, (std::string(tr("NBA JAM Recomp - cargando ", "NBA JAM Recomp - loading ")) + g.title + "...").c_str());
	m_mach = m_cat->create(g, err);
	if (!m_mach)
	{
		message("Error: " + err, 5000);
		SDL_SetWindowTitle(m_win, "NBA JAM Recomp");
		return false;
	}
	if (!net_client)
	{
		std::vector<u8> nv;
		if (read_whole_file(nv_path(g), nv)) m_mach->set_nvram(nv);
		m_mach->set_dips(m_cfg.dips);
	}
	m_mach->reset();
	m_cur_game = index;
	m_paused = false;
	m_test = false;
	m_screen = screen::game;
	SDL_SetWindowTitle(m_win, (std::string("NBA JAM Recomp - ") + g.title + (m_net_game ? tr("   [EN RED]", "   [ONLINE]") : tr("   [Esc: menu  F1: ayuda]", "   [Esc: menu  F1: help]"))).c_str());
	SDL_ClearQueuedAudio(m_audio);
	if (!m_net_game) message(tr("Pulsa F1 para ver los controles", "Press F1 to see the controls"), 3000);
	return true;
}

void app::stop_game()
{
	// a netplay client never overwrites its own NVRAM with the host's
	bool save = !(m_net_game && m_net.role() == netplay::mode::client);
	if (m_mach && m_cur_game >= 0 && save)
		write_whole_file(nv_path(game_list()[size_t(m_cur_game)]), m_mach->nvram());
	m_mach.reset();
	m_cur_game = -1;
	if (m_net_game)
	{
		std::string err = m_net.error();
		m_net.stop();
		m_net_game = false;
		if (!err.empty()) message(tr("Red: ", "Network: ") + err, 6000);
	}
	m_screen = screen::menu;
	SDL_ClearQueuedAudio(m_audio);
	SDL_SetWindowTitle(m_win, "NBA JAM Recomp");
}

void app::draw_overlay(canvas &cv)
{
	if (m_show_help)
	{
		cv.blend(0, 0, cv.w, cv.h, 0xff000000, 200);
		int y = 6;
		auto line = [&](const std::string &s, u32 c = 0xffffffff) { cv.text(8, y, s, c); y += 11; };
		line(tr("CONTROLES (teclado / mando)", "CONTROLS (keyboard / pad)"), 0xffffd040);
		y += 2;
		line(tr("            tiro/pase/turbo  start moneda", "            shoot/pass/turbo start coin"), 0xffb0b0b0);
		line(tr("J1 flechas  Z / X / C         1     5", "P1 arrows   Z / X / C         1     5"));
		line(tr("J2 WASD     F / G / V         2     6", "P2 WASD     F / G / V         2     6"));
		line(tr("J3 num8462  n0 / n. / nIntro 3     7", "P3 num8462  n0 / n. / nEnter 3     7"));
		line(tr("J4 IJKL     H / ; / U         4     8", "P4 IJKL     H / ; / U         4     8"));
		line(tr("Mando: A tiro B pase X/RB turbo Back moneda", "Pad: A shoot B pass X/RB turbo Back coin"));
		y += 3;
		line(tr("Esc  volver al menu        P  pausa", "Esc  back to menu          P  pause"), 0xffa0e0ff);
		line(tr("F2   interruptor TEST      F3 reset", "F2   TEST switch           F3 reset"), 0xffa0e0ff);
		line(tr("F4   video: nitido/suave/scanl./CRT", "F4   video: sharp/smooth/scanl./CRT"), 0xffa0e0ff);
		line(tr("F5   escalado entero       F6 int. scanlines", "F5   integer scaling       F6 scanline str."), 0xffa0e0ff);
		line(tr("F7   aspecto: 4:3 / 16:9 / 16:10 / llenar", "F7   aspect: 4:3 / 16:9 / 16:10 / fill"), 0xffa0e0ff);
		line(tr("F11  pantalla completa     - / +  volumen", "F11  fullscreen            - / +  volume"), 0xffa0e0ff);
		line(tr("F12  captura de pantalla   9  credito serv.", "F12  screenshot            9  service credit"), 0xffa0e0ff);
		y += 3;
		line(std::string("Video: ") + shader_name(m_cfg.shader) + (m_cfg.integer_scale ? tr(", entero", ", integer") : ""), 0xffb0b0b0);
		line(tr("Teclas configurables en nbajam_recomp.ini", "Keys can be changed in nbajam_recomp.ini"), 0xffb0b0b0);
	}
	if (m_net_game)
	{
		std::string tag = tr("RED J", "NET P") + std::to_string(m_net.local_slot() + 1) + " " + std::to_string(m_net.ping_ms()) + "ms";
		cv.blend(cv.w - cv.text_w(tag) - 6, 0, cv.text_w(tag) + 6, 10, 0xff000000, 140);
		cv.text(cv.w - cv.text_w(tag) - 3, 1, tag, 0xff80ff80, 1, false);
		if (m_net.desynced())
		{
			cv.blend(0, 12, cv.w, 12, 0xff800000, 200);
			cv.text_wrap(4, 14, tr("DESINCRONIZACION: reinicia la partida en red", "DESYNC: restart the online game"), 0xffffffff);
		}
		if (m_net.stall_ms() > 400)
			{
				std::string wt = tr("Esperando a la red...", "Waiting for the network...");
				cv.text(cv.w / 2 - cv.text_w(wt) / 2, cv.h / 2 - 4, wt, 0xffffff40);
			}
	}
	if (m_paused) { std::string pt = tr("PAUSA", "PAUSED"); cv.text(cv.w / 2 - cv.text_w(pt, 2) / 2, cv.h / 2 - 8, pt, 0xffffff40, 2); }
	if (!m_message.empty() && SDL_GetTicks() < m_message_until)
	{
		cv.blend(0, cv.h - 14, cv.w, 14, 0xff000000, 170);
		cv.text(4, cv.h - 11, m_message.substr(0, size_t(cv.w / 8)), 0xffffffff);
	}
}

void app::draw_background(canvas &cv)
{
	for (int y = 0; y < cv.h; y++)
	{
		u32 r = 20 + y / 10, g = 10 + y / 14, b = 40 + y / 5;
		cv.fill(0, y, cv.w, 1, 0xff000000 | (r << 16) | (g << 8) | b);
	}
	u32 t = SDL_GetTicks();
	for (int i = 0; i < 6; i++)
	{
		int x = int((t / 30 + i * 80) % 520) - 60;
		cv.blend(x, 0, 18, cv.h, 0xffff6020, 18);
	}
}

void app::save_screenshot(const u32 *px, int w, int h)
{
	m_shot = false;
	std::vector<u8> rgb(size_t(w) * h * 3);
	for (size_t i = 0; i < size_t(w) * h; i++)
	{
		rgb[i * 3] = u8(px[i] >> 16); rgb[i * 3 + 1] = u8(px[i] >> 8); rgb[i * 3 + 2] = u8(px[i]);
	}
	size_t len = 0;
	void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &len);
	if (!png) { message(tr("No se pudo crear la captura", "Could not create the screenshot"), 3000); return; }
	std::time_t t = std::time(nullptr);
	char stamp[32];
	std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&t));
	std::string name = std::string(m_cur_game >= 0 ? game_list()[size_t(m_cur_game)].id : "menu") + "_" + stamp;
	std::string dir = path_join(m_base, "screenshots"), path;
	for (int n = 0; n < 100; n++)
	{
		path = path_join(dir, name + (n ? "_" + std::to_string(n) : std::string()) + ".png");
		if (!file_exists(path)) break;
	}
	bool ok = write_whole_file(path, std::vector<u8>(static_cast<u8 *>(png), static_cast<u8 *>(png) + len));
	mz_free(png);
	message(ok ? std::string(tr("Captura guardada: screenshots/", "Screenshot saved: screenshots/")) + path.substr(dir.size() + 1)
	           : std::string(tr("No se pudo guardar ", "Could not save ")) + path, 3000);
}

// message bar of the menu screens (two lines, word-wrapped)
static void menu_message(canvas &cv, const std::string &m)
{
	cv.blend(0, cv.h - 46, cv.w, 24, 0xff800000, 200);
	cv.text_wrap(4, cv.h - 43, m.substr(0, 96), 0xffffffff, 10, true);
}

void app::menu_frame()
{
	const auto &list = game_list();
	int ng = int(list.size());
	int n = ng + 2;                       // + language, about
	if (m_menu_up) m_sel = (m_sel + n - 1) % n;
	if (m_menu_down) m_sel = (m_sel + 1) % n;
	if (m_menu_back) { m_quit = true; return; }
	if (m_menu_net && m_sel < ng) { m_screen = screen::net_menu; m_net_sel = 0; m_local_ips = net_local_ipv4(); return; }
	if (m_sel == ng && (m_menu_ok || m_menu_left || m_menu_right))
	{
		g_lang_en = !g_lang_en;
		m_cfg.lang = g_lang_en ? 1 : 0;
		refresh_availability();
		save_config(m_cfgpath, m_cfg);
	}
	else if (m_menu_ok)
	{
		if (m_sel == ng + 1) { m_screen = screen::about; return; }
		if (m_avail[size_t(m_sel)]) { if (start_game(m_sel)) return; }
		else message(m_why[size_t(m_sel)], 6000);
	}

	canvas cv(400, 300);
	draw_background(cv);
	cv.text(cv.w / 2 - cv.text_w("NBA JAM", 4) / 2, 8, "NBA JAM", 0xffff8020, 4);
	cv.text(cv.w / 2 - cv.text_w("RECOMP", 2) / 2, 42, "RECOMP", 0xffffffff, 2);
	std::string pick = tr("Elige un juego", "Choose a game");
	cv.text(cv.w / 2 - cv.text_w(pick) / 2, 61, pick, 0xffc0c0c0);

	int y0 = 76;
	for (int i = 0; i < ng; i++)
	{
		const game_info &g = list[size_t(i)];
		int y = y0 + i * 25;
		bool sel = i == m_sel;
		if (sel) cv.blend(12, y - 3, cv.w - 24, 24, 0xffffa020, 90);
		u32 col = m_avail[size_t(i)] ? (sel ? 0xffffffff : 0xffffe080) : 0xff707070;
		cv.text(20, y, g.title, col);
		cv.text(cv.w - 20 - cv.text_w(g.hardware) - (g.from_source ? 30 : 0), y, g.hardware, m_avail[size_t(i)] ? 0xffc0a0ff : 0xff606060, 1, false);
		if (g.from_source) cv.text(cv.w - 44, y, "SRC", 0xff60ff60);
		cv.text(20, y + 10, std::string(g_lang_en ? g.subtitle_en : g.subtitle).substr(0, 46), m_avail[size_t(i)] ? 0xffa0c8ff : 0xff606060, 1, false);
	}
	{
		int y = y0 + ng * 25 + 2;
		bool sel = m_sel == ng;
		if (sel) cv.blend(12, y - 3, cv.w - 24, 14, 0xffffa020, 90);
		cv.text(20, y, std::string("Idioma / Language: ") + (g_lang_en ? "English" : "Castellano"), sel ? 0xffffffff : 0xffc8d8ff);
		if (sel) cv.text(cv.w - 36, y, "< >", 0xffffffff);
		y += 14;
		sel = m_sel == ng + 1;
		if (sel) cv.blend(12, y - 3, cv.w - 24, 14, 0xffffa020, 90);
		cv.text(20, y, tr("Acerca de NBA JAM Recomp", "About NBA JAM Recomp"), sel ? 0xffffffff : 0xffc8d8ff);
	}
	cv.text(8, cv.h - 21, tr("Enter/A: jugar   N/Y: juego en red", "Enter/A: play   N/Y: online game"), 0xffb0b0b0);
	cv.text(8, cv.h - 11, tr("Esc: salir  F4: video  F11: pant. completa", "Esc: quit  F4: video  F11: fullscreen"), 0xffb0b0b0);
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(16);
}

void app::about_frame()
{
	if (m_menu_back || m_menu_ok) { m_screen = screen::menu; return; }
	canvas cv(400, 300);
	draw_background(cv);
	cv.text(cv.w / 2 - cv.text_w("NBA JAM RECOMP", 2) / 2, 8, "NBA JAM RECOMP", 0xffff8020, 2);
	std::string ver = "Version " + app_version();
	cv.text(cv.w / 2 - cv.text_w(ver) / 2, 26, ver, 0xffffffff);
	int y = 40;
	auto line = [&](const std::string &s, u32 c = 0xffd8e0ff) { y = cv.text_wrap(12, y, s, c); };
	auto gap = [&]() { y += 4; };
	line(tr("Creado por Pocket_lucho con Claude (Anthropic)", "Created by Pocket_lucho with Claude (Anthropic)"), 0xffffe080);
	gap();
	line(tr("Recompilacion de la saga NBA JAM de Midway a partir del codigo fuente original publicado.",
	        "A recompilation of Midway's NBA JAM saga from the original source code that was made public."));
	gap();
	line(tr("Compilados desde la fuente:", "Built from source:"), 0xff80ff80);
	line(tr(" T.E. rev 4.0 (identica a la ROM final)", " T.E. rev 4.0 (identical to the final ROM)"));
	line(tr(" T.E. Special Edition rev 5.02 (identica)", " T.E. Special Edition rev 5.02 (identical)"));
	line(tr(" Maximum Hangtime 1.3 (identica a la ROM)", " Maximum Hangtime 1.3 (identical to the ROM)"));
	gap();
	line(tr("Herramientas: ensamblador TI GSPA propio y LOAD2 de Midway (emu2). Emulacion: nucleos de MAME (TMS34010, 6809, ADSP-2105) e ymfm.",
	        "Tools: own TI GSPA assembler and Midway's LOAD2 (emu2). Emulation: MAME cores (TMS34010, 6809, ADSP-2105) and ymfm."), 0xffc0c8e0);
	gap();
	line(tr("Fuentes: github.com/historicalsource y Asure", "Sources: github.com/historicalsource and Asure"), 0xffb0b0b0);
	line("github.com/PocketLucho/NBA-JAM---Recompiled", 0xffb0b0b0);
	gap();
	line(tr("Si quieres apoyarme ve a", "If you want to support me, go to"), 0xffffc040);
	line("https://buymeacoffee.com/pocket_lucho", 0xffffc040);
	gap();
	line(tr("NBA JAM (c) Midway. Proyecto sin animo de lucro.", "NBA JAM (c) Midway. Non-profit fan project."), 0xffb0b0b0);
	std::string back = tr("Enter / Esc: volver", "Enter / Esc: back");
	cv.text(cv.w / 2 - cv.text_w(back) / 2, cv.h - 11, back, 0xffb0b0b0);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(16);
}

void app::net_menu_frame()
{
	const game_info &g = game_list()[size_t(m_sel)];
	const int nopt = 5;
	bool join_now = false;
	if (m_editing)
	{
		if (m_menu_ok) { m_editing = false; SDL_StopTextInput(); m_cfg.net_host = m_edit; join_now = !m_edit.empty(); }
		else if (m_menu_back) { m_editing = false; SDL_StopTextInput(); }
	}
	else
	{
		if (m_menu_back) { m_screen = screen::menu; return; }
		if (m_menu_up) m_net_sel = (m_net_sel + nopt - 1) % nopt;
		if (m_menu_down) m_net_sel = (m_net_sel + 1) % nopt;
		if (m_net_sel == 1 && (m_menu_left || m_menu_right)) { m_edit = m_cfg.net_host; m_editing = true; SDL_StartTextInput(); }
		if (m_net_sel == 2 && (m_menu_left || m_menu_right)) m_cfg.net_port = std::clamp(m_cfg.net_port + (m_menu_right ? 1 : -1), 1024, 65535);
		if (m_net_sel == 3 && (m_menu_left || m_menu_right)) m_cfg.net_delay = std::clamp(m_cfg.net_delay + (m_menu_right ? 1 : -1), 1, 10);
		if (m_menu_ok)
		{
			std::string err;
			if (m_net_sel == 0)
			{
				if (!m_avail[size_t(m_sel)]) message(m_why[size_t(m_sel)], 6000);
				else if (!m_net.host(u16(m_cfg.net_port), err)) message(tr("Red: ", "Network: ") + err, 6000);
				else { m_screen = screen::net_lobby; return; }
			}
			if (m_net_sel == 1)
			{
				if (m_cfg.net_host.empty()) { m_edit.clear(); m_editing = true; SDL_StartTextInput(); }
				else join_now = true;
			}
			if (m_net_sel == 4) { m_screen = screen::menu; return; }
		}
	}
	if (join_now)
	{
		std::string err;
		if (!m_net.join(m_cfg.net_host, u16(m_cfg.net_port), err)) message(tr("Red: ", "Network: ") + err, 6000);
		else { m_screen = screen::net_lobby; return; }
	}

	canvas cv(400, 300);
	draw_background(cv);
	std::string head = tr("JUEGO EN RED", "ONLINE GAME");
	cv.text(cv.w / 2 - cv.text_w(head, 2) / 2, 12, head, 0xffff8020, 2);
	cv.text(16, 36, g.title, 0xffffe080, 1, false);
	std::string join = tr("Unirse a: ", "Join: ");
	std::string ip = m_editing ? m_edit + ((SDL_GetTicks() / 400) % 2 ? "_" : " ") : (m_cfg.net_host.empty() ? tr("(escribe la IP)", "(type the IP)") : m_cfg.net_host);
	size_t room = size_t((cv.w - 24) / 8) - join.size();
	if (ip.size() > room) ip = ip.substr(ip.size() - room);       // keep the end (the cursor) visible
	const std::string opts[nopt] = {
		tr("Crear partida (anfitrion, J1)", "Host a game (host, P1)"),
		join + ip,
		tr("Puerto UDP: < ", "UDP port: < ") + std::to_string(m_cfg.net_port) + " >",
		tr("Retardo de entrada: < ", "Input delay: < ") + std::to_string(m_cfg.net_delay) + " frames >",
		tr("Volver", "Back"),
	};
	for (int i = 0; i < nopt; i++)
	{
		int y = 56 + i * 20;
		if (i == m_net_sel) cv.blend(12, y - 4, cv.w - 24, 16, 0xffffa020, 90);
		cv.text(20, y, opts[i], i == m_net_sel ? 0xffffffff : 0xffc8d8ff);
	}
	int y = 162;
	y = cv.text_wrap(12, y, tr("El anfitrion elige el juego (el seleccionado en el menu) y debe abrir el puerto UDP en su router, "
	                           "o usar una red virtual (Tailscale, ZeroTier, Radmin VPN...). Todos necesitan las mismas ROMs. "
	                           "Retardo 3-4 en LAN, 5-7 por internet; lo decide el anfitrion.",
	                           "The host picks the game (the one selected in the menu) and must open the UDP port on the router, "
	                           "or use a virtual LAN (Tailscale, ZeroTier, Radmin VPN...). Everybody needs the same ROMs. "
	                           "Delay 3-4 on a LAN, 5-7 over the internet; the host decides."), 0xffa0a0a0);
	std::string ips;
	for (auto &s : m_local_ips) ips += (ips.empty() ? "" : ", ") + s;
	y = cv.text_wrap(12, y + 3, tr("Tu IP local: ", "Your local IP: ") + (ips.empty() ? std::string("?") : ips), 0xff80ff80);
	cv.text_wrap(12, y + 3, m_editing ? tr("Escribe la IP y pulsa Enter (Ctrl+V pega)", "Type the IP and press Enter (Ctrl+V pastes)")
	                                  : tr("Enter: elegir  Izq/Der: cambiar  Esc: volver", "Enter: select  Left/Right: change  Esc: back"), 0xffb0b0b0);
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(16);
}

void app::net_lobby_frame()
{
	m_net.poll();
	if (!m_net.active())
	{
		message(tr("Red: ", "Network: ") + m_net.error(), 6000);
		m_screen = screen::net_menu;
		return;
	}
	if (m_menu_back) { m_net.stop(); m_screen = screen::net_menu; return; }
	const game_info &g = game_list()[size_t(m_sel)];
	bool host = m_net.role() == netplay::mode::host;

	if (host && m_menu_ok)
	{
		m_net_game = true;
		if (!start_game(m_sel)) { m_net_game = false; m_net.stop(); m_screen = screen::net_menu; return; }
		m_net.host_start(g.id, m_mach->fingerprint(), m_cfg.dips, m_mach->nvram(), m_cfg.net_delay);
		message(tr("Partida en red: ", "Online game: ") + std::to_string(m_net.players()) + tr(" jugadores", " players"), 3000);
		return;
	}
	if (!host && m_net.start_received())
	{
		int idx = -1;
		for (size_t i = 0; i < game_list().size(); i++) if (m_net.start_game_id() == game_list()[i].id) idx = int(i);
		std::string why;
		if (idx < 0) why = tr("juego desconocido: ", "unknown game: ") + m_net.start_game_id();
		else if (!m_avail[size_t(idx)]) why = std::string(tr("no tienes ", "you don't have ")) + game_list()[size_t(idx)].title;
		if (why.empty())
		{
			m_net_game = true;
			if (!start_game(idx, true)) why = tr("no se pudo cargar el juego", "the game could not be loaded");
			else if (m_mach->fingerprint() != m_net.start_fingerprint()) why = tr("tus ROMs no coinciden con las del anfitrion", "your ROMs are not the same as the host's");
		}
		if (!why.empty())
		{
			m_net.client_ready(false, why);
			m_net.poll();
			m_mach.reset();
			m_net_game = false;
			m_net.stop(why.c_str());
			message(tr("Red: ", "Network: ") + why, 6000);
			m_screen = screen::net_menu;
			return;
		}
		m_mach->set_nvram(m_net.start_nvram());
		m_mach->set_dips(m_net.start_dips());
		m_mach->reset();
		m_net.client_ready(true, "");
		return;
	}

	canvas cv(400, 300);
	draw_background(cv);
	std::string head = tr("JUEGO EN RED", "ONLINE GAME");
	cv.text(cv.w / 2 - cv.text_w(head, 2) / 2, 12, head, 0xffff8020, 2);
	int y = 40;
	auto line = [&](const std::string &s, u32 c = 0xffffffff) { y = cv.text_wrap(16, y, s, c, 12, true); };
	if (host)
	{
		line(std::string(tr("Anfitrion: ", "Host: ")) + g.title, 0xffffe080);
		std::string ips;
		for (auto &s : m_local_ips) ips += (ips.empty() ? "" : ", ") + s;
		line("IP: " + ips, 0xff80ff80);
		line(tr("Puerto UDP: ", "UDP port: ") + std::to_string(m_cfg.net_port) + tr("   retardo: ", "   delay: ") + std::to_string(m_cfg.net_delay), 0xff80ff80);
		y += 6;
		line(tr("Jugadores: ", "Players: ") + std::to_string(m_net.players()) + " / 4");
		line(tr("J1  tu (anfitrion)", "P1  you (host)"), 0xffc8d8ff);
		for (auto &p : m_net.peer_list()) line(p, 0xffc8d8ff);
		y = 240;
		line(tr("Enter: empezar     Esc: cancelar", "Enter: start     Esc: cancel"), 0xffb0b0b0);
		line(tr("(puedes empezar solo para probar)", "(you can start alone to try it)"), 0xff808080);
	}
	else
	{
		line(tr("Conectando con ", "Connecting to ") + m_cfg.net_host + ":" + std::to_string(m_cfg.net_port), 0xffffe080);
		y += 6;
		if (m_net.local_slot() < 0) line(tr("Esperando respuesta", "Waiting for an answer") + std::string((SDL_GetTicks() / 400) % 4, '.'));
		else
		{
			line(tr("Conectado como jugador ", "Connected as player ") + std::to_string(m_net.local_slot() + 1) + "  (" + std::to_string(m_net.ping_ms()) + " ms)", 0xff80ff80);
			line(tr("Jugadores: ", "Players: ") + std::to_string(m_net.players()));
			line(m_net.stage() == netplay::phase::transfer ? tr("Recibiendo datos de la partida...", "Receiving the game data...")
			                                               : tr("Esperando a que el anfitrion empiece...", "Waiting for the host to start..."));
		}
		y = 240;
		line(tr("Usas los controles de J1 / primer mando", "You use the P1 keys / first pad"), 0xffb0b0b0);
		line(tr("Esc: cancelar", "Esc: cancel"), 0xffb0b0b0);
	}
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(10);
}

bool app::game_frame()
{
	if (m_menu_back) { stop_game(); return false; }
	bool ran = false;
	if (m_net_game)
	{
		m_net.poll();
		if (!m_net.active()) { stop_game(); return false; }
		if (m_net.can_run())
		{
			cabinet_inputs local = gather_inputs();
			cabinet_inputs in = m_net.frame_inputs(local.p[0]);
			m_mach->set_inputs(in);
			m_mach->run_frame();
			m_net.frame_done(m_mach->state_hash());
			ran = true;
		}
	}
	else if (!m_paused)
	{
		m_mach->set_inputs(gather_inputs());
		m_mach->run_frame();
		ran = true;
	}
	if (ran)
	{
		m_abuf.clear();
		m_mach->take_audio(m_abuf);
		if (m_audio && !m_abuf.empty())
		{
			for (float &v : m_abuf) v = std::clamp(v * m_cfg.volume * 1.6f, -1.0f, 1.0f);
			// keep latency bounded (about 120 ms)
			if (SDL_GetQueuedAudioSize(m_audio) < 48000 * 4 / 8)
				SDL_QueueAudio(m_audio, m_abuf.data(), Uint32(m_abuf.size() * sizeof(float)));
		}
	}
	int w = m_mach->width(), h = m_mach->height();
	canvas cv(w, h);
	std::memcpy(cv.px.data(), m_mach->frame(), size_t(w) * h * 4);
	if (m_shot) save_screenshot(cv.px.data(), w, h);      // the game picture, without the overlay
	draw_overlay(cv);
	present(cv.px.data(), w, h, true);
	return ran;
}

int app::run()
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "NBA JAM Recomp", SDL_GetError(), nullptr);
		return 1;
	}
	char *bp = SDL_GetBasePath();
	m_base = bp ? bp : ".";
	SDL_free(bp);
	m_cfgpath = path_join(m_base, "nbajam_recomp.ini");
	load_config(m_cfgpath, m_cfg);
	if (m_cfg.lang < 0)
	{
		// first run: follow the system language (Spanish -> castellano, anything else -> English)
		g_lang_en = true;
		if (SDL_Locale *loc = SDL_GetPreferredLocales())
		{
			if (loc[0].language && std::string(loc[0].language) == "es") g_lang_en = false;
			SDL_free(loc);
		}
	}
	else g_lang_en = m_cfg.lang == 1;
	m_cat = std::make_unique<game_catalog>(m_base);

	open_window();
	SDL_StopTextInput();
	SDL_AudioSpec want{}, have{};
	want.freq = 48000;
	want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = 1024;
	m_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	if (m_audio) SDL_PauseAudioDevice(m_audio, 0);

	refresh_availability();
	// start on the first available game
	for (size_t i = 0; i < m_avail.size(); i++) if (m_avail[i]) { m_sel = int(i); break; }

	using clock = std::chrono::steady_clock;
	auto next = clock::now();
	while (!m_quit)
	{
		poll_events();
		if (m_quit) break;
		switch (m_screen)
		{
			case screen::menu: menu_frame(); next = clock::now(); continue;
			case screen::net_menu: net_menu_frame(); next = clock::now(); continue;
			case screen::net_lobby: net_lobby_frame(); next = clock::now(); continue;
			case screen::about: about_frame(); next = clock::now(); continue;
			case screen::game: break;
		}
		bool ran = game_frame();
		if (m_screen != screen::game) continue;
		if (m_net_game && !ran)
		{
			// waiting for the network: don't advance the clock, poll again shortly
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		// pace to the game's own refresh rate (~54.7 Hz on these boards)
		double hz = m_mach->refresh_hz();
		if (hz < 30 || hz > 120) hz = 60;
		next += std::chrono::nanoseconds(static_cast<long long>(1e9 / hz));
		auto now = clock::now();
		// netplay: if frames are piling up (we are behind the others), skip the wait
		bool behind = m_net_game && m_net.buffered() > m_net.delay() + 1;
		if (next > now && !behind) std::this_thread::sleep_for(next - now);
		else if (now - next > std::chrono::milliseconds(100)) next = now;
	}
	if (m_screen == screen::game) stop_game();
	m_net.stop();
	save_config(m_cfgpath, m_cfg);
	for (auto *c : m_pads) SDL_GameControllerClose(c);
	if (m_audio) SDL_CloseAudioDevice(m_audio);
	if (m_tex) SDL_DestroyTexture(m_tex);
	m_gl.shutdown();
	if (m_ren) SDL_DestroyRenderer(m_ren);
	SDL_DestroyWindow(m_win);
	SDL_Quit();
	return 0;
}

int main(int, char **)
{
	SDL_SetMainReady();
	try
	{
		app a;
		return a.run();
	}
	catch (const std::exception &e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "NBA JAM Recomp", e.what(), nullptr);
		return 1;
	}
}
