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
#include <limits>
#include <string>
#include <vector>

#include <L3DFile.h>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "3D/MeshBounds.h"
#include "Graphics/MeshDetail.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

constexpr uint32_t k_Bones = 3;
/// The groups of the case meshes, in order: how many vertices, and their bone
constexpr std::array<std::array<uint32_t, 2>, 3> k_Groups {{{1, 2}, {1, 0}, {2, 1}}};

struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }
};

std::vector<l3d::L3DBone> Bones(const Row& row)
{
	const std::array<uint32_t, k_Bones> parents {std::numeric_limits<uint32_t>::max(), 0, row.U("parent2")};
	std::vector<l3d::L3DBone> bones(k_Bones);
	for (uint32_t b = 0; b < k_Bones; ++b)
	{
		auto& bone = bones[b];
		bone.parent = parents.at(b);
		const auto name = [b](int i) { return "bone" + std::to_string(b) + "_" + std::to_string(i); };
		for (int i = 0; i < 9; ++i)
		{
			bone.orientation.at(static_cast<size_t>(i)) = row.F(name(i));
		}
		bone.position = {row.F(name(9)), row.F(name(10)), row.F(name(11))};
	}
	return bones;
}

/// The box of the case mesh, as openblack bounds a part with bones
AxisAlignedBoundingBox Box(const Row& row)
{
	const auto pose = mesh_bounds::RestPose(Bones(row));
	AxisAlignedBoundingBox box {glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
	uint32_t vertex = 0;
	for (const auto& [count, bone] : k_Groups)
	{
		for (uint32_t j = 0; j < count; ++j, ++vertex)
		{
			const auto prefix = "vertex" + std::to_string(vertex) + "_";
			const glm::vec3 point {row.F(prefix + "x"), row.F(prefix + "y"), row.F(prefix + "z")};
			const auto placed = mesh_bounds::OnBone(pose.at(bone), point);
			box.minima = glm::min(box.minima, placed);
			box.maxima = glm::max(box.maxima, placed);
		}
	}
	return box;
}

} // namespace

TEST(OracleMeshBox, TheBoxOfAMeshWithBonesHasTheGamesBits)
{
	const auto loaded = oracle::Table::Load("mesh_box.txt");
	ASSERT_TRUE(loaded.has_value());
	const auto& table = *loaded;
	ASSERT_GT(table.Rows(), 1000u);
	size_t wrong = 0;
	std::string first;
	for (size_t r = 0; r < table.Rows(); ++r)
	{
		const Row row {table, r};
		const auto box = Box(row);
		const auto centre = box.Center();
		const auto half = box.Size() * 0.5f;
		const std::array<float, 7> ours {
		    centre.x, centre.y, centre.z, half.x, half.y, half.z, graphics::mesh_detail::HalfDiagonal(box.Size())};
		const std::array<std::string, 7> names {"centre_x", "centre_y", "centre_z",     "half_x",
		                                        "half_y",   "half_z",   "half_diagonal"};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			if (std::bit_cast<uint32_t>(ours.at(i)) != row.U(names.at(i)))
			{
				if (wrong++ < 5)
				{
					first += " row " + std::to_string(r) + " " + names.at(i) + " game " + oracle::Hex(row.F(names.at(i))) +
					         " ours " + oracle::Hex(ours.at(i));
				}
			}
		}
	}
	EXPECT_EQ(wrong, 0u) << first;
}
