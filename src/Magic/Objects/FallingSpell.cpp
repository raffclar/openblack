/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingSpell.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <exception>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Common/GameRandom.h"
#include "Common/TruncateToInt.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/FallingSpellSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/ModelLight.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Video/FallingSpellVideo.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::falling_spell;

namespace
{
// frac(|x|) x 0.25 + 0.25 or + 0.5; frac by x minus x truncated toward zero
float BurstV(float value)
{
	const float a = std::fabs(value);
	return (a - static_cast<float>(TruncateToInt(a))) * 0.25f + 0.25f;
}
float BurstU(float value)
{
	const float a = std::fabs(value);
	return (a - static_cast<float>(TruncateToInt(a))) * 0.25f + 0.5f;
}

// The screen of the sparks: the fall's FOV of pi / 4 (set by the update before every Draw), the lens of
// Camera/ScreenPoint.h (the screen to 3D point and the sprites' projection in camera space)
screen_point::Lens LensOf(int width, int height)
{
	return screen_point::LensOf(width, height, k_FallFov);
}
} // namespace

void LightBurst::Init()
{
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		// LocalFloatRand(1) + 0.5
		radius.at(i) = game_random::LocalFloatRand(1.0f) + 0.5f;
		// Random(0, 2 pi)
		phase.at(i) = game_random::crt::Random(0.0f, 6.2831855f);
		// Random(2, 20)
		rate.at(i) = game_random::crt::Random(2.0f, 20.0f);
		// Random(0, 1) < 0.5 negates it
		if (game_random::crt::Random(0.0f, 1.0f) < 0.5f)
		{
			rate.at(i) = -rate.at(i);
		}
		// Random(-0.8, 0.8)
		wobble.at(i) = game_random::crt::Random(-0.8f, 0.8f);
		// LocalRand(16) < 2: x 1.4
		if (game_random::LocalRand(0x10) < 2)
		{
			radius.at(i) *= 1.4f;
		}
		// LocalRand(16) < 1: x 0.714286 (1 / 1.4)
		if (game_random::LocalRand(0x10) < 1)
		{
			radius.at(i) *= 0.71428573f;
		}
	}
}

std::array<int, 3 * k_BurstSpokes> falling_spell::BurstIndices()
{
	std::array<int, 3 * k_BurstSpokes> indices {};
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		// 2i, 2i + 1, 2 ((i + 1) & 0x3F) + 1
		indices.at(3 * i) = 2 * i;
		indices.at(3 * i + 1) = 2 * i + 1;
		indices.at(3 * i + 2) = 2 * ((i + 1) & 0x3F) + 1;
	}
	return indices;
}

BurstFan falling_spell::DrawBurst(const LightBurst& burst, float x, float y, float radius, float phase, float shape,
                                  uint32_t argb)
{
	BurstFan fan;
	// the centre truncated; the rim colour keeps only the alpha
	const glm::vec2 centre(static_cast<float>(TruncateToInt(x)), static_cast<float>(TruncateToInt(y)));
	const uint32_t rim = argb & 0xFF000000u;
	// w = shape^2 pi / 4; k = 1 - shape^2 clamped to [0, 1]
	const float shapeSquared = shape * shape;
	const float w = shapeSquared * 0.7853982f;
	float k = 1.0f - shapeSquared;
	if (k <= 0.0f)
	{
		k = 0.0f;
	}
	else if (!(k < 1.0f))
	{
		k = 1.0f;
	}
	const float rest = 1.0f - k;
	// v of the centre 0.4 phase, of the rim 1 + 0.4 phase
	const float vCentre = phase * 0.4f;
	const float vRim = vCentre + 1.0f;
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		const auto spoke = static_cast<size_t>(i & 0x3F);
		// (R + W sin(phase B + P)) x radius
		float r =
		    (burst.radius.at(spoke) + burst.wobble.at(spoke) * std::sin(phase * burst.rate.at(spoke) + burst.phase.at(spoke))) *
		    radius;
		// s = |sin((i + 1) w)|, r (4 s^2 k) + (1 - k) r
		const float s = std::fabs(std::sin(static_cast<float>(i + 1) * w));
		r = s * s * 4.0f * r * k + rest * r;
		// the angle i pi / 32 + phase, sin to x, cos to y, each truncated
		const float theta = static_cast<float>(i) * 0.09817477f + phase;
		const glm::vec2 point(static_cast<float>(TruncateToInt(r * std::sin(theta) + x)),
		                      static_cast<float>(TruncateToInt(r * std::cos(theta) + y)));
		// u = frac(i / 64) / 4 + 0.5 for both, v = frac(0.4 phase) / 4 + 0.25 (centre),
		// frac(1 + 0.4 phase) / 4 + 0.25 (rim)
		const float u = BurstU(static_cast<float>(i) * 0.015625f);
		fan.vertices.at(static_cast<size_t>(2 * i)) = {centre, {u, BurstV(vCentre)}, argb};
		fan.vertices.at(static_cast<size_t>(2 * i + 1)) = {point, {u, BurstV(vRim)}, rim};
	}
	return fan;
}

