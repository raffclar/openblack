/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleMaths.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Common/GameRandom.h"
#include "Common/X87.h"
#include "Common/X87Matrix.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// The low six bits of a sprite cell number are the cell
constexpr uint32_t k_SpriteCellMask = 0x3Fu;
/// A looping animation's frame may be drawn up to this many steps ahead of the last
constexpr float k_MaxLoopedFrameLerp = 5.0f;
/// Frames further out than this can no longer be moved by whole cycles in single precision
constexpr float k_LargestSteppableFrame = 16777216.0f;
/// The order in which the products of a point with the rows are added, to move a point of a hierarchy into the world
constexpr openblack::x87::AxisOrders k_FrameToWorldOrder {{{2, 1, 0}, {1, 0, 2}, {2, 1, 0}}};
/// The order in which the products are added, to move a point of the world into a hierarchy
constexpr openblack::x87::AxisOrders k_WorldToFrameOrder {{{1, 2, 0}, {1, 0, 2}, {2, 1, 0}}};
/// A quarter turn, as the float the game adds to an angle
constexpr float k_QuarterTurn = 1.5707963705062866f;

/// The permutation the noise reads its lattice through
constexpr std::array<uint8_t, 256> k_Permutation {
    225, 155, 210, 108, 175, 199, 221, 144, 203, 116, 70,  213, 69,  158, 33,  252, 5,   82,  173, 133, 222, 139, 174, 27,
    9,   71,  90,  246, 75,  130, 91,  191, 169, 138, 2,   151, 194, 235, 81,  7,   25,  113, 228, 159, 205, 253, 134, 142,
    248, 65,  224, 217, 22,  121, 229, 63,  89,  103, 96,  104, 156, 17,  201, 129, 36,  8,   165, 110, 237, 117, 231, 56,
    132, 211, 152, 20,  181, 111, 239, 218, 170, 163, 51,  172, 157, 47,  80,  212, 176, 250, 87,  49,  99,  242, 136, 189,
    162, 115, 44,  43,  124, 94,  150, 16,  141, 247, 32,  10,  198, 223, 255, 72,  53,  131, 84,  57,  220, 197, 58,  50,
    208, 11,  241, 28,  3,   192, 62,  202, 18,  215, 153, 24,  76,  41,  15,  179, 39,  46,  55,  6,   128, 167, 23,  188,
    106, 34,  187, 140, 164, 73,  112, 182, 244, 195, 227, 13,  35,  77,  196, 185, 26,  200, 226, 119, 31,  123, 168, 125,
    249, 68,  183, 230, 177, 135, 160, 180, 12,  1,   243, 148, 102, 166, 38,  238, 251, 37,  240, 126, 64,  74,  161, 40,
    184, 149, 171, 178, 101, 66,  29,  59,  146, 61,  254, 107, 42,  86,  154, 4,   236, 232, 120, 21,  233, 209, 45,  98,
    193, 114, 78,  19,  206, 14,  118, 127, 48,  79,  147, 85,  30,  207, 219, 54,  88,  234, 190, 122, 95,  67,  143, 109,
    137, 214, 145, 93,  92,  100, 245, 0,   216, 186, 60,  83,  105, 97,  204, 52};

glm::mat3 Rotation(float glmAngle, const glm::vec3& axis)
{
	return glm::mat3(glm::rotate(glm::mat4(1.0f), glmAngle, axis));
}
} // namespace

std::optional<float> maths::TimedValue(float age, float dt, float start, float stop, float from, float to)
{
	if (age >= start && age <= stop)
	{
		return stop > start ? from + ((age - start) / (stop - start)) * (to - from) : to;
	}
	if (age > stop && age - dt <= stop)
	{
		return to;
	}
	return std::nullopt;
}

float maths::ScaleAtCameraDistance(float distance, float nearDistance, float farDistance, float nearScale, float farScale)
{
	if (distance < nearDistance)
	{
		return nearScale;
	}
	if (distance > farDistance)
	{
		return farScale;
	}
	return (distance - nearDistance) / (farDistance - nearDistance) * (farScale - nearScale) + nearScale;
}

uint8_t maths::TruncateToByte(float value)
{
	if (!std::isfinite(value))
	{
		return 0;
	}
	return static_cast<uint8_t>(static_cast<int64_t>(value) & 0xFF);
}

