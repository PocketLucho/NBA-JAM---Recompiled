// shaders.h - GLSL 1.10 / GLSL ES 1.00 shaders for the video output.
// Each fragment shader gets:
//   u_tex  source frame (GL_LINEAR), u_src source size in pixels,
//   u_out  size of the output rectangle in window pixels, u_opt per-shader option.
#pragma once

static const char *kVertexShader = R"GLSL(
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() { v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }
)GLSL";

static const char *kFragHeader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
uniform sampler2D u_tex;
uniform vec2 u_src;
uniform vec2 u_out;
uniform float u_opt;
varying vec2 v_uv;
)GLSL";

struct shader_def { const char *name; const char *src; };

// 0 - sharp: nearest-neighbour look without uneven pixels at any scale (sharp bilinear)
static const char *kSharp = R"GLSL(
void main()
{
	vec2 texel = v_uv * u_src;
	vec2 scale = max(floor(u_out / u_src), vec2(1.0));
	vec2 range = 0.5 - 0.5 / scale;
	vec2 d = fract(texel) - 0.5;
	vec2 f = (d - clamp(d, -range, range)) * scale + 0.5;
	gl_FragColor = vec4(texture2D(u_tex, (floor(texel) + f) / u_src).rgb, 1.0);
}
)GLSL";

// 1 - smooth: plain bilinear
static const char *kSmooth = R"GLSL(
void main() { gl_FragColor = vec4(texture2D(u_tex, v_uv).rgb, 1.0); }
)GLSL";

// 2 - scanlines: pixel-perfect when the vertical scale is an integer; every source line
// becomes N output rows with a beam profile (brighter pixels get a wider beam).
static const char *kScanlines = R"GLSL(
vec3 sharpfetch(vec2 texel)
{
	vec2 scale = max(floor(u_out / u_src), vec2(1.0));
	float range = 0.5 - 0.5 / scale.x;
	float d = fract(texel.x) - 0.5;
	float fx = (d - clamp(d, -range, range)) * scale.x + 0.5;
	return texture2D(u_tex, vec2(floor(texel.x) + fx, floor(texel.y) + 0.5) / u_src).rgb;
}
void main()
{
	vec2 texel = v_uv * u_src;
	vec3 c = sharpfetch(texel);
	float n = u_out.y / u_src.y;                    // output rows per source line
	float strength = u_opt;                          // 0..1
	float w;
	if (abs(n - floor(n + 0.5)) < 0.01 && n < 6.5)
	{
		// integer scale: one beam centre row per line, exact per-row weights
		float rows = floor(n + 0.5);
		float k = floor(fract(texel.y) * rows);
		float centre = floor((rows - 1.0) * 0.5);
		float dist = abs(k - centre) / rows;
		w = exp(-dist * dist * 14.0);
	}
	else
	{
		float dist = fract(texel.y) - 0.5;
		w = exp(-dist * dist * 9.0);
	}
	float luma = dot(c, vec3(0.30, 0.59, 0.11));
	w = mix(w, 1.0, luma * 0.35);                     // bright lines bloom a little
	float m = mix(1.0, w, strength);
	gl_FragColor = vec4(c * m * mix(1.0, 1.25, strength), 1.0);
}
)GLSL";

// 3/4 - CRT: gaussian beam per scanline, horizontal beam softness, aperture grille mask,
// gamma-correct mixing; u_opt = curvature amount (0 = flat)
static const char *kCRT = R"GLSL(
vec3 tolin(vec3 c) { return c * c; }
vec3 tosrgb(vec3 c) { return sqrt(max(c, 0.0)); }
vec3 fetch(vec2 p)
{
	return tolin(texture2D(u_tex, (floor(p) + 0.5) / u_src).rgb);
}
vec2 warp(vec2 uv)
{
	uv = uv * 2.0 - 1.0;
	vec2 off = abs(uv.yx) / vec2(6.0, 4.5);
	uv = uv + uv * off * off * u_opt;
	return uv * 0.5 + 0.5;
}
// horizontal line: 4 taps, gaussian
vec3 hline(vec2 t, float row)
{
	float x0 = floor(t.x - 0.5);
	vec3 acc = vec3(0.0);
	float wsum = 0.0;
	for (int i = -1; i <= 2; i++)
	{
		float cx = x0 + float(i) + 0.5;
		float d = cx - t.x;
		float w = exp2(-2.6 * d * d);
		acc += fetch(vec2(cx, row)) * w;
		wsum += w;
	}
	return acc / wsum;
}
void main()
{
	vec2 uv = warp(v_uv);
	if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
	vec2 t = uv * u_src;
	float y0 = floor(t.y - 0.5) + 0.5;
	vec3 col = vec3(0.0);
	for (int j = -1; j <= 1; j++)
	{
		float row = y0 + float(j);
		vec3 c = hline(t, row);
		float luma = dot(c, vec3(0.30, 0.59, 0.11));
		float dy = row - t.y;
		float hard = mix(-11.0, -5.5, sqrt(luma));     // brighter = wider beam
		col += c * exp2(hard * dy * dy);
	}
	// aperture grille (RGB stripes) at window resolution
	float px = mod(floor(gl_FragCoord.x), 3.0);
	vec3 mask = vec3(0.70);
	if (px < 0.5) mask.r = 1.30; else if (px < 1.5) mask.g = 1.30; else mask.b = 1.30;
	col *= mask * 1.45;
	// soft vignette when curved
	vec2 v = uv * (1.0 - uv.yx);
	float vig = pow(clamp(v.x * v.y * 18.0, 0.0, 1.0), 0.18 * u_opt);
	col *= vig;
	gl_FragColor = vec4(tosrgb(col), 1.0);
}
)GLSL";

static const shader_def kShaders[] = {
	{ "Nitido (pixel perfect)", kSharp },
	{ "Suavizado", kSmooth },
	{ "Scanlines", kScanlines },
	{ "CRT", kCRT },
	{ "CRT curvo", kCRT },
};
static constexpr int kNumShaders = 5;
