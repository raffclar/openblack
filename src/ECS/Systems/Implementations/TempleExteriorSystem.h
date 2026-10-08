/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <L3DFile.h>

#include "3D/TempleExteriorMorph.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::graphics
{
class L3DMesh;
} // namespace openblack::graphics

namespace openblack::ecs::systems
{

class TempleExteriorSystem final: public TempleExteriorSystemInterface
{
public:
	/// Data/Citadel/OutsideMeshes's files by their names in lower case
	using Files = std::unordered_map<std::string, std::filesystem::path>;

	void Create(entt::entity heart) override;
	[[nodiscard]] bool Step(entt::entity heart, float alignmentTarget, float sizeTarget) override;
	void Blend(entt::entity heart) override;
	[[nodiscard]] std::optional<TempleExteriorMorph::State> GetLook(entt::entity heart) const override;
	void SnapAlignment(entt::entity heart, float alignmentTarget) override;

private:
	/// The first temple's file, whose shape and skins every temple's own mesh has, and the fifteen it is blended from,
	/// read once from Data/Citadel/OutsideMeshes through the byte cache. False when one could not be read
	bool LoadMeshFiles();
	/// The texture between the two looks about the alignment, of the owner's set, into the mesh's skin
	void BlendTexture(graphics::L3DMesh& mesh, float alignment, PlayerNames owner);

	bool _triedLoading {false};
	bool _textureMissingLogged {false};
	Files _files;
	std::unique_ptr<l3d::L3DFile> _firstTemple;
	std::array<std::unique_ptr<l3d::L3DFile>, TempleExteriorMorph::k_Sizes * TempleExteriorMorph::k_Stages> _temples;
	/// Kept between blends so that each does not allocate them again
	std::vector<l3d::L3DVertex> _vertices;
	std::vector<uint16_t> _texels;
};

} // namespace openblack::ecs::systems