std::array<uint8_t, 4> maths::TintWithPlayerColour(std::array<uint8_t, 4> rgba, uint32_t playerRgb, float blend)
{
	uint32_t colour = playerRgb & 0xFFFFFFu;
	if (colour == 0)
	{
		colour = 0xFFFFFFu;
	}
	std::array<uint32_t, 3> channels {(colour >> 16u) & 0xFFu, (colour >> 8u) & 0xFFu, colour & 0xFFu};
	if (blend != 1.0f)
	{
		const int b = static_cast<int>(blend * 255.0f) & 0xFF;
		for (auto& c : channels)
		{
			c = static_cast<uint32_t>((255 + (((static_cast<int>(c) - 255) * b) >> 8)) & 0xFF);
		}
	}
	return {static_cast<uint8_t>((rgba[0] * channels[0]) >> 8u), static_cast<uint8_t>((rgba[1] * channels[1]) >> 8u),
	        static_cast<uint8_t>((rgba[2] * channels[2]) >> 8u), static_cast<uint8_t>((rgba[3] * 255u) >> 8u)};
}

float maths::ChakraFade(float age, float ageMax, float ageZero)
{
	const float t = age < ageMax ? age / ageMax : 1.0f - ((age - ageMax) / (ageZero - ageMax));
	// Not a number, as at an age of 0 with no rise, counts as nothing
	if (!(t > 0.0f))
	{
		return 0.0f;
	}
	return std::min(t, 1.0f);
}

glm::vec3 maths::BurstVelocity(glm::vec3 direction, float speed, float scaleY, bool upwards)
{
	if (upwards)
	{
		direction.y = std::abs(direction.y);
	}
	direction = x87::Scaled<x87::SumOrder::Zxy>(direction);
	return {direction.x * speed, (direction.y * speed) * scaleY, direction.z * speed};
}

int maths::SoundSizeFromRadius(float radius, float small, float medium)
{
	if (radius < small)
	{
		return 3;
	}
	return radius < medium ? 2 : 1;
}

void maths::AdvanceFrame(float& previous, float& current, float dt, float rate, int frames, bool playing)
{
	previous = current;
	if (!playing)
	{
		return;
	}
	current = previous + dt * rate;
	if (frames <= 0)
	{
		return;
	}
	const auto n = static_cast<float>(frames);
	const float twoN = n + n;
	if (!std::isfinite(current) || std::abs(current) > k_LargestSteppableFrame || std::abs(previous) > k_LargestSteppableFrame)
	{
		current = std::isfinite(current) ? std::fmod(current, n) : 0.0f;
		previous = current;
	}
	if (rate > 0.0f)
	{
		while (current > twoN && previous > twoN)
		{
			current -= n;
			previous -= n;
		}
	}
	else
	{
		while (current < 0.0f || previous < 0.0f)
		{
			current += twoN;
			previous += twoN;
		}
	}
}

float maths::LerpFrame(float previous, float current, float t, bool loop)
{
	const float limit = loop ? k_MaxLoopedFrameLerp : 1.0f;
	return (current - previous) * std::clamp(t, 0.0f, limit) + previous;
}

int maths::FrameIndex(float frame, int frames, bool loop)
{
	const int n = std::max(1, frames);
	if (loop)
	{
		float wrapped = std::fmod(frame, static_cast<float>(n));
		if (wrapped < 0.0f)
		{
			wrapped += static_cast<float>(n);
		}
		return std::min(static_cast<int>(wrapped), n - 1);
	}
	return std::clamp(static_cast<int>(frame), 0, n - 1);
}

maths::UvRect maths::SpriteCellUv(int cell, int cellsPerRow)
{
	const auto n = static_cast<uint32_t>(std::max(1, cellsPerRow));
	const auto c = static_cast<uint32_t>(cell) & k_SpriteCellMask;
	const float size = 1.0f / static_cast<float>(n);
	return {.corner = {static_cast<float>(c % n) * size, static_cast<float>(c / n) * size}, .size = {size, size}};
}

