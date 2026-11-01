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
		};
		std::vector<Skin> skins;
		/// A temple whose own mesh is not in the cache. The threads prepare its mesh from its blended shape after the
		/// other temples (see FinishMorphs).
		struct Load
		{
			entt::id_type id;
			std::string name;
			/// The index of its shape in shapes
			size_t shape;
			float alignment;
			PlayerNames owner;
		};
		std::vector<Load> loads;
	};

	/// Blends the temple's own mesh from the four about its size and alignment, and its texture between
	/// the two looks about its alignment, of its player's set
	void Morph(entt::entity entity, components::Mesh& mesh, const components::TempleExterior& exterior, PlayerNames owner,
	           MadeBlends& made);
	/// Makes the own meshes of the temples that Morph could not find in the cache, in the order of the temples. The threads
	/// do the work on the processor at the same time. Then this thread makes the objects on the graphics device.
	void FinishMorphs(MadeBlends& made);
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
