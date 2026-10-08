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
#include <functional>
#include <optional>
#include <vector>

#include <glm/mat3x4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

/// What the falling spell scene draws besides the film fall.bik, which video::FallingSpellVideo plays
/// (Video/FallingSpellVideo.h). Wiki: docs/bw1-notes/miracles.md.
///
/// - The 16 sparks: sprites with their records, anchored to the screen at a random depth, drawn while the sparks flag
///   is set (the update sets it once at 13.45 s; Draw rewrites it with "a spark was still seen").
/// - The light bursts: one LightBurst, initialised twice, drawn as four 64-spoke additive fans a frame by the finish
///   frame callback from state 2 (37.75 s), round the falling creature's projected centre. openblack has no falling
///   creature: the centre comes from a hook that has none, so the bursts neither move nor draw (pending: the creature).
/// - The camera path data\spells\fall\fall.cm2, sampled at the film's ms and given to the camera with the position and
///   focus x 0.8 and a FOV of pi / 4 (at Init and every update). It sets the drawn camera with the path's rotation (see
///   WorldToCamera) for the frame. The game camera leaves it alone in mode 2 and its zoomers are not touched, so nothing
///   puts the game camera back: the first frame in mode 0 draws it again. Applied through Hooks::applyCamera every
///   update, cleared at Close. In mode 2 no land or model is drawn, so only the creature would show it (pending: the
///   creature).
/// - The model light: Init keeps it, Draw puts it at (0, 0, 1000) for the creature, Close puts the kept one back.
///
/// Not ported (they need the creature, which openblack does not have): the falling creature (a copy of the player's
/// creature) with its time update, look reset, start action and hand glows 0 and 2 at Init, its draw and tint in Draw
/// (hidden from 19 550 to 27 350 ms, a random flicker colour scaled by the film time, the hand glows' scale/power/time
/// windows), its advance in the update and the debug keys that turn it.
namespace openblack::magic::falling_spell
{

/// 16 sprites and 16 spark records
inline constexpr int k_SparkCount = 0x10;
/// A LightBurst holds four arrays of 64 floats
inline constexpr int k_BurstSpokes = 0x40;
/// The fan's vertices: the centre and the rim point of each spoke
inline constexpr int k_BurstVertices = 0x80;
/// The FOV set before every Draw: pi / 4
inline constexpr float k_FallFov = 0.7853981852531433f;
/// 0.8: the path's position and focus are scaled by it before they reach the camera
inline constexpr float k_PathScale = 0.800000011920929f;
/// The model light while the creature is drawn
inline constexpr glm::vec3 k_CreatureLight {0.0f, 0.0f, 1000.0f};

/// A spark's record (set at Init)
struct Spark
{
	float x {0.0f};      ///< Random(-0.1, 1.1), a fraction of the screen width
	float y {0.0f};      ///< Random(-0.1, 1.1), of the height; Draw moves it towards 1
	float depth {0.0f};  ///< Random(5, 25), the depth of the screen to world projection
	float size {0.0f};   ///< Random(4, 8)
	float age {0.0f};    ///< 0; + the frame's delta time x 0.0013 each Draw
	float spin {0.0f};   ///< Random(-2, 2)
	float shrink {0.0f}; ///< 2 - y
	uint32_t rgb {0};    ///< (min(2 v, 255), v, v / 3) with v = the integer part of Random(16, 100)
};

/// A LightBurst: its 64 spokes
struct LightBurst
{
	std::array<float, k_BurstSpokes> radius {}; ///< Random(0, 1) (local) + 0.5, x 1.4 (1/8) and x 0.714286 (1/16)
	std::array<float, k_BurstSpokes> phase {};  ///< Random(0, 2 pi)
	std::array<float, k_BurstSpokes> rate {};   ///< Random(2, 20), negated when Random(0, 1) < 0.5
	std::array<float, k_BurstSpokes> wobble {}; ///< Random(-0.8, 0.8)

	/// Fills the spokes at random
	void Init();
};

/// A vertex in screen pixels (x right, y down), as the world triangles and sprites are projected
struct ScreenVertex
{
	glm::vec2 pixel {0.0f};
	glm::vec2 uv {0.0f};
	uint32_t argb {0};
};

/// One burst draw: 0x80 vertices and 0x40 triangles (2i, 2i + 1, 2 ((i + 1) & 63) + 1)
struct BurstFan
{
	std::array<ScreenVertex, k_BurstVertices> vertices {};
};
/// The fan's index list
[[nodiscard]] std::array<int, 3 * k_BurstSpokes> BurstIndices();

/// A burst at the screen point (x, y) (truncated to integers): a fan whose spoke i reaches
/// r = radius x (R[i] + W[i] sin(phase B[i] + P[i])) x (4 s^2 k + 1 - k),
/// s = |sin((i + 1) shape^2 pi / 4)|, k = clamp(1 - shape^2, 0, 1), at the angle i pi / 32 + phase (sin to x, cos to
/// y); the centre has `argb`, the rim the same alpha and no colour. The original's depth only places the points in the
/// world for the Z (they project back to the same pixels), so it is not an argument here.
/// Drawn with the additive atmosphere material (atmos.raw, mode 13) as world triangles
[[nodiscard]] BurstFan DrawBurst(const LightBurst& burst, float x, float y, float radius, float phase, float shape,
                                 uint32_t argb);

/// fall.cm2: +0 the size in bytes, +4 the duration (ms), +8 the count of keys, then the keys (0x48 bytes): position,
/// focus and 12 floats (a camera matrix)
struct CameraPath
{
	struct Key
	{
		glm::vec3 position {0.0f};
		glm::vec3 focus {0.0f};
		std::array<float, 12> matrix {};
	};
	uint32_t duration {0};
	std::vector<Key> keys;

