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

#include <array>
#include <bit>
#include <format>
#include <string>
#include <tuple>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "OracleData.h"
#include "Physics/Body.h"

using namespace openblack;
using namespace openblack::physics;

namespace
{

/// The columns of a table row, read by name
struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }

	[[nodiscard]] glm::vec3 Vec(const std::string& prefix) const { return {F(prefix + "x"), F(prefix + "y"), F(prefix + "z")}; }

	/// The matrix in the columns with this prefix: each row of the game's matrix is one column
	[[nodiscard]] glm::mat3 Matrix(const std::string& prefix) const
	{
		glm::mat3 matrix;
		for (glm::length_t column = 0; column < 3; ++column)
		{
			for (glm::length_t value = 0; value < 3; ++value)
			{
				matrix[column][value] = F(prefix + std::to_string((3 * column) + value));
			}
		}
		return matrix;
	}
};

/// Our value against the game's bits, as text when they differ
std::string Mismatch(const std::string& name, float ours, uint32_t game)
{
	const auto bits = std::bit_cast<uint32_t>(ours);
	if (bits == game || (std::isnan(ours) && std::isnan(std::bit_cast<float>(game))))
	{
		return {};
	}
	return std::format(" {}: ours {:08x} game {:08x}", name, bits, game);
}

Shape Cube()
{
	Shape shape;
	for (int i = 0; i < 8; ++i)
	{
		shape.points.emplace_back((i & 1) != 0 ? 0.5f : -0.5f, (i & 2) != 0 ? 0.5f : -0.5f, (i & 4) != 0 ? 0.5f : -0.5f);
	}
	shape.radius = 0.8f;
	return shape;
}

/// A body with the state of one row before the step. Our torque and spin have the other sign: the game turns a body
/// the other way about its spin, so both turns are the same.
Body BodyOf(const Row& row)
{
	Body body({}, Cube());
	Body::VisitSaved(body,
	                 [&row](auto& velocity, auto& angularMomentum, auto&, auto&, auto&, auto& resting, auto& touched, auto&,
	                        auto&, auto&, auto& axes, auto& centre, auto&, auto&, auto&, auto&, auto& speed, auto& mass, auto&,
	                        auto&, auto&, auto& spinKept, auto&, auto& radius, auto& dynamic, auto& contacts, auto& restCounter,
	                        auto& force, auto& torque, auto&, auto& inverseInertia, auto&, auto&, auto&) {
		                 velocity = row.Vec("v");
		                 angularMomentum = -row.Vec("s");
		                 resting = row.U("resting") != 0;
		                 touched = row.U("touched") != 0;
		                 axes = row.Matrix("axes");
		                 centre = row.Vec("c");
		                 speed = 0.0f;
		                 mass = row.F("mass");
		                 spinKept = row.F("kept");
		                 radius = row.F("radius");
		                 dynamic = true;
		                 contacts = static_cast<int>(row.U("contacts"));
		                 restCounter = static_cast<int>(row.U("counter"));
		                 force = row.Vec("f");
		                 torque = -row.Vec("t");
		                 for (size_t i = 0; i < 3; ++i)
		                 {
			                 for (size_t j = 0; j < 3; ++j)
			                 {
				                 inverseInertia.at(i).at(j) = row.F("inv" + std::to_string((3 * i) + j));
			                 }
		                 }
	                 });
	return body;
}

} // namespace

TEST(OraclePhysicsStep, OneBodyStepIsTheGamesStepBitForBit)
{
	const auto loaded = oracle::Table::Load("physics_step.txt");
	ASSERT_TRUE(loaded.has_value());
	const auto& table = *loaded;
	ASSERT_GT(table.Rows(), 0u);
	size_t failures = 0;
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		auto body = BodyOf(row);
		const auto result = body.Integrate();
		std::string text;
		if (static_cast<uint32_t>(result) != row.U("result"))
		{
			text += std::format(" result: ours {} game {}", static_cast<int>(result), row.U("result"));
		}
		const auto out = [&](const std::string& name, float ours) { text += Mismatch(name, ours, row.U(name)); };
		// A spin that the step clears is a positive zero in both
		const auto gameSign = [](float spin) { return spin == 0.0f ? spin : -spin; };
		out("out_sx", gameSign(body.angularMomentum.x));
		out("out_sy", gameSign(body.angularMomentum.y));
		out("out_sz", gameSign(body.angularMomentum.z));
		for (glm::length_t column = 0; column < 3; ++column)
		{
			for (glm::length_t value = 0; value < 3; ++value)
			{
				out("out_axes" + std::to_string((3 * column) + value), body.Axes()[column][value]);
			}
		}
		out("out_cx", body.Centre().x);
		out("out_cy", body.Centre().y);
		out("out_cz", body.Centre().z);
		out("out_vx", body.velocity.x);
		out("out_vy", body.velocity.y);
		out("out_vz", body.velocity.z);
		out("out_speed", body.Speed());
		if (!text.empty())
		{
			++failures;
			if (failures <= 10)
			{
				ADD_FAILURE() << "row " << index << ":" << text;
			}
		}
	}
	EXPECT_EQ(failures, 0u) << "of " << table.Rows() << " rows";
}

TEST(OraclePhysicsStep, TheMomentOfInertiaIsTheGamesBitForBit)
{
	const auto loaded = oracle::Table::Load("physics_inertia.txt");
	ASSERT_TRUE(loaded.has_value());
	const auto& table = *loaded;
	ASSERT_GT(table.Rows(), 0u);
	size_t failures = 0;
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		Shape shape;
		for (uint32_t point = 0; point < row.U("count"); ++point)
		{
			const auto name = "p" + std::to_string(point);
			shape.points.emplace_back(row.F(name + "x"), row.F(name + "y"), row.F(name + "z"));
		}
		shape.radius = row.F("radius");
		const Body body({.mass = row.F("mass"), .material = {.drag = row.F("drag")}}, shape);
		std::array<std::array<float, 3>, 3> inverse {};
		Body::VisitSaved(body, [&inverse](auto&&... fields) {
			const auto all = std::tie(fields...);
			inverse = std::get<30>(all);
		});
		std::string text;
		for (size_t i = 0; i < 3; ++i)
		{
			for (size_t j = 0; j < 3; ++j)
			{
				const auto part = std::to_string((3 * i) + j);
				text += Mismatch("inertia" + part, body.Inertia().at(i).at(j), row.U("inertia" + part));
				text += Mismatch("inverse" + part, inverse.at(i).at(j), row.U("inverse" + part));
			}
		}
		text += Mismatch("out_drag", body.Drag(), row.U("out_drag"));
		if (!text.empty())
		{
			++failures;
			if (failures <= 10)
			{
				ADD_FAILURE() << "row " << index << ":" << text;
			}
		}
	}
	EXPECT_EQ(failures, 0u) << "of " << table.Rows() << " rows";
}
