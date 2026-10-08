/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// magic::falling_spell (src/Magic/Objects/FallingSpell.h): what the falling spell draws besides the film fall.bik,
// without the GPU: the sparks (init, draw, as sprites), the light bursts (init, draw and their callback), the model
// light kept and put back, and the fall.cm2 camera path.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Magic/Objects/FallingSpell.h"

using namespace openblack;
using namespace openblack::magic::falling_spell;
using FallCamera = openblack::magic::falling_spell::Camera; // not openblack::Camera

namespace
{
struct Fake
{
	glm::vec3 light {1.0f, 2.0f, 3.0f};
	std::optional<glm::ivec2> centre;
};

FallingSpell Make(Fake& fake)
{
	FallingSpell::Hooks hooks;
	hooks.light = [&fake]() { return fake.light; };
	hooks.setLight = [&fake](const glm::vec3& position) { fake.light = position; };
	hooks.burstCentre = [&fake](int, int) { return fake.centre; };
	return FallingSpell(std::move(hooks));
}

std::vector<uint8_t> PathBytes(uint32_t duration, const std::vector<float>& xs)
{
	const auto count = static_cast<uint32_t>(xs.size());
	std::vector<uint8_t> bytes(0xC + 0x48 * xs.size(), 0);
	const uint32_t size = static_cast<uint32_t>(bytes.size());
	std::memcpy(bytes.data(), &size, 4);
	std::memcpy(bytes.data() + 4, &duration, 4);
	std::memcpy(bytes.data() + 8, &count, 4);
	for (size_t i = 0; i < xs.size(); ++i)
	{
		std::array<float, 18> key {};
		key[0] = xs[i];        // position x
		key[3] = 2.0f * xs[i]; // focus x
		key[6] = 3.0f * xs[i]; // matrix 0
		std::memcpy(bytes.data() + 0xC + 0x48 * i, key.data(), 0x48);
	}
	return bytes;
}

std::optional<std::filesystem::path> GamePath()
{
	for (const char* name : {"OPENBLACK_TEST_GAME_PATH", "OPENBLACK_TEST_BW_ROOT"})
	{
		if (const char* value = std::getenv(name); value != nullptr && *value != '\0')
		{
			return std::filesystem::path(value);
		}
	}
	return std::nullopt;
}
} // namespace

TEST(FallingSpell, BurstIndicesAreTheFan)
{
	const auto indices = BurstIndices();
	EXPECT_EQ(indices[0], 0);
	EXPECT_EQ(indices[1], 1);
	EXPECT_EQ(indices[2], 3);
	// the last spoke closes on the first rim point (the index wraps at 64)
	EXPECT_EQ(indices[3 * 63], 126);
	EXPECT_EQ(indices[3 * 63 + 1], 127);
	EXPECT_EQ(indices[3 * 63 + 2], 1);
}

TEST(FallingSpell, BurstGeometry)
{
	LightBurst burst;
	burst.radius.fill(1.0f);
	burst.wobble.fill(0.0f);
	burst.phase.fill(0.0f);
	burst.rate.fill(0.0f);
	// shape = 1: k = 0, every spoke reaches the full radius at i pi / 32 + phase (sin to x, cos to y)
	const auto fan = DrawBurst(burst, 100.0f, 50.0f, 10.0f, 0.25f, 1.0f, 0x800020FFu);
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		const auto& centre = fan.vertices.at(static_cast<size_t>(2 * i));
		const auto& rim = fan.vertices.at(static_cast<size_t>(2 * i + 1));
		EXPECT_EQ(centre.pixel, glm::vec2(100.0f, 50.0f));
		const float theta = static_cast<float>(i) * 0.09817477f + 0.25f;
		EXPECT_EQ(rim.pixel.x, static_cast<float>(static_cast<int32_t>(10.0f * std::sin(theta) + 100.0f)));
		EXPECT_EQ(rim.pixel.y, static_cast<float>(static_cast<int32_t>(10.0f * std::cos(theta) + 50.0f)));
		EXPECT_EQ(centre.argb, 0x800020FFu);
		EXPECT_EQ(rim.argb, 0x80000000u); // the rim keeps the alpha only
		EXPECT_FLOAT_EQ(centre.uv.x, 0.5f + 0.25f * static_cast<float>(i) / 64.0f);
		EXPECT_FLOAT_EQ(centre.uv.x, rim.uv.x);
		EXPECT_FLOAT_EQ(centre.uv.y, 0.25f + 0.25f * 0.1f);
	}
	// shape = 0: k = 1 and sin(0) = 0: every spoke has no length, the fan is a point
	const auto none = DrawBurst(burst, 100.0f, 50.0f, 10.0f, 0.0f, 0.0f, 0xFF000000u);
	for (const auto& vertex : none.vertices)
	{
		EXPECT_EQ(vertex.pixel, glm::vec2(100.0f, 50.0f));
	}
}

