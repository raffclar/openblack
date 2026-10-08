/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellResource.h"

#include <cmath>

#include <spdlog/spdlog.h>

#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
const GMagicResourceInfo& ResourceInfoOf(entt::entity spell)
{
	// the spell's magic info as a resource record
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicResourceInfo>(Locator::infoConstants::value(), type);
}

SpellResourceData& DataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellResourceData>(spell); data != nullptr)
	{
		return *data;
	}
	return registry.Assign<SpellResourceData>(spell);
}

int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	// the first event is not done yet (cleared before the effect can send its events)
	DataOf(spell).firstDone = false;
	return base::InitWithPos(spell, position, castData, info);
}

/// A grain or log lands: pay for it and put the resource down
int SpellEvent(entt::entity entity, const psys::SpellEventInfo& event)
{
	// the default effect first (FOOD / WOOD EffectValues: only alignment 1) and the reaction LOOK_AT_NICE_SPELL
	const int result = spell_event::SpellEvent(entity, event);
	auto& spell = Locator::entitiesRegistry::value().Get<Spell>(entity);
	if (event.type == psys::SpellEventInfo::Type::Started || spell.closedDown)
	{
		return result;
	}
	const auto& info = ResourceInfoOf(entity);
	// only x, z
	const glm::vec3 position(event.position.x, 0.0f, event.position.z);
	auto& data = DataOf(entity);
	const auto cost = ResourceEvent(info, data.firstDone);
	// pay costPerUnit x n, not forced: the result is not used
	chants::PayFor(spell, ChantContextOf(entity), cost.chants, false);
	data.firstDone = true;
	const bool inBounds = cast_rules::InBounds(position);
	const bool dryLand = inBounds && ecs::pot_resource::IsDryLand(position);
	const float strength = inBounds && dryLand ? GetSpellStrength(entity) : 0.0f;
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Spell trace: spell {} SpellResource event {} at ({:.1f}, {:.1f}): {} units, paid {:.0f}, chants left {:.1f}, "
		    "in bounds {}, dry land {}, strength {:.3f}",
		    static_cast<uint32_t>(entity), static_cast<int>(event.type), position.x, position.z, cost.units, cost.chants,
		    spell.chants, inBounds, dryLand, strength);
	}
	if (!inBounds || !dryLand || !(strength > 0.0f))
	{
		return result; // paid for, but nothing lands (water, the edge of the map, no chants left)
	}
	const auto amount = static_cast<uint32_t>(GetTribalPower(entity) * static_cast<float>(cost.units)); // truncated
	// the dropper is the spell's player's leader interface (only a human player has one).
	// (inferred: single player, so every human player is the local interface, and it is my interface). It carries the
	// interface's record of who owned what its hand last picked up
	ecs::pot_resource::Dropper dropper;
	if (spell.hasPlayer && players::IsHuman(spell.player))
	{
		dropper = {.hasInterface = true,
		           .player = spell.player,
		           .isMyInterface = true,
		           .sourceOwner =
		               Locator::handSystem::has_value() ? Locator::handSystem::value().GetSourceOwner() : std::nullopt};
	}
	if (info.resourceType == ResourceType::Food)
	{
		// AddResourceToPos(pos, dropper, FOOD, amount, 0, poisoned == 1): the "poisoned" flag of FOOD_PU1 goes to the
		// last argument, which is SetSpeedUp (PILEFOOD_SPEEDUP sparkles), not SetPoisoned
		ecs::pot_resource::AddResourceToPos(position, dropper, ResourceType::Food, amount, false, info.speedUp == 1);
	}
	else
	{
		ecs::pot_resource::AddResourceToPos(position, dropper, ResourceType::Wood, amount, false, false);
	}
	return result;
}

bool HasEnoughChantsAndLifeForRecast(entt::entity spell)
{
	return HasEnoughChantsForResourceRecast(ResourceInfoOf(spell),
	                                        Locator::entitiesRegistry::value().Get<const Spell>(spell).chants);
}
} // namespace

ResourceEventCost magic::ResourceEvent(const GMagicResourceInfo& info, bool firstDone)
{
	const uint32_t units = firstDone ? info.resourceAmountPerEvent : info.resourceAmountFirstEvent;
	// the integer product as a 64-bit value, then a float
	return {units, static_cast<float>(static_cast<int64_t>(info.costPerUnit) * static_cast<int64_t>(units))};
}

bool magic::HasEnoughChantsForResourceRecast(const GMagicResourceInfo& info, float chants)
{
	return static_cast<float>(static_cast<int32_t>(info.costPerUnit * info.resourceAmountFirstEvent)) <= chants;
}

void openblack::magic::RegisterResourceSpell()
{
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = base::Process, // no Process override: the plain spell process
	                    .spellEvent = SpellEvent,
	                    .costToMaintain = base::CalculateCostToMaintain,
	                    .closeDown = base::CloseDown,
	                    .toBeDeleted = nullptr,
	                    .hasEnoughChantsForRecast = HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::Resource, ops);
}
