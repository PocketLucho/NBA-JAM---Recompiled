// midway_video.cpp - Midway T-unit / Wolf-unit video + DMA blitter.
// Port of MAME's midtunit_v.cpp (BSD-3-Clause; copyright-holders Alex Pasadyn,
// Zsolt Vasvari, Ernesto Corvi, Aaron Giles). Drawing code is MAME's, verbatim.
#include "midway_video.h"
#include "midtunit_v.ipp"

void midtunit_video_device::start(const u8 *gfxrom, size_t gfxrom_size)
{
#ifdef NBAJAM_LOWMEM
	// low-memory targets (Dreamcast): use the caller's buffer in place; it must carry
	// NBAJAM_GFX_GUARD zero bytes after gfxrom_size. DMAs that start past the end are skipped.
	m_gfxrom = gfxrom;
	m_gfxrom_size = gfxrom_size;
#else
	// pad the graphics ROM out to the full 256Mbit DMA address range so that
	// stray source addresses read zeros instead of running off the buffer
	m_gfx_padded.assign(std::max<size_t>(gfxrom_size, 0x2000000) + 64, 0);
	std::memcpy(m_gfx_padded.data(), gfxrom, gfxrom_size);
	m_gfxrom = m_gfx_padded.data();
	m_gfxrom_size = gfxrom_size;
#endif

	m_local_videoram = std::make_unique<u16[]>(0x100000 / 2);
	std::fill_n(m_local_videoram.get(), 0x100000 / 2, 0);
	m_dma_timer = m_machine.timer_alloc([this](s32) {
		m_dma_register[DMA_COMMAND] &= ~0x8000; // tell the cpu we're done
		if (dma_irq_cb) dma_irq_cb(ASSERT_LINE);
	});
	memset(&m_dma_state, 0, sizeof(dma_state));
	m_dma_state.gfxrom = m_gfxrom;

	INIT_TEMPLATED_DMA_DRAW_GROUP(m_dma_draw_skip_scale,     true,  true);
	INIT_TEMPLATED_DMA_DRAW_GROUP(m_dma_draw_noskip_scale,   false, true);
	INIT_TEMPLATED_DMA_DRAW_GROUP(m_dma_draw_skip_noscale,   true,  false);
	INIT_TEMPLATED_DMA_DRAW_GROUP(m_dma_draw_noskip_noscale, false, false);
}

u16 midtunit_video_device::midtunit_gfxrom_r(offs_t offset)
{
	u8 const *const base = &m_gfxrom[m_gfxbank_offset[(offset >> 21) & 1]];
	offset = (offset & 0x01fffff) * 2;
	return base[offset] | (base[offset + 1] << 8);
}

u16 midtunit_video_device::midwunit_gfxrom_r(offs_t offset)
{
	u8 const *const base = &m_gfxrom[m_gfxbank_offset[0]];
	offset *= 2;
	return base[offset] | (base[offset + 1] << 8);
}

void midtunit_video_device::midtunit_vram_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset *= 2;
	if (m_videobank_select)
	{
		if (ACCESSING_BITS_0_7)
			m_local_videoram[offset] = (data & 0xff) | ((m_dma_register[DMA_PALETTE] & 0xff) << 8);
		if (ACCESSING_BITS_8_15)
			m_local_videoram[offset + 1] = ((data >> 8) & 0xff) | (m_dma_register[DMA_PALETTE] & 0xff00);
	}
	else
	{
		if (ACCESSING_BITS_0_7)
			m_local_videoram[offset] = (m_local_videoram[offset] & 0xff) | ((data & 0xff) << 8);
		if (ACCESSING_BITS_8_15)
			m_local_videoram[offset + 1] = (m_local_videoram[offset + 1] & 0xff) | (data & 0xff00);
	}
}

void midtunit_video_device::midtunit_vram_data_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset *= 2;
	if (ACCESSING_BITS_0_7)
		m_local_videoram[offset] = (data & 0xff) | ((m_dma_register[DMA_PALETTE] & 0xff) << 8);
	if (ACCESSING_BITS_8_15)
		m_local_videoram[offset + 1] = ((data >> 8) & 0xff) | (m_dma_register[DMA_PALETTE] & 0xff00);
}

