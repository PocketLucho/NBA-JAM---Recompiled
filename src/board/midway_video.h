// midway_video.h - Midway T-unit / Wolf-unit video + DMA blitter.
// Port of MAME's midtunit_v.cpp (BSD-3-Clause, Alex Pasadyn, Zsolt Vasvari,
// Ernesto Corvi, Aaron Giles) to the minimame framework.
#pragma once
#ifdef NBAJAM_LOWMEM
#define NBAJAM_GFX_GUARD 0x40000
#endif
#include "emu.h"
#include "cpu/tms34010/tms34010.h"

class midtunit_video_device
{
public:
	enum class kind { tunit, wunit };

	midtunit_video_device(running_machine &m, kind k) : m_machine(m), m_kind(k) {}

	void start(const u8 *gfxrom, size_t gfxrom_size);
	void set_gfx_rom_large(bool large) { m_gfx_rom_large = large; }
	std::function<void(int)> dma_irq_cb;

	void to_shiftreg(offs_t address, u16 *shiftreg);
	void from_shiftreg(offs_t address, u16 *shiftreg);
	void scanline_update(screen_device &screen, bitmap_ind16 &bitmap, int scanline, const tms340x0_device::display_params *params);

	u16 midtunit_vram_r(offs_t offset);
	void midtunit_vram_w(offs_t offset, u16 data, u16 mem_mask = 0xffff);
	u16 midtunit_gfxrom_r(offs_t offset);
	u16 midwunit_gfxrom_r(offs_t offset);
	void midtunit_vram_data_w(offs_t offset, u16 data, u16 mem_mask = 0xffff);
	void midtunit_vram_color_w(offs_t offset, u16 data, u16 mem_mask = 0xffff);
	u16 midtunit_vram_data_r(offs_t offset);
	u16 midtunit_vram_color_r(offs_t offset);
	u16 dma_r(offs_t offset);
	void dma_w(offs_t offset, u16 data, u16 mem_mask = 0xffff);
	void midtunit_control_w(offs_t offset, u16 data, u16 mem_mask = 0xffff);
	void midwunit_control_w(offs_t offset, u16 data, u16 mem_mask = 0xffff);
	u16 midwunit_control_r() { return m_midtunit_control; }

	enum op_type_t { PIXEL_SKIP = 0, PIXEL_COLOR = 1, PIXEL_COPY = 2 };

	// counters for diagnostics
	u64 m_dma_count = 0;

private:
	running_machine &m_machine;
	kind m_kind;
	const u8 *m_gfxrom = nullptr;
	size_t m_gfxrom_size = 0;
	emu_timer *m_dma_timer = nullptr;

	static constexpr u32 XPOSMASK = 0x3ff;
	static constexpr u32 YPOSMASK = 0x1ff;

	template <int BitsPerPixel, bool XFlip, bool Skip, bool Scale, op_type_t Zero, op_type_t NonZero> void dma_draw();
	void dma_draw_none() {}
	typedef void (midtunit_video_device::*draw_func)();
	draw_func m_dma_draw_skip_scale[8 * 32];
	draw_func m_dma_draw_noskip_scale[8 * 32];
	draw_func m_dma_draw_skip_noscale[8 * 32];
	draw_func m_dma_draw_noskip_noscale[8 * 32];

	enum
	{
		DMA_LRSKIP = 0, DMA_COMMAND, DMA_OFFSETLO, DMA_OFFSETHI, DMA_XSTART, DMA_YSTART,
		DMA_WIDTH, DMA_HEIGHT, DMA_PALETTE, DMA_COLOR, DMA_SCALE_X, DMA_SCALE_Y,
		DMA_TOPCLIP, DMA_BOTCLIP, DMA_UNKNOWN_E, DMA_CONFIG, DMA_LEFTCLIP, DMA_RIGHTCLIP
	};

	u16 m_midtunit_control = 0;
	bool m_gfx_rom_large = false;
	u32 m_gfxbank_offset[2] = { 0, 0x400000 };
	std::unique_ptr<u16[]> m_local_videoram;
	u8 m_videobank_select = 0;
	u16 m_dma_register[18] = {};

	struct dma_state
	{
		const u8 *gfxrom;
		u32 offset;
		s32 rowbits;
		s32 xpos, ypos, width, height;
		u16 palette, color;
		u8 yflip, preskip, postskip;
		s32 topclip, botclip, leftclip, rightclip;
		s32 startskip, endskip;
		u16 xstep, ystep;
	};
	dma_state m_dma_state;
	std::vector<u8> m_gfx_padded;   // gfx rom + guard bytes for the bit extractor
};