std::optional<falling_spell::CameraPath> falling_spell::CameraPath::Parse(const std::vector<uint8_t>& bytes)
{
	constexpr size_t k_Header = 0xC;
	constexpr size_t k_KeySize = 0x48; // 18 floats: position, focus and a 3x4 matrix
	if (bytes.size() < k_Header)
	{
		return std::nullopt;
	}
	const auto u32 = [&bytes](size_t at) {
		uint32_t v = 0;
		std::memcpy(&v, bytes.data() + at, sizeof(v));
		return v;
	};
	const uint32_t count = u32(8);
	// (openblack) a guard: the original reads the keys unchecked (fall.cm2: 1449 keys, 104 340 bytes)
	if (count == 0 || bytes.size() < k_Header + static_cast<size_t>(count) * k_KeySize)
	{
		return std::nullopt;
	}
	falling_spell::CameraPath path;
	path.duration = u32(4);
	path.keys.resize(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		std::array<float, 18> floats {};
		std::memcpy(floats.data(), bytes.data() + k_Header + static_cast<size_t>(i) * k_KeySize, k_KeySize);
		auto& key = path.keys.at(i);
		key.position = {floats[0], floats[1], floats[2]};
		key.focus = {floats[3], floats[4], floats[5]};
		std::copy(floats.begin() + 6, floats.end(), key.matrix.begin());
	}
	return path;
}

falling_spell::CameraPath::Key falling_spell::CameraPath::At(uint32_t ms) const
{
	const auto n = static_cast<uint32_t>(keys.size());
	// (openblack) a guard: the original divides by n - 1 and by the duration unchecked
	if (n < 2 || duration == 0)
	{
		return keys.empty() ? Key {} : keys.front();
	}
	// past the duration, duration - 1 (unsigned)
	uint32_t t = ms;
	if (t > duration)
	{
		t = duration - 1;
	}
	// the key (n - 1) t / duration, unsigned, wrapped into n
	uint32_t key = static_cast<uint32_t>((static_cast<uint64_t>(n - 1) * t) / duration);
	if (key >= n)
	{
		key %= n;
	}
	// step = duration / (n - 1) as a float, the fraction (t - i j) / step, i = t / step and j = step,
	// both truncated toward zero:
	// the remainder takes the step's integer part only, so the fraction grows past 1 along the path (in fall.cm2, step
	// 33.379 and 33 x k, above 1 from about 2.9 s): the original extrapolates from the key, and so does this
	const float step = static_cast<float>(duration) / static_cast<float>(n - 1);
	const int32_t k = TruncateToInt(static_cast<float>(t) / step);
	const int32_t whole = TruncateToInt(step);
	const auto remainder = static_cast<uint32_t>(static_cast<int32_t>(t) - k * whole);
	const float f = static_cast<float>(remainder) / step;
	// the next key, the same one at the end
	uint32_t next = key + 1;
	if (next >= n)
	{
		next = key;
	}
	const float g = 1.0f - f;
	const auto& a = keys.at(key);
	const auto& b = keys.at(next);
	// position and focus: a (1 - f) + b f; the matrix the same way (a's twelve x (1 - f), b x f, then the sum)
	Key out {
	    .position = a.position * g + b.position * f,
	    .focus = a.focus * g + b.focus * f,
	};
	for (size_t i = 0; i < out.matrix.size(); ++i)
	{
		out.matrix.at(i) = a.matrix.at(i) * g + b.matrix.at(i) * f;
	}
	return out;
}