void midtunit_video_device::midtunit_vram_color_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset *= 2;
	if (ACCESSING_BITS_0_7)
		m_local_videoram[offset] = (m_local_videoram[offset] & 0xff) | ((data & 0xff) << 8);
	if (ACCESSING_BITS_8_15)
		m_local_videoram[offset + 1] = (m_local_videoram[offset + 1] & 0xff) | (data & 0xff00);
}

u16 midtunit_video_device::midtunit_vram_r(offs_t offset)
{
	offset *= 2;
	if (m_videobank_select)
		return (m_local_videoram[offset] & 0x00ff) | (m_local_videoram[offset + 1] << 8);
	else
		return (m_local_videoram[offset] >> 8) | (m_local_videoram[offset + 1] & 0xff00);
}

u16 midtunit_video_device::midtunit_vram_data_r(offs_t offset)
{
	offset *= 2;
	return (m_local_videoram[offset] & 0x00ff) | (m_local_videoram[offset + 1] << 8);
}

u16 midtunit_video_device::midtunit_vram_color_r(offs_t offset)
{
	offset *= 2;
	return (m_local_videoram[offset] >> 8) | (m_local_videoram[offset + 1] & 0xff00);
}

void midtunit_video_device::to_shiftreg(offs_t address, u16 *shiftreg)
{
	u32 idx = std::min<u32>(address >> 3, 0x80000 - 1024);
	memcpy(shiftreg, &m_local_videoram[idx], 2 * 512 * sizeof(u16));
}

void midtunit_video_device::from_shiftreg(offs_t address, u16 *shiftreg)
{
	u32 idx = std::min<u32>(address >> 3, 0x80000 - 1024);
	memcpy(&m_local_videoram[idx], shiftreg, 2 * 512 * sizeof(u16));
}

void midtunit_video_device::midtunit_control_w(offs_t offset, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_midtunit_control);
	if (!(m_midtunit_control & 0x0080) || !m_gfx_rom_large)
		m_gfxbank_offset[0] = 0x000000;
	else
		m_gfxbank_offset[0] = 0x800000;
	m_videobank_select = (m_midtunit_control >> 5) & 1;
}

void midtunit_video_device::midwunit_control_w(offs_t offset, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_midtunit_control);
	m_gfxbank_offset[0] = 0x800000 * ((m_midtunit_control >> 8) & 3);
	m_videobank_select = (m_midtunit_control >> 11) & 1;
}

/*** fast pixel extractors ***/
#if !defined(ALIGN_SHORTS) && defined(LSB_FIRST)
#define EXTRACTGEN(m)   ((*(uint16_t *)&base[o >> 3] >> (o & 7)) & (m))
#elif defined(powerc)
#define EXTRACTGEN(m)   ((__lhbrx(base, o >> 3) >> (o & 7)) & (m))
#else
#define EXTRACTGEN(m)   (((base[o >> 3] | (base[(o >> 3) + 1] << 8)) >> (o & 7)) & (m))
#endif

