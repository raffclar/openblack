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

	/// The matrix in the columns with this prefix, one glm column after the other
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

/// The cube of the data: the same corners and faces as the script that makes the data
Shape Cube(float half, float radius)
{
	Shape shape;
	for (int i = 0; i < 8; ++i)
	{
		const float x = ((i & 1) ^ ((i >> 1) & 1)) != 0 ? half : -half;
		const float y = (i & 2) != 0 ? half : -half;
		const float z = (i & 4) != 0 ? half : -half;
		shape.points.emplace_back(x, y, z);
	}
	shape.faces = {{0, 3, 2}, {0, 2, 1}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
	               {3, 7, 6}, {3, 6, 2}, {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}};
	shape.radius = radius;
	return shape;
}

} // namespace

TEST(OraclePhysicsTouch, ThePointsOfOneBodyAgainstTheFacesOfAnotherAreTheGamesBitForBit)
{
	const auto loaded = oracle::Table::Load("physics_touch.txt");
	ASSERT_TRUE(loaded.has_value());
	const auto& table = *loaded;
	ASSERT_GT(table.Rows(), 0u);
	size_t failures = 0;
	size_t hits = 0;
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		Body a({}, Cube(row.F("half_a"), 1.0f));
		Body b({}, Cube(row.F("half_b"), row.F("radius_b")));
		a.SetPoseDirect(row.Matrix("axes_a"), {row.F("ca_x"), row.F("ca_y"), row.F("ca_z")});
		b.SetPoseDirect(row.Matrix("axes_b"), {row.F("cb_x"), row.F("cb_y"), row.F("cb_z")});
		a.resting = row.U("resting_a") != 0;
		b.resting = row.U("resting_b") != 0;
		if (row.U("skeleton_a") != 0)
		{
			// Only the presence of a skeleton on the body with the points changes the test
			a.SetSkeleton({Ellipsoid {}});
		}
		// The game's inputs: our set-up must give the same bits, or the row tests nothing
		std::string inputs;
		for (size_t i = 0; i < 8; ++i)
		{
			const auto name = std::to_string(i);
			const auto& pa = a.Points()[i];
			const auto& pb = b.Points()[i];
			inputs += Mismatch("wa" + name + "x", pa.world.x, row.U("wa" + name + "x"));
			inputs += Mismatch("wa" + name + "y", pa.world.y, row.U("wa" + name + "y"));
			inputs += Mismatch("wa" + name + "z", pa.world.z, row.U("wa" + name + "z"));
			inputs += Mismatch("ahead" + name, pa.aheadLength, row.U("ahead" + name));
			inputs += Mismatch("wb" + name + "x", pb.world.x, row.U("wb" + name + "x"));
			inputs += Mismatch("wb" + name + "y", pb.world.y, row.U("wb" + name + "y"));
			inputs += Mismatch("wb" + name + "z", pb.world.z, row.U("wb" + name + "z"));
		}
		for (size_t i = 0; i < 12; ++i)
		{
			const auto name = "nb" + std::to_string(i);
			const auto& normal = b.Faces()[i].worldNormal;
			inputs += Mismatch(name + "x", normal.x, row.U(name + "x"));
			inputs += Mismatch(name + "y", normal.y, row.U(name + "y"));
			inputs += Mismatch(name + "z", normal.z, row.U(name + "z"));
		}
		ASSERT_TRUE(inputs.empty()) << "row " << index << " inputs:" << inputs;

		a.TouchFaces(b);

		std::string text;
		if (static_cast<uint32_t>(a.Contacts()) != row.U("contacts"))
		{
			text += std::format(" contacts: ours {} game {}", a.Contacts(), row.U("contacts"));
		}
		for (size_t i = 0; i < 8; ++i)
		{
			const auto name = std::to_string(i);
			const auto& point = a.Points()[i];
			const bool gameHit = row.U("other" + name) != 0;
			if ((point.other == &b) != gameHit)
			{
				text += std::format(" other{}: ours {} game {}", i, point.other == &b, gameHit);
				continue;
			}
			if (!gameHit)
			{
				continue;
			}
			++hits;
			text += Mismatch("hit" + name + "x", point.contact.x, row.U("hit" + name + "x"));
			text += Mismatch("hit" + name + "y", point.contact.y, row.U("hit" + name + "y"));
			text += Mismatch("hit" + name + "z", point.contact.z, row.U("hit" + name + "z"));
			text += Mismatch("normal" + name + "x", point.normal.x, row.U("normal" + name + "x"));
			text += Mismatch("normal" + name + "y", point.normal.y, row.U("normal" + name + "y"));
			text += Mismatch("normal" + name + "z", point.normal.z, row.U("normal" + name + "z"));
			text += Mismatch("depth" + name, point.depth, row.U("depth" + name));
		}
		const auto flag = [&text, &row](const std::string& name, bool ours) {
			if (static_cast<uint32_t>(ours) != row.U(name))
			{
				text += std::format(" {}: ours {} game {}", name, ours, row.U(name));
			}
		};
		flag("touched_a", a.touched);
		flag("touched_b", b.touched);
		flag("last_a", a.lastHit == &b);
		flag("last_b", b.lastHit == &a);
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
	// The data must hold enough points that hit a face
	EXPECT_GT(hits, 100u);
}
