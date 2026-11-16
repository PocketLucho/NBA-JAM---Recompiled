// overlay.h - overlay / border images around the 4:3 picture, in the manner of RetroArch.
//
// The user's images live in <data folder>/overlays/. An entry is either
//   * a PNG file, or
//   * a RetroArch overlay .cfg: its first overlay image is used (overlay0_overlay = "x.png",
//     path relative to the .cfg). Input buttons described in such a file are ignored; only
//     the picture is taken, which is all a border / bezel overlay is.
// A PNG that a .cfg in the same folder points to is listed once, under the .cfg's name.
// bezel.png in the data folder itself (the first form of this feature) is listed too.
//
// How an image is shown is decided by the presenter (gl_present.h): scaled to the height of
// the screen, centred; over the game if it has a transparent opening, behind it otherwise.
// No SDL in here.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct overlay_entry
{
	std::string name;    // what the user sees and what is saved: file name without extension
	std::string image;   // the PNG to load
};

// Sorted by name. Missing folder = empty list.
std::vector<overlay_entry> list_overlays(const std::string &data_dir);

// The image a RetroArch .cfg points to, or "" if it names none.
std::string overlay_image_from_cfg(const std::string &cfg_path);

// Loads the image of an entry. False and err if it cannot be read.
bool load_overlay(const overlay_entry &e, std::vector<uint8_t> &rgba, int &w, int &h, std::string &err);
