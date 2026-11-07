// games.cpp - the NBA JAM saga: game list, where each game's ROM data comes from, and
// construction of the emulated board.
#include "games.h"
#include "lang.h"
#include "tunit.h"
#include "wunit.h"

#include <cstring>

const std::vector<game_info> &game_list()
{
	// ordered by release date
	static const std::vector<game_info> list = {
		{ "nbajam",   "NBA JAM",                      "1993 - rev 3.01 (romset MAME nbajam)",         "1993 - rev 3.01 (MAME romset nbajam)",         "T-Unit",    false },
		{ "nbajamte", "NBA JAM Tournament Edition",   "1994 - rev 4.0 (fuente compilada = ROM)",      "1994 - rev 4.0 (source build = ROM)",          "T-Unit",    true },
		{ "nbajamse", "NBA JAM T.E. Special Edition", "1994/2025 - rev 5.02 de Asure (fuente = ROM)", "1994/2025 - Asure's rev 5.02 (source = ROM)",  "T-Unit",    true },
		{ "nbahangt", "NBA Hangtime",                 "1996 - ver L1.3 o L1.1 (romset MAME)",         "1996 - ver L1.3 or L1.1 (MAME romset)",        "Wolf Unit", false },
		{ "nbamht",   "NBA Maximum Hangtime",         "1996 - ver 1.3 (fuente = ROM) o 1.03",         "1996 - ver 1.3 (source = ROM) or 1.03",        "Wolf Unit", true },
	};
	return list;
}

// ---------------------------------------------------------------------------
// ROM descriptions: MAME romsets (names / CRCs from MAME midtunit.cpp, midwunit.cpp) and
// the files of the public source repositories (ROMKIT, rom/ROM5.02, ROMS).  Files are
// matched by CRC32 only, so any file name, folder or ZIP works.
// ---------------------------------------------------------------------------
struct rom_desc { const char *name; uint32_t crc; };

static const rom_desc nbajam_prog[2] = { { "l3_nba_jam_game_rom_uj12.uj12", 0xb93e271c }, { "l3_nba_jam_game_rom_ug12.ug12", 0x407d3390 } };
static const rom_desc nbajam_snd[3] = { { "l2_nba_jam_u3_sound_rom.u3", 0x3a3ea480 }, { "l1_nba_jam_u12_sound_rom.u12", 0xb94847f1 }, { "l1_nba_jam_u13_sound_rom.u13", 0xb6fe24bd } };
static const rom_desc nbajam_gfx[16] = {
	{ "l1_nba_jam_game_rom_ug14.ug14", 0x04bb9f64 }, { "l1_nba_jam_game_rom_uj14.uj14", 0xb34b7af3 }, { "l1_nba_jam_game_rom_ug19.ug19", 0xa8f22fbb }, { "l1_nba_jam_game_rom_uj19.uj19", 0x8130a8a2 },
	{ "l1_nba_jam_game_rom_ug16.ug16", 0x8591c572 }, { "l1_nba_jam_game_rom_uj16.uj16", 0xd2e554f1 }, { "l1_nba_jam_game_rom_ug20.ug20", 0x44fd6221 }, { "l1_nba_jam_game_rom_uj20.uj20", 0xf9cebbb6 },
	{ "l1_nba_jam_game_rom_ug17.ug17", 0x6f921886 }, { "l1_nba_jam_game_rom_uj17.uj17", 0xb2e14981 }, { "l1_nba_jam_game_rom_ug22.ug22", 0xab05ed89 }, { "l1_nba_jam_game_rom_uj22.uj22", 0x59a95878 },
	{ "l1_nba_jam_game_rom_ug18.ug18", 0x5162d3d6 }, { "l1_nba_jam_game_rom_uj18.uj18", 0xfdee0037 }, { "l1_nba_jam_game_rom_ug23.ug23", 0x7b934c7a }, { "l1_nba_jam_game_rom_uj23.uj23", 0x427d2eee } };

