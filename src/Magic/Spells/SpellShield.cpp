/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellShield.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MapShield.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// The shield spells, newest first (Locator::spellSystem)
std::vector<entt::entity>& ShieldSpells()
{
	if (!Locator::spellSystem::has_value())
	{
		std::fputs("magic::spell_shield: no spell system in the locator (Locator::spellSystem)\n", stderr);
		std::abort();
	}
	return Locator::spellSystem::value().ShieldSpells();
}

constexpr float k_ReactionRadiusAdd = 30.0f; ///< Added to the shield's radius for its reaction
constexpr float k_TownRadius = 500.0f;       ///< The nearest town search radius at the cast

const GMagicShieldInfo& ShieldInfoOf(entt::entity spell)
{
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicShieldInfo>(Locator::infoConstants::value(), type);
}

SpellShieldData& DataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellShieldData>(spell); data != nullptr)
	{
		return *data;
	}
	// first use: into the list, the reactions and the town cleared
	auto& spells = ShieldSpells();
	spells.insert(spells.begin(), spell);
	return registry.Assign<SpellShieldData>(spell);
}

/// Every player's (and the neutral one's) towns in their lists' order, the
/// nearest strictly closer than r (ecs::map_cells)
entt::entity NearestTown(const glm::vec3& position, float radius)
{
	return ecs::map_cells::GetNearestTown(map_coords::FromMetres(glm::vec2(position.x, position.z)), radius);
}

/// The cast: radius, reactions, town, anti-influence rings and the world shield
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const auto& shieldInfo = ShieldInfoOf(spell);
	auto& data = DataOf(spell);
	// the cast radius clamped to [minRadius, maxRadius] and written back. The original reads the cast data
	// directly; the fallback for a null castData (radius 40, no chants, time -1) is openblack's (inferred)
	SpellCastData fallback {40.0f, 0.0f, -1.0f, -1};
	SpellCastData* cast = castData != nullptr ? castData : &fallback;
	const float radius = ClampShieldRadius(shieldInfo, cast->magnitude);
	cast->magnitude = radius;
	const int result = base::InitWithPos(spell, position, cast, info);
	auto& component = Locator::entitiesRegistry::value().Get<Spell>(spell);
	// REACTION_REACT_TO_MAGIC_SHIELD (13) by the spell's player, radius r + 30
	data.shieldReaction = reactions::CreateReaction(spell, Reaction::ReactToMagicShield, component.player, false);
	reactions::SetRadius(data.shieldReaction, radius + k_ReactionRadiusAdd);
	data.town = NearestTown(position, k_TownRadius);
	// an anti-influence ring of the spell's magnitude for every other active player (the seven players' slots in use;
	// approximate: here the players the land made an entity for)
	const float magnitude = component.magnitude;
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::NEUTRAL); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		if (players::EntityOf(player) == entt::null || (component.hasPlayer && component.player == player))
		{
			continue;
		}
		const auto ring = influence::CreateRing(position, player, magnitude, true);
		if (ring != entt::null)
		{
			data.rings.insert(data.rings.begin(), ring);
		}
	}
	// the world shield onto the spell's object list
	if (const auto shield = map_shield::Create(position, spell, radius); shield != entt::null)
	{
		spell_objects::Add(spell, shield);
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Spell trace: spell {} SpellShield::InitWithPos at ({:.1f}, {:.1f}): radius {:.1f}, {} anti rings, "
		    "reaction {}, town {}, upkeep {:.2f}/turn",
		    static_cast<uint32_t>(spell), position.x, position.z, radius, data.rings.size(), data.shieldReaction,
		    data.town == entt::null ? -1 : static_cast<int>(data.town),
		    ShieldCostToMaintain(EffectInfoOf(spell).costPerGameTurn, magnitude, shieldInfo.radiusForNormalCost));
	}
	return result;
}

/// The struck reaction goes once it is not available, then the spell-with-objects process
int Process(entt::entity spell)
{
	auto& data = DataOf(spell);
	if (data.struckReaction != 0 && reactions::Find(data.struckReaction) == nullptr)
	{
		data.struckReaction = 0;
	}
	return spell_objects::Process(spell);
}