TEST(FallingSpell, BurstInitRanges)
{
	game_random::testing::ScopedState state;
	LightBurst burst;
	burst.Init();
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		EXPECT_GE(burst.radius.at(i), 0.5f * 0.71428573f);
		EXPECT_LE(burst.radius.at(i), 1.5f * 1.4f);
		EXPECT_GE(std::fabs(burst.rate.at(i)), 2.0f);
		EXPECT_LE(std::fabs(burst.rate.at(i)), 20.0f);
		EXPECT_LE(std::fabs(burst.wobble.at(i)), 0.8f);
		EXPECT_GE(burst.phase.at(i), 0.0f);
		EXPECT_LE(burst.phase.at(i), 6.2831855f);
	}
}

TEST(FallingSpell, LightKeptAndPutBack)
{
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	EXPECT_TRUE(spell.IsActive());
	spell.Draw(16, 640, 480);
	EXPECT_EQ(fake.light, k_CreatureLight);
	spell.Close();
	EXPECT_FALSE(spell.IsActive());
	EXPECT_EQ(fake.light, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(FallingSpell, SparksInit)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	EXPECT_FALSE(spell.SparksOn());
	for (const auto& spark : spell.Sparks())
	{
		EXPECT_GE(spark.x, -0.1f);
		EXPECT_LE(spark.x, 1.1f);
		EXPECT_GE(spark.depth, 5.0f);
		EXPECT_LE(spark.depth, 25.0f);
		EXPECT_GE(spark.size, 4.0f);
		EXPECT_LE(spark.size, 8.0f);
		EXPECT_FLOAT_EQ(spark.shrink, 2.0f - spark.y);
		EXPECT_EQ(spark.age, 0.0f);
		const uint32_t v = (spark.rgb >> 8) & 0xFFu;
		EXPECT_GE(v, 16u);
		EXPECT_LE(v, 100u);
		EXPECT_EQ(spark.rgb >> 16, std::min(2 * v, 0xFFu));
		EXPECT_EQ(spark.rgb & 0xFFu, v / 3);
	}
}

TEST(FallingSpell, SparksOnlyAfterTheUpdateSetsThem)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	spell.Draw(100, 640, 480);
	EXPECT_TRUE(spell.SparkQuads(640, 480, 1.0f).empty());
	EXPECT_EQ(spell.Sparks()[0].age, 0.0f); // Draw skips them
}

