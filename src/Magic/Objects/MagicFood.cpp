/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Pot.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Script/ScriptPlayer.h"
#include "MagicPiles.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
glm::vec3 OnLand(const glm::vec3& position)
{
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                         : position.y;
	return {position.x, ground, position.z};
}
} // namespace

entt::entity objects::CreateMagicResourcePile(const glm::vec3& position, std::optional<PlayerNames> player, ResourceType type,
                                              uint32_t amount, bool allowEmpty)
{
	// The pile's creation step is its rise out of the land (PotArchetype::Create). A magic food pile also clears two
	// shadow flags on its 3D object, so it receives no projected shadow (approximate: the exact meaning of the second
	// flag).
	// Nothing to do here: RenderingSystem.cpp's ReceivesDynamicShadow already covers this for the MagicFood and HandFood
	// pot types, CastsStaticShadow rejects every Pot, and Graphics/ShadowList.cpp's CastsPhysicsShadow rejects every Pot
	// too.
	switch (type)
	{
	case ResourceType::Food:
		return CreateMagicFood(position, player, amount, allowEmpty);
	case ResourceType::Wood:
		return CreateMagicWood(position, player, amount, allowEmpty);
	default:
		return entt::null;
	}
}

entt::entity objects::CreateMagicFood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount,
                                      bool allowEmpty)
{
	// a food pile (pot info 10, the amount, scale 1.0); the scale 0.3 is PotArchetype's
	const auto pile = ecs::archetypes::PotArchetype::Create(OnLand(position), 0.0f, PotInfo::MagicFood,
	                                                        static_cast<int32_t>(amount), allowEmpty);
	if (pile != entt::null)
	{
		// the owner: no player -> the neutral player (ScriptPlayer.h)
		Locator::entitiesRegistry::value().Get<ecs::components::Pot>(pile).owner = player.value_or(k_NeutralPlayerSlot);
		// on creation the pile (type 21, counted as fixed) goes at the tail of its cell's fixed list at once
		ecs::map_cells::InsertMapObject(pile);
	}
	return pile;
}
