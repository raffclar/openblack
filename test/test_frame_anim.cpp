/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The frame clocks of src/3D/FrameAnim.h against the original's formulas: every one picks a whole frame (no blend),
// with its own constants and edges (the cell-32 flame, the mist's skipped cells 14-15, the fish's cell before its wrap,
// the icon's negative fmod, the hand flow's rounding), and the loaders for mods.

#include <cmath>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"

using namespace openblack::graphics;

TEST(FrameAnim, spriteCellAndOffset)
{
	// the sprite's cell is its low 6 bits, the UVs of billboard::CellUv
	EXPECT_EQ(frame_anim::SpriteCell(64 + 9), 9);
	EXPECT_EQ(frame_anim::SpriteCell(-1), 63);
	const auto uv = frame_anim::SpriteCellUv(9, 8);
	EXPECT_FLOAT_EQ(uv[0].x, 0.125f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f);
	EXPECT_FLOAT_EQ(uv[2].x, 0.25f);
	// (0, 0) is off; a fixed material (bit 0x10) keeps its UVs
	EXPECT_FALSE(frame_anim::IsAnimatedUv({0.0f, 0.0f}));
	EXPECT_TRUE(frame_anim::IsAnimatedUv({0.0f, -0.5f}));
	EXPECT_FLOAT_EQ(frame_anim::OffsetUv({0.1f, 0.2f}, {0.25f, 0.5f}, false).y, 0.7f);
	EXPECT_FLOAT_EQ(frame_anim::OffsetUv({0.1f, 0.2f}, {0.25f, 0.5f}, true).y, 0.2f);
	// PackUvOffset is exact for the icons' 32/256 steps: v + 4 x 32 col
	for (int col = 0; col < 8; ++col)
	{
		const float u = static_cast<float>(col) * 0.00390625f * 32.0f;
		const float packed = frame_anim::PackUvOffset(u, 0.375f);
		const float steps = std::floor((packed + 2.0f) / 4.0f);
		EXPECT_FLOAT_EQ(steps / 256.0f, u);
		EXPECT_FLOAT_EQ(packed - steps * 4.0f, 0.375f);
	}
}

TEST(FrameAnim, animTexturedCell)
{
	// cells of 64 x 32 in rows of 256 / 64 = 4
	frame_anim::AnimTexturedSheet sheet {64, 32, false, false, 16};
	const auto uv = frame_anim::AnimTexturedCell(6, sheet);
	EXPECT_FLOAT_EQ(uv.x, 0.5f);   // 64 / 256 x (6 % 4)
	EXPECT_FLOAT_EQ(uv.y, 0.125f); // 32 / 256 x (6 / 4)
	// the slide: H f / (N 256)
	sheet.slideV = true;
	sheet.frames = 1000;
	EXPECT_FLOAT_EQ(frame_anim::AnimTexturedCell(500, sheet).y, 32.0f * 500.0f / 256000.0f);
	EXPECT_FLOAT_EQ(frame_anim::AnimTexturedCell(500, sheet).x, 0.0f);
}

TEST(FrameAnim, oneOffAndSpellIcon)
{
	// 18 frames a second over 16, 4 x 4
	float phase = 0.0f;
	auto uv = frame_anim::OneOffFrame(phase, 500.0f); // 9 frames
	EXPECT_FLOAT_EQ(phase, 9.0f);
	EXPECT_FLOAT_EQ(uv.x, 0.25f);
	EXPECT_FLOAT_EQ(uv.y, 0.5f);
	uv = frame_anim::OneOffFrame(phase, 500.0f); // 18 -> 2
	EXPECT_FLOAT_EQ(phase, 2.0f);
	EXPECT_FLOAT_EQ(uv.x, 0.5f);
	EXPECT_FLOAT_EQ(uv.y, 0.0f);
	// -15 a second, fmod 32 and + 32 when negative; 8 x 4 in 32/256 steps
	float icon = 0.0f;
	uv = frame_anim::SpellIconFrame(icon, 0.01f); // -0.15 -> 31.85: frame 31
	EXPECT_NEAR(icon, 31.85f, 1e-5f);
	EXPECT_FLOAT_EQ(uv.x, 0.875f);
	EXPECT_FLOAT_EQ(uv.y, 0.375f);
	uv = frame_anim::SpellIconFrame(icon, 1.0f); // 16.85: frame 16
	EXPECT_FLOAT_EQ(uv.x, 0.0f);
	EXPECT_FLOAT_EQ(uv.y, 0.25f);
}

