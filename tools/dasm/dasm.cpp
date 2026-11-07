// dasm.cpp - TMS34010 disassembler CLI built on MAME's 34010dsm
//   dasm table            -> every opcode with marker params (encoding survey)
//   dasm rom lo hi start end  -> disassemble a program ROM pair (bit addrs, base ff800000)
#include "emu.h"
#include "cpu/tms34010/34010dsm.h"
#include <fstream>
#include <iterator>
#include <sstream>

int main(int argc, char **argv)
{
	tms34010_disassembler d(false);
	std::string mode = argc > 1 ? argv[1] : "table";
	if (mode == "table")
	{
		for (u32 op = 0; op < 0x10000; op++)
		{
			u16 words[6] = { u16(op), 0x1234, 0x5678, 0x9abc, 0xdef0, 0x0f0f };
			util::disasm_interface::data_buffer buf;
			buf.reader = [&](offs_t a, int bits) -> u64 {
				offs_t w = (a - 0x10000) >> 4;
				u64 v = 0;
				for (int i = 0; i < bits / 16; i++) v |= u64(w + i < 6 ? words[w + i] : 0) << (16 * i);
				return v;
			};
			std::ostringstream ss;
			offs_t r = d.disassemble(ss, 0x10000, buf, buf);
			printf("%04X %d %s\n", op, int(r & 0xffff), ss.str().c_str());
		}
		return 0;
	}
	if (mode == "rom")
	{
		std::ifstream flo(argv[2], std::ios::binary), fhi(argv[3], std::ios::binary);
		std::vector<u8> lo((std::istreambuf_iterator<char>(flo)), {}), hi((std::istreambuf_iterator<char>(fhi)), {});
		u32 start = strtoul(argv[4], nullptr, 16), end = strtoul(argv[5], nullptr, 16);
		util::disasm_interface::data_buffer buf;
		buf.reader = [&](offs_t a, int bits) -> u64 {
			u64 v = 0;
			for (int i = 0; i < bits / 16; i++)
			{
				u32 w = ((a - 0xff800000) >> 4) + i;
				if (w < lo.size()) v |= u64(lo[w] | (hi[w] << 8)) << (16 * i);
			}
			return v;
		};
		for (u32 pc = start; pc < end;)
		{
			std::ostringstream ss;
			offs_t r = d.disassemble(ss, pc, buf, buf);
			printf("%08X: %s\n", pc, ss.str().c_str());
			pc += (r & 0xffff);
		}
	}
	return 0;
}