static const rom_desc te_prog[2] = { { "l4_nba_jam_tournament_game_rom_uj12.uj12", 0xd7c21bc4 }, { "l4_nba_jam_tournament_game_rom_ug12.ug12", 0x7ad49229 } };
static const rom_desc te_snd[3] = { { "l1_nba_jam_tournament_u3_sound_rom.u3", 0xd4551195 }, { "l1_nba_jam_tournament_u12_sound_rom.u12", 0x4fac97bc }, { "l1_nba_jam_tournament_u13_sound_rom.u13", 0x6f27b202 } };
static const rom_desc te_gfx[16] = {
	{ "l1_nba_jam_tournament_game_rom_ug14.ug14", 0x04bb9f64 }, { "l1_nba_jam_tournament_game_rom_uj14.uj14", 0xb34b7af3 }, { "l1_nba_jam_tournament_game_rom_ug19.ug19", 0xa8f22fbb }, { "l1_nba_jam_tournament_game_rom_uj19.uj19", 0x8130a8a2 },
	{ "l1_nba_jam_tournament_game_rom_ug16.ug16", 0xc7ce74d0 }, { "l1_nba_jam_tournament_game_rom_uj16.uj16", 0x905ad88b }, { "l1_nba_jam_tournament_game_rom_ug20.ug20", 0x8a48728c }, { "l1_nba_jam_tournament_game_rom_uj20.uj20", 0xbf263d61 },
	{ "l1_nba_jam_tournament_game_rom_ug17.ug17", 0x9401be62 }, { "l1_nba_jam_tournament_game_rom_uj17.uj17", 0x8a852b9e }, { "l1_nba_jam_tournament_game_rom_ug22.ug22", 0x3b05133b }, { "l1_nba_jam_tournament_game_rom_uj22.uj22", 0x39791051 },
	{ "l1_nba_jam_tournament_game_rom_ug18.ug18", 0x6fd08f57 }, { "l1_nba_jam_tournament_game_rom_uj18.uj18", 0x4eb73c26 }, { "l1_nba_jam_tournament_game_rom_ug23.ug23", 0x854f73bc }, { "l1_nba_jam_tournament_game_rom_uj23.uj23", 0xf8c30998 } };

static const rom_desc ht_dcs[4] = {
	{ "l1.0_nba_hangtime_u_2_music_spch.u2", 0x3f0b0d0a }, { "l1.0_nba_hangtime_u_3_music_spch.u3", 0xec1db988 },
	{ "l1.0_nba_hangtime_u_4_music_spch.u4", 0xc7f847a3 }, { "l1.0_nba_hangtime_u_5_music_spch.u5", 0xef19316a } };
static const struct { rom_desc r; uint32_t off; } ht_gfx[20] = {
	{ { "l1.0_nba_hangtime_u_133_image_rom.u133", 0x3163feed }, 0x0000000 }, { { "l1.0_nba_hangtime_u_132_image_rom.u132", 0x428eaf44 }, 0x0000001 },
	{ { "l1.0_nba_hangtime_u_131_image_rom.u131", 0x5f7c5111 }, 0x0000002 }, { { "l1.0_nba_hangtime_u_130_image_rom.u130", 0xc7c0c514 }, 0x0000003 },
	{ { "l1.0_nba_hangtime_u_129_image_rom.u129", 0xb3d0daa0 }, 0x0400000 }, { { "l1.0_nba_hangtime_u_128_image_rom.u128", 0x3704ee69 }, 0x0400001 },
	{ { "l1.0_nba_hangtime_u_127_image_rom.u127", 0x4ea64d5a }, 0x0400002 }, { { "l1.0_nba_hangtime_u_126_image_rom.u126", 0x0c5c19b7 }, 0x0400003 },
	{ { "l1.0_nba_hangtime_u_125_image_rom.u125", 0x46c43d67 }, 0x0800000 }, { { "l1.0_nba_hangtime_u_124_image_rom.u124", 0xed495156 }, 0x0800001 },
	{ { "l1.0_nba_hangtime_u_123_image_rom.u123", 0xb48aa5da }, 0x0800002 }, { { "l1.0_nba_hangtime_u_122_image_rom.u122", 0xb18cd181 }, 0x0800003 },
	{ { "l1.0_nba_hangtime_u_121_image_rom.u121", 0x5acb267a }, 0x0c00000 }, { { "l1.0_nba_hangtime_u_120_image_rom.u120", 0x28e05f86 }, 0x0c00001 },
	{ { "l1.0_nba_hangtime_u_119_image_rom.u119", 0xb4f604ea }, 0x0c00002 }, { { "l1.0_nba_hangtime_u_118_image_rom.u118", 0xa257b973 }, 0x0c00003 },
	{ { "l1.0_nba_hangtime_u_113_image_rom.u113", 0xd712a779 }, 0x1400000 }, { { "l1.0_nba_hangtime_u_112_image_rom.u112", 0x644e1bca }, 0x1400001 },
	{ { "l1.0_nba_hangtime_u_111_image_rom.u111", 0x10d3b768 }, 0x1400002 }, { { "l1.0_nba_hangtime_u_110_image_rom.u110", 0x8575aeb2 }, 0x1400003 } };