TEST(FallingSpell, SparksOnScreen)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	spell.StartSparks();
	const auto before = spell.Sparks();
	spell.Draw(100, 640, 480);
	EXPECT_TRUE(spell.SparksOn());
	const auto quads = spell.SparkQuads(640, 480, 1.0f);
	ASSERT_EQ(quads.size(), static_cast<size_t>(k_SparkCount));
	const float tanHalf = std::tan(k_FallFov * 0.5f);
	const float age = 100.0f * 0.0013f;
	// each quad is centred on its spark's truncated pixel, half width size' 320 / (T depth) on both axes
	for (int i = 0; i < k_SparkCount; ++i)
	{
		const auto& spark = before.at(static_cast<size_t>(i));
		const auto& sprite = spell.Sprites().at(static_cast<size_t>(i));
		EXPECT_FLOAT_EQ(spell.Sparks().at(static_cast<size_t>(i)).age, age);
		EXPECT_EQ(sprite.argb >> 24, 0xFFu); // 255 - 32 x 0 (the age before the step)
		EXPECT_FLOAT_EQ(sprite.size, ((spark.size - spark.shrink * age * 0.5f) + 1.0f) * 0.75f);
		EXPECT_FLOAT_EQ(sprite.angle, spark.spin * age + static_cast<float>(i));
		EXPECT_EQ(sprite.cell, 1); // trunc(0.13 x 8)
		bool found = false;
		for (const auto& quad : quads)
		{
			const glm::vec2 centre = (quad[0].pixel + quad[2].pixel) * 0.5f;
			const glm::vec2 want(static_cast<float>(static_cast<int32_t>(640.0f * spark.x)),
			                     static_cast<float>(static_cast<int32_t>(480.0f * spark.y)));
			if (glm::length(centre - want) < 0.01f && quad[0].argb == sprite.argb)
			{
				found = true;
				const float half = sprite.size * 320.0f / (tanHalf * spark.depth);
				EXPECT_NEAR(glm::length(quad[1].pixel - quad[0].pixel), 2.0f * half, 0.01f);
				EXPECT_NEAR(glm::length(quad[3].pixel - quad[0].pixel), 2.0f * half, 0.01f);
			}
		}
		EXPECT_TRUE(found) << i;
		// y moves by (spin + 1) (1 - y) dt 0.1
		EXPECT_FLOAT_EQ(spell.Sparks().at(static_cast<size_t>(i)).y,
		                spark.y + (spark.spin + 1.0f) * (1.0f - spark.y) * age * 0.1f);
	}
	// far to near (sorted by |pos - camera|^2): the keys of the quads' sprites never grow
	float last = 1e30f;
	for (const auto& quad : quads)
	{
		const glm::vec2 centre = (quad[0].pixel + quad[2].pixel) * 0.5f;
		for (int i = 0; i < k_SparkCount; ++i)
		{
			const auto& spark = before.at(static_cast<size_t>(i));
			const glm::vec2 want(static_cast<float>(static_cast<int32_t>(640.0f * spark.x)),
			                     static_cast<float>(static_cast<int32_t>(480.0f * spark.y)));
			if (glm::length(centre - want) < 0.01f)
			{
				const auto& sprite = spell.Sprites().at(static_cast<size_t>(i));
				const float key = glm::dot(sprite.position, sprite.position);
				EXPECT_LE(key, last * 1.0001f);
				last = key;
				break;
			}
		}
	}
	// the near plane: past every depth nothing is drawn
	EXPECT_TRUE(spell.SparkQuads(640, 480, 30.0f).empty());
}

TEST(FallingSpell, SparksFadeAndStop)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	spell.StartSparks();
	// 255 / 32 = 7.97 of age: 6131 ms at 0.0013 a ms
	for (int frame = 0; frame < 6100 / 50; ++frame)
	{
		spell.Draw(50, 640, 480);
	}
	EXPECT_TRUE(spell.SparksOn());
	for (int frame = 0; frame < 10; ++frame)
	{
		spell.Draw(50, 640, 480);
	}
	EXPECT_FALSE(spell.SparksOn()); // none seen
	EXPECT_TRUE(spell.SparkQuads(640, 480, 1.0f).empty());
}

TEST(FallingSpell, BurstsNeedStateTwoAndACentre)
{
	game_random::testing::ScopedState state;
	Fake fake;
	auto spell = Make(fake);
	spell.Init(std::nullopt);
	fake.centre = glm::ivec2(320, 240);
	spell.FinishFrame(100, 1, 640, 480);
	EXPECT_TRUE(spell.Bursts().empty());
	fake.centre.reset();
	spell.FinishFrame(100, 2, 640, 480);
	EXPECT_TRUE(spell.Bursts().empty());
	EXPECT_EQ(spell.BurstAlpha(), 0.0f); // no step without the centre
	fake.centre = glm::ivec2(320, 240);
	spell.FinishFrame(1000, 2, 640, 480);
	ASSERT_EQ(spell.Bursts().size(), 4u);
	EXPECT_FLOAT_EQ(spell.BurstAlpha(), 0.07f);
	EXPECT_FLOAT_EQ(spell.BurstShape(), 0.175f);
	EXPECT_FLOAT_EQ(spell.BurstGrow(), 1.0f);
	EXPECT_EQ(spell.Bursts()[0].vertices[0].argb, (17u << 24) | 0x0020FF40u); // trunc(0.07 x 255) = 17
	EXPECT_EQ(spell.Bursts()[3].vertices[0].argb, (17u << 24) | 0x00FF4040u);
	for (int i = 0; i < 2; ++i)
	{
		spell.FinishFrame(1000, 2, 640, 480);
	}
	EXPECT_FLOAT_EQ(spell.BurstShape(), 0.525f);
	EXPECT_FLOAT_EQ(spell.BurstGrow(), 4.5f); // past 0.5: + 5 x 0.7
}

