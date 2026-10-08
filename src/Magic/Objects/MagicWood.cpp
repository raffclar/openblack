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

entt::entity objects::CreateMagicWood(const glm::vec3& position, std::optional<PlayerNames> player, uint32_t amount,
                                      bool allowEmpty)
{
	// a resource pile of pot info 9 (wood); the scale 0.7 is PotArchetype's. There is no Process and no expiry: the
	// pile stays until it is emptied. The creature AI's "wood pile outside a storage pit" check always answers 1 (not
	// ported).
	// Unlike the food pile, the wood pile keeps whatever shadow settings every pile has (no extra setters): its creation
	// is just the resource pile's, without the cast dynamic shadow / shadow on texture switched off that the food pile
	// adds (MagicFood.cpp). So, like any Pot, it bakes no shadow (RenderingSystem.cpp CastsStaticShadow) but still takes
	// the dynamic one (ReceivesDynamicShadow).
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                         : position.y;
	const auto pile = ecs::archetypes::PotArchetype::Create(glm::vec3(position.x, ground, position.z), 0.0f, PotInfo::MagicWood,
	                                                        static_cast<int32_t>(amount), allowEmpty);
	if (pile != entt::null)
	{
		// the owner, none -> the neutral player (ScriptPlayer.h)
		Locator::entitiesRegistry::value().Get<ecs::components::Pot>(pile).owner = player.value_or(k_NeutralPlayerSlot);
		// on creation the pile (type 21, counted as fixed) goes at the tail of its cell's fixed list at once
		ecs::map_cells::InsertMapObject(pile);
	}
	return pile;
}