// NBA JAM T.E. Special Edition rev 5.02 (Asure): rom/l4_* and rom/ROM5.02 of the SE repository
static const rom_desc se_prog[2] = { { "l4_nba_jam_tournament_game_rom_uj12.uj12 (SE)", 0xbe473944 }, { "l4_nba_jam_tournament_game_rom_ug12.ug12 (SE)", 0x688cffc4 } };
static const rom_desc se_snd[3] = { { "l1_nba_jam_tournament_u3_sound_rom.u3", 0xd4551195 }, { "l1_nba_jam_tournament_u12_sound_rom.u12 (SE)", 0x7be1622b }, { "l1_nba_jam_tournament_u13_sound_rom.u13 (SE)", 0xf939bfcb } };
static const rom_desc se_gfx[16] = {
	{ "l1_nba_jam_special_game_rom_ug14.ug14", 0x04bb9f64 }, { "l1_nba_jam_special_game_rom_uj14.uj14", 0xb34b7af3 }, { "l1_nba_jam_special_game_rom_ug19.ug19", 0xa8f22fbb }, { "l1_nba_jam_special_game_rom_uj19.uj19", 0x8130a8a2 },
	{ "l1_nba_jam_special_game_rom_ug16.ug16", 0xc8da980e }, { "l1_nba_jam_special_game_rom_uj16.uj16", 0x205c5fb7 }, { "l1_nba_jam_special_game_rom_ug20.ug20", 0x9cd4f985 }, { "l1_nba_jam_special_game_rom_uj20.uj20", 0x7a47f364 },
	{ "l1_nba_jam_special_game_rom_ug17.ug17", 0x1b7ddbe9 }, { "l1_nba_jam_special_game_rom_uj17.uj17", 0xcb08465f }, { "l1_nba_jam_special_game_rom_ug22.ug22", 0x5427ca82 }, { "l1_nba_jam_special_game_rom_uj22.uj22", 0x569b1ae5 },
	{ "l1_nba_jam_special_game_rom_ug18.ug18", 0xa182457b }, { "l1_nba_jam_special_game_rom_uj18.uj18", 0xed6b08a5 }, { "l1_nba_jam_special_game_rom_ug23.ug23", 0x7755db95 }, { "l1_nba_jam_special_game_rom_uj23.uj23", 0xa659b604 } };
// NBA JAM T.E. rev 4.0 graphics as LOAD2 image files (ROMKIT of the T.E. repository)
static const rom_desc te_irw[4] = { { "ROMKIT/BB.IRW", 0xbc4487c4 }, { "ROMKIT/BB2.IRW", 0x7d49d40f }, { "ROMKIT/BB3.IRW", 0x33a4e464 }, { "ROMKIT/BBMUG.IRW", 0x1289e1a2 } };

// Wolf Unit program versions
struct prog_version { const char *label; rom_desc u54, u63; };
static const prog_version ht_versions[] = {
	{ "L1.3", { "l1.3_nba_hangtime_u_54_game_rom.u54 / ROMS/7M_13L.U54", 0xfd9ccca2 }, { "l1.3_nba_hangtime_u_63_game_rom.u63", 0x57de886f } },
	{ "L1.1", { "htime54.bin (L1.1)", 0xc2875d98 }, { "htime63.bin (L1.1)", 0x6f4728c3 } },
};
static const prog_version mht_versions[] = {
	{ "1.3", { "ROMS/7M_13M.U54 (nba-hangtime repo)", 0x3ee3a9f4 }, { "ROMS/7M_13M.U63", 0x42e6aeca } },
	{ "1.03", { "mhtu54_v103.bin (MAME nbamht)", 0x21b0d9e1 }, { "mhtu63_v103.bin", 0xc6fdbb97 } },
};

