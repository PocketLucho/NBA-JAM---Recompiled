// ui_sdl.h - SDL events into the toolkit's raw events (U1.3). Header only; the app is the
// only user. Pointer positions are converted from window pixels to canvas units with the
// rectangle the presenter drew the menu into.
#pragma once
#include "ui.h"
#include "../view.h"
#include <SDL.h>

namespace ui
{

inline device pad_family(SDL_GameController *c)
{
	if (!c) return device::xbox;
#if SDL_VERSION_ATLEAST(2, 0, 12)
	switch (SDL_GameControllerGetType(c))
	{
	case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5: return device::playstation;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return device::nintendo;
	default: return device::xbox;
	}
#else
	return device::xbox;
#endif
}

// Returns false for events the menus do not use. `shown` is where the 400 x 300 menu
// picture is in the window (compute_view), win_w / win_h the window in the same units as
// the mouse position.
inline bool from_sdl(const SDL_Event &e, const view_rect &shown, int win_w, int win_h, raw_event &out)
{
	auto to_canvas = [&](float px, float py) {
		out.x = (px - shown.x) * float(screen::kW) / float(shown.w > 0 ? shown.w : 1);
		out.y = (py - shown.y) * float(screen::kH) / float(shown.h > 0 ? shown.h : 1);
	};
	switch (e.type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	{
		if (e.type == SDL_KEYDOWN && e.key.repeat) return false;      // the toolkit does its own repeat
		out.kind = e.type == SDL_KEYDOWN ? raw_event::key_down : raw_event::key_up;
		out.shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
		switch (e.key.keysym.sym)
		{
		case SDLK_UP: out.k = key::up; break;
		case SDLK_DOWN: out.k = key::down; break;
		case SDLK_LEFT: out.k = key::left; break;
		case SDLK_RIGHT: out.k = key::right; break;
		case SDLK_RETURN: case SDLK_KP_ENTER: out.k = key::enter; break;
		case SDLK_SPACE: out.k = key::space; break;
		case SDLK_ESCAPE: out.k = key::escape; break;
		case SDLK_BACKSPACE: out.k = key::backspace; break;
		case SDLK_TAB: out.k = key::tab; break;
		case SDLK_PAGEUP: out.k = key::page_up; break;
		case SDLK_PAGEDOWN: out.k = key::page_down; break;
		case SDLK_q: out.k = key::q; break;
		case SDLK_e: out.k = key::e; break;
		case SDLK_DELETE: out.k = key::del; break;
		default: return false;
		}
		return true;
	}
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	{
		out.kind = e.type == SDL_CONTROLLERBUTTONDOWN ? raw_event::pad_down : raw_event::pad_up;
		out.dev = pad_family(SDL_GameControllerFromInstanceID(e.cbutton.which));
		switch (e.cbutton.button)
		{
		case SDL_CONTROLLER_BUTTON_DPAD_UP: out.p = pad::up; break;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: out.p = pad::down; break;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: out.p = pad::left; break;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: out.p = pad::right; break;
		case SDL_CONTROLLER_BUTTON_A: out.p = pad::south; break;
		case SDL_CONTROLLER_BUTTON_B: out.p = pad::east; break;
		case SDL_CONTROLLER_BUTTON_X: out.p = pad::west; break;
		case SDL_CONTROLLER_BUTTON_Y: out.p = pad::north; break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: out.p = pad::l1; break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: out.p = pad::r1; break;
		case SDL_CONTROLLER_BUTTON_START: out.p = pad::start; break;
		case SDL_CONTROLLER_BUTTON_BACK: out.p = pad::select; break;
		default: return false;
		}
		return true;
	}
	case SDL_CONTROLLERAXISMOTION:
	{
		SDL_GameController *c = SDL_GameControllerFromInstanceID(e.caxis.which);
		out.dev = pad_family(c);
		if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
		{
			out.kind = raw_event::pad_stick;
			out.x = c ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f : 0;
			out.y = c ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f : 0;
			return true;
		}
		if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
		{
			// triggers page through long lists: treated as buttons at half travel
			static bool held[2] = { false, false };
			int i = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
			bool now = e.caxis.value > 16000;
			if (now == held[i]) return false;
			held[i] = now;
			out.kind = now ? raw_event::pad_down : raw_event::pad_up;
			out.p = i ? pad::r2 : pad::l2;
			return true;
		}
		return false;
	}
	case SDL_MOUSEMOTION:
		if (e.motion.which == SDL_TOUCH_MOUSEID) return false;        // touch arrives as finger events
		out.kind = raw_event::mouse_move; to_canvas(float(e.motion.x), float(e.motion.y)); return true;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) return false;
		out.kind = e.type == SDL_MOUSEBUTTONDOWN ? raw_event::mouse_down : raw_event::mouse_up;
		to_canvas(float(e.button.x), float(e.button.y));
		return true;
	case SDL_MOUSEWHEEL:
		out.kind = raw_event::wheel; out.x = 0; out.y = float(e.wheel.y); return true;
	case SDL_FINGERDOWN:
	case SDL_FINGERMOTION:
	case SDL_FINGERUP:
		out.kind = e.type == SDL_FINGERDOWN ? raw_event::touch_down : e.type == SDL_FINGERMOTION ? raw_event::touch_move : raw_event::touch_up;
		to_canvas(e.tfinger.x * float(win_w), e.tfinger.y * float(win_h));   // finger positions are 0..1 of the window
		return true;
	default:
		return false;
	}
}

} // namespace ui
