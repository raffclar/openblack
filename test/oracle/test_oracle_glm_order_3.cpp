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
#include <optional>
#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureHair.h"
#include "OracleData.h"
#include "Particles/ParticleFlocking.h"

using namespace openblack;

namespace
{

/// The rows of a 4x3 matrix of the game, as our matrix holds them
std::array<float, 12> RowsOf(const glm::mat4& matrix)
{
	std::array<float, 12> rows {};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			rows.at(static_cast<size_t>((row * 3) + column)) = matrix[row][column];
		}
	}
	return rows;
}

} // namespace

TEST(OracleGlmOrder3, TheCreaturesEyeAsTheGameDrawsIt)
{
	const auto table = oracle::Table::Load("creature_eye.txt");
	ASSERT_TRUE(table.has_value());
	const auto column = [&](const std::string& name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto f = [&](const std::string& name) { return table->Float(row, column(name)); };
		const auto u = [&](const std::string& name) { return table->Bits(row, column(name)); };
		const auto vertex = [&](uint32_t i) {
			const auto prefix = "v" + std::to_string(i);
			return glm::vec3(f(prefix + "x"), f(prefix + "y"), f(prefix + "z"));
		};
		const auto surface = [&](uint32_t i) {
			return creature_eyes::PointOnTriangle(vertex(3 * i), vertex((3 * i) + 1), vertex((3 * i) + 2),
			                                      f("s" + std::to_string(i)), f("t" + std::to_string(i)));
		};
		const auto eye = u("eye");
		const auto mode = static_cast<creature_eyes::Mode>(u("mode"));
		const glm::vec3 target {f("target_x"), f("target_y"), f("target_z")};
		const auto yaw = f("yaw");
		const auto offset = static_cast<int32_t>(u("rand")) - 100;
		const creature_eyes::Look look {
		    .mode = mode,
		    .openness = f("openness"),
		    .blink = {.state = static_cast<creature_eyes::Blink::State>(u("blink")),
		              .timerMs = static_cast<int32_t>(u("timer")),
		              .intervalMs = creature_eyes::k_BlinkIntervalMs},
		    .lookAt = target != glm::vec3(0.0f) ? std::optional(target) : std::nullopt,
		    .ahead = {x87::Mul(x87::Sin(yaw), -1.0f), 0.0f, x87::RoundToFloat(x87::Cos(yaw))},
		    .darts = {offset, offset, offset},
		};
		const creature_eyes::LidAngles lids {.open = f("lid_open"), .closed = f("lid_closed"), .calm = f("lid_calm")};
		const auto eyeSize = creature_eyes::EyeSize(f("size"), f("eye_scale"), f("evil_good"), mode);
		Zoomer3 turn;
		const auto anchor = u("has_lid") != 0 ? std::optional(surface(eye + 2).position) : std::nullopt;
		const auto placed = creature_eyes::PlaceEye(surface(eye), anchor, f("depth"), eye == 1, look, lids, eyeSize, turn,
		                                            static_cast<int32_t>(u("ms")));
		const auto ball = RowsOf(placed.eyeball);
		const auto lid = placed.eyelid ? RowsOf(*placed.eyelid) : std::array<float, 12> {};
		bool same = true;
		std::string text;
		for (size_t i = 0; i < 12; ++i)
		{
			const auto ballName = "ball" + std::to_string(i);
			const auto lidName = "lid" + std::to_string(i);
			if (std::bit_cast<uint32_t>(ball.at(i)) != u(ballName))
			{
				same = false;
				text += " " + ballName + " game " + oracle::Hex(f(ballName)) + " ours " + oracle::Hex(ball.at(i));
			}
			if (std::bit_cast<uint32_t>(lid.at(i)) != u(lidName))
			{
				same = false;
				text += " " + lidName + " game " + oracle::Hex(f(lidName)) + " ours " + oracle::Hex(lid.at(i));
			}
		}
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << " mode " << u("mode") << " blink " << u("blink") << " eye " << eye << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder3, TheFlockRulesFalloff)
{
	const auto table = oracle::Table::Load("flock_falloff.txt");
	ASSERT_TRUE(table.has_value());
	const auto distance = table->Column("distance").value_or(0);
	const auto modifier = table->Column("modifier").value_or(0);
	const auto type = table->Column("type").value_or(0);
	const auto invert = table->Column("invert").value_or(0);
	const auto falloff = table->Column("falloff").value_or(0);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = particles::flocking::Falloff(table->Float(row, distance), static_cast<int>(table->Bits(row, type)),
		                                                table->Bits(row, invert) != 0, table->Float(row, modifier));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, falloff) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << " game " << oracle::Hex(table->Float(row, falloff)) << " ours "
			              << oracle::Hex(ours);
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder3, AVectorMadeALengthLong)
{
	const auto table = oracle::Table::Load("set_length.txt");
	ASSERT_TRUE(table.has_value());
	const auto c = [&](const char* name) { return table->Column(name).value_or(0); };
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec3 v {table->Float(row, c("x")), table->Float(row, c("y")), table->Float(row, c("z"))};
		const auto ours = x87::Scaled<x87::SumOrder::Zyx>(v, table->Float(row, c("length")));
		const bool same = std::bit_cast<uint32_t>(ours.x) == table->Bits(row, c("rx")) &&
		                  std::bit_cast<uint32_t>(ours.y) == table->Bits(row, c("ry")) &&
		                  std::bit_cast<uint32_t>(ours.z) == table->Bits(row, c("rz"));
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << " game " << oracle::Hex(table->Float(row, c("rx"))) << " ours "
			              << oracle::Hex(ours.x);
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder3, AStepOfAHairStrand)
{
	constexpr size_t k_Points = 6;
	const auto table = oracle::Table::Load("hair_step.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto f = [&](const std::string& name) { return table->Float(row, table->Column(name).value_or(0)); };
		const auto u = [&](const std::string& name) { return table->Bits(row, table->Column(name).value_or(0)); };
		const auto vec = [&](const std::string& prefix, size_t i) {
			return glm::vec3(f(prefix + std::to_string(3 * i)), f(prefix + std::to_string((3 * i) + 1)),
			                 f(prefix + std::to_string((3 * i) + 2)));
		};
		const auto point = creature_eyes::PointOnTriangle(vec("t", 0), vec("t", 1), vec("t", 2), f("s"), f("t"));
		const auto alignment = f("alignment");
		const auto blend = [&](size_t first) {
			return creature_hair::ByAlignment(std::array {f("look" + std::to_string(first)),
			                                              f("look" + std::to_string(first + 1)),
			                                              f("look" + std::to_string(first + 2))},
			                                  alignment);
		};
		const creature_hair::Look look {
		    .colour = glm::ivec3(0), .length = blend(0), .damping = blend(3), .stiffness = blend(6), .thickness = blend(9)};
		const auto physics = creature_hair::PhysicsFor(look, f("scale"), k_Points);
		creature_hair::Strand strand {.positions = std::vector<glm::vec3>(k_Points),
		                              .velocities = std::vector<glm::vec3>(k_Points)};
		for (size_t i = 0; i < k_Points; ++i)
		{
			strand.positions[i] = vec("p", i);
			strand.velocities[i] = vec("v", i);
		}
		const creature_hair::Root root {.position = point.position,
		                                .inwardNormal = point.normal,
		                                .direction = creature_hair::GrowthDirection(point.normal)};
		creature_hair::Step(strand, root, physics, f("seconds"));
		bool same = true;
		std::string text;
		for (size_t i = 0; i < k_Points; ++i)
		{
			for (glm::length_t axis = 0; axis < 3; ++axis)
			{
				const auto index = std::to_string((3 * i) + static_cast<size_t>(axis));
				for (const auto& [name, ours] : {std::pair {"rp" + index, strand.positions[i][axis]},
				                                 std::pair {"rv" + index, strand.velocities[i][axis]}})
				{
					if (std::bit_cast<uint32_t>(ours) != u(name))
					{
						same = false;
						text += " " + name + " game " + oracle::Hex(f(name)) + " ours " + oracle::Hex(ours);
					}
				}
			}
		}
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}
