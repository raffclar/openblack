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

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "3D/L3DMesh.h"
#include "3D/TempleExteriorMorph.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
class WorkerPool;
}

namespace openblack::ecs::components
{
struct Mesh;
struct TempleExterior;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class TempleExteriorSystem final: public TempleExteriorSystemInterface
{
public:
	TempleExteriorSystem();
	~TempleExteriorSystem() override;
	void UpdateTurn() override;
	void NoteSizeTarget(PlayerNames player, std::optional<float> share) override;
	void FindAfterLoad() override;
	void ForgetLand() override;

private:
	/// The blends made in one call, which the temples with the same look or the same texture use again. Each blend
	/// comes only from the look and the meshes and images of the cache, which do not change in the call.
	struct MadeBlends
	{
		/// The blended shape of each look
		std::vector<std::pair<glm::vec2, l3d::L3DFile>> shapes;
		struct Skin
		{
			TempleExteriorMorph::TextureBlend texture;
			uint32_t set;
			std::vector<uint16_t> texels;
			/// The texels prepared for the first temple mesh that took them, which the meshes that fit use again
			std::optional<graphics::L3DMesh::PreparedSkin> prepared;
		};
		std::vector<Skin> skins;
		/// A temple whose blend changes. It gets its mesh after the other temples, which the temples of the same blend
		/// share (see FinishMorphs).
		struct Change
		{
			entt::id_type id;
			std::string name;
			/// The index of its shape in shapes
			size_t shape;
			float alignment;
			PlayerNames owner;
		};
		std::vector<Change> changes;
	};

	/// Blends the temple's own mesh from the four about its size and alignment, and its texture between
	/// the two looks about its alignment, of its player's set
	void Morph(entt::entity entity, components::Mesh& mesh, const components::TempleExterior& exterior, PlayerNames owner,
	           MadeBlends& made);
	/// Gives the temples that Morph changed their meshes: one mesh for each blend, which the temples of that blend share.
	/// A blend keeps a mesh that only its temples have, and changes it. For each other blend, the threads do the work of
	/// a new mesh on the processor, and then this thread makes the objects on the graphics device.
	void FinishMorphs(MadeBlends& made);
	/// Takes the temple's own mesh, the mesh of its worship sites and its shape out of the cache
	static void Unbind(entt::id_type id);
	/// Gives the temple the mesh and the shape that the cache keeps for the other temple
	static void Share(entt::id_type id, entt::id_type source);
	/// Keeps the temple's blended shape in the cache, and gives its own mesh the texture between the two looks about its
	/// alignment, of its player's set. The shape is a copy of the blend that the threads made, or nothing for a copy here.
	static void Dress(entt::id_type id, const l3d::L3DFile& blended, std::shared_ptr<l3d::L3DFile> copy, float alignment,
	                  PlayerNames owner, MadeBlends& made);
	/// The threads that prepare the meshes, made at the first use
	[[nodiscard]] WorkerPool& Workers();

	std::unique_ptr<WorkerPool> _workers;

	/// Of each temple's own mesh in the cache, by its id, the blend that this computer last made for it
	std::unordered_map<entt::id_type, TempleExteriorMorph::Blend> _blends;
};

} // namespace openblack::ecs::systems