// ---------------------------------------------------------------------------
game_catalog::game_catalog(const std::string &base_dir) : m_base(base_dir) {}

rom_pool &game_catalog::pool()
{
	if (!m_pool_scanned)
	{
		m_pool.add_tree(path_join(m_base, "roms"));
		m_pool_scanned = true;
	}
	return m_pool;
}

std::string game_catalog::data_dir(const game_info &g) const
{
	return path_join(path_join(m_base, "data"), g.id);
}

bool game_catalog::uses_data_dir(const game_info &g) const
{
	std::string d = data_dir(g);
	return file_exists(path_join(d, "program.bin")) && file_exists(path_join(d, "gfx.bin"));
}

// CRC-only lookups: names such as BB.IRW exist in several repositories with other contents
static bool have(rom_pool &p, const rom_desc &d) { return p.has("", d.crc); }
static bool have_all(rom_pool &p, const rom_desc *d, int n)
{
	for (int i = 0; i < n; i++)
		if (!have(p, d[i])) return false;
	return true;
}
static bool load_desc(rom_pool &p, const rom_desc &d, std::vector<u8> &out, std::string &err)
{
	if (!p.get("", d.crc, out)) { err = std::string(tr("no se encuentra ", "missing ")) + d.name; return false; }
	return true;
}
template <size_t N>
static const prog_version *find_version(rom_pool &p, const prog_version (&v)[N])
{
	for (auto &x : v)
		if (have(p, x.u54) && have(p, x.u63)) return &x;
	return nullptr;
}

static bool wolf_gfx_ok(rom_pool &p)
{
	for (auto &x : ht_gfx) if (!have(p, x.r)) return false;
	return true;
}

bool game_catalog::available(const game_info &g, std::string &why)
{
	if (from_roms(g, why)) return true;
	return uses_data_dir(g);           // own build from the sources (tools/build_roms.sh)
}

bool game_catalog::from_roms(const game_info &g, std::string &why)
{
	std::string id = g.id;
	rom_pool &p = pool();
	if (id == "nbajam")
	{
		if (have_all(p, nbajam_prog, 2) && have_all(p, nbajam_gfx, 16) && have_all(p, nbajam_snd, 3)) return true;
		why = tr("Copia el romset MAME nbajam.zip en la carpeta roms/", "Copy the MAME romset nbajam.zip to the roms/ folder");
		return false;
	}
	if (id == "nbajamte")
	{
		bool prog = have_all(p, te_prog, 2), gfx = have_all(p, te_gfx, 16) || have_all(p, te_irw, 4);
		bool snd = have_all(p, te_snd, 3) || have_all(p, se_snd, 3);
		if (prog && gfx && snd) return true;
		why = prog && gfx ? tr("Faltan las ROMs de sonido: copia nbajamte.zip (MAME) en roms/", "Sound ROMs missing: copy nbajamte.zip (MAME) to roms/")
		                  : tr("Copia nbajamte.zip (MAME) o el ZIP del repositorio nba-jam-tournament-edition en roms/",
		                       "Copy nbajamte.zip (MAME) or the ZIP of the nba-jam-tournament-edition repository to roms/");
		return false;
	}
	if (id == "nbajamse")
	{
		if (have_all(p, se_prog, 2) && have_all(p, se_gfx, 16) && have_all(p, se_snd, 3)) return true;
		why = tr("Copia el ZIP del repositorio nba-jam-tournament-edition-SE (Asure) en roms/",
		         "Copy the ZIP of the nba-jam-tournament-edition-SE repository (Asure) to roms/");
		return false;
	}
	bool ht = id == "nbahangt";
	if (ht || id == "nbamht")
	{
		const prog_version *v = ht ? find_version(p, ht_versions) : find_version(p, mht_versions);
		bool rest = wolf_gfx_ok(p) && have_all(p, ht_dcs, 4);
		if (v && rest) return true;
		if (!v)
			why = ht ? tr("Copia el romset MAME nbahangt.zip en roms/", "Copy the MAME romset nbahangt.zip to roms/")
			         : tr("Copia el ZIP del repositorio nba-hangtime (ROMS/7M_13M) o nbamht.zip (MAME) en roms/",
			              "Copy the ZIP of the nba-hangtime repository (ROMS/7M_13M) or nbamht.zip (MAME) to roms/");
		else
			why = tr("Faltan graficos/sonido: copia nbahangt.zip (MAME) en roms/", "Graphics/sound missing: copy nbahangt.zip (MAME) to roms/");
		return false;
	}
	why = tr("juego desconocido", "unknown game");
	return false;
}