glm::mat4 falling_spell::WorldToCamera(const Camera& camera)
{
	const auto& a = camera.matrix;
	// the path's 3x3 transposed with its third row negated, in the caller's matrix
	std::array<float, 9> m {a[0], a[3], -a[6], a[1], a[4], -a[7], a[2], a[5], -a[8]};
	// each row of three times the inverse square root of its length squared. (approximate) an exact 1 / sqrt, not the
	// original's lookup table
	for (size_t row = 0; row < 3; ++row)
	{
		const glm::vec3 v(m.at(3 * row), m.at(3 * row + 1), m.at(3 * row + 2));
		const float inverse = 1.0f / std::sqrt(glm::dot(v, v));
		for (size_t k = 0; k < 3; ++k)
		{
			m.at(3 * row + k) *= inverse;
		}
	}
	// the rotation, then the translation -(m0 p.x + m3 p.y + m6 p.z), -(m1 p.x + m4 p.y + m7 p.z),
	// -(m2 p.x + m5 p.y + m8 p.z)
	const glm::vec3& p = camera.position;
	const glm::vec3 c0(m[0], m[3], m[6]);
	const glm::vec3 c1(m[1], m[4], m[7]);
	const glm::vec3 c2(m[2], m[5], m[8]);
	return {glm::vec4(m[0], m[1], m[2], 0.0f), glm::vec4(m[3], m[4], m[5], 0.0f), glm::vec4(m[6], m[7], m[8], 0.0f),
	        glm::vec4(-glm::dot(c0, p), -glm::dot(c1, p), -glm::dot(c2, p), 1.0f)};
}

FallingSpell::FallingSpell(Hooks hooks)
    : _hooks(std::move(hooks))
{
}

void FallingSpell::Init(std::optional<falling_spell::CameraPath> path)
{
	// Close first when it is open
	if (_active)
	{
		Close();
	}
	// the path
	_path = std::move(path);
	_camera.reset();
	// open; the model light is kept
	_active = true;
	_keptLight = _hooks.light ? _hooks.light() : glm::vec3(0.0f);
	// 16 sprites with billboard::Sprite's defaults (Draw sets the material, here the renderer's smoke one)
	_sprites.fill(graphics::billboard::Sprite {});
	_queued.fill(false);
	// the 16 records
	for (auto& spark : _sparks)
	{
		// v = Random(16, 100) truncated toward zero, rgb = (min(2 v, 255) << 16) | (v << 8) | v / 3
		const int32_t v = TruncateToInt(game_random::crt::Random(16.0f, 100.0f));
		int32_t red = v + v;
		if (red > 0xFF)
		{
			red = 0xFF;
		}
		spark.rgb = static_cast<uint32_t>((((red << 8) | v) << 8) | (v / 3));
		// Random(-0.1, 1.1) twice
		spark.x = game_random::crt::Random(-0.1f, 1.1f);
		spark.y = game_random::crt::Random(-0.1f, 1.1f);
		// Random(5, 25)
		spark.depth = game_random::crt::Random(5.0f, 25.0f);
		// 2 - y
		spark.shrink = (1.0f - spark.y) + 1.0f;
		// Random(4, 8)
		spark.size = game_random::crt::Random(4.0f, 8.0f);
		// Random(-2, 2)
		spark.spin = game_random::crt::Random(-2.0f, 2.0f);
		spark.age = 0.0f;
	}
	// the sparks off (the states are the film's, video::FallingSpellVideo)
	_sparksOn = false;
	// the burst is set up twice, as the original does
	_burst.Init();
	_burst.Init();
	// the burst's shape 0, growth 1 and alpha 0; FinishFrame runs once a frame from now on
	_burstShape = 0.0f;
	_burstGrow = 1.0f;
	_burstAlpha = 0.0f;
	_bursts.clear();
}

void FallingSpell::Close()
{
	// nothing when it is not open
	if (!_active)
	{
		return;
	}
	// no more FinishFrame; closed
	_active = false;
	// the path freed
	_path.reset();
	_camera.reset();
	// (openblack) the game camera back: Close does not touch the camera, the next frame's camera update in the normal
	// mode draws it again (with its own FOV and its zoomers, which the fall's mode left alone)
	if (_hooks.applyCamera)
	{
		_hooks.applyCamera(std::nullopt);
	}
	// the model light put back
	if (_hooks.setLight)
	{
		_hooks.setLight(_keptLight);
	}
	// the records, the sprites and the burst freed
	_queued.fill(false);
	_bursts.clear();
	_sparksOn = false;
}

