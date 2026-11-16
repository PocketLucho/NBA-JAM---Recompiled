// png_read.h - a small PNG reader for images the user supplies (the bezel of W1.5).
// Reads non-interlaced PNGs of 8 bits per channel: grey, grey+alpha, RGB, RGBA and palette.
// Anything else is refused; the caller then goes on without the image.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// rgba: w*h*4 bytes, top row first. False (and err) if the data is not a PNG this reader handles.
bool png_decode(const std::vector<uint8_t> &file, std::vector<uint8_t> &rgba, int &w, int &h, std::string &err);
