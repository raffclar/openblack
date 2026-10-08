/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The frame animations of textures, one function per clock of the original (wiki: rendering-objects.md,
/// "Frame-animated textures"). Pure logic: no rendering state.
///
/// The original never blends two frames. Every user picks one whole frame (truncated, or an integer division) and
/// draws once; what looks smooth comes from many frames at 15 to 25 a second and from the continuous UV scrolls. There
/// is no common clock in the original either: every user keeps its own accumulator (per object, or a global where the
/// original has a global) with its own constants, so each one has its own function here and the caller keeps the state
/// where the original keeps it.
///
/// Two engine primitives draw the frames: the cell of a sprite (SpriteCell, the UVs are billboard::CellUv) and the UV
/// offset of a mesh object (UvOffset).
///
/// The clocks are float, as the original runs them: the game sets the FPU to single precision at startup and at the
/// start of every turn and 3D engine update, so every add, multiply and subtract is rounded like a float operation,
/// and the value kept between a fmod and the truncation is the float one
namespace openblack::graphics::frame_anim
{

// ---------------------------------------------------------------------------------------------------------------------
// Primitives

/// The sprite cell, the low 6 bits of the sprite's flags; the users write flags = (flags & ~0x3F) | (cell & 0x3F)
[[nodiscard]] constexpr uint8_t SpriteCell(int cell) noexcept
{
	return static_cast<uint8_t>(static_cast<uint32_t>(cell) & 0x3Fu);
}
/// The four corner UVs of a sprite cell: SpriteCell then billboard::CellUv
[[nodiscard]] std::array<glm::vec2, 4> SpriteCellUv(int cell, uint8_t cellsPerRow = 8);

/// The animated UV offset (u, v) of a mesh object; (0, 0) turns it off. Every object draw copies it to the renderer's
/// current offset (on when not both 0)
using UvOffset = glm::vec2;
/// The offset is on unless both are 0
[[nodiscard]] constexpr bool IsAnimatedUv(const UvOffset& offset) noexcept
{
	return offset.x != 0.0f || offset.y != 0.0f;
}
/// Every vertex UV += the offset unless the current material has its fixed-UV bit (L3DSubMesh::Primitive::uvOffset
/// clear). Some draw paths add it without testing the bit (fixedMaterial false)
[[nodiscard]] glm::vec2 OffsetUv(const glm::vec2& uv, const UvOffset& offset, bool fixedMaterial) noexcept;
/// (openblack) the transport of an object's offset to vs_object.sc: v + 4 x round(256 frac(u)) in the w of an instance's
/// second column; the shader unpacks v in -2..2 and u in 1/256 steps. Exact for every original user: the orbs' quarters,
/// the icons' 32/256 steps, the AnimTextured cells of whole pixels; a SlideU over 1000 frames is rounded to 1/256
[[nodiscard]] float PackUvOffset(float u, float v) noexcept;

/// The texture of an animated-texture particle mesh: its size, slides and frame count N
struct AnimTexturedSheet
{
	int width {64};  ///< TextureWidth, 0..128 pixels of a 256 texture
	int height {64}; ///< TextureHeight
	bool slideU {false};
	bool slideV {false};
	int frames {1}; ///< N: NumFrames, or 1000 when sliding
};
/// The cells of W x H pixels of a 256 texture, cols = 256 / W (integer division), u = (W / 256) (f % cols),
/// v = (H / 256) (f / cols) (f unsigned); with a slide, u = W f / (N 256) and v = H f / (N 256).
/// (openblack guard) cols is at least 1: the original divides by 256 / W unchecked
[[nodiscard]] UvOffset AnimTexturedCell(int frame, const AnimTexturedSheet& sheet) noexcept;

// ---------------------------------------------------------------------------------------------------------------------
// The clocks of the original. The state is the caller's, kept where the original keeps it.

/// The integer clocks take the game's frame time as it is: game_clock::FrameGameMs() (whole ms, the remainder of the
/// turn kept by the game clock itself).

/// The miracle bubble (O_Bibble_up.l3d, 4 x 4): phase = fmod(phase + ms x 18 x 0.001, 16), f = truncated phase,
/// offset ((f % 4) x 0.25, (f / 4) x 0.25)
[[nodiscard]] UvOffset OneOffFrame(float& phase, float milliseconds) noexcept;

/// The creature spell phials: phase = fmod(phase - 15 dt, 32), + 32 when negative, f = truncated phase,
/// u = (f % 8) (1/256) 32, v = (f / 8) (1/256) 32: 32 frames of an 8 x 4 sheet, 15 a second backwards.
/// dt = frame time x 0.001. When the fmod is a tiny negative the + 32 rounds to 32 itself (single precision, see the
/// top of this file): f = 32, (0, 0.5)
[[nodiscard]] UvOffset SpellIconFrame(float& phase, float seconds) noexcept;

/// The hand's flow effect: phase += dt x rate (rate = -20); with rate > 0, fmod(.., 2 N) once above 2 N; with
/// rate <= 0, fmod(.., 2 N) + 2 N once below 0 (N = 32). f = phase ROUNDED to the nearest (the FPU default), not
/// truncated, % N; u = (f % 8) x 0.125, v = (f / 8) x 0.125, an 8 x 4 sheet. The original writes it into the hand
/// mesh's vertices, not through the object's UV offset
[[nodiscard]] UvOffset HandFlowFrame(float& phase, float seconds, float rate = -20.0f, int frames = 32) noexcept;

/// A particle atom's frame step: previous = current; then only when animation is on and PlayAnim is 1 (else no step
/// and no wrap), current = dt x rate + previous, and with rate > 0, while both are above 2 N they go down by N;
/// with rate <= 0, while either is below 0 both go up by 2 N (N = the frame count)
void ParticleFrameAdvance(float& previous, float& current, float dt, float rate, int frames, bool play) noexcept;
/// The frame between two steps, t' = clamp(t, 0, 5) when looped (LoopAnim), else clamp(t, 0, 1):
/// previous + (current - previous) t'
[[nodiscard]] float ParticleFrameLerp(float previous, float current, float t, bool loop) noexcept;
/// The whole frame drawn. Looped: truncated fmod(f, N), + N first when the fmod is negative; otherwise f truncated and
/// clamped to 0..N - 1
[[nodiscard]] int ParticleFrameIndex(float frame, int frames, bool loop) noexcept;

/// The counter of a mist and of each smoke puff: cell = (c x 45 / 900) & 15 in integers. c = 900 is kept (the wrap is
/// c > 900), so c / 20 reaches 45: the cells run 0..15, 0..15, 0..13 (14 at c = 900) and back to 0; cells 14 and 15
/// are skipped once every 900
[[nodiscard]] constexpr int MistCell(int counter) noexcept
{
	return (counter * 45 / 900) & 15;
}
/// Offset ((f & 7) x 0.125, (f >> 3 & 7) x 0.125 + 0.25) for the effect mists (the sky clouds and the effect mists:
/// rows 2-3 of smoke.raw), without the 0.25 for the others (the map mists: rows 0-1)
[[nodiscard]] UvOffset MistCellUv(int cell, bool effect) noexcept;
/// The mist clock: only run by the draw of a mist the Z-sorter got (only the ones whose sphere touches the screen)
struct MistClock
{
	int counter {0};
	float remainder {0.0f}; ///< (openblack) the fraction of ms x 0.255 kept between frames, see Advance
};
/// The original: counter += truncated(frame time x 0.255); if > 900, %= 900
void MistAdvanceExact(MistClock& clock, uint32_t gameTimeIncMs) noexcept;
/// (openblack) the same with the fraction of ms x 0.255 kept, as every mist, cloud and smoke of the port does: the
/// original's truncation of each frame would stop the animation under 4 ms a frame (openblack runs uncapped)
void MistAdvance(MistClock& clock, float milliseconds) noexcept;
/// A new mist's counter: truncated(Random(0, 16)) & 15: cell 0
[[nodiscard]] constexpr int MistStartCounter(float random0To16) noexcept
{
	return static_cast<int>(random0To16) & 15;
}
/// Smoke: dt = min(frame time x 0.001, 100); every puff's age += truncated(dt x 255), back past 900; its cell is
/// MistCell(age). (openblack) the fraction of dt x 255 is kept in remainder and the whole step is the same for the
/// puffs of one smoke, as their dt is
[[nodiscard]] int SmokeAgeStep(float& remainder, float milliseconds) noexcept;

/// Fire flames: cell = truncated(fmod(-25 age, 32) + 32), age the sprite's age. The + 32 is unconditional: at age 0,
/// -25 x 0 = -0, fmod gives -0 and the cell is 32 (a flame drawn before any time went by)
[[nodiscard]] int FireCell(float age) noexcept;
/// cell = truncated(fmod(25 age, 32)), the steam and the grey smoke
[[nodiscard]] int SteamCell(float age) noexcept;

/// The fish of a fish farm: dt = min(frame time x 0.001, 0.1), frame += dt x speed x 25; the cell
/// (8 + (truncated(frame) & 15)) & 0x3F goes to the sprite BEFORE the wrap frame -= 15 truncated(frame x (1/15)), so a
/// frame of 15.x shows cell 23. @return the cell
[[nodiscard]] uint8_t FishFrame(float& frame, float seconds, float speed) noexcept;
/// The fish's dt: min(dt, 0.1)
[[nodiscard]] float FishDt(float seconds) noexcept;

/// The street lanterns and bonfires: one GLOBAL clock += frame time, if > 700 %= 700, a = c x 31 / 700 (integers); run
/// only while the village light alpha is not 0 and the light list is not empty. @return a
[[nodiscard]] int LanternAdvance(int& clockMs, uint32_t gameTimeIncMs) noexcept;
/// The GLOBAL start table of the light's three sprites, one entry each: {0, 13, 0} at load, and every new light writes
/// truncated(Random(0, 31)) into all three, so every light uses the values of the last one made (all in phase).
/// LanternStart is that value
using LanternStarts = std::array<int, 3>;
inline constexpr LanternStarts k_LanternFileStarts = {0, 13, 0};
/// truncated(Random(0, 31)), random a value in [0, 31]
[[nodiscard]] constexpr int LanternStart(float random0To31) noexcept
{
	return static_cast<int>(random0To31);
}
/// Flame i (0, 1) of a light, cell = (10 i + 31 - ((start[i] + a) & 31)) & 31, start from the global table; the third
/// sprite (the glow) only takes the colour
[[nodiscard]] uint8_t LanternCell(int a, int flame, const LanternStarts& starts) noexcept;

/// The citadel's leashes: phase += 10 dt; phase -= 15 truncated(phase x (1/15)); cell = truncated(phase) & 0x3F (after
/// the wrap): 15 cells at 10 a second
[[nodiscard]] uint8_t LeashCell(float& phase, float seconds) noexcept;
/// u += 0.5 dt, minus its whole part; the offset is (u, the colour row: n x 0.125, or 0.375)
[[nodiscard]] float LeashScroll(float& u, float seconds) noexcept;

/// The golden shower drops: cell = ((t / 50) + base + drop offset) % 32, signed C division and remainder, & 0x3F.
/// (inferred) t in milliseconds
[[nodiscard]] uint8_t GoldenShowerCell(int32_t t, int32_t base, uint8_t dropOffset) noexcept;
/// The creature room: cell = 31 - (((GetTickCount() >> 5) + i) & 31): the real clock, 31.25 a second backwards, each
/// sprite i one step on
[[nodiscard]] uint8_t CreatureRoomCell(uint32_t tickCount, int sprite) noexcept;
/// The 3D cursor: cell = (GetTickCount() / 50) & 15: the real clock, 20 a second, 16 cells, 8 a row
[[nodiscard]] uint8_t CursorCell(uint32_t tickCount) noexcept;
/// The help system: clock += the step (the real frame time capped to 500 inside the citadel, else the game frame time),
/// cell = (clock / 200) & 15: 5 a second
[[nodiscard]] uint8_t HelpSystemCell(int32_t& clockMs, int32_t stepMs) noexcept;
/// JCSpecial, each of its 3 sprites: f += ms x 0.01; f > 15 restarts it at 0 (not a fmod); cell = truncated(f) & 15
[[nodiscard]] uint8_t JCSpecialCell(float& frame, float milliseconds) noexcept;
/// The player symbol: layer 0, A -= ms x 0.02; layer 1, B -= ms x 0.023; each + 32 while below 0; the cells
/// truncated(A) & 0x3F and truncated(B) & 0x3F of the two glows of one symbol (two draws, not a blend). Both start at
/// 0. ms = the game frame time
[[nodiscard]] uint8_t PlayerSymbolCell(float& phase, float milliseconds, int layer) noexcept;
/// The player symbol's second glow angle += ms x 0.002, - 2 pi while above 2 pi; written to its sprite. It starts at 0.
/// @return the angle
[[nodiscard]] float PlayerSymbolSpin(float& angle, float milliseconds) noexcept;
/// DisappearSmoke: cell = truncated(life x 15) & 0x3F (the life, 1 down to 0)
[[nodiscard]] uint8_t DisappearSmokeCell(float life) noexcept;
/// The collision dust (ECS/Physics/Dust.cpp): cell = 16 + ((rand % 16 + truncated(2 age)) & 15), rows 2-3 of blobs.raw
[[nodiscard]] uint8_t DustCell(uint32_t seed, float age) noexcept;

/// The influence circle border's scroll: one GLOBAL clock (int ms) = (c + frame time) % 10000 (signed), then the offset
/// u = float(c) x 0.0001, v = float(-c) x 0.0002: u runs 0 -> 1 and v 0 -> -2 every 10 s, whole repeats of a tiled
/// texture, so it is seamless. Run only when the draw passes its camera gate (influence::CurtainAlpha); the offset is
/// turned off after the circles. @return the offset
[[nodiscard]] UvOffset InfluenceScroll(int32_t& clockMs, uint32_t gameTimeIncMs) noexcept;

/// The designed waterfall: V -= 0.5 dt, minus its whole part, so it stays in -1..0; the offset is (0, V). @return V
[[nodiscard]] float WaterfallScroll(float& v, float seconds) noexcept;

/// The ghost of an object taken away: t goes from 500 ms to 0; x = t / 500, offset (2 cos x, 1.7 sin(0.7 x)) (1.7 the
/// float 1.7 widened to double), drawn, then (0, 0)
/// and drawn again with mode 10; the material's alpha byte = 255 - truncated(255 t / 500) ((inferred) its ALPHAREF)
struct Gooloo
{
	UvOffset uv;
	uint8_t materialByte;
};
[[nodiscard]] Gooloo GoolooFrame(float t) noexcept;
/// The ghost's whole time, ms
inline constexpr float k_GhostMs = 500.0f;
/// One frame of a ghost: t falls by the frame's ms; it goes once t is not above 0
struct GhostTime
{
	float remainingMs;
	bool expired;
};
[[nodiscard]] GhostTime GhostStep(float remainingMs, float frameMs) noexcept;

/// The rotating-UV particle mesh: the offset between two steps, lerp(previous -> current, t), then the period taken off
/// while above it (nothing added below 0), with the tiling forced. The only offset interpolated between turns: the UV,
/// not a frame
[[nodiscard]] UvOffset RotatingUv(const UvOffset& previous, const UvOffset& current, float t, const glm::vec2& period) noexcept;

/// The whole clock of a rotating-UV particle mesh (the ZR_SurfRevol atom's draw object): the rule adds its step to
/// `destination` and GameUpdate moves it down the chain once a step, so a draw between two steps interpolates.
/// `period` is the tiling (TextureWidth / 256, TextureHeight / 256).
struct RotatingUvClock
{
	UvOffset previous {0.0f, 0.0f};
	UvOffset current {0.0f, 0.0f};
	UvOffset destination {0.0f, 0.0f};
	glm::vec2 period {1.0f, 1.0f};

	/// Called once a step per atom after the atoms update: per axis, while both `destination` and `current` are under
	/// -2 period it adds the period to both and while both are over +2 period it takes it off both - the pair moves
	/// together, so their difference, which is what the draw interpolates, never changes. Then previous = current and
	/// current = destination.
	void GameUpdate() noexcept;
	/// The draw with t the draw fraction of the step kept by the particle system
	[[nodiscard]] UvOffset Interpolated(float t) const noexcept { return RotatingUv(previous, current, t, period); }
};

/// The chains' v-scroll (only when animation is on): scroll += frame time x rate x 0.001, fmod (FrameHeight / 256),
/// + that when negative. The rate is set only by UR_SimpleBeam (SpeedV) and UR_Plasma's arcs (SpeedV), neither ported;
/// 0 for every other chain (both start at 0). @return the scroll
[[nodiscard]] float ChainScroll(float& scroll, float milliseconds, float rate, int frameHeight) noexcept;
/// One segment of a chain's ribbon: the chain's FrameHeight, FrameWidth, FrameOfHead, FrameOfTail, the textures over
/// the whole chain (NumTexturesForWholeChain, or joints - 1 for -1) and FileOffset
struct ChainSheet
{
	int frameWidth {32};  ///< default 32
	int frameHeight {64}; ///< default 64
	int frameOfHead {0};
	int frameOfTail {0};
	int fileOffset {0};
	int textures {1}; ///< T: the original divides by it unchecked, see ChainSegmentUv
};
/// Segment i of S = joints - 1: k = ((i + 1) T - 1) / S, its first segment b = k S / T, its count
/// n = (k + 1) S / T - b, j = i - b (integer divisions); F = FileOffset + (k == T - 1 ? FrameOfHead : k == 0 ?
/// FrameOfTail : 0); uv0 = (F W, H j / n) / 256, uv1 = ((F + 1) W, H j / n) / 256, uv2 = (F W, H (j + 1) / n) / 256,
/// uv3 = ((F + 1) W, H (j + 1) / n) / 256, then the scroll added to the four v. Along the chain the V runs; across it
/// the U, over one W-pixel column of the sheet. (openblack guard) S, T and n are at least 1: the original divides by S
/// and T unchecked, so T = 0 (DefineProperties allows -1..32, and -1 is replaced at creation) would fault there, and by
/// n in float
[[nodiscard]] std::array<glm::vec2, 4> ChainSegmentUv(int segment, int segments, const ChainSheet& sheet,
                                                      float scroll) noexcept;

// ---------------------------------------------------------------------------------------------------------------------
// Loaders

/// A bitmap of `frames` frames of pitch x pitch texels of `channels` bytes, one after the other, rows along the first
/// axis (the land stamps' x)
struct StackedFrames
{
	int pitch {0};
	int frames {0};
	int channels {0}; ///< the bpp asked for: 3 (RGB, the light maps) or 1 (grey, the shadow maps)
	std::vector<uint8_t> data;
};
/// Loads a bitmap (name, pitch, bpp, framesInFile, framesInUse) from the file's bytes: nothing unless the file is
/// exactly bpp x pitch^2 x framesInFile bytes; min(framesInUse, framesInFile) frames, taken out of the file's grid
/// of truncated(sqrt(framesInFile)) frames per row (frame f at column f % n, row f / n) into pitch x pitch frames one
/// after the other. Used by the light map particles (bpp 3), the mist particles (bpp 1 or 3) and S_LMFireBall; the file
/// reading is land_light::LoadBitmapFile's
[[nodiscard]] std::optional<StackedFrames> LoadBitmapFromFile(std::span<const uint8_t> bytes, int pitch, int bpp,
                                                              int framesInFile, int framesInUse);
/// The texels of one frame: frame % frames (unsigned word), bpp x that x pitch^2 bytes in; null for a bitmap without
/// data
[[nodiscard]] const uint8_t* FrameTexels(const StackedFrames& bitmap, int frame) noexcept;
} // namespace openblack::graphics::frame_anim
