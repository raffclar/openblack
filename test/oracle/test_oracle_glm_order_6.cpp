/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Matrix.h"
#include "OracleData.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{

/// How fast the smoothing of a sprite's velocity follows, as the rule scales it
constexpr float k_SmoothingRateScale = -10.0f;

/// The columns of a table row, read by name
struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }
	[[nodiscard]] glm::vec3 Vec(const std::string& x, const std::string& y, const std::string& z) const
	{
		return {F(x), F(y), F(z)};
	}
	/// Three rows of the engine, from nine columns, as a glm matrix whose columns are the rows
	[[nodiscard]] glm::mat3 Rows(const std::string& prefix) const
	{
		glm::mat3 m;
		for (int i = 0; i < 3; ++i)
		{
			for (int k = 0; k < 3; ++k)
			{
				m[i][k] = F(prefix + std::to_string(3 * i + k));
			}
		}
		return m;
	}
	/// Three rows and a place of the engine, from twelve columns
	[[nodiscard]] glm::mat4 Matrix(const std::string& prefix) const
	{
		glm::mat4 m {1.0f};
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 3; ++k)
			{
				m[i][k] = F(prefix + std::to_string(3 * i + k));
			}
		}
		return m;
	}
};

/// Adds the names of the parts that differ from the game's bits to the text
bool SameBits(const Row& row, const std::vector<std::string>& names, const std::vector<float>& ours, std::string& text)
{
	bool same = true;
	for (size_t i = 0; i < names.size(); ++i)
	{
		if (std::bit_cast<uint32_t>(ours[i]) != row.U(names[i]))
		{
			same = false;
			text += " " + names[i] + " game " + oracle::Hex(row.F(names[i])) + " ours " + oracle::Hex(ours[i]);
		}
	}
	return same;
}

std::vector<std::string> Names(const std::string& prefix, int count)
{
	std::vector<std::string> names;
	for (int i = 0; i < count; ++i)
	{
		names.push_back(prefix + std::to_string(i));
	}
	return names;
}

std::vector<float> Flat(const glm::mat3& m)
{
	std::vector<float> values;
	for (int i = 0; i < 3; ++i)
	{
		for (int k = 0; k < 3; ++k)
		{
			values.push_back(m[i][k]);
		}
	}
	return values;
}

std::vector<float> Flat(const glm::mat4& m)
{
	std::vector<float> values;
	for (int i = 0; i < 4; ++i)
	{
		for (int k = 0; k < 3; ++k)
		{
			values.push_back(m[i][k]);
		}
	}
	return values;
}