TEST(FallingSpell, CameraPathFraction)
{
	// 4 keys over 10 ms: step 3.333, its integer part 3
	const auto path = CameraPath::Parse(PathBytes(10, {0.0f, 1.0f, 2.0f, 3.0f}));
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->keys.size(), 4u);
	EXPECT_EQ(path->At(0).position.x, 0.0f);
	// t = 5: key 3 x 5 / 10 = 1, k = trunc(1.5) = 1, remainder 5 - 3 = 2, f = 0.6
	EXPECT_FLOAT_EQ(path->At(5).position.x, 1.0f * 0.4f + 2.0f * 0.6f);
	EXPECT_FLOAT_EQ(path->At(5).focus.x, 2.0f * 1.6f);
	EXPECT_FLOAT_EQ(path->At(5).matrix[0], 3.0f * 1.6f);
	// past the duration: duration - 1 = 9, key 2, k = 2, remainder 3, f = 0.9
	EXPECT_FLOAT_EQ(path->At(50).position.x, 2.0f * 0.1f + 3.0f * 0.9f);
	EXPECT_FALSE(CameraPath::Parse({1, 2, 3}).has_value());
	EXPECT_FALSE(CameraPath::Parse(PathBytes(10, {})).has_value());
}

TEST(FallingSpell, CameraPathDriftsLikeTheOriginal)
{
	// fall.cm2's shape: 1449 keys over 48333 ms, the key's index as its x
	std::vector<float> xs(1449);
	for (size_t i = 0; i < xs.size(); ++i)
	{
		xs[i] = static_cast<float>(i);
	}
	const auto path = CameraPath::Parse(PathBytes(48333, xs));
	ASSERT_TRUE(path.has_value());
	// t = 48000: key 1438, k = trunc(48000 / 33.379) = 1438, remainder 48000 - 1438 x 33 = 546, f = 16.36
	const float step = 48333.0f / 1448.0f;
	const float f = 546.0f / step;
	EXPECT_GT(f, 16.0f);
	EXPECT_FLOAT_EQ(path->At(48000).position.x, 1438.0f * (1.0f - f) + 1439.0f * f);
}

TEST(FallingSpell, CameraFollowsTheFilm)
{
	Fake fake;
	auto spell = Make(fake);
	spell.Init(CameraPath::Parse(PathBytes(10, {0.0f, 1.0f, 2.0f, 3.0f})));
	spell.UpdateCamera(5);
	ASSERT_TRUE(spell.CameraNow().has_value());
	EXPECT_FLOAT_EQ(spell.CameraNow()->position.x, (0.4f + 1.2f) * k_PathScale);
	EXPECT_FLOAT_EQ(spell.CameraNow()->focus.x, 3.2f * k_PathScale);
	EXPECT_FLOAT_EQ(spell.CameraNow()->matrix[0], 4.8f); // the matrix is not scaled
	EXPECT_FLOAT_EQ(spell.CameraNow()->fov, k_FallFov);
	spell.Close();
	EXPECT_FALSE(spell.CameraNow().has_value());
}

