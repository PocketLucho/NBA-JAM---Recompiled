// nettest - run one side of a netplay session headless with scripted inputs.
// usage: nettest <base> <game> host <port> <frames> <delay> [loss]
//        nettest <base> <game> client <ip> <port> <frames> [loss]
#include "games.h"
#include "netplay.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static cabinet_inputs::player script(int slot, u32 f)
{
	cabinet_inputs::player p{};
	u32 h = (u32(slot) * 2654435761u) ^ ((f / 12) * 40503u);
	h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
	p.up = (h & 3) == 0; p.down = (h & 3) == 1; p.left = (h >> 2 & 3) == 0; p.right = (h >> 2 & 3) == 1;
	p.shoot = (h >> 4 & 7) == 0; p.pass = (h >> 7 & 7) == 0; p.turbo = (h >> 10 & 1);
	u32 g = f % 600;
	p.coin = g >= 60u + u32(slot) * 8 && g < 64u + u32(slot) * 8;
	p.start = g >= 120u + u32(slot) * 8 && g < 126u + u32(slot) * 8;
	return p;
}

static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage\n"); return 1; }
	std::string base = argv[1], id = argv[2], role = argv[3];
	bool host = role == "host";
	netplay np;
	std::string err;
	int frames, delay = 4;
	double loss = 0;
	if (host)
	{
		frames = atoi(argv[5]); delay = atoi(argv[6]); if (argc > 7) loss = atof(argv[7]);
		if (!np.host(u16(atoi(argv[4])), err)) { fprintf(stderr, "host: %s\n", err.c_str()); return 1; }
	}
	else
	{
		frames = atoi(argv[6]); if (argc > 7) loss = atof(argv[7]);
		if (!np.join(argv[4], u16(atoi(argv[5])), err)) { fprintf(stderr, "join: %s\n", err.c_str()); return 1; }
	}
	np.socket().loss = loss;
	int want_players = getenv("PLAYERS") ? atoi(getenv("PLAYERS")) : 2;

	game_catalog cat(base);
	const game_info *gi = nullptr;
	for (auto &g : game_list()) if (id == g.id) gi = &g;
	if (!gi) { fprintf(stderr, "unknown game\n"); return 1; }
	std::unique_ptr<arcade_machine> m;

	auto t0 = net_now_ms();
	if (host)
	{
		while (np.players() < want_players)
		{
			np.poll(); sleep_ms(5);
			if (net_now_ms() - t0 > 30000) { fprintf(stderr, "no client\n"); return 1; }
		}
		m = cat.create(*gi, err);
		if (!m) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
		dip_settings d;
		m->set_dips(d);
		m->reset();
		np.host_start(id, m->fingerprint(), d, m->nvram(), delay);
	}
	else
	{
		while (!np.start_received())
		{
			np.poll(); sleep_ms(5);
			if (!np.active()) { fprintf(stderr, "client: %s\n", np.error().c_str()); return 1; }
		}
		m = cat.create(*gi, err);
		if (!m) { np.client_ready(false, err); return 1; }
		if (m->fingerprint() != np.start_fingerprint()) { np.client_ready(false, "ROMs distintas"); np.poll(); fprintf(stderr, "fingerprint mismatch\n"); return 1; }
		m->set_nvram(np.start_nvram());
		m->set_dips(np.start_dips());
		m->reset();
		np.client_ready(true, "");
	}
	fprintf(stderr, "%s: slot %d, players %d, start\n", role.c_str(), np.local_slot(), np.players());
	u32 stalls = 0;
	auto tstart = net_now_ms();
	while (np.frame() < u32(frames))
	{
		np.poll();
		if (!np.active()) { fprintf(stderr, "%s: ended: %s at frame %u\n", role.c_str(), np.error().c_str(), np.frame()); return 1; }
		if (!np.can_run()) { stalls++; sleep_ms(1); continue; }
		cabinet_inputs in = np.frame_inputs(script(np.local_slot(), np.frame()));
		if (getenv("CHEAT") && np.frame() >= 400 && np.frame() < 460) in.p[0].left = true;
		m->set_inputs(in);
		m->run_frame();
		std::vector<float> a; m->take_audio(a);
		u32 h = m->state_hash();
		if (np.frame() % 300 == 0) printf("%u %08x\n", np.frame(), h);
		np.frame_done(h);
	}
	// keep serving the peer for a moment so it can finish too
	auto te = net_now_ms();
	while (net_now_ms() - te < 1500) { np.poll(); sleep_ms(2); }
	fprintf(stderr, "%s: done %d frames in %u ms, stall polls %u, desync=%d ping=%d\n", role.c_str(), frames, net_now_ms() - tstart, stalls, np.desynced(), np.ping_ms());
	np.stop();
	return np.desynced() ? 2 : 0;
}