float CostToMaintain(entt::entity spell)
{
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	return ShieldCostToMaintain(base::CalculateCostToMaintain(spell), component.magnitude,
	                            ShieldInfoOf(spell).radiusForNormalCost);
}

/// The base close down, then SetDying on every object not already going
void ShieldCloseDown(entt::entity spell)
{
	base::CloseDown(spell);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		if (registry.Valid(object))
		{
			map_shield::SetDying(object);
		}
	}
}

/// The class part; the base spell's deletion runs after it
void ToBeDeleted(entt::entity spell)
{
	std::erase(ShieldSpells(), spell);
	reactions::RemoveAllReactionsInitiatedByObject(spell);
	auto* data = Locator::entitiesRegistry::value().TryGet<SpellShieldData>(spell);
	if (data != nullptr)
	{
		data->shieldReaction = 0;
	}
	// the close down, the object list emptied
	magic::CloseDown(spell);
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		spell_objects::Remove(spell, object);
	}
	// then every influence ring deleted
	if (data != nullptr)
	{
		for (const auto ring : data->rings)
		{
			influence::DeleteRing(ring);
		}
		data->rings.clear();
	}
}
} // namespace

float magic::ClampShieldRadius(const GMagicShieldInfo& info, float radius)
{
	// r = max unless max > r; then r = min unless min < r
	if (!(info.maxRadius > radius))
	{
		radius = info.maxRadius;
	}
	if (!(info.minRadius < radius))
	{
		radius = info.minRadius;
	}
	return radius;
}

float magic::ShieldCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost)
{
	const float k = magnitude / radiusForNormalCost;
	return baseCost * (k * k);
}

void spell_shield::UpdateStruckReaction(entt::entity spell)
{
	auto& data = DataOf(spell);
	if (data.struckReaction == 0)
	{
		const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
		data.struckReaction = reactions::CreateReaction(spell, Reaction::ReactToMagicShieldStruck, component.player, false);
		return;
	}
	reactions::Stamp(data.struckReaction); // the reaction's turn = the game turn
}

void spell_shield::SetUpDestroyedReaction(entt::entity spell)
{
	auto& data = DataOf(spell);
	reactions::RemoveAllReactionsOfTypeInitiatedBy(spell, Reaction::ReactToMagicShield);
	data.shieldReaction = 0;
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	reactions::CreateReaction(spell, Reaction::ReactToMagicShieldDestroyed, component.player, false);
}

bool spell_shield::IsUnder(entt::entity spell, const glm::vec3& point, float margin)
{
	// the spell's radius (its magnitude) - margin, against the distance to castPos
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	const float distance = gutils::GetDistanceInMetres(point, component.castPos);
	return distance < component.magnitude - margin;
}

entt::entity spell_shield::FindShieldAt(const glm::vec3& point, uint32_t mask)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto spell : ShieldSpells())
	{
		if (!registry.Valid(spell) || !registry.AllOf<Spell>(spell))
		{
			continue;
		}
		const auto& component = registry.Get<const Spell>(spell);
		const uint32_t bit =
		    component.magicType == MagicType::Shield ? 2u : (component.magicType == MagicType::PhysicalShield ? 1u : 0u);
		if ((bit | mask) == 0)
		{
			continue;
		}
		// the distance from originalCastPos (not castPos) to the point, then the 2D radius (the magnitude):
		// radius > distance
		const float distance = gutils::GetDistanceInMetres(component.originalCastPos, point);
		if (component.magnitude > distance)
		{
			return spell;
		}
	}
	return entt::null;
}

entt::entity spell_shield::TownOf(entt::entity spell)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const SpellShieldData>(spell);
	return data != nullptr ? data->town : entt::null;
}

const std::vector<entt::entity>& spell_shield::Spells()
{
	return ShieldSpells();
}

void spell_shield::Clear()
{
	ShieldSpells().clear();
}

void openblack::magic::RegisterShieldSpell()
{
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = CostToMaintain,
	                    .closeDown = ShieldCloseDown, // (a plain CloseDown here would find magic::CloseDown: endless recursion)
	                    .toBeDeleted = ToBeDeleted,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType,
	                    .updateStruckReaction = spell_shield::UpdateStruckReaction,
	                    .setUpDestroyedReaction = spell_shield::SetUpDestroyedReaction};
	RegisterOps(SpellClass::Shield, ops);
}
