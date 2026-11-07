// netplay.h - online play for NBA JAM Recomp.
//
// Deterministic lockstep with input delay over UDP, star topology:
//   * the host is player 1, up to three clients take players 2..4;
//   * every peer runs the same emulation from power-on with the same NVRAM and DIPs;
//   * each peer samples its local input for frame f+delay while running frame f;
//   * clients send their inputs to the host, the host merges them and broadcasts the
//     merged input of every frame; nobody runs frame f before it has the merged input.
// Packets carry redundant history so lost datagrams are recovered by the next one.
// State hashes are exchanged every 30 frames to detect desyncs.
#pragma once
#include "machine.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

u16 net_pack_input(const cabinet_inputs::player &p);
void net_unpack_input(u16 v, cabinet_inputs::player &p);

struct net_addr { u32 ip = 0; u16 port = 0; bool operator==(const net_addr &o) const { return ip == o.ip && port == o.port; } };
std::string net_addr_str(const net_addr &a);
std::vector<std::string> net_local_ipv4();

class udp_socket
{
public:
	~udp_socket() { close(); }
	bool open(u16 port, std::string &err);        // port 0 = any
	void close();
	bool is_open() const { return m_fd >= 0; }
	void send(const net_addr &to, const void *data, size_t len);
	int recv(net_addr &from, void *data, size_t cap);   // -1 when nothing pending
	static bool resolve(const std::string &host, u16 port, net_addr &out, std::string &err);
	double loss = 0.0;                             // test hook: drop this fraction of sends
private:
	long long m_fd = -1;
	u32 m_lossrng = 12345;
};

class netplay
{
public:
	enum class mode { off, host, client };
	enum class phase { lobby, transfer, running, ended };
	static constexpr int MAX_HIST = 32;

	netplay();
	~netplay();

	bool host(u16 port, std::string &err);
	bool join(const std::string &host, u16 port, std::string &err);
	void stop(const char *reason = nullptr);      // sends BYE

	void poll();                                  // receive + periodic sends; call every loop iteration

	mode role() const { return m_mode; }
	phase stage() const { return m_phase; }
	bool active() const { return m_mode != mode::off; }
	int local_slot() const { return m_slot; }
	int players() const { return m_nplayers; }
	std::vector<std::string> peer_list() const;   // host: one line per connected client
	std::string error() const { return m_error; }
	int ping_ms() const { return m_ping; }
	bool desynced() const { return m_desync; }

	// --- host: start the session (game must be loaded locally) ---
	void host_start(const std::string &game_id, u32 fingerprint, const dip_settings &dips, const std::vector<u8> &nvram, int delay);
	bool all_ready() const;                       // host: every client confirmed

	// --- client: the start description, once fully received ---
	bool start_received() const { return m_phase == phase::transfer && m_got_start && m_chunks_ok == m_nchunks; }
	std::string start_game_id() const { return m_game_id; }
	u32 start_fingerprint() const { return m_fp; }
	const dip_settings &start_dips() const { return m_dips; }
	const std::vector<u8> &start_nvram() const { return m_nv; }
	void client_ready(bool ok, const std::string &why);   // after loading the game

	// --- running ---
	int delay() const { return m_delay; }
	u32 frame() const { return m_frame; }
	bool can_run() const;                          // merged input for m_frame available
	cabinet_inputs frame_inputs(const cabinet_inputs::player &local_now); // records local input for frame+delay, returns merged input of frame
	void frame_done(u32 hash);                     // after running the frame
	u32 stall_ms() const;
	int buffered() const;                          // merged frames ready beyond the current one

	udp_socket &socket() { return m_sock; }

private:
	struct peer
	{
		net_addr addr;
		int slot = 0;
		bool ready = false;
		u32 last_seen = 0;
		u32 echo = 0;            // last timestamp received from it
		u32 echo_at = 0;         // local time when it arrived
		u32 acked_merged = 0;    // client: merged frames received below this (exclusive)
		u32 chunk_mask_lo = 0, chunk_mask_hi = 0;
		bool start_acked = false;
		int ping = 0;
	};
	void reset_state();
	void handle(const net_addr &from, const u8 *p, int n);
	void send_raw(const net_addr &to, const std::vector<u8> &b);
	void tick_host(u32 now);
	void tick_client(u32 now);
	void try_merge();
	u16 input_of(int slot, u32 f) const;
	bool has_input(int slot, u32 f) const;

	udp_socket m_sock;
	mode m_mode = mode::off;
	phase m_phase = phase::lobby;
	std::string m_error;
	int m_slot = 0, m_nplayers = 1;
	std::vector<peer> m_peers;               // host: clients; client: [0] = host
	u32 m_last_tick = 0, m_hello_t = 0, m_start_t = 0;
	int m_ping = 0;

	// session description
	bool m_got_start = false;
	std::string m_game_id;
	u32 m_fp = 0;
	dip_settings m_dips;
	std::vector<u8> m_nv;
	int m_nchunks = 0, m_chunks_ok = 0;
	std::vector<bool> m_chunk_have;
	bool m_client_ready_sent = false, m_client_ok = false;
	std::string m_client_why;
	int m_delay = 4;

	// lockstep
	u32 m_frame = 0;                         // next frame to run
	std::map<u32, u16> m_in[4];              // per-slot inputs by frame (host: all slots; client: own)
	std::map<u32, u64> m_merged;             // merged inputs (4 x 16 bits)
	u32 m_merged_upto = 0;                   // frames [0, m_merged_upto) merged (host)
	u32 m_stall_since = 0;
	std::map<u32, u32> m_hash;               // own hashes
	bool m_desync = false;
};

u32 net_now_ms();