/*** core blitter routine macro ***/
template <int BitsPerPixel, bool XFlip, bool Skip, bool Scale, midtunit_video_device::op_type_t Zero, midtunit_video_device::op_type_t NonZero>
void midtunit_video_device::dma_draw()
{
	int const height = m_dma_state.height << 8;
	uint8_t const *const base = m_dma_state.gfxrom;
	uint32_t offset = m_dma_state.offset;
	uint16_t pal = m_dma_state.palette;
	uint16_t color = pal | m_dma_state.color;
	int sy = m_dma_state.ypos;
	int iy = 0;
	int ty;
	int const mask = (1 << BitsPerPixel) - 1;
	int const xstep = Scale ? m_dma_state.xstep : 0x100;

	// loop over the height
	while (iy < height)
	{
		int const startskip = m_dma_state.startskip << 8;
		[[maybe_unused]] int endskip = m_dma_state.endskip << 8;
		int width = m_dma_state.width << 8;
		int sx = m_dma_state.xpos;
		int ix = 0;
		int tx;
		uint32_t o = offset;
		int pre, post;
		uint16_t *d;

		// handle skipping
		if (Skip)
		{
			uint8_t const value = EXTRACTGEN(0xff);
			o += 8;

			// adjust for preskip
			pre = (value & 0x0f) << (m_dma_state.preskip + 8);
			tx = pre / xstep;
			if (XFlip)
				sx = (sx - tx) & XPOSMASK;
			else
				sx = (sx + tx) & XPOSMASK;
			ix += tx * xstep;

			// adjust for postskip
			post = ((value >> 4) & 0x0f) << (m_dma_state.postskip + 8);
			width -= post;
			endskip -= post;
		}

		// handle Y clipping
		if (sy < m_dma_state.topclip || sy > m_dma_state.botclip)
			goto clipy;

		// handle start skip
		if (ix < startskip)
		{
			tx = ((startskip - ix) / xstep) * xstep;
			ix += tx;
			o += (tx >> 8) * BitsPerPixel;
		}

		// handle end skip
		if ((width >> 8) > m_dma_state.width - m_dma_state.endskip)
			width = (m_dma_state.width - m_dma_state.endskip) << 8;

		// determine destination pointer
		d = &m_local_videoram[sy * 512];

		// loop until we draw the entire width
		while (ix < width)
		{
			// only process if not clipped
			if (sx >= m_dma_state.leftclip && sx <= m_dma_state.rightclip)
			{
				if (Zero == NonZero)
				{
					// special case similar handling of zero/non-zero
					if (Zero == PIXEL_COLOR)
						d[sx] = color;
					else if (Zero == PIXEL_COPY)
						d[sx] = (EXTRACTGEN(mask)) | pal;
				}
				else
				{
					// otherwise, read the pixel and look
					int pixel = (EXTRACTGEN(mask));

					// non-zero pixel case
					if (pixel)
					{
						if (NonZero == PIXEL_COLOR)
							d[sx] = color;
						else if (NonZero == PIXEL_COPY)
							d[sx] = pixel | pal;
					}

					// zero pixel case
					else
					{
						if (Zero == PIXEL_COLOR)
							d[sx] = color;
						else if (Zero == PIXEL_COPY)
							d[sx] = pal;
					}
				}
			}

			// update pointers
			if (XFlip)
				sx = (sx - 1) & XPOSMASK;
			else
				sx = (sx + 1) & XPOSMASK;

			// advance to the next pixel
			if (!Scale)
			{
				ix += 0x100;
				o += BitsPerPixel;
			}
			else
			{
				tx = ix >> 8;
				ix += xstep;
				tx = (ix >> 8) - tx;
				o += BitsPerPixel * tx;
			}
		}

	clipy:
		// advance to the next row
		if (m_dma_state.yflip)
			sy = (sy - 1) & YPOSMASK;
		else
			sy = (sy + 1) & YPOSMASK;
		if (!Scale)
		{
			iy += 0x100;
			width = m_dma_state.width;
			if (Skip)
			{
				offset += 8;
				width -= (pre + post) >> 8;
				if (width > 0) offset += width * BitsPerPixel;
			}
			else
			{
				offset += width * BitsPerPixel;
			}
		}
		else
		{
			ty = iy >> 8;
			iy += m_dma_state.ystep;
			ty = (iy >> 8) - ty;
			if (!Skip)
			{
				offset += ty * m_dma_state.width * BitsPerPixel;
			}
			else if (ty--)
			{
				o = offset + 8;
				width = m_dma_state.width - ((pre + post) >> 8);
				if (width > 0) o += width * BitsPerPixel;
				while (ty--)
				{
					uint8_t const value = EXTRACTGEN(0xff);
					o += 8;
					pre = (value & 0x0f) << m_dma_state.preskip;
					post = ((value >> 4) & 0x0f) << m_dma_state.postskip;
					width = m_dma_state.width - pre - post;
					if (width > 0) o += width * BitsPerPixel;
				}
				offset = o;
			}
		}
	}
}

DEFINE_TEMPLATED_DMA_DRAW_GROUP(true,  true);
DEFINE_TEMPLATED_DMA_DRAW_GROUP(false, true);
DEFINE_TEMPLATED_DMA_DRAW_GROUP(true,  false);
DEFINE_TEMPLATED_DMA_DRAW_GROUP(false, false);


u16 midtunit_video_device::dma_r(offs_t offset)
{
	if (offset == 0)
		offset = 1;
	return m_dma_register[offset];
}

