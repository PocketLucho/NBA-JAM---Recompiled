// netplay.cpp - see netplay.h
#include "netplay.h"
#include "lang.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
static void sock_init() { static bool done = false; if (!done) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); done = true; } }
static void sock_close(long long fd) { closesocket(SOCKET(fd)); }
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
static void sock_init() {}
static void sock_close(long long fd) { ::close(int(fd)); }
#endif

static constexpr u8 MAGIC = 0x4a;
static constexpr u16 VERSION = 1;
static constexpr u32 TIMEOUT_MS = 10000;
static constexpr int CHUNK = 1024;

u32 net_now_ms()
{
	using namespace std::chrono;
	static const auto t0 = steady_clock::now();
	return u32(duration_cast<milliseconds>(steady_clock::now() - t0).count()) + 1;
}

// ---------------------------------------------------------------------------
// input packing
// ---------------------------------------------------------------------------
u16 net_pack_input(const cabinet_inputs::player &p)
{
	return u16(p.up | p.down << 1 | p.left << 2 | p.right << 3 | p.shoot << 4 | p.pass << 5 | p.turbo << 6 | p.start << 7 | p.coin << 8);
}

void net_unpack_input(u16 v, cabinet_inputs::player &p)
{
	p.up = v & 1; p.down = v >> 1 & 1; p.left = v >> 2 & 1; p.right = v >> 3 & 1; p.shoot = v >> 4 & 1;
	p.pass = v >> 5 & 1; p.turbo = v >> 6 & 1; p.start = v >> 7 & 1; p.coin = v >> 8 & 1;
}

// ---------------------------------------------------------------------------
// sockets
// ---------------------------------------------------------------------------
std::string net_addr_str(const net_addr &a)
{
	char b[32];
	snprintf(b, sizeof(b), "%u.%u.%u.%u:%u", a.ip >> 24, a.ip >> 16 & 255, a.ip >> 8 & 255, a.ip & 255, a.port);
	return b;
}

bool udp_socket::open(u16 port, std::string &err)
{
	sock_init();
	close();
#ifdef _WIN32
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) { err = tr("no se pudo crear el socket", "could not create the socket"); return false; }
	u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
	m_fd = (long long)s;
#else
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) { err = tr("no se pudo crear el socket", "could not create the socket"); return false; }
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
	m_fd = s;
#endif
	int one = 1;
	setsockopt(decltype(socket(0, 0, 0))(m_fd), SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	a.sin_port = htons(port);
	if (bind(decltype(socket(0, 0, 0))(m_fd), (sockaddr *)&a, sizeof(a)) != 0)
	{
		err = tr("no se pudo abrir el puerto UDP ", "could not open UDP port ") + std::to_string(port) + tr(" (en uso?)", " (in use?)");
		close();
		return false;
	}
	return true;
}

void udp_socket::close()
{
	if (m_fd >= 0) sock_close(m_fd);
	m_fd = -1;
}

void udp_socket::send(const net_addr &to, const void *data, size_t len)
{
	if (m_fd < 0) return;
	if (loss > 0)
	{
		m_lossrng = m_lossrng * 1103515245u + 12345u;
		if (double((m_lossrng >> 8) & 0xffff) / 65536.0 < loss) return;
	}
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(to.ip);
	a.sin_port = htons(to.port);
	sendto(decltype(socket(0, 0, 0))(m_fd), (const char *)data, int(len), 0, (sockaddr *)&a, sizeof(a));
}

int udp_socket::recv(net_addr &from, void *data, size_t cap)
{
	if (m_fd < 0) return -1;
	sockaddr_in a{};
	socklen_t al = sizeof(a);
	int n = int(recvfrom(decltype(socket(0, 0, 0))(m_fd), (char *)data, int(cap), 0, (sockaddr *)&a, &al));
	if (n < 0) return -1;
	from.ip = ntohl(a.sin_addr.s_addr);
	from.port = ntohs(a.sin_port);
	return n;
}

