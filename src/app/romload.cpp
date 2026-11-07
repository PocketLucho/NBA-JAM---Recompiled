// romload.cpp
#include "romload.h"
#include "miniz.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

static std::string u8s(const fs::path &p)
{
	auto u = p.u8string();
	return std::string(u.begin(), u.end());
}

static fs::path P(const std::string &s)
{
	return fs::path(std::u8string(s.begin(), s.end()));
}

static FILE *fopen_u8(const std::string &path, const char *mode)
{
#ifdef _WIN32
	std::wstring wm(mode, mode + std::strlen(mode));
	return _wfopen(P(path).c_str(), wm.c_str());
#else
	return std::fopen(path.c_str(), mode);
#endif
}

static std::string lower(std::string s)
{
	for (auto &c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

uint32_t crc32_of(const uint8_t *d, size_t n)
{
	return uint32_t(mz_crc32(MZ_CRC32_INIT, d, n));
}

bool read_whole_file(const std::string &path, std::vector<uint8_t> &out)
{
	FILE *f = fopen_u8(path, "rb");
	if (!f) return false;
	std::fseek(f, 0, SEEK_END);
	long n = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (n < 0) { std::fclose(f); return false; }
	out.resize(size_t(n));
	size_t got = n ? std::fread(out.data(), 1, size_t(n), f) : 0;
	std::fclose(f);
	return got == size_t(n);
}

bool write_whole_file(const std::string &path, const std::vector<uint8_t> &data)
{
	std::error_code ec;
	fs::create_directories(P(path).parent_path(), ec);
	FILE *f = fopen_u8(path, "wb");
	if (!f) return false;
	size_t w = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
	std::fclose(f);
	return w == data.size();
}

bool file_exists(const std::string &path)
{
	std::error_code ec;
	return fs::is_regular_file(P(path), ec);
}

std::string path_join(const std::string &a, const std::string &b)
{
	return u8s(P(a) / P(b));
}

void rom_pool::add(const entry &e, const std::string &name)
{
	m_byname.emplace(lower(name), e);
	m_bycrc.emplace(e.crc, e);
}

void rom_pool::add_dir(const std::string &dir)
{
	std::error_code ec;
	if (!fs::is_directory(P(dir), ec)) return;
	for (auto &it : fs::directory_iterator(P(dir), ec))
	{
		if (!it.is_regular_file(ec)) continue;
		std::string name = u8s(it.path().filename());
		std::string full = u8s(it.path());
		if (lower(u8s(it.path().extension())) == ".zip") { add_zip(full); continue; }
		auto sz = it.file_size(ec);
		if (sz > 64 * 1024 * 1024) continue;
		std::vector<uint8_t> d;
		if (!read_whole_file(full, d)) continue;
		add(entry{ full, "", crc32_of(d.data(), d.size()), d.size() }, name);
	}
}

void rom_pool::add_tree(const std::string &dir, int depth)
{
	std::error_code ec;
	if (!fs::is_directory(P(dir), ec)) return;
	for (auto &it : fs::directory_iterator(P(dir), ec))
	{
		std::string full = u8s(it.path());
		std::string ext = lower(u8s(it.path().extension()));
		if (it.is_directory(ec))
		{
			if (depth > 0 && u8s(it.path().filename())[0] != '.') add_tree(full, depth - 1);
			continue;
		}
		if (!it.is_regular_file(ec)) continue;
		if (ext == ".zip") { add_zip(full); continue; }
		auto sz = it.file_size(ec);
		bool romlike = sz == 0x20000 || sz == 0x80000 || sz == 0x100000 || sz == 0x200000 || (ext == ".irw" && sz < 16 * 1024 * 1024);
		if (!romlike) continue;
		std::vector<uint8_t> d;
		if (!read_whole_file(full, d)) continue;
		add(entry{ full, "", crc32_of(d.data(), d.size()), d.size() }, u8s(it.path().filename()));
	}
}

void rom_pool::add_zip(const std::string &zippath)
{
	mz_zip_archive zip{};
	if (!mz_zip_reader_init_file(&zip, zippath.c_str(), 0)) return;
	unsigned n = mz_zip_reader_get_num_files(&zip);
	for (unsigned i = 0; i < n; i++)
	{
		mz_zip_archive_file_stat st;
		if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory) continue;
		std::string name = st.m_filename;
		auto slash = name.find_last_of("/\\");
		std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
		add(entry{ zippath, name, uint32_t(st.m_crc32), size_t(st.m_uncomp_size) }, base);
	}
	mz_zip_reader_end(&zip);
}

bool rom_pool::load(const entry &e, std::vector<uint8_t> &out)
{
	if (e.zipname.empty()) return read_whole_file(e.path, out);
	mz_zip_archive zip{};
	if (!mz_zip_reader_init_file(&zip, e.path.c_str(), 0)) return false;
	size_t sz = 0;
	void *p = mz_zip_reader_extract_file_to_heap(&zip, e.zipname.c_str(), &sz, 0);
	bool ok = p != nullptr;
	if (ok) { out.assign(static_cast<uint8_t *>(p), static_cast<uint8_t *>(p) + sz); mz_free(p); }
	mz_zip_reader_end(&zip);
	return ok;
}

bool rom_pool::has(const std::string &name, uint32_t crc)
{
	if (crc && m_bycrc.count(crc)) return true;
	return !name.empty() && m_byname.count(lower(name));
}

bool rom_pool::get(const std::string &name, uint32_t crc, std::vector<uint8_t> &out)
{
	if (crc)
	{
		auto r = m_bycrc.equal_range(crc);
		for (auto it = r.first; it != r.second; ++it)
			if (load(it->second, out)) return true;
	}
	if (!name.empty())
	{
		auto it = m_byname.find(lower(name));
		if (it != m_byname.end() && load(it->second, out)) return true;
	}
	return false;
}