TEST(FrameAnim, handFlowRounds)
{
	// -20 a second, into [0, 64) from below; rounded to the nearest
	float phase = 0.0f;
	auto uv = frame_anim::HandFlowFrame(phase, 0.03f); // -0.6 -> 63.4 -> 63 % 32 = 31
	EXPECT_NEAR(phase, 63.4f, 1e-5f);
	EXPECT_FLOAT_EQ(uv.x, 0.875f);
	EXPECT_FLOAT_EQ(uv.y, 0.375f);
	phase = 10.6f;
	uv = frame_anim::HandFlowFrame(phase, 0.0f); // rounded to 11, not truncated to 10
	EXPECT_FLOAT_EQ(uv.x, 0.375f);
	EXPECT_FLOAT_EQ(uv.y, 0.125f);
}

TEST(FrameAnim, psysFrames)
{
	// looped, fmod then + N when negative; t' up to 5
	EXPECT_EQ(frame_anim::ParticleFrameIndex(17.5f, 16, true), 1);
	EXPECT_EQ(frame_anim::ParticleFrameIndex(-0.5f, 16, true), 15);
	// not looped: clamped to 0..N - 1
	EXPECT_EQ(frame_anim::ParticleFrameIndex(-3.0f, 16, false), 0);
	EXPECT_EQ(frame_anim::ParticleFrameIndex(40.0f, 16, false), 15);
	EXPECT_FLOAT_EQ(frame_anim::ParticleFrameLerp(2.0f, 4.0f, 0.5f, true), 3.0f);
	EXPECT_FLOAT_EQ(frame_anim::ParticleFrameLerp(2.0f, 4.0f, 3.0f, true), 8.0f);
	EXPECT_FLOAT_EQ(frame_anim::ParticleFrameLerp(2.0f, 4.0f, 3.0f, false), 4.0f);
	EXPECT_FLOAT_EQ(frame_anim::ParticleFrameLerp(2.0f, 4.0f, 9.0f, true), 12.0f);
	// both kept in [0, 2N): above 2 N down by N, below 0 up by 2 N
	float previous = 0.0f;
	float current = 31.5f;
	frame_anim::ParticleFrameAdvance(previous, current, 0.1f, 10.0f, 16, true); // 32.5, previous 31.5: not both above 32
	EXPECT_FLOAT_EQ(current, 32.5f);
	frame_anim::ParticleFrameAdvance(previous, current, 0.1f, 10.0f, 16, true); // 33.5 and 32.5 -> 17.5 and 16.5
	EXPECT_FLOAT_EQ(current, 17.5f);
	EXPECT_FLOAT_EQ(previous, 16.5f);
	current = 0.5f;
	frame_anim::ParticleFrameAdvance(previous, current, 0.1f, -10.0f, 16, true); // -0.5 -> 31.5, previous 32.5
	EXPECT_FLOAT_EQ(current, 31.5f);
	EXPECT_FLOAT_EQ(previous, 32.5f);
	// without PlayAnim no step and no wrap, a negative frame stays negative
	current = -3.0f;
	frame_anim::ParticleFrameAdvance(previous, current, 0.1f, 10.0f, 16, false);
	EXPECT_FLOAT_EQ(current, -3.0f);
	EXPECT_FLOAT_EQ(previous, -3.0f);
	EXPECT_EQ(frame_anim::ParticleFrameIndex(current, 16, false), 0);
	// PlayAnim with rate 0 still wraps: -3 + 32
	frame_anim::ParticleFrameAdvance(previous, current, 0.1f, 0.0f, 16, true);
	EXPECT_FLOAT_EQ(current, 29.0f);
}