void FallingSpell::UpdateCamera(int32_t filmMs)
{
	if (!_active || !_path.has_value())
	{
		return;
	}
	// the path's key at t, the FOV pi / 4, position and focus x 0.8, then the camera set from them. Init does the same
	// at the film's first frame without the FOV; here both come in the first frame (approximate: see FrameUpdate)
	const auto key = _path->At(static_cast<uint32_t>(filmMs));
	_camera = Camera {key.position * k_PathScale, key.focus * k_PathScale, key.matrix, k_FallFov};
	if (_hooks.applyCamera)
	{
		_hooks.applyCamera(_camera);
	}
}

void FallingSpell::Draw(uint32_t deltaMs, int width, int height)
{
	_queued.fill(false);
	// nothing when it is not open (and nothing without a film, which ends the spell anyway)
	if (!_active)
	{
		return;
	}
	// the model light at (0, 0, 1000) for the creature (not ported) drawn just after
	if (_hooks.setLight)
	{
		_hooks.setLight(k_CreatureLight);
	}
	// the sparks only while they are on
	if (!_sparksOn)
	{
		return;
	}
	const auto lens = LensOf(width, height);
	// the frame's delta time x 0.0013
	const float dt = static_cast<float>(deltaMs) * 0.0013f;
	bool seen = false;
	for (int i = 0; i < k_SparkCount; ++i)
	{
		auto& spark = _sparks.at(static_cast<size_t>(i));
		auto& sprite = _sprites.at(static_cast<size_t>(i));
		// the alpha 255 - 32 age of the age before this frame's step
		int32_t alpha = TruncateToInt(255.0f - spark.age * 32.0f);
		spark.age += dt;
		// none left: not drawn
		if (alpha <= 0)
		{
			continue;
		}
		if (alpha >= 0xFF)
		{
			alpha = 0xFF;
		}
		// the colour rgb + alpha << 24; seen
		sprite.argb = spark.rgb + (static_cast<uint32_t>(alpha) << 24);
		seen = true;
		// the half width ((size - shrink age 0.5) + 1) 0.75, at least 1e-4
		float half = ((spark.size - spark.shrink * spark.age * 0.5f) + 1.0f) * 0.75f;
		if (half < 0.0001f)
		{
			half = 0.0001f;
		}
		// the origin times half / size, then the size = half
		const float ratio = half / sprite.size;
		sprite.origin *= ratio;
		sprite.size = half;
		// the angle = spin age + i
		sprite.angle = spark.spin * spark.age + static_cast<float>(i);
		// the cell (age x 8 truncated toward zero) & 15
		sprite.cell = static_cast<uint8_t>(TruncateToInt(spark.age * 8.0f) & 0xF);
		// the 3D point of the screen point (W x, H y), truncated toward zero, at its depth, W and H the screen's size
		const int32_t sx = TruncateToInt(static_cast<float>(width) * spark.x);
		const int32_t sy = TruncateToInt(static_cast<float>(height) * spark.y);
		sprite.position = screen_point::CameraPointFromScreen(lens, sx, sy, spark.depth);
		// y += (spin + 1) (1 - y) dt 0.1
		spark.y += (spark.spin + 1.0f) * (1.0f - spark.y) * dt * 0.1f;
		// queued for the draw, with the smoke material (smoke.raw, mode 6)
		_queued.at(static_cast<size_t>(i)) = true;
	}
	// the sparks stay on while one was seen
	_sparksOn = seen;
}