glm::mat3 maths::AngleY(float a)
{
	// The cosine and sine are stored as floats, and the other parts are exact
	const float c = x87::RoundToFloat(x87::Cos(a));
	const float s = x87::RoundToFloat(x87::Sin(a));
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

glm::mat3 maths::AngleXYZ(float x, float y, float z)
{
	return Rotation(-z, {0.0f, 0.0f, 1.0f}) * Rotation(-y, {0.0f, 1.0f, 0.0f}) * Rotation(-x, {1.0f, 0.0f, 0.0f});
}

glm::mat3 maths::TurnAboutAxis(const glm::mat3& rotation, int axis, float a)
{
	glm::vec3 direction(0.0f);
	direction[std::clamp(axis, 0, 2)] = 1.0f;
	return Rotation(-a, direction) * rotation;
}

float maths::TumbleSpeed(const glm::vec3& velocity, float tumbleSpeed)
{
	return x87::LengthZyx(velocity) * tumbleSpeed;
}

glm::mat3 maths::Tumble(const glm::mat3& rotation, float angle, bool aboutForward)
{
	const double c = x87::Cos(angle);
	const double s = x87::Sin(angle);
	// The two parts of each axis that the turn changes
	const int first = aboutForward ? 0 : 1;
	const int second = first + 1;
	auto turned = rotation;
	for (int i = 0; i < 3; ++i)
	{
		const float a = rotation[i][first];
		const float b = rotation[i][second];
		turned[i][first] = x87::Mul(s, b) + x87::Mul(c, a);
		turned[i][second] = x87::Mul(c, b) - x87::Mul(s, a);
	}
	return turned;
}

float maths::SpriteAngle(const glm::mat3& rotation)
{
	return std::atan2(rotation[0][2], rotation[0][0]);
}

float maths::ScreenVelocityAngle(const glm::vec3& velocity, const glm::vec3& right, const glm::vec3& up)
{
	const float x = x87::Dot<x87::SumOrder::Zyx>(velocity, right);
	const float y = x87::Dot<x87::SumOrder::Zxy>(velocity, up);
	return x87::Add(x87::Atan2(-y, x), k_QuarterTurn);
}

glm::vec3 maths::FrameToWorld(std::span<const glm::mat4> frames, const glm::vec3& point)
{
	glm::mat4 frame {1.0f};
	for (const auto& next : frames)
	{
		frame = x87::Product(frame, next);
	}
	return x87::TransformPoint(frame, point, k_FrameToWorldOrder);
}

glm::vec3 maths::WorldToFrame(std::span<const glm::mat4> frames, const glm::vec3& point)
{
	if (frames.empty())
	{
		return point;
	}
	glm::mat4 frame {1.0f};
	for (const auto& next : frames)
	{
		frame = x87::PreProduct(x87::Inverse(next), frame);
	}
	return x87::TransformPoint(frame, point, k_WorldToFrameOrder);
}

glm::mat3 maths::OrientAlong(const glm::vec3& p, const glm::mat3& start)
{
	// The heading and the angle from the vertical stay wide; each cosine is stored as a float and each sine stays wide
	const double yaw = x87::Atan2(p.z, p.x);
	const float across = std::sqrt((p.z * p.z) + (p.x * p.x));
	const float colatitude = x87::Sub(k_QuarterTurn, x87::Atan2(p.y, across));
	glm::mat3 m = start;
	const float cc = x87::RoundToFloat(x87::Cos(colatitude));
	const double sc = x87::Sin(colatitude);
	const float cy = x87::RoundToFloat(x87::Cos(yaw));
	const double sy = x87::Sin(yaw);
	for (int i = 0; i < 3; ++i)
	{
		const float x = m[i].x;
		const float y = m[i].y;
		m[i].x = x87::Mul(sc, y) + (x * cc);
		m[i].y = (y * cc) - x87::Mul(sc, x);
		const float x2 = m[i].x;
		const float z = m[i].z;
		m[i].x = (x2 * cy) - x87::Mul(sy, z);
		m[i].z = (z * cy) + x87::Mul(sy, x2);
	}
	return m;
}

glm::mat3 maths::TurnWithMove(const glm::mat3& rotation, const glm::vec3& last, const glm::vec3& p)
{
	const float lengths = std::sqrt(x87::DotZyx(p, p) * x87::DotZyx(last, last));
	const auto axis = glm::cross(last, p);
	if (!(lengths > 0.0f) || axis == glm::vec3(0.0f))
	{
		return rotation;
	}
	const float between = x87::Dot<x87::SumOrder::Yzx>(last, p) / lengths;
	// The game gives not a number when rounding takes the cosine past 1; the clamp keeps the turn whole
	const float angle = x87::RoundToFloat(x87::Acos(std::clamp(between, -1.0f, 1.0f)));
	const auto turn = x87::AxisTurn(axis, angle);
	auto turned = rotation;
	for (glm::length_t i = 0; i < 3; ++i)
	{
		turned[i] = x87::TurnPoint(turn, rotation[i], x87::k_MeshPointOrder);
	}
	return turned;
}

float maths::FadedSpinAngle(float spin, float fade, float dt)
{
	return ((-spin * fade) + spin) * dt;
}

glm::mat3 maths::SpinAboutVertical(const glm::mat3& rotation, float angle)
{
	const float c = x87::RoundToFloat(x87::Cos(angle));
	const double s = x87::Sin(angle);
	auto turned = rotation;
	for (glm::length_t k = 0; k < 3; ++k)
	{
		const float first = rotation[0][k];
		const float third = rotation[2][k];
		turned[0][k] = (c * first) + x87::Mul(s, third);
		turned[2][k] = (c * third) - x87::Mul(s, first);
	}
	return turned;
}

float maths::SmoothingRate(float smoothing, float scale)
{
	return x87::Mul(std::log(static_cast<double>(1.0f - smoothing)), scale);
}

float maths::SmoothingShare(float dt, float rate)
{
	return 1.0f - x87::Exp(-(dt * rate));
}

maths::ValueNoise::ValueNoise()
{
	// Drawn once from a seed of 0 before the game seeds its generators, so the same values in every game
	uint32_t seed = 0;
	for (auto& value : _values)
	{
		const float r = game_random::FloatRand(2.0f, seed);
		value = 1.0f - r;
	}
}

float maths::ValueNoise::Lattice(int x, int y, int z) const
{
	const auto index = [](int i) { return static_cast<size_t>(static_cast<uint32_t>(i) & 0xFFu); };
	const auto pz = k_Permutation.at(index(z));
	const auto py = k_Permutation.at(index(static_cast<int>(pz) + y));
	const auto px = k_Permutation.at(index(static_cast<int>(py) + x));
	return _values.at(px);
}

float maths::ValueNoise::Smooth(float x) const
{
	const auto ix = static_cast<int>(std::floor(x));
	const float t = x - static_cast<float>(ix);
	std::array<float, 4> k {};
	for (int i = 0; i < 4; ++i)
	{
		const auto index = static_cast<size_t>(static_cast<uint32_t>(ix + i - 1) & 0xFFu);
		k.at(static_cast<size_t>(i)) = _values.at(k_Permutation.at(index));
	}
	// A Catmull-Rom spline through the four values, between the middle two
	const float c3 = -0.5f * k[0] + 1.5f * k[1] - 1.5f * k[2] + 0.5f * k[3];
	const float c2 = k[0] - 2.5f * k[1] + 2.0f * k[2] - 0.5f * k[3];
	const float c1 = -0.5f * k[0] + 0.5f * k[2];
	const float c0 = k[1];
	return ((c3 * t + c2) * t + c1) * t + c0;
}

float maths::ValueNoise::Line(float x) const
{
	const auto ix = static_cast<int>(std::floor(x));
	const auto value = [this](int i) {
		return _values.at(k_Permutation.at(static_cast<size_t>(static_cast<uint32_t>(i) & 0xFFu)));
	};
	const float a = value(ix);
	return (x - static_cast<float>(ix)) * (value(ix + 1) - a) + a;
}

float maths::ValueNoise::At(const glm::vec3& point) const
{
	const auto ix = static_cast<int>(std::floor(point.x));
	const auto iy = static_cast<int>(std::floor(point.y));
	const auto iz = static_cast<int>(std::floor(point.z));
	const float fx = point.x - static_cast<float>(ix);
	const float fy = point.y - static_cast<float>(iy);
	const float fz = point.z - static_cast<float>(iz);
	std::array<float, 2> alongZ {};
	for (int k = 0; k < 2; ++k)
	{
		std::array<float, 2> alongY {};
		for (int j = 0; j < 2; ++j)
		{
			const float a = Lattice(ix, iy + j, iz + k);
			const float b = Lattice(ix + 1, iy + j, iz + k);
			alongY.at(static_cast<size_t>(j)) = (b - a) * fx + a;
		}
		alongZ.at(static_cast<size_t>(k)) = (alongY[1] - alongY[0]) * fy + alongY[0];
	}
	return (alongZ[1] - alongZ[0]) * fz + alongZ[0];
}

glm::vec2 maths::ValueNoise::Wind(const glm::vec3& point) const
{
	return {At(point), At({point.y, point.z, point.x})};
}

glm::mat3 maths::LerpRotation(const glm::mat3& from, const glm::mat3& to, float t)
{
	return glm::mat3_cast(glm::slerp(glm::quat_cast(from), glm::quat_cast(to), t));
}