TEST(FrameAnim, mistSkipsCells14And15)
{
	// (c x 45 / 900) & 15, c up to 900 (kept), so cells 14 and 15 are not reached in the third turn
	EXPECT_EQ(frame_anim::MistCell(0), 0);
	EXPECT_EQ(frame_anim::MistCell(319), 15);
	EXPECT_EQ(frame_anim::MistCell(320), 0);
	EXPECT_EQ(frame_anim::MistCell(899), 12);
	EXPECT_EQ(frame_anim::MistCell(900), 13);
	bool seen14 = false;
	for (int c = 640; c <= 900; ++c)
	{
		seen14 = seen14 || frame_anim::MistCell(c) >= 14;
	}
	EXPECT_FALSE(seen14);
	// the wrap only past 900
	frame_anim::MistClock clock {896, 0.0f};
	frame_anim::MistAdvanceExact(clock, 16); // + trunc(4.08) = 900: kept
	EXPECT_EQ(clock.counter, 900);
	frame_anim::MistAdvanceExact(clock, 16); // 904 -> 4
	EXPECT_EQ(clock.counter, 4);
	// openblack keeps the fraction: 4 frames of 1 ms make one count
	frame_anim::MistClock fine {0, 0.0f};
	for (int i = 0; i < 4; ++i)
	{
		frame_anim::MistAdvance(fine, 1.0f);
	}
	EXPECT_EQ(fine.counter, 1);
	// rows 2-3 in the effect branch
	EXPECT_FLOAT_EQ(frame_anim::MistCellUv(9, true).x, 0.125f);
	EXPECT_FLOAT_EQ(frame_anim::MistCellUv(9, true).y, 0.375f);
	EXPECT_FLOAT_EQ(frame_anim::MistCellUv(9, false).y, 0.125f);
	EXPECT_EQ(frame_anim::MistStartCounter(15.9f), 15);
	// the smoke's age step: dt x 255 with the fraction kept, dt at most 100 s
	float remainder = 0.0f;
	EXPECT_EQ(frame_anim::SmokeAgeStep(remainder, 16.0f), 4);
	EXPECT_NEAR(remainder, 0.08f, 1e-4f);
}

TEST(FrameAnim, fireCell32)
{
	// trunc(fmod(-25 age, 32) + 32); at age 0 the cell is 32
	EXPECT_EQ(frame_anim::FireCell(0.0f), 32);
	EXPECT_EQ(frame_anim::FireCell(0.04f), 31);
	// trunc(fmod(25 age, 32))
	EXPECT_EQ(frame_anim::SteamCell(0.0f), 0);
	EXPECT_EQ(frame_anim::SteamCell(1.3f), 0); // 32.5 -> 0.5
	EXPECT_EQ(frame_anim::SteamCell(0.5f), 12);
}

TEST(FrameAnim, fishCellBeforeWrap)
{
	// dt at most 0.1; the cell is taken before the frame wraps
	float frame = 14.5f;
	const auto cell = frame_anim::FishFrame(frame, 0.04f, 1.0f); // 15.5: cell 8 + 15, then 0.5
	EXPECT_EQ(cell, 23);
	EXPECT_NEAR(frame, 0.5f, 1e-5f);
	frame = 0.0f;
	(void)frame_anim::FishFrame(frame, 1.0f, 1.0f); // dt 0.1: 2.5 frames
	EXPECT_FLOAT_EQ(frame, 2.5f);
	EXPECT_FLOAT_EQ(frame_anim::FishDt(0.5f), 0.1f);
}

TEST(FrameAnim, lanterns)
{
	// one clock, 31 steps in 700 ms; the flames start at a shared table ({0, 13} in the file)
	int clock = 0;
	EXPECT_EQ(frame_anim::LanternAdvance(clock, 350), 15);
	EXPECT_EQ(frame_anim::LanternAdvance(clock, 350), 31); // 700 is kept
	EXPECT_EQ(clock, 700);
	EXPECT_EQ(frame_anim::LanternAdvance(clock, 10), 0); // 710 -> 10
	const auto& starts = frame_anim::k_LanternFileStarts;
	EXPECT_EQ(frame_anim::LanternCell(0, 0, starts), 31);
	EXPECT_EQ(frame_anim::LanternCell(0, 1, starts), (10 + 31 - 13) & 31);
	EXPECT_EQ(frame_anim::LanternCell(5, 0, starts), 26);
	// every new light rewrites the table with trunc(Random(0, 31))
	const frame_anim::LanternStarts written = {frame_anim::LanternStart(30.9f), frame_anim::LanternStart(7.2f), 0};
	EXPECT_EQ(written[0], 30);
	EXPECT_EQ(frame_anim::LanternCell(0, 1, written), (10 + 31 - 7) & 31);
}