void FallingSpell::FinishFrame(uint32_t deltaMs, int32_t state, int width, int height)
{
	_bursts.clear();
	// from state 2 only
	if (!_active || state < 2)
	{
		return;
	}
	// the creature's centre on the screen; nothing (and no step) when it has none (at or before the near plane)
	const auto centre = _hooks.burstCentre ? _hooks.burstCentre(width, height) : std::nullopt;
	if (!centre.has_value())
	{
		return;
	}
	// dt in seconds; d = 0.7 dt; alpha += 0.1 d, the byte alpha min(255 alpha truncated toward zero, 255); shape += 0.25 d, and
	// past 0.5 growth += 5 d
	const float d = static_cast<float>(deltaMs) * 0.001f * 0.7f;
	_burstAlpha = d * 0.1f + _burstAlpha;
	int32_t alpha = TruncateToInt(_burstAlpha * 255.0f);
	if (alpha > 0xFF)
	{
		alpha = 0xFF;
	}
	_burstShape = d * 0.25f + _burstShape;
	if (_burstShape > 0.5f)
	{
		_burstGrow = d * 5.0f + _burstGrow;
	}
	const auto a = static_cast<uint32_t>(alpha) << 24;
	const float x = static_cast<float>(centre->x);
	const float y = static_cast<float>(centre->y);
	const float g = _burstGrow;
	// (the depth x 1.1 only places them for the Z)
	// 10 g^3, phase = alpha, the colour bytes 0x40, 0xFF, 0x20 (B, G, R)
	_bursts.push_back(DrawBurst(_burst, x, y, g * g * g * 10.0f, _burstAlpha, _burstShape, a | 0x0020FF40u));
	// 25 g, phase = -alpha, 0xFF, 0x40, 0xFF
	_bursts.push_back(DrawBurst(_burst, x, y, g * 25.0f, -_burstAlpha, _burstShape, a | 0x00FF40FFu));
	// 50 g, phase = -2 alpha, 0xFF, 0x80, 0x40
	_bursts.push_back(DrawBurst(_burst, x, y, g * 50.0f, _burstAlpha * -2.0f, _burstShape, a | 0x004080FFu));
	// 75 g^2, phase = 2 alpha, 0x40, 0x40, 0xFF
	_bursts.push_back(DrawBurst(_burst, x, y, g * g * 75.0f, _burstAlpha + _burstAlpha, _burstShape, a | 0x00FF4040u));
}

std::vector<std::array<ScreenVertex, 4>> FallingSpell::SparkQuads(int width, int height, float nearZ) const
{
	std::vector<std::array<ScreenVertex, 4>> quads;
	if (!_active || width <= 0 || height <= 0)
	{
		return quads;
	}
	const auto lens = LensOf(width, height);
	// the Z-sorter's key |pos - camera|^2, far to near.
	// (approximate) the key from the camera space position (the original's world sum can differ in the last bit)
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	const graphics::billboard::CameraFrame frame; // the camera at the origin, looking down +z (camera space)
	for (int i = 0; i < k_SparkCount; ++i)
	{
		if (_queued.at(static_cast<size_t>(i)))
		{
			queue.Submit(i, graphics::zsort::Key(_sprites.at(static_cast<size_t>(i)).position, glm::vec3(0.0f)));
		}
	}
	for (const auto& entry : queue.Drain())
	{
		const auto& sprite = _sprites.at(static_cast<size_t>(*entry.item));
		// nothing at or before the near plane
		if (!(sprite.position.z > nearZ))
		{
			continue;
		}
		// billboard::Screen in camera space, then the projection
		const auto quad = graphics::billboard::Screen(sprite, frame);
		std::array<ScreenVertex, 4> out {};
		for (size_t k = 0; k < out.size(); ++k)
		{
			out.at(k) = {screen_point::ProjectCamera(lens, quad.corners.at(k)), quad.uv.at(k), sprite.argb};
		}
		quads.push_back(out);
	}
	return quads;
}

namespace
{
/// The game's falling spell and its sparks flag (Locator::fallingSpellSystem)
ecs::systems::FallingSpellSystemInterface& FallingSpellState()
{
	if (!Locator::fallingSpellSystem::has_value())
	{
		std::fputs("magic::falling_spell: no falling spell in the locator (Locator::fallingSpellSystem)\n", stderr);
		std::abort();
	}
	return Locator::fallingSpellSystem::value();
}

/// OPENBLACK_TEST_FALL_LOG's last logged second of film, in the debug hooks' store (Locator::debugHooks)
struct FallingSpellDebugHooksState
{
	int32_t loggedSecond {-1};
};
} // namespace

