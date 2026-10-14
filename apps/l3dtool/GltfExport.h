/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace openblack::morph
{
class MorphFile;
} // namespace openblack::morph

/// Writes an L3D mesh as glTF 2.0: the submeshes with their materials and skins (as PNG), the bones and the vertex
/// weights, the morph targets and the animations
namespace openblack::l3dtool
{
/// A mesh that has the same vertices as the base mesh, in the same order. It becomes a morph target of the base mesh.
struct MorphTarget
{
	std::string name;
	std::filesystem::path mesh;
};

struct GltfExportOptions
{
	std::filesystem::path mesh;
	std::vector<MorphTarget> targets;
	/// The animations of the mesh's bones, or none
	const morph::MorphFile* animations {nullptr};
	/// Flip the z axis, to change the game's left-handed axes into the right-handed axes of glTF. A creature then faces +z.
	bool rightHanded {false};
	/// Names and values that go into the extras of the scene
	std::vector<std::pair<std::string, std::string>> notes;
};

/// Writes a .glb file when the output ends in .glb, otherwise a .gltf file. Returns EXIT_SUCCESS or EXIT_FAILURE.
[[nodiscard]] int ExportGltf(const GltfExportOptions& options, const std::filesystem::path& output) noexcept;
} // namespace openblack::l3dtool