TEST(FrameAnim, influenceScroll)
{
	// the influence circle: one int clock modulo 10000 ms, u = c x 0.0001 and v = -c x 0.0002
	int32_t clock = 0;
	auto offset = frame_anim::InfluenceScroll(clock, 2500);
	EXPECT_EQ(clock, 2500);
	EXPECT_NEAR(offset.x, 0.25f, 1e-6f);
	EXPECT_NEAR(offset.y, -0.5f, 1e-6f);
	offset = frame_anim::InfluenceScroll(clock, 7600); // 10100 -> 100
	EXPECT_EQ(clock, 100);
	EXPECT_NEAR(offset.x, 0.01f, 1e-6f);
	EXPECT_NEAR(offset.y, -0.02f, 1e-6f);
	EXPECT_TRUE(frame_anim::IsAnimatedUv(offset));
	offset = frame_anim::InfluenceScroll(clock, 9900); // exactly 10000 -> 0: no offset
	EXPECT_EQ(clock, 0);
	EXPECT_FALSE(frame_anim::IsAnimatedUv(offset));
}

TEST(FrameAnim, otherCellClocks)
{
	// the leash: 10 a second over 15, after the wrap
	float leash = 14.0f;
	EXPECT_EQ(frame_anim::LeashCell(leash, 0.15f), 0); // 15.5 -> 0.5
	float u = 0.9f;
	EXPECT_NEAR(frame_anim::LeashScroll(u, 0.4f), 0.1f, 1e-5f);
	// golden shower: (t / 50 + base + drop) % 32
	EXPECT_EQ(frame_anim::GoldenShowerCell(1000, 3, 10), 1);
	// creature room: 31 - (((tick >> 5) + i) & 31)
	EXPECT_EQ(frame_anim::CreatureRoomCell(32u * 5u, 1), 25);
	// cursor: (tick / 50) & 15
	EXPECT_EQ(frame_anim::CursorCell(850), 1);
	// help system: (clock / 200) & 15
	int32_t help = 0;
	EXPECT_EQ(frame_anim::HelpSystemCell(help, 3300), 0);
	EXPECT_EQ(frame_anim::HelpSystemCell(help, 100), 1);
	// + ms x 0.01, restarted at 0 past 15
	float jc = 14.0f;
	EXPECT_EQ(frame_anim::JCSpecialCell(jc, 100.0f), 15);
	EXPECT_EQ(frame_anim::JCSpecialCell(jc, 10.0f), 0);
	EXPECT_FLOAT_EQ(jc, 0.0f);
	// player symbol: -= ms x 0.02 (layer 0) or 0.023 (layer 1), + 32 while negative
	float symbol = 0.0f;
	EXPECT_EQ(frame_anim::PlayerSymbolCell(symbol, 50.0f, 0), 31);
	float symbolB = 0.0f;
	EXPECT_EQ(frame_anim::PlayerSymbolCell(symbolB, 100.0f, 1), 29); // 32 - 2.3
	float spin = 6.0f;
	EXPECT_NEAR(frame_anim::PlayerSymbolSpin(spin, 200.0f), 6.4f - 6.28318548f, 1e-5f);
	EXPECT_EQ(frame_anim::DisappearSmokeCell(0.99f), 14);
	EXPECT_EQ(frame_anim::DustCell(3, 7.0f), 16 + 1);
}