static void interleave(const std::vector<u8> &lo, const std::vector<u8> &hi, std::vector<u8> &out)
{
	out.assign(0x100000, 0xff);
	for (size_t i = 0; i < 0x80000 && i < lo.size() && i < hi.size(); i++) { out[i * 2] = lo[i]; out[i * 2 + 1] = hi[i]; }
}

static bool load_tunit_lanes(rom_pool &p, const rom_desc *gfx, std::vector<u8> &out, std::string &err)
{
	std::vector<u8> d;
	out.assign(0xc00000, 0);
	for (int b = 0; b < 4; b++)
		for (int l = 0; l < 4; l++)
		{
			if (!load_desc(p, gfx[b * 4 + l], d, err)) return false;
			for (size_t k = 0; k < 0x80000 && k < d.size(); k++) out[b * 0x200000 + k * 4 + l] = d[k];
		}
	return true;
}

// LOAD2 .IRW files: records of { header (bit address at 0x2c, byte length at 0x30), data at 0x44 }
static bool load_tunit_irw(rom_pool &p, const rom_desc *irw, int n, std::vector<u8> &out, std::string &err)
{
	out.assign(0xc00000, 0);
	std::vector<u8> d;
	for (int i = 0; i < n; i++)
	{
		if (!load_desc(p, irw[i], d, err)) return false;
		size_t pos = 0;
		while (pos + 0x44 <= d.size())
		{
			u32 addr, len;
			std::memcpy(&addr, &d[pos + 0x2c], 4);
			std::memcpy(&len, &d[pos + 0x30], 4);
			size_t off = (addr - 0x2000000u) / 8;
			if (addr < 0x2000000u || pos + 0x44 + len > d.size() || off + len > out.size()) { err = std::string(tr("fichero IRW no valido: ", "bad IRW file: ")) + irw[i].name; return false; }
			std::memcpy(&out[off], &d[pos + 0x44], len);
			pos += 0x44 + len;
		}
	}
	return true;
}

static bool load_snd(rom_pool &p, const rom_desc *snd, tunit_roms &r, std::string &err)
{
	if (!load_desc(p, snd[0], r.snd_u3, err) || !load_desc(p, snd[1], r.snd_u12, err) || !load_desc(p, snd[2], r.snd_u13, err)) return false;
	r.snd_u3.resize(0x20000, 0xff); r.snd_u12.resize(0x80000, 0); r.snd_u13.resize(0x80000, 0);
	return true;
}

static void load_dcs_words(rom_pool &p, std::vector<u16> &dcs)
{
	dcs.assign(0x400000, 0xff);
	std::vector<u8> d;
	for (int k = 0; k < 4; k++)
		if (p.get("", ht_dcs[k].crc, d))
			for (size_t j = 0; j < d.size() && j < 0x100000; j++) dcs[k * 0x100000 + j] = d[j];
}

static bool load_wolf_gfx(rom_pool &p, std::vector<u8> &gfx, std::string &err)
{
	std::vector<u8> d;
	gfx.assign(0x2000000, 0);
	for (auto &x : ht_gfx)
	{
		if (!load_desc(p, x.r, d, err)) return false;
		u32 base = x.off & ~3u, lane = x.off & 3u;
		for (size_t k = 0; k < d.size() && k < 0x100000; k++) gfx[base + k * 4 + lane] = d[k];
	}
	return true;
}