TEST(FallingSpell, CameraAppliedEveryUpdateAndGivenBackAtClose)
{
	FallingSpell::Hooks hooks;
	std::vector<std::optional<FallCamera>> applied;
	hooks.applyCamera = [&applied](const std::optional<FallCamera>& camera) { applied.push_back(camera); };
	FallingSpell spell(std::move(hooks));
	spell.UpdateCamera(5); // not open: nothing
	EXPECT_TRUE(applied.empty());
	spell.Init(CameraPath::Parse(PathBytes(10, {0.0f, 1.0f, 2.0f, 3.0f})));
	spell.UpdateCamera(5);
	spell.UpdateCamera(6);
	ASSERT_EQ(applied.size(), 2u);
	ASSERT_TRUE(applied[1].has_value());
	EXPECT_FLOAT_EQ(applied[1]->position.x, spell.CameraNow()->position.x);
	EXPECT_FLOAT_EQ(applied[1]->fov, k_FallFov);
	spell.Close();
	ASSERT_EQ(applied.size(), 3u);
	EXPECT_FALSE(applied[2].has_value()); // the game camera back
	spell.Close();                        // closed: nothing
	EXPECT_EQ(applied.size(), 3u);
}

TEST(FallingSpell, WorldToCameraTransposesAndNormalisesTheRows)
{
	// (a0, a3, -a6, a1, a4, -a7, a2, a5, -a8), each row normalised, translation -(column . position)
	FallCamera camera;
	camera.position = {1.0f, 2.0f, 3.0f};
	camera.matrix = {2.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 4.0f, 7.0f, 8.0f, 9.0f};
	const glm::mat4 view = WorldToCamera(camera);
	EXPECT_EQ(view[0], glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(view[1], glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
	EXPECT_EQ(view[2], glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
	EXPECT_EQ(view[3], glm::vec4(-1.0f, -2.0f, 3.0f, 1.0f)); // m9..m11 of the path are not read
	EXPECT_EQ(glm::vec3(view * glm::vec4(camera.position, 1.0f)), glm::vec3(0.0f));
	// rows: right +z, up +y, back +x: a point ahead (-x) is at +z in camera space, +z is to the right
	camera.position = glm::vec3(0.0f);
	camera.matrix = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	const glm::mat4 turned = WorldToCamera(camera);
	EXPECT_EQ(glm::vec3(turned * glm::vec4(-5.0f, 0.0f, 0.0f, 1.0f)), glm::vec3(0.0f, 0.0f, 5.0f));
	EXPECT_EQ(glm::vec3(turned * glm::vec4(0.0f, 0.0f, 2.0f, 1.0f)), glm::vec3(2.0f, 0.0f, 0.0f));
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(FallingSpell, RealFallCm2)
{
	const auto root = GamePath();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	std::ifstream file(*root / "Data" / "Spells" / "fall" / "fall.cm2", std::ios::binary);
	ASSERT_TRUE(file.good());
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	EXPECT_EQ(bytes.size(), 104340u);
	const auto path = CameraPath::Parse(bytes);
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->duration, 48333u);
	EXPECT_EQ(path->keys.size(), 1449u);
	// the first key, bytes 12..20 of the file
	EXPECT_FLOAT_EQ(path->keys[0].position.x, 8.2829723f);
	EXPECT_EQ(path->At(0).position, path->keys[0].position);
}

/// The checks of RealFallCm2 on a small path built here: the header's duration and key count, each key read in
/// order, and the start of the path is its first key
TEST(FallingSpell, RealFallCm2Synthetic)
{
	const std::vector<float> xs {0.5f, 1.5f, 2.5f};
	const auto bytes = PathBytes(40, xs);
	// a 12-byte header and 72 bytes per key
	EXPECT_EQ(bytes.size(), 0xCu + 0x48u * xs.size());
	const auto path = CameraPath::Parse(bytes);
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->duration, 40u);
	ASSERT_EQ(path->keys.size(), xs.size());
	for (size_t i = 0; i < xs.size(); ++i)
	{
		EXPECT_FLOAT_EQ(path->keys[i].position.x, xs[i]) << i;
		EXPECT_FLOAT_EQ(path->keys[i].focus.x, 2.0f * xs[i]) << i;
		EXPECT_FLOAT_EQ(path->keys[i].matrix[0], 3.0f * xs[i]) << i;
	}
	EXPECT_EQ(path->At(0).position, path->keys[0].position);
}