TEST(FrameAnim, scrolls)
{
	// waterfall: V -= 0.5 dt, minus its whole part: in -1..0
	float v = 0.0f;
	EXPECT_FLOAT_EQ(frame_anim::WaterfallScroll(v, 1.0f), -0.5f);
	EXPECT_FLOAT_EQ(frame_anim::WaterfallScroll(v, 1.0f), 0.0f);
	// at t = 500: x = 1
	const auto ghost = frame_anim::GoolooFrame(500.0f);
	EXPECT_NEAR(ghost.uv.x, 2.0f * std::cos(1.0f), 1e-5f);
	EXPECT_NEAR(ghost.uv.y, 1.7f * std::sin(0.7f), 1e-5f);
	EXPECT_EQ(ghost.materialByte, 0);
	EXPECT_EQ(frame_anim::GoolooFrame(0.0f).materialByte, 255);
	// the ghost's time: 500 ms down by the frame's ms, gone at 0; its first frame (t = 500) has the byte 0, its last
	// ones run up to 255
	auto time = frame_anim::GhostTime {frame_anim::k_GhostMs, false};
	int frames = 0;
	while (!time.expired)
	{
		time = frame_anim::GhostStep(time.remainingMs, 16.0f);
		++frames;
	}
	EXPECT_EQ(frames, 32); // 31 x 16 = 496 left 4 ms, the 32nd frame ends it
	EXPECT_FLOAT_EQ(time.remainingMs, 0.0f);
	EXPECT_FALSE(frame_anim::GhostStep(500.0f, 499.0f).expired);
	EXPECT_FLOAT_EQ(frame_anim::GhostStep(500.0f, 499.0f).remainingMs, 1.0f);
	EXPECT_TRUE(frame_anim::GhostStep(500.0f, 500.0f).expired);
	EXPECT_EQ(frame_anim::GoolooFrame(4.0f).materialByte, 255 - 2); // 255 x 4 / 500 = 2.04
	EXPECT_NEAR(frame_anim::GoolooFrame(4.0f).uv.x, 2.0f * std::cos(0.008f), 1e-5f);
	// lerp, then the period taken off while above it (nothing added below 0)
	const auto rotating = frame_anim::RotatingUv({0.5f, -0.25f}, {2.5f, -0.75f}, 0.5f, {1.0f, 1.0f});
	EXPECT_FLOAT_EQ(rotating.x, 0.5f);
	EXPECT_FLOAT_EQ(rotating.y, -0.5f);
	// + ms x rate x 0.001, kept in FrameHeight / 256
	float scroll = 0.0f;
	EXPECT_NEAR(frame_anim::ChainScroll(scroll, 1000.0f, 0.3f, 64), 0.05f, 1e-6f);
	EXPECT_NEAR(frame_anim::ChainScroll(scroll, 1000.0f, -0.1f, 64), 0.2f, 1e-6f);
	float none = 0.0f;
	EXPECT_FLOAT_EQ(frame_anim::ChainScroll(none, 1000.0f, 0.0f, 64), 0.0f);
	// segment 1 of 4 over 2 textures: the second half of the first texture
	frame_anim::ChainSheet sheet;
	sheet.textures = 2;
	sheet.frameOfTail = 1;
	const auto uv = frame_anim::ChainSegmentUv(1, 4, sheet, 0.0f);
	EXPECT_FLOAT_EQ(uv[0].x, 0.125f); // frame 1 x 32 / 256
	EXPECT_FLOAT_EQ(uv[1].x, 0.25f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f); // 64 x 1/2 / 256
	EXPECT_FLOAT_EQ(uv[3].y, 0.25f);
}

TEST(FrameAnim, bandToEye)
{
	// columns -D, U, U x D, with D from the eye to the band
	const auto axes = billboard::BandToEye({0.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f});
	EXPECT_NEAR(axes[0].z, -1.0f, 1e-6f);
	EXPECT_NEAR(axes[1].y, 1.0f, 1e-6f);
	EXPECT_NEAR(axes[2].x, 1.0f, 1e-6f); // U x D = (0, 1, 0) x (0, 0, 1)
	// straight above: pushed off the vertical, still a rotation
	const auto above = billboard::BandToEye({0.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f});
	EXPECT_NEAR(glm::length(above[1]), 1.0f, 1e-4f);
	EXPECT_NEAR(glm::dot(above[0], above[1]), 0.0f, 1e-4f);
}

TEST(FrameAnim, loadBitmapFromFile)
{
	// the exact size only
	EXPECT_FALSE(frame_anim::LoadBitmapFromFile(std::vector<uint8_t>(10, 0), 2, 1, 4, 4).has_value());
	EXPECT_FALSE(frame_anim::LoadBitmapFromFile(std::vector<uint8_t>(2 * 2 * 3 * 3 + 1, 0), 2, 3, 3, 3).has_value());
	// 4 frames of 2 x 2 grey in the file's 2 x 2 grid: rows of 4 bytes
	std::vector<uint8_t> bytes(16);
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		bytes[i] = static_cast<uint8_t>(i);
	}
	const auto bitmap = frame_anim::LoadBitmapFromFile(bytes, 2, 1, 4, 4);
	ASSERT_TRUE(bitmap.has_value());
	EXPECT_EQ(bitmap->channels, 1);
	EXPECT_EQ(bitmap->frames, 4);
	const auto* frame1 = frame_anim::FrameTexels(*bitmap, 1);
	EXPECT_EQ(frame1[0], 2);
	EXPECT_EQ(frame1[1], 3);
	EXPECT_EQ(frame1[2], 6);
	EXPECT_EQ(frame_anim::FrameTexels(*bitmap, 2)[0], 8);
	// frame % frames
	EXPECT_EQ(frame_anim::FrameTexels(*bitmap, 5), frame1);
	// min(framesInUse, framesInFile) frames
	EXPECT_EQ(frame_anim::LoadBitmapFromFile(bytes, 2, 1, 4, 2)->frames, 2);
	EXPECT_EQ(frame_anim::FrameTexels(frame_anim::StackedFrames {}, 0), nullptr);
}