void midtunit_video_device::dma_w(offs_t offset, u16 data, u16 mem_mask)
{
	static const u8 register_map[2][16] =
	{
		{ 0,1,2,3,4,5,6,7,8,9,10,11,16,17,14,15 },
		{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 }
	};
	int const regbank = (m_dma_register[DMA_CONFIG] >> 5) & 1;
	int pixels = 0;

	int regnum = register_map[regbank][offset & 15];
	COMBINE_DATA(&m_dma_register[regnum]);
	if (regnum != DMA_COMMAND)
		return;

	int const command = m_dma_register[DMA_COMMAND];
	if (dma_irq_cb) dma_irq_cb(CLEAR_LINE);
	if (!(command & 0x8000))
		return;
	m_dma_count++;

	int const bpp = (command >> 12) & 7;
	m_dma_state.xpos = m_dma_register[DMA_XSTART] & XPOSMASK;
	m_dma_state.ypos = m_dma_register[DMA_YSTART] & YPOSMASK;
	m_dma_state.width = m_dma_register[DMA_WIDTH] & 0x3ff;
	m_dma_state.height = m_dma_register[DMA_HEIGHT] & 0x3ff;
	m_dma_state.palette = m_dma_register[DMA_PALETTE] & 0x7f00;
	m_dma_state.color = m_dma_register[DMA_COLOR] & 0xff;
	m_dma_state.yflip = (command & 0x20) >> 5;
	m_dma_state.preskip = (command >> 8) & 3;
	m_dma_state.postskip = (command >> 10) & 3;
	m_dma_state.xstep = m_dma_register[DMA_SCALE_X] ? m_dma_register[DMA_SCALE_X] : 0x100;
	m_dma_state.ystep = m_dma_register[DMA_SCALE_Y] ? m_dma_register[DMA_SCALE_Y] : 0x100;
	m_dma_state.topclip = m_dma_register[DMA_TOPCLIP] & 0x1ff;
	m_dma_state.botclip = m_dma_register[DMA_BOTCLIP] & 0x1ff;
	m_dma_state.leftclip = m_dma_register[DMA_LEFTCLIP] & 0x3ff;
	m_dma_state.rightclip = m_dma_register[DMA_RIGHTCLIP] & 0x3ff;

	u32 gfxoffset = m_dma_register[DMA_OFFSETLO] | (m_dma_register[DMA_OFFSETHI] << 16);
	if ((command & 0x0f) == 0x0c)
		gfxoffset = 0;
	if (!m_gfx_rom_large && gfxoffset >= 0x2000000)
		gfxoffset -= 0x2000000;
	if (gfxoffset >= 0xf8000000)
		gfxoffset -= 0xf8000000;
	if (gfxoffset < 0x10000000)
		m_dma_state.offset = gfxoffset;
	else
		goto skipdma;
#ifdef NBAJAM_LOWMEM
	if ((gfxoffset >> 3) >= m_gfxrom_size)
		goto skipdma;
#endif

	if (command & 0x40)
	{
		m_dma_state.startskip = m_dma_register[DMA_LRSKIP] & 0xff;
		m_dma_state.endskip = m_dma_register[DMA_LRSKIP] >> 8;
	}
	else
	{
		m_dma_state.startskip = 0;
		m_dma_state.endskip = m_dma_register[DMA_LRSKIP];
	}

	if (m_dma_state.xstep == 0x100 && m_dma_state.ystep == 0x100)
	{
		if (command & 0x80)
			((this)->*(m_dma_draw_skip_noscale[(command & 0x1f) * 8 + bpp]))();
		else
			((this)->*(m_dma_draw_noskip_noscale[(command & 0x1f) * 8 + bpp]))();
		pixels = m_dma_state.width * m_dma_state.height;
	}
	else
	{
		if (command & 0x80)
			((this)->*(m_dma_draw_skip_scale[(command & 0x1f) * 8 + bpp]))();
		else
			((this)->*(m_dma_draw_noskip_scale[(command & 0x1f) * 8 + bpp]))();
		if (m_dma_state.xstep && m_dma_state.ystep)
			pixels = ((m_dma_state.width << 8) / m_dma_state.xstep) * ((m_dma_state.height << 8) / m_dma_state.ystep);
		else
			pixels = 0;
	}

skipdma:
	m_dma_timer->adjust(attotime::from_nsec(41 * pixels));
}

void midtunit_video_device::scanline_update(screen_device &screen, bitmap_ind16 &bitmap, int scanline, const tms340x0_device::display_params *params)
{
	u16 const *const src = &m_local_videoram[(params->rowaddr << 9) & 0x3fe00];
	u16 *const dest = &bitmap.pix(scanline);
	int coladdr = params->coladdr << 1;
	for (int x = params->heblnk; x < params->hsblnk; x++)
		dest[x] = src[coladdr++ & 0x1ff] & 0x7fff;
}