FallingSpell& falling_spell::Get()
{
	auto& spell = FallingSpellState().Spell();
	if (spell.has_value())
	{
		return *spell;
	}
	spell.emplace([]() {
		FallingSpell::Hooks hooks;
		// (pending: the creature) openblack has no falling creature, so no centre. OPENBLACK_TEST_FALL_BURST_AT="fx,fy"
		// (openblack test hook) puts it at that fraction of the screen, to see the bursts
		hooks.burstCentre = [](int width, int height) -> std::optional<glm::ivec2> {
			const char* at = std::getenv("OPENBLACK_TEST_FALL_BURST_AT");
			glm::vec2 fraction(0.5f);
			if (at == nullptr || std::sscanf(at, "%f,%f", &fraction.x, &fraction.y) != 2)
			{
				return std::nullopt;
			}
			return glm::ivec2(glm::vec2(static_cast<float>(width), static_cast<float>(height)) * fraction);
		};
		hooks.light = []() { return model_light::Light(); };
		hooks.setLight = [](const glm::vec3& position) { model_light::SetLight(position); };
		// the fall's camera and FOV on openblack's camera. The FOV is the horizontal angle (tan(fov / 2) near, and that
		// / aspect), as the config's cameraXFov; the config keeps the game camera's own FOV, which the clear puts back.
		// (inferred) the near plane stays openblack's (the original takes it from the game camera in every mode). Not
		// ported: the debug camera overrides and the shake on the fall's camera, and the readers of the drawn camera
		// while the fall runs (the weather smoothing)
		hooks.applyCamera = [](const std::optional<Camera>& fall) {
			if (!Locator::camera::has_value() || !Locator::config::has_value() || !Locator::windowing::has_value())
			{
				return;
			}
			auto& camera = Locator::camera::value();
			const auto& config = Locator::config::value();
			const float xFov = fall.has_value() ? glm::degrees(fall->fov) : config.cameraXFov;
			camera.SetProjectionMatrixPerspective(xFov, Locator::windowing::value().GetAspectRatio(), config.cameraNearClip,
			                                      config.cameraFarClip);
			// the drawn view, the fall's world to camera, over the game camera's look-at without touching its zoomers
			camera.SetDrawnView(fall.has_value() ? std::optional(WorldToCamera(*fall)) : std::nullopt);
		};
		return hooks;
	}());
	return *spell;
}

namespace
{
// Init's camera path, data\spells\fall\fall.cm2
std::optional<falling_spell::CameraPath> LoadFallPath()
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	try
	{
		// the blob key stays "Data/Spells/fall/fall.cm2": BlobId hashes the generic (forward slash) form
		const auto path = Locator::filesystem::value().GetPath<openblack::filesystem::Path::Data>() / "Spells/fall/fall.cm2";
		return falling_spell::CameraPath::Parse(resources::LoadBlob(Locator::resources::value().GetBlobs(), path));
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}
} // namespace

void falling_spell::FrameUpdate()
{
	auto& film = video::GetFallingSpell();
	auto& spell = Get();
	auto& sparksSeen = FallingSpellState().SparksSeen();
	if (!film.IsActive())
	{
		// the film's end closes the spell
		spell.Close();
		return;
	}
	// (approximate) a kick-off over a running one (an end, then a new Init) is only seen once the old one had its
	// sparks: the film's sparkles are back off
	if (!spell.IsActive() || (sparksSeen && !film.SparklesOn()))
	{
		// the film's kick-off starts the spell. (approximate) here at the first frame of the film, not in the turn of
		// the script call: the CRT and local random draws come a little later in their streams
		spell.Init(LoadFallPath());
		sparksSeen = false;
	}
	// the update's part: the sparks on once with the state 0 -> 1, the camera with the film's ms
	if (film.SparklesOn() && !sparksSeen)
	{
		spell.StartSparks();
		sparksSeen = true;
	}
	spell.UpdateCamera(film.LastMs());
	glm::ivec2 size(0);
	if (Locator::windowing::has_value())
	{
		size = Locator::windowing::value().GetSize();
	}
	const uint32_t deltaMs = game_clock::FrameRealMs();
	// while the fall runs the engine draws it with the frame's delta time; FinishFrame after it
	spell.Draw(deltaMs, size.x, size.y);
	spell.FinishFrame(deltaMs, film.State(), size.x, size.y);
	// (openblack test hook) OPENBLACK_TEST_FALL_LOG=1: one line a second of film, to place the screenshots in time
	auto& loggedSecond = Locator::debugHooks::value().Get<FallingSpellDebugHooksState>().loggedSecond;
	if (std::getenv("OPENBLACK_TEST_FALL_LOG") != nullptr && film.LastMs() / 1000 != loggedSecond)
	{
		loggedSecond = film.LastMs() / 1000;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "FallingSpell: film {} ms, state {}, sparks {} ({} quads), bursts {}",
		                   film.LastMs(), film.State(), spell.SparksOn(), spell.SparkQuads(size.x, size.y, 0.0f).size(),
		                   spell.Bursts().size());
	}
}