/// Counts the rows that differ and reports the first few
class Mismatches
{
public:
	void Check(bool same, size_t index, const std::string& text)
	{
		if (!same)
		{
			if (_count < 5)
			{
				ADD_FAILURE() << "row " << index << ":" << text;
			}
			++_count;
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};

} // namespace

TEST(OracleGlmOrder6, ATumblingParticle)
{
	const auto table = oracle::Table::Load("tumble.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto velocity = row.Vec("vx", "vy", "vz");
		float speed = maths::TumbleSpeed(velocity, row.F("speed"));
		if (row.U("restrict") != 0)
		{
			const float most = row.F("most");
			speed = speed > most ? most : (speed < -most ? -most : speed);
		}
		const auto turned = maths::Tumble(row.Rows("m"), speed * row.F("dt"), std::abs(velocity.z) < std::abs(velocity.x));
		std::string text;
		mismatches.Check(SameBits(row, Names("r", 9), Flat(turned), text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder6, ASpriteRolledByItsSmoothedVelocity)
{
	const auto table = oracle::Table::Load("orient_sprite.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto velocity = row.Vec("vx", "vy", "vz");
		auto smoothed = row.Vec("sx", "sy", "sz");
		float rate = row.F("rate");
		if (row.U("first") != 0)
		{
			smoothed = velocity;
			rate = maths::SmoothingRate(row.F("smoothing"), k_SmoothingRateScale);
		}
		smoothed += (velocity - smoothed) * maths::SmoothingShare(row.F("dt"), rate);
		const glm::vec3 seen(-smoothed.x + 0.0f, -smoothed.y + row.F("upward"), -smoothed.z + 0.0f);
		const auto camera = row.Rows("c");
		const glm::vec3 right(camera[0][0], camera[1][0], camera[2][0]);
		const glm::vec3 up(camera[0][1], camera[1][1], camera[2][1]);
		const auto rows = maths::AngleY(maths::ScreenVelocityAngle(seen, right, up));
		auto ours = std::vector<float> {smoothed.x, smoothed.y, smoothed.z, rate};
		const auto flat = Flat(rows);
		ours.insert(ours.end(), flat.begin(), flat.end());
		auto names = std::vector<std::string> {"osx", "osy", "osz", "orate"};
		const auto rowNames = Names("r", 9);
		names.insert(names.end(), rowNames.begin(), rowNames.end());
		std::string text;
		mismatches.Check(SameBits(row, names, ours, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder6, AShieldParticleSpinningToAStop)
{
	const auto table = oracle::Table::Load("initial_spin.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const float fade = std::clamp(row.F("age") / row.F("fade"), 0.0f, 1.0f);
		const auto turned = maths::SpinAboutVertical(row.Rows("m"), maths::FadedSpinAngle(row.F("spin"), fade, row.F("dt")));
		std::string text;
		mismatches.Check(SameBits(row, Names("r", 9), Flat(turned), text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder6, AShieldDomePatchTurnedByItsParent)
{
	constexpr float k_StillMove = 1e-4f;
	const auto table = oracle::Table::Load("vapour_turn.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto p = row.Vec("px", "py", "pz");
		auto m = row.Rows("m");
		if (row.U("first") != 0)
		{
			m = maths::OrientAlong(p, m);
		}
		else
		{
			const auto last = p - row.Vec("vx", "vy", "vz") * row.F("dt");
			const auto moved = glm::abs(last - p);
			if (moved.x > k_StillMove || moved.y > k_StillMove || moved.z > k_StillMove)
			{
				m = maths::TurnWithMove(m, last, p);
			}
		}
		std::string text = row.U("first") != 0 ? " first" : " moved";
		mismatches.Check(SameBits(row, Names("r", 9), Flat(m), text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder6, TheEnginesMatrixProducts)
{
	const auto table = oracle::Table::Load("matrix_products.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto a = row.Matrix("a");
		const auto b = row.Matrix("b");
		auto ours = Flat(x87::Product(a, b));
		const auto front = Flat(x87::PreProduct(a, b));
		ours.insert(ours.end(), front.begin(), front.end());
		auto names = Names("ab", 12);
		const auto frontNames = Names("ba", 12);
		names.insert(names.end(), frontNames.begin(), frontNames.end());
		std::string text;
		mismatches.Check(SameBits(row, names, ours, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder6, TheEnginesTurnAboutAnAxis)
{
	const auto table = oracle::Table::Load("axis_turn.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto turn = x87::AxisTurn(row.Vec("x", "y", "z"), row.F("angle"));
		std::string text;
		mismatches.Check(SameBits(row, Names("m", 12), Flat(turn), text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder6, APointOfAParticleHierarchy)
{
	const auto table = oracle::Table::Load("hierarchy_points.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		std::vector<glm::mat4> frames;
		for (int k = 0; k < 3; ++k)
		{
			if (row.U("h" + std::to_string(k)) == 0)
			{
				continue;
			}
			const auto prefix = "f" + std::to_string(k);
			Atom atom;
			atom.rotation = row.Rows(prefix + "m");
			atom.baseScale = row.F(prefix + "base");
			atom.ruleScale = row.F(prefix + "rule");
			atom.stretch = row.F(prefix + "stretch");
			atom.position = row.Vec(prefix + "x", prefix + "y", prefix + "z");
			frames.push_back(Effect::FrameMatrix(atom));
		}
		const auto world = maths::FrameToWorld(frames, row.Vec("lx", "ly", "lz"));
		const auto local = maths::WorldToFrame(frames, row.Vec("wx", "wy", "wz"));
		std::string text;
		mismatches.Check(SameBits(row, {"owx", "owy", "owz", "olx", "oly", "olz"},
		                          {world.x, world.y, world.z, local.x, local.y, local.z}, text),
		                 index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}