	/// The file's bytes; nothing when they do not hold the header's count of keys
	[[nodiscard]] static std::optional<CameraPath> Parse(const std::vector<uint8_t>& bytes);
	/// The path at `ms`, untransformed: see the .cpp for its fraction
	[[nodiscard]] Key At(uint32_t ms) const;
};

/// The camera the fall gives: the path's key at the film ms, position and focus x 0.8, and the FOV pi / 4
struct Camera
{
	glm::vec3 position {0.0f};
	glm::vec3 focus {0.0f};
	std::array<float, 12> matrix {};
	float fov {k_FallFov};
};

/// The world to camera matrix (rows, x' = m0 x + m3 y + m6 z + m9) from the path's first nine floats a0..a8:
/// (a0, a3, -a6, a1, a4, -a7, a2, a5, -a8), each row of three normalised, then m9..m11 = -(the columns . position).
/// The camera's right, up and forward are so the path matrix's rows 0, 1 and -2; the focus does not turn it (the
/// look-at is overwritten). As a glm (column) matrix with the same memory, like openblack's glm::lookAt view
[[nodiscard]] glm::mat4 WorldToCamera(const Camera& camera);

/// The falling spell's state that is not the film's
class FallingSpell
{
public:
	struct Hooks
	{
		/// The bursts' centre: the falling creature's middle point projected to the screen, nothing when it is off
		/// screen. openblack: none (no falling creature), unless the test hook OPENBLACK_TEST_FALL_BURST_AT gives a
		/// screen fraction
		std::function<std::optional<glm::ivec2>(int width, int height)> burstCentre;
		/// The model light (model_light::Light / SetLight)
		std::function<glm::vec3()> light;
		std::function<void(const glm::vec3&)> setLight;
		/// Sets the drawn camera and FOV: the fall's camera each update, nothing to give back the game's camera (what
		/// the game camera does in mode 0 every frame)
		std::function<void(const std::optional<Camera>&)> applyCamera;
	};
	explicit FallingSpell(Hooks hooks);

	/// Init without the film and with the camera path: `path` = fall.cm2 when it loaded
	void Init(std::optional<CameraPath> path);
	/// Close without the film: the light put back, the sprites, records and burst freed
	void Close();
	[[nodiscard]] bool IsActive() const { return _active; }

	/// The update's camera: the path at the film's ms, given to Hooks::applyCamera
	void UpdateCamera(int32_t filmMs);
	/// Turns the sparks on (once, with the state 0 -> 1 at 13.45 s)
	void StartSparks() { _sparksOn = true; }

	/// Draw without the film and the creature: the light and the sparks, with a frame delta of `deltaMs` on a `width`
	/// x `height` screen
	void Draw(uint32_t deltaMs, int width, int height);
	/// The finish frame callback (from state 2): the four bursts
	void FinishFrame(uint32_t deltaMs, int32_t state, int width, int height);

	/// The sparks queued this frame, as the sprites are drawn in mode A from the Z-sorter, far to near (key
	/// |pos - camera|^2), none at or before the near plane `nearZ`
	[[nodiscard]] std::vector<std::array<ScreenVertex, 4>> SparkQuads(int width, int height, float nearZ) const;
	/// The bursts of the last FinishFrame (four, or none)
	[[nodiscard]] const std::vector<BurstFan>& Bursts() const { return _bursts; }

	[[nodiscard]] bool SparksOn() const { return _sparksOn; }
	[[nodiscard]] const std::array<Spark, k_SparkCount>& Sparks() const { return _sparks; }
	[[nodiscard]] const std::array<graphics::billboard::Sprite, k_SparkCount>& Sprites() const { return _sprites; }
	[[nodiscard]] const LightBurst& Burst() const { return _burst; }
	[[nodiscard]] float BurstShape() const { return _burstShape; }
	[[nodiscard]] float BurstAlpha() const { return _burstAlpha; }
	[[nodiscard]] float BurstGrow() const { return _burstGrow; }
	[[nodiscard]] const std::optional<CameraPath>& Path() const { return _path; }
	[[nodiscard]] const std::optional<Camera>& CameraNow() const { return _camera; }

private:
	Hooks _hooks;
	bool _active {false};
	std::optional<CameraPath> _path;
	glm::vec3 _keptLight {0.0f}; ///< The model light at Init
	bool _sparksOn {false};
	float _burstShape {0.0f};
	float _burstAlpha {0.0f};
	float _burstGrow {1.0f};
	std::array<graphics::billboard::Sprite, k_SparkCount> _sprites {};
	std::array<bool, k_SparkCount> _queued {}; ///< AddDrawing called this frame
	std::array<Spark, k_SparkCount> _sparks {};
	LightBurst _burst;
	std::vector<BurstFan> _bursts;
	std::optional<Camera> _camera;
};

/// The game's one; FrameUpdate drives it from video::GetFallingSpell()
[[nodiscard]] FallingSpell& Get();
/// Once a frame after video::GetFallingSpell().ProcessFrame (Game.cpp): Init when the film's falling spell started,
/// Close when it ended, else the camera (the update's part), Draw and the finish frame callback, with the frame delta
/// game_clock::FrameRealMs() on the window's size (MagicLoop.cpp)
void FrameUpdate();

} // namespace openblack::magic::falling_spell