bool udp_socket::resolve(const std::string &host, u16 port, net_addr &out, std::string &err)
{
	sock_init();
	addrinfo hints{}, *res = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res)
	{
		err = tr("no se encuentra la direccion '", "address not found: '") + host + "'";
		return false;
	}
	out.ip = ntohl(((sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
	out.port = port;
	freeaddrinfo(res);
	return true;
}

std::vector<std::string> net_local_ipv4()
{
	sock_init();
	std::vector<std::string> out;
	auto add = [&](u32 ip) {
		if (ip == 0 || (ip >> 24) == 127) return;
		net_addr a{ ip, 0 };
		std::string s = net_addr_str(a);
		s = s.substr(0, s.find(':'));
		if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
	};
	// primary outbound interface (no packet is sent by connect() on UDP)
	auto s = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(53);
	a.sin_addr.s_addr = htonl(0x08080808);
	if (connect(s, (sockaddr *)&a, sizeof(a)) == 0)
	{
		sockaddr_in me{};
		socklen_t ml = sizeof(me);
		if (getsockname(s, (sockaddr *)&me, &ml) == 0) add(ntohl(me.sin_addr.s_addr));
	}
	sock_close((long long)s);
	char name[256];
	if (gethostname(name, sizeof(name)) == 0)
	{
		addrinfo hints{}, *res = nullptr;
		hints.ai_family = AF_INET;
		if (getaddrinfo(name, nullptr, &hints, &res) == 0)
		{
			for (addrinfo *r = res; r; r = r->ai_next) add(ntohl(((sockaddr_in *)r->ai_addr)->sin_addr.s_addr));
			freeaddrinfo(res);
		}
	}
	return out;
}

// ---------------------------------------------------------------------------
// packet helpers
// ---------------------------------------------------------------------------
namespace {
struct wbuf
{
	std::vector<u8> b;
	void u8_(u8 v) { b.push_back(v); }
	void u16_(u16 v) { u8_(u8(v)); u8_(u8(v >> 8)); }
	void u32_(u32 v) { u16_(u16(v)); u16_(u16(v >> 16)); }
	void u64_(u64 v) { u32_(u32(v)); u32_(u32(v >> 32)); }
	void str(const std::string &s) { u8_(u8(std::min<size_t>(s.size(), 255))); b.insert(b.end(), s.begin(), s.begin() + long(std::min<size_t>(s.size(), 255))); }
	void bytes(const u8 *p, size_t n) { b.insert(b.end(), p, p + n); }
};
struct rbuf
{
	const u8 *p; int n, i = 0;
	bool ok = true;
	bool need(int k) { if (i + k > n) ok = false; return ok; }
	u8 u8_() { return need(1) ? p[i++] : 0; }
	u16 u16_() { u16 lo = u8_(); return u16(lo | (u8_() << 8)); }
	u32 u32_() { u32 lo = u16_(); return lo | (u32(u16_()) << 16); }
	u64 u64_() { u64 lo = u32_(); return lo | (u64(u32_()) << 32); }
	std::string str() { int l = u8_(); if (!need(l)) return {}; std::string s((const char *)p + i, size_t(l)); i += l; return s; }
};
}

// ---------------------------------------------------------------------------
// netplay
// ---------------------------------------------------------------------------
netplay::netplay() {}
netplay::~netplay() { stop(); }

bool netplay::host(u16 port, std::string &err)
{
	stop();
	reset_state();
	if (!m_sock.open(port, err)) return false;
	m_mode = mode::host;
	m_phase = phase::lobby;
	m_slot = 0;
	m_nplayers = 1;
	return true;
}

bool netplay::join(const std::string &host, u16 port, std::string &err)
{
	stop();
	net_addr a;
	if (!udp_socket::resolve(host, port, a, err)) return false;
	reset_state();
	if (!m_sock.open(0, err)) return false;
	m_mode = mode::client;
	m_phase = phase::lobby;
	peer h;
	h.addr = a;
	h.last_seen = net_now_ms();
	m_peers.push_back(h);
	m_slot = -1;
	return true;
}

void netplay::stop(const char *reason)
{
	if (m_mode != mode::off)
	{
		std::vector<u8> b{ u8('b'), MAGIC, 0, 0, 0, 0, 0, 0, 0, 0 };
		for (int k = 0; k < 3; k++)
			for (auto &p : m_peers) send_raw(p.addr, b);
	}
	m_sock.close();
	m_mode = mode::off;
	m_phase = phase::ended;
	if (reason) m_error = reason;
}

void netplay::reset_state()
{
	m_phase = phase::lobby;
	m_error.clear();
	m_slot = 0; m_nplayers = 1;
	m_peers.clear();
	m_last_tick = m_hello_t = m_start_t = 0;
	m_ping = 0;
	m_got_start = false; m_game_id.clear(); m_fp = 0; m_dips = dip_settings(); m_nv.clear();
	m_nchunks = m_chunks_ok = 0; m_chunk_have.clear();
	m_client_ready_sent = m_client_ok = false; m_client_why.clear();
	m_delay = 4; m_frame = 0;
	for (auto &m : m_in) m.clear();
	m_merged.clear(); m_merged_upto = 0; m_stall_since = 0; m_hash.clear();
	m_desync = false;
}

void netplay::send_raw(const net_addr &to, const std::vector<u8> &b)
{
	m_sock.send(to, b.data(), b.size());
}

std::vector<std::string> netplay::peer_list() const
{
	std::vector<std::string> v;
	for (auto &p : m_peers)
		v.push_back("J" + std::to_string(p.slot + 1) + "  " + net_addr_str(p.addr) + "  " + std::to_string(p.ping) + " ms" + (p.ready ? tr("  listo", "  ready") : ""));
	return v;
}

void netplay::host_start(const std::string &game_id, u32 fingerprint, const dip_settings &dips, const std::vector<u8> &nvram, int delay)
{
	m_game_id = game_id;
	m_fp = fingerprint;
	m_dips = dips;
	m_nv = nvram;
	m_delay = std::clamp(delay, 1, 15);
	m_nchunks = int((m_nv.size() + CHUNK - 1) / CHUNK);
	m_phase = phase::transfer;
	m_frame = 0;
	for (int s = 0; s < 4; s++) { m_in[s].clear(); for (int f = 0; f < m_delay; f++) m_in[s][u32(f)] = 0; }
	m_merged.clear();
	for (int f = 0; f < m_delay; f++) m_merged[u32(f)] = 0;
	m_merged_upto = u32(m_delay);
	for (auto &p : m_peers) { p.ready = false; p.start_acked = false; p.chunk_mask_lo = p.chunk_mask_hi = 0; p.acked_merged = 0; }
	if (m_peers.empty()) m_phase = phase::running;
}

bool netplay::all_ready() const
{
	for (auto &p : m_peers) if (!p.ready) return false;
	return true;
}

void netplay::client_ready(bool ok, const std::string &why)
{
	m_client_ready_sent = true;
	m_client_ok = ok;
	m_client_why = why;
	if (!ok) m_error = why;
}

bool netplay::has_input(int slot, u32 f) const { return m_in[slot].count(f) != 0; }
u16 netplay::input_of(int slot, u32 f) const { auto it = m_in[slot].find(f); return it == m_in[slot].end() ? 0 : it->second; }

void netplay::try_merge()
{
	while (true)
	{
		u32 f = m_merged_upto;
		for (int s = 0; s < m_nplayers; s++) if (!has_input(s, f)) return;
		u64 m = 0;
		for (int s = 0; s < m_nplayers; s++) m |= u64(input_of(s, f)) << (16 * s);
		m_merged[f] = m;
		m_merged_upto = f + 1;
	}
}

bool netplay::can_run() const
{
	return m_phase == phase::running && m_merged.count(m_frame) != 0;
}

int netplay::buffered() const
{
	int n = 0;
	while (m_merged.count(m_frame + u32(n))) n++;
	return n;
}

u32 netplay::stall_ms() const
{
	return m_stall_since ? net_now_ms() - m_stall_since : 0;
}

cabinet_inputs netplay::frame_inputs(const cabinet_inputs::player &local_now)
{
	u32 tf = m_frame + u32(m_delay);
	if (!has_input(m_slot, tf)) m_in[m_slot][tf] = net_pack_input(local_now);
	if (m_mode == mode::host) try_merge();
	cabinet_inputs in{};
	auto it = m_merged.find(m_frame);
	u64 m = it == m_merged.end() ? 0 : it->second;
	for (int s = 0; s < 4; s++) net_unpack_input(u16(m >> (16 * s)), in.p[s]);
	return in;
}

void netplay::frame_done(u32 hash)
{
	if (m_frame % 30 == 0) m_hash[m_frame] = hash;
	m_frame++;
	// forget old history
	u32 keep = m_frame > 900 ? m_frame - 900 : 0;
	auto trim = [&](auto &mp) { while (!mp.empty() && mp.begin()->first < keep) mp.erase(mp.begin()); };
	for (auto &m : m_in) trim(m);
	trim(m_merged);
	trim(m_hash);
	if (m_mode == mode::host) try_merge();
}

// ---------------------------------------------------------------------------
// receive
// ---------------------------------------------------------------------------
void netplay::poll()
{
	if (m_mode == mode::off) return;
	u8 buf[2048];
	net_addr from;
	int n;
	while ((n = m_sock.recv(from, buf, sizeof(buf))) >= 0)
		if (n >= 10 && buf[1] == MAGIC) handle(from, buf, n);
	if (m_mode == mode::off) return;
	u32 now = net_now_ms();
	if (m_phase == phase::running && !m_merged.count(m_frame)) { if (!m_stall_since) m_stall_since = now; }
	else m_stall_since = 0;
	if (m_mode == mode::host) tick_host(now); else tick_client(now);
}

void netplay::handle(const net_addr &from, const u8 *p, int n)
{
	rbuf r{ p, n };
	u8 type = r.u8_();
	r.u8_();
	u32 tsend = r.u32_(), techo = r.u32_();
	u32 now = net_now_ms();

	peer *pp = nullptr;
	for (auto &q : m_peers) if (q.addr == from) pp = &q;

	if (m_mode == mode::host)
	{
		if (type == 'h')
		{
			u16 ver = r.u16_();
			if (!pp)
			{
				wbuf w;
				if (ver != VERSION || m_phase != phase::lobby || m_nplayers >= 4)
				{
					w.u8_('r'); w.u8_(MAGIC); w.u32_(now); w.u32_(tsend);
					w.str(ver != VERSION ? "version distinta de NBA JAM Recomp / different NBA JAM Recomp version" : m_phase != phase::lobby ? "la partida ya ha empezado / the game has already started" : "la partida esta llena / the game is full (4)");
					send_raw(from, w.b);
					return;
				}
				peer np;
				np.addr = from;
				np.slot = m_nplayers++;
				m_peers.push_back(np);
				pp = &m_peers.back();
			}
			wbuf w;
			w.u8_('w'); w.u8_(MAGIC); w.u32_(now); w.u32_(tsend);
			w.u8_(u8(pp->slot)); w.u8_(u8(m_nplayers));
			send_raw(from, w.b);
		}
		if (!pp) return;
		pp->last_seen = now;
		pp->echo = tsend;
		pp->echo_at = now;
		if (techo) pp->ping = (pp->ping * 3 + int(now - techo)) / 4;
		if (type == 'b')
		{
			std::string who = tr("J", "P") + std::to_string(pp->slot + 1);
			if (m_phase == phase::lobby)
			{
				// renumber remaining clients
				m_peers.erase(m_peers.begin() + (pp - m_peers.data()));
				m_nplayers = 1;
				for (auto &q : m_peers) q.slot = m_nplayers++;
			}
			else stop((tr("el jugador ", "player ") + who + tr(" se ha desconectado", " has disconnected")).c_str());
			return;
		}
		if (type == 'a')
		{
			pp->start_acked = r.u8_() != 0;
			pp->chunk_mask_lo = r.u32_();
			pp->chunk_mask_hi = r.u32_();
			u8 ready = r.u8_();
			std::string why = r.str();
			if (ready == 2) { stop(("J" + std::to_string(pp->slot + 1) + ": " + why).c_str()); return; }
			pp->ready = ready == 1;
			if (m_phase == phase::transfer && all_ready()) m_phase = phase::running;
		}
		if (type == 'i' && m_phase == phase::running)
		{
			int slot = r.u8_();
			u32 acked = r.u32_();
			u32 first = r.u32_();
			int cnt = r.u8_();
			if (slot != pp->slot) return;
			pp->acked_merged = std::max(pp->acked_merged, acked);
			for (int k = 0; k < cnt && r.ok; k++)
			{
				u16 v = r.u16_();
				if (r.ok && first + u32(k) >= u32(m_delay)) m_in[slot].emplace(first + u32(k), v);
			}
			u32 hf = r.u32_(), h = r.u32_();
			if (r.ok && hf)
			{
				auto it = m_hash.find(hf);
				if (it != m_hash.end() && it->second != h) m_desync = true;
			}
			try_merge();
		}
		return;
	}

	// ---- client ----
	if (!pp) return;    // only talk to our host
	pp->last_seen = now;
	pp->echo = tsend;
	pp->echo_at = now;
	if (techo) m_ping = (m_ping * 3 + int(now - techo)) / 4;
	switch (type)
	{
		case 'r': stop((tr("rechazado: ", "rejected: ") + r.str()).c_str()); return;
		case 'b': stop(m_phase == phase::running ? tr("el anfitrion ha cerrado la partida", "the host has closed the game") : tr("el anfitrion ha cancelado", "the host has cancelled")); return;
		case 'w': m_slot = r.u8_(); m_nplayers = r.u8_(); break;
		case 'l': m_nplayers = r.u8_(); m_slot = r.u8_(); break;
		case 's':
		{
			if (m_got_start) break;
			m_game_id = r.str();
			m_fp = r.u32_();
			m_dips.free_play = r.u8_(); m_dips.four_players = r.u8_(); m_dips.country = r.u8_(); m_dips.video_clips = r.u8_();
			m_delay = r.u8_();
			u32 nvsz = r.u32_();
			m_nchunks = r.u16_();
			m_nplayers = r.u8_();
			m_slot = r.u8_();
			if (!r.ok || nvsz > 64 * CHUNK) break;
			m_nv.assign(nvsz, 0);
			m_chunk_have.assign(size_t(m_nchunks), false);
			m_chunks_ok = 0;
			m_got_start = true;
			m_phase = phase::transfer;
			m_frame = 0;
			for (int s = 0; s < 4; s++) { m_in[s].clear(); for (int f = 0; f < m_delay; f++) m_in[s][u32(f)] = 0; }
			m_merged.clear();
			for (int f = 0; f < m_delay; f++) m_merged[u32(f)] = 0;
			break;
		}
		case 'c':
		{
			if (!m_got_start) break;
			int idx = r.u16_(), len = r.u16_();
			if (!r.ok || idx >= m_nchunks || !r.need(len) || size_t(idx) * CHUNK + size_t(len) > m_nv.size()) break;
			if (!m_chunk_have[size_t(idx)])
			{
				std::memcpy(&m_nv[size_t(idx) * CHUNK], p + r.i, size_t(len));
				m_chunk_have[size_t(idx)] = true;
				m_chunks_ok++;
			}
			break;
		}
		case 'f':
		{
			if (!m_got_start || !m_client_ready_sent || !m_client_ok) break;
			if (m_phase == phase::transfer) m_phase = phase::running;
			u32 first = r.u32_();
			int cnt = r.u8_();
			for (int k = 0; k < cnt && r.ok; k++)
			{
				u64 v = r.u64_();
				if (r.ok) m_merged.emplace(first + u32(k), v);
			}
			u8 flags = r.u8_();
			if (r.ok && (flags & 1)) m_desync = true;
			u32 need = r.u32_();          // next input frame the host wants from us
			if (r.ok) pp->acked_merged = need;
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// periodic sends
// ---------------------------------------------------------------------------
void netplay::tick_host(u32 now)
{
	for (size_t i = 0; i < m_peers.size(); i++)
		if (now - m_peers[i].last_seen > TIMEOUT_MS)
		{
			if (m_phase == phase::lobby)
			{
				m_peers.erase(m_peers.begin() + long(i));
				m_nplayers = 1;
				for (auto &q : m_peers) q.slot = m_nplayers++;
				return;
			}
			stop((tr("se ha perdido la conexion con J", "lost the connection with P") + std::to_string(m_peers[i].slot + 1)).c_str());
			return;
		}

	bool fast = now - m_last_tick >= 8;
	bool slow = now - m_hello_t >= 100;
	if (!fast) return;
	m_last_tick = now;
	if (slow) m_hello_t = now;

	for (auto &p : m_peers)
	{
		auto hdr = [&](wbuf &w, char t) { w.u8_(u8(t)); w.u8_(MAGIC); w.u32_(now); w.u32_(p.echo ? p.echo + (now - p.echo_at) : 0); };
		if (m_phase == phase::lobby && slow)
		{
			wbuf w; hdr(w, 'l'); w.u8_(u8(m_nplayers)); w.u8_(u8(p.slot)); send_raw(p.addr, w.b);
		}
		if (m_phase == phase::transfer && slow)
		{
			if (!p.start_acked)
			{
				wbuf w; hdr(w, 's');
				w.str(m_game_id); w.u32_(m_fp);
				w.u8_(m_dips.free_play); w.u8_(m_dips.four_players); w.u8_(u8(m_dips.country)); w.u8_(m_dips.video_clips);
				w.u8_(u8(m_delay)); w.u32_(u32(m_nv.size())); w.u16_(u16(m_nchunks)); w.u8_(u8(m_nplayers)); w.u8_(u8(p.slot));
				send_raw(p.addr, w.b);
			}
			else
				for (int c = 0; c < m_nchunks; c++)
				{
					bool have = c < 32 ? (p.chunk_mask_lo >> c & 1) : (p.chunk_mask_hi >> (c - 32) & 1);
					if (have) continue;
					wbuf w; hdr(w, 'c');
					size_t off = size_t(c) * CHUNK, len = std::min<size_t>(CHUNK, m_nv.size() - off);
					w.u16_(u16(c)); w.u16_(u16(len)); w.bytes(&m_nv[off], len);
					send_raw(p.addr, w.b);
				}
		}
		if (m_phase == phase::running)
		{
			wbuf w; hdr(w, 'f');
			u32 first = std::max(p.acked_merged, m_merged_upto > MAX_HIST ? m_merged_upto - MAX_HIST : 0u);
			u32 last = m_merged_upto;
			if (first > last) first = last;
			w.u32_(first);
			w.u8_(u8(last - first));
			for (u32 f = first; f < last; f++) { auto it = m_merged.find(f); w.u64_(it == m_merged.end() ? 0 : it->second); }
			w.u8_(m_desync ? 1 : 0);
			// next input frame we still need from this client
			u32 need = m_merged_upto;
			while (has_input(p.slot, need)) need++;
			w.u32_(need);
			send_raw(p.addr, w.b);
		}
	}
}

void netplay::tick_client(u32 now)
{
	peer &h = m_peers[0];
	if (now - h.last_seen > TIMEOUT_MS)
	{
		stop(m_slot < 0 ? tr("no hay respuesta del anfitrion (IP, puerto o cortafuegos?)", "no answer from the host (IP, port or firewall?)") : tr("se ha perdido la conexion con el anfitrion", "lost the connection with the host"));
		return;
	}
	bool fast = now - m_last_tick >= 8;
	bool slow = now - m_hello_t >= 100;
	if (!fast) return;
	m_last_tick = now;
	if (slow) m_hello_t = now;
	auto hdr = [&](wbuf &w, char t) { w.u8_(u8(t)); w.u8_(MAGIC); w.u32_(now); w.u32_(h.echo ? h.echo + (now - h.echo_at) : 0); };

	if (!m_got_start)
	{
		if (slow && (now - m_start_t >= 400 || m_slot < 0))
		{
			m_start_t = now;
			wbuf w; hdr(w, 'h'); w.u16_(VERSION); send_raw(h.addr, w.b);
		}
		return;
	}
	if (m_phase == phase::transfer && slow)
	{
		u32 lo = 0, hi = 0;
		for (int c = 0; c < m_nchunks; c++)
			if (m_chunk_have[size_t(c)]) { if (c < 32) lo |= 1u << c; else hi |= 1u << (c - 32); }
		wbuf w; hdr(w, 'a');
		w.u8_(1); w.u32_(lo); w.u32_(hi);
		w.u8_(m_client_ready_sent ? (m_client_ok ? 1 : 2) : 0);
		w.str(m_client_why);
		send_raw(h.addr, w.b);
		if (m_client_ready_sent && !m_client_ok) stop(m_client_why.c_str());
		return;
	}
	if (m_phase == phase::running)
	{
		// next merged frame we are missing
		u32 have = m_frame;
		while (m_merged.count(have)) have++;
		wbuf w; hdr(w, 'i');
		w.u8_(u8(m_slot));
		w.u32_(have);
		u32 first = std::max(h.acked_merged, u32(m_delay));
		auto &mine = m_in[m_slot];
		u32 last = mine.empty() ? first : mine.rbegin()->first + 1;
		if (last > first + MAX_HIST) first = last - MAX_HIST;
		if (first > last) first = last;
		w.u32_(first);
		w.u8_(u8(last - first));
		for (u32 f = first; f < last; f++) w.u16_(input_of(m_slot, f));
		u32 hf = 0, hv = 0;
		if (!m_hash.empty()) { hf = m_hash.rbegin()->first; hv = m_hash.rbegin()->second; }
		w.u32_(hf); w.u32_(hv);
		send_raw(h.addr, w.b);
	}
}
