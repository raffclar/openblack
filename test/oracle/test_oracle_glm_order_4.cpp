/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureEyes.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureRig.h"
#include "OracleData.h"
#include "Particles/ParticleFlocking.h"
#include "Particles/ParticleMiracleMaths.h"

using namespace openblack;

namespace
{

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
	[[nodiscard]] glm::vec3 Indexed(const std::string& prefix, size_t first) const
	{
		return {F(prefix + std::to_string(first)), F(prefix + std::to_string(first + 1)),
		        F(prefix + std::to_string(first + 2))};
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

/// The game's rows of a turn whose columns are the game's rows
std::vector<float> RowsOf(const glm::mat3& m)
{
	return {m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]};
}

std::vector<std::string> Names(const std::string& prefix, size_t count)
{
	std::vector<std::string> names;
	for (size_t i = 0; i < count; ++i)
	{
		names.push_back(prefix + std::to_string(i));
	}
	return names;
}

} // namespace

TEST(OracleGlmOrder4, TheShieldsSphereTests)
{
	const auto table = oracle::Table::Load("shield_sphere.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto centre = row.Vec("cx", "cy", "cz");
		const auto first = row.Vec("ax", "ay", "az");
		const auto second = row.Vec("bx", "by", "bz");
		const float radius = row.F("r");
		const float margin = row.F("margin");
		const bool inside = particles::maths::InsideSphere(first, centre, radius, margin);
		const auto entry = particles::maths::SphereEntry(first, second, centre, radius, margin);
		const auto deflected = particles::maths::DeflectOffSphere(first, centre, row.Vec("vx", "vy", "vz"));
		std::string text;
		bool same = SameBits(row, {"ex", "ey", "ez", "dx", "dy", "dz"},
		                     {entry.x, entry.y, entry.z, deflected.x, deflected.y, deflected.z}, text);
		if ((inside ? 1U : 0U) != row.U("inside"))
		{
			same = false;
			text += " inside";
		}
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder4, TheFlockRulesTurns)
{
	const auto table = oracle::Table::Load("flock_turn.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto velocity = row.Vec("vx", "vy", "vz");
		const auto banked = particles::flocking::Banking(velocity, row.Vec("ax", "ay", "az"), row.F("share"), row.F("gravity"));
		// The camera's rows: the right axis is the first column, the up axis the second
		const glm::vec3 right {row.F("c0"), row.F("c3"), row.F("c6")};
		const glm::vec3 up {row.F("c1"), row.F("c4"), row.F("c7")};
		const auto sprite = particles::flocking::SpriteTurn(velocity, right, up);
		std::string text;
		const bool bankSame = SameBits(row, Names("b", 9), RowsOf(banked), text);
		const bool spriteSame = SameBits(row, Names("s", 9), RowsOf(sprite), text);
		if (!(bankSame && spriteSame) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder4, TheLiftOfAThrow)
{
	const auto table = oracle::Table::Load("throw_lift.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto direction = particles::maths::LiftDirection(row.Vec("vx", "vy", "vz"), particles::maths::k_ThrowLift);
		std::string text;
		if (!SameBits(row, {"dx", "dy", "dz"}, {direction.x, direction.y, direction.z}, text) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder4, AStepOfATurnedHairStrand)
{
	constexpr size_t k_Points = 6;
	const auto table = oracle::Table::Load("hair_turned.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		// Each bone: three rows, then the place. The columns of our matrix are the game's rows.
		std::array<glm::mat3, 3> frames {};
		std::array<glm::vec3, 3> places {};
		for (size_t b = 0; b < 3; ++b)
		{
			const size_t base = 12 * b;
			frames.at(b) = glm::mat3(row.Indexed("bone", base), row.Indexed("bone", base + 3), row.Indexed("bone", base + 6));
			places.at(b) = row.Indexed("bone", base + 9);
		}
		const auto placed = [&](size_t b) {
			const auto& m = frames.at(b);
			const glm::mat4 bone {glm::vec4(m[0], 0.0f), glm::vec4(m[1], 0.0f), glm::vec4(m[2], 0.0f),
			                      glm::vec4(places.at(b), 1.0f)};
			return creature::PlaceByBone(bone, row.Indexed("t", 3 * b), creature::k_TurnedHairOrder.at(b));
		};
		const auto point = creature_eyes::PointOnTriangle(placed(0), placed(1), placed(2), row.F("s"), row.F("t"));
		const auto alignment = row.F("alignment");
		const auto blend = [&](const std::string& prefix, size_t firstIndex) {
			return creature_hair::ByAlignment(std::array {row.F(prefix + std::to_string(firstIndex)),
			                                              row.F(prefix + std::to_string(firstIndex + 1)),
			                                              row.F(prefix + std::to_string(firstIndex + 2))},
			                                  alignment);
		};
		const creature_hair::Look look {.colour = glm::ivec3(0),
		                                .length = blend("look", 0),
		                                .damping = blend("look", 3),
		                                .stiffness = blend("look", 6),
		                                .thickness = blend("look", 9)};
		const auto physics = creature_hair::PhysicsFor(look, row.F("scale"), k_Points);
		creature_hair::Strand strand {.positions = std::vector<glm::vec3>(k_Points),
		                              .velocities = std::vector<glm::vec3>(k_Points)};
		for (size_t i = 0; i < k_Points; ++i)
		{
			strand.positions[i] = row.Indexed("p", 3 * i);
			strand.velocities[i] = row.Indexed("v", 3 * i);
		}
		const glm::vec3 angles {blend("angle", 0), blend("angle", 3), blend("angle", 6)};
		const auto direction = creature_hair::GrowthDirection(point.normal, creature_hair::BoneFrame(frames), angles);
		const creature_hair::Root root {.position = point.position, .inwardNormal = point.normal, .direction = direction};
		creature_hair::Step(strand, root, physics, row.F("seconds"));
		std::vector<std::string> names;
		std::vector<float> ours;
		for (size_t i = 0; i < k_Points; ++i)
		{
			for (glm::length_t axis = 0; axis < 3; ++axis)
			{
				const auto part = std::to_string((3 * i) + static_cast<size_t>(axis));
				names.push_back("rp" + part);
				ours.push_back(strand.positions[i][axis]);
				names.push_back("rv" + part);
				ours.push_back(strand.velocities[i][axis]);
			}
		}
		std::string text;
		if (!SameBits(row, names, ours, text) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}
