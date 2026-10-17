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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Matrix.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureRig.h"
#include "OracleData.h"
#include "Particles/SurfaceOfRevolution.h"

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

} // namespace

TEST(OracleGlmOrder5, APointOnASkinnedMesh)
{
	const auto table = oracle::Table::Load("skin_point.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		std::array<glm::vec3, 3> placed {};
		for (size_t b = 0; b < 3; ++b)
		{
			// Each bone: three rows, then the place. The columns of our matrix are the game's rows.
			const size_t base = 12 * b;
			const glm::mat4 bone {glm::vec4(row.Indexed("bone", base), 0.0f), glm::vec4(row.Indexed("bone", base + 3), 0.0f),
			                      glm::vec4(row.Indexed("bone", base + 6), 0.0f),
			                      glm::vec4(row.Indexed("bone", base + 9), 1.0f)};
			placed.at(b) = x87::TransformPoint(bone, row.Indexed("t", 3 * b), creature::k_SkinPointOrder.at(b));
		}
		const auto point = creature_eyes::PointOnTriangle(placed[0], placed[1], placed[2], row.F("s"), row.F("t"));
		std::string text;
		if (!SameBits(row, {"px", "py", "pz", "nx", "ny", "nz"},
		              {point.position.x, point.position.y, point.position.z, point.normal.x, point.normal.y, point.normal.z},
		              text) &&
		    ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder5, TheEnginesInverseOfAMatrix)
{
	const auto table = oracle::Table::Load("engine_inverse.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const glm::mat4 matrix {glm::vec4(row.Indexed("m", 0), 0.0f), glm::vec4(row.Indexed("m", 3), 0.0f),
		                        glm::vec4(row.Indexed("m", 6), 0.0f), glm::vec4(row.Indexed("m", 9), 1.0f)};
		const auto inverse = x87::Inverse(matrix);
		std::vector<std::string> names;
		std::vector<float> ours;
		for (glm::length_t r = 0; r < 4; ++r)
		{
			for (glm::length_t c = 0; c < 3; ++c)
			{
				names.push_back("i" + std::to_string((3 * r) + c));
				ours.push_back(inverse[r][c]);
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

TEST(OracleGlmOrder5, ThePointsOfAMeshMovedByAMatrix)
{
	const auto table = oracle::Table::Load("mesh_points.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const glm::mat4 matrix {glm::vec4(row.Indexed("m", 0), 0.0f), glm::vec4(row.Indexed("m", 3), 0.0f),
		                        glm::vec4(row.Indexed("m", 6), 0.0f), glm::vec4(row.Indexed("m", 9), 1.0f)};
		std::vector<std::string> names;
		std::vector<float> ours;
		for (size_t point = 0; point < 4; ++point)
		{
			const auto moved = x87::TransformPoint(matrix, row.Indexed("p", 3 * point), x87::k_MeshPointOrder);
			for (glm::length_t axis = 0; axis < 3; ++axis)
			{
				names.push_back("r" + std::to_string((3 * point) + static_cast<size_t>(axis)));
				ours.push_back(moved[axis]);
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

TEST(OracleGlmOrder5, ASurfaceOfRevolutionSwayed)
{
	constexpr int k_Around = 5;
	constexpr int k_Rings = 4;
	constexpr size_t k_Count = k_Around * k_Rings;
	const auto table = oracle::Table::Load("surface_sway.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		particles::surface::Mesh still;
		for (size_t i = 0; i < k_Count; ++i)
		{
			still.vertices.push_back({.position = row.Indexed("p", 3 * i),
			                          .uv = {row.F("uv" + std::to_string(2 * i)), row.F("uv" + std::to_string((2 * i) + 1))},
			                          .colour = 0,
			                          .specular = 0});
		}
		const particles::surface::Shape shape {.numU = k_Around, .numV = k_Rings};
		particles::surface::Mesh swayed;
		particles::surface::Sway(still, shape, row.F("twist"), row.F("slide"), row.F("sway"), swayed);
		std::vector<std::string> names;
		std::vector<float> ours;
		for (size_t i = 0; i < k_Count; ++i)
		{
			for (glm::length_t axis = 0; axis < 3; ++axis)
			{
				names.push_back("rp" + std::to_string((3 * i) + static_cast<size_t>(axis)));
				ours.push_back(swayed.vertices[i].position[axis]);
			}
			// The game slides only the first texture coordinate and leaves the second as it was in the new list
			names.push_back("ruv" + std::to_string(2 * i));
			ours.push_back(swayed.vertices[i].uv.x);
		}
		std::string text;
		if (!SameBits(row, names, ours, text) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleGlmOrder5, TheNormalsOfASmallMesh)
{
	constexpr size_t k_Points = 6;
	constexpr std::array<uint32_t, 24> k_Faces {0, 1, 2, 0, 2, 3, 1, 2, 3, 3, 4, 5, 0, 0, 4, 2, 5, 1, 4, 4, 4, 5, 3, 0};
	const auto table = oracle::Table::Load("surface_normals.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		particles::surface::Mesh mesh;
		for (size_t i = 0; i < k_Points; ++i)
		{
			mesh.vertices.push_back({.position = row.Indexed("p", 3 * i), .uv = {}, .colour = 0, .specular = 0});
		}
		mesh.indices.assign(k_Faces.begin(), k_Faces.end());
		particles::surface::ComputeNormals(mesh);
		std::vector<std::string> names;
		std::vector<float> ours;
		for (size_t i = 0; i < k_Points; ++i)
		{
			for (glm::length_t axis = 0; axis < 3; ++axis)
			{
				names.push_back("n" + std::to_string((3 * i) + static_cast<size_t>(axis)));
				ours.push_back(mesh.normals[i][axis]);
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