std::unique_ptr<arcade_machine> game_catalog::create(const game_info &g, std::string &err)
{
	std::string id = g.id;
	rom_pool &p = pool();
	bool wolf = id == "nbamht" || id == "nbahangt";
	std::string why;
	if (!from_roms(g, why))
	{
		if (!uses_data_dir(g)) { err = why; return nullptr; }
		// own build from the sources (tools/build_roms.sh)
		std::string d = data_dir(g);
		std::vector<u8> prog, gfx;
		if (!read_whole_file(path_join(d, "program.bin"), prog) || !read_whole_file(path_join(d, "gfx.bin"), gfx))
		{
			err = tr("no se pueden leer los ficheros de ", "cannot read the files in ") + d;
			return nullptr;
		}
		if (wolf)
		{
			wunit_roms r;
			r.program = std::move(prog);
			r.gfx = std::move(gfx);
			load_dcs_words(p, r.dcs);     // music/speech ROMs are not part of the source release
			return std::make_unique<wunit_machine>(std::move(r), 528);
		}
		tunit_roms r;
		r.program = std::move(prog);
		r.gfx = std::move(gfx);
		std::string sd = path_join(d, "sound");
		const rom_desc *snd = id == "nbajamse" ? se_snd : id == "nbajam" ? nbajam_snd : te_snd;
		if (!have_all(p, snd, 3) && id == "nbajamte") snd = se_snd;
		if (have_all(p, snd, 3)) { if (!load_snd(p, snd, r, err)) return nullptr; }
		else if (!read_whole_file(path_join(sd, "u3.bin"), r.snd_u3) || !read_whole_file(path_join(sd, "u12.bin"), r.snd_u12) ||
			!read_whole_file(path_join(sd, "u13.bin"), r.snd_u13))
		{
			err = tr("faltan las ROMs de sonido en ", "sound ROMs missing in ") + sd;
			return nullptr;
		}
		r.snd_u3.resize(0x20000, 0xff); r.snd_u12.resize(0x80000, 0); r.snd_u13.resize(0x80000, 0);
		return std::make_unique<tunit_machine>(std::move(r), id == "nbajam" ? tunit_protection::nbajam : tunit_protection::nbajamte);
	}
	std::vector<u8> lo, hi;
	if (id == "nbajam" || id == "nbajamte" || id == "nbajamse")
	{
		tunit_roms r;
		const rom_desc *prog = id == "nbajam" ? nbajam_prog : id == "nbajamte" ? te_prog : se_prog;
		if (!load_desc(p, prog[0], lo, err) || !load_desc(p, prog[1], hi, err)) return nullptr;
		interleave(lo, hi, r.program);
		if (id == "nbajam") { if (!load_tunit_lanes(p, nbajam_gfx, r.gfx, err)) return nullptr; }
		else if (id == "nbajamse") { if (!load_tunit_lanes(p, se_gfx, r.gfx, err)) return nullptr; }
		else if (have_all(p, te_gfx, 16)) { if (!load_tunit_lanes(p, te_gfx, r.gfx, err)) return nullptr; }
		else if (!load_tunit_irw(p, te_irw, 4, r.gfx, err)) return nullptr;
		const rom_desc *snd = id == "nbajam" ? nbajam_snd : id == "nbajamse" ? se_snd : have_all(p, te_snd, 3) ? te_snd : se_snd;
		if (!load_snd(p, snd, r, err)) return nullptr;
		return std::make_unique<tunit_machine>(std::move(r), id == "nbajam" ? tunit_protection::nbajam : tunit_protection::nbajamte);
	}
	if (wolf)
	{
		const prog_version *v = id == "nbahangt" ? find_version(p, ht_versions) : find_version(p, mht_versions);
		wunit_roms r;
		if (!load_desc(p, v->u54, lo, err) || !load_desc(p, v->u63, hi, err)) return nullptr;
		interleave(lo, hi, r.program);
		if (!load_wolf_gfx(p, r.gfx, err)) return nullptr;
		load_dcs_words(p, r.dcs);
		return std::make_unique<wunit_machine>(std::move(r), 528);
	}
	err = tr("juego desconocido", "unknown game");
	return nullptr;
}
