/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellStormAndTornado.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <string>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Spell.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "SpellClasses.h"
#include "StormDebugHooks.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// The storm spells, each new one first (Locator::spellSystem)
std::vector<entt::entity>& StormSpells()
{
	if (!Locator::spellSystem::has_value())
	{
		std::fputs("magic::spell_storm: no spell system in the locator (Locator::spellSystem)\n", stderr);
		std::abort();
	}
	return Locator::spellSystem::value().StormSpells();
}

const GMagicStormAndTornadoInfo& StormInfoOf(entt::entity spell)
{
	// the magic info of the spell's magic type
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicStormAndTornadoInfo>(Locator::infoConstants::value(), type);
}

SpellStormData& DataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellStormData>(spell); data != nullptr)
	{
		return *data;
	}
	// a new storm: both fields zeroed, into the list first
	auto& spells = StormSpells();
	spells.insert(spells.begin(), spell);
	return registry.Assign<SpellStormData>(spell);
}

/// The storm is cast at a position
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const auto& stormInfo = StormInfoOf(spell);
	auto& data = DataOf(spell);
	// the radius (castData's magnitude) clamped to [minRadius, maxRadius] and written back. The original reads it
	// directly; the fallback for a NULL castData (radius 40, no chants, time -1) is openblack's (inferred)
	SpellCastData fallback {40.0f, 0.0f, -1.0f, -1};
	SpellCastData* cast = castData != nullptr ? castData : &fallback;
	cast->magnitude = ClampStormRadius(stormInfo.minRadius, stormInfo.maxRadius, cast->magnitude);
	const int result = base::InitWithPos(spell, position, cast, info);
	// the swirl at the hand: SF_StormCast (106) at pos with the process info's direction, strength 1.0, then its
	// magnitude is the spell's
	const auto file = psys::ParticleTypeFile(ParticleType::StormCast);
	if (!file.empty())
	{
		data.castEffect = psys::manager::StartForSpell(std::string(file), ToWorld(position), info.direction, 1.0f, nullptr);
		if (auto* effect = psys::manager::Find(data.castEffect); effect != nullptr)
		{
			effect->SetMagnitude(Locator::entitiesRegistry::value().Get<const Spell>(spell).magnitude);
		}
	}
	if (TraceEnabled())
	{
		const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Spell trace: spell {} SpellStormAndTornado::InitWithPos at ({:.1f}, {:.1f}): radius {:.1f} "
		    "(min {:.0f}, max {:.0f}), upkeep {:.2f}/turn, rain {:.0f}, cast psys {}",
		    static_cast<uint32_t>(spell), position.x, position.z, component.magnitude, stormInfo.minRadius, stormInfo.maxRadius,
		    StormCostToMaintain(EffectInfoOf(spell).costPerGameTurn, component.magnitude, stormInfo.radiusForNormalCost),
		    stormInfo.rainAmount, data.castEffect);
	}
	return result;
}

/// The storm's turn
int Process(entt::entity spell)
{
	auto& data = DataOf(spell);
	// a water reaction that is no longer available is forgotten (the same check as the water spell's turn)
	if (data.waterReaction != 0 && !reactions::IsAvailable(reactions::Find(data.waterReaction)))
	{
		data.waterReaction = 0;
	}
	if (data.castEffect != 0)
	{
		// a process info of zeros with strength 1 and enabled, filled in by the creator (UpdateSpellInfo), strength 1
		// again; no creator, or the effect finished: it goes
		const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
		psys::ProcessInfo info {
		    .interfacePos = glm::vec3(0.0f),
		    .handPos = glm::vec3(0.0f),
		    .cameraForward = glm::vec3(0.0f),
		    .direction = glm::vec3(0.0f),
		    .power = 1.0f,
		    .curl = 0.0f,
		    .enabled = true,
		};
		const bool hasCreator = component.creator.kind != SpellCreator::Kind::None;
		if (hasCreator)
		{
			creator::UpdateSpellInfo(component.creator, spell, info);
		}
		info.power = 1.0f;
		if (!hasCreator || !psys::manager::ProcessForSpell(data.castEffect, info, static_cast<float>(k_TurnMs) * 0.001f))
		{
			psys::manager::Delete(data.castEffect);
			data.castEffect = 0;
		}
	}
	storm_debug::OnTurn(spell); // OPENBLACK_TEST_STORM_SHOT, OPENBLACK_STORM_TRACE
	return base::Process(spell);
}

/// The storm's upkeep
float CostToMaintain(entt::entity spell)
{
	const auto& component = Locator::entitiesRegistry::value().Get<const Spell>(spell);
	return StormCostToMaintain(base::CalculateCostToMaintain(spell), component.magnitude,
	                           StormInfoOf(spell).radiusForNormalCost);
}

/// The storm's own part of the deletion: out of the list, then (after the base spell's part) the cast PSys deleted
void StormToBeDeleted(entt::entity spell)
{
	std::erase(StormSpells(), spell);
	if (auto* data = Locator::entitiesRegistry::value().TryGet<SpellStormData>(spell); data != nullptr)
	{
		if (data->castEffect != 0)
		{
			psys::manager::Delete(data->castEffect);
			data->castEffect = 0;
		}
	}
}
} // namespace

float magic::ClampStormRadius(float minRadius, float maxRadius, float radius)
{
	// keep the radius only when below max, then only when above min
	if (!(radius < maxRadius))
	{
		radius = maxRadius;
	}
	if (!(radius > minRadius))
	{
		radius = minRadius;
	}
	return radius;
}

float magic::StormCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost)
{
	// the square of magnitude / radiusForNormalCost times the base spell's upkeep
	const float k = magnitude / radiusForNormalCost;
	return baseCost * (k * k);
}

void spell_storm::ReactToRainOnFire(const glm::vec3& objectPosition)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto spell : StormSpells())
	{
		if (!registry.Valid(spell) || !registry.AllOf<Spell, SpellStormData>(spell))
		{
			continue;
		}
		auto& data = registry.Get<SpellStormData>(spell);
		if (data.waterReaction != 0)
		{
			continue;
		}
		const auto& component = registry.Get<const Spell>(spell);
		// the spell's 2D radius (its magnitude) must be above the distance to the spell's position
		const float distance = gutils::GetDistanceInMetres(objectPosition, component.position);
		if (component.magnitude > distance)
		{
			data.waterReaction =
			    reactions::CreateReaction(spell, Reaction::ReactToMagicWaterPuttingOutFire, component.player, true);
			return;
		}
	}
}

const std::vector<entt::entity>& spell_storm::Spells()
{
	return StormSpells();
}

uint32_t spell_storm::CastEffectOf(entt::entity spell)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const SpellStormData>(spell);
	return data != nullptr ? data->castEffect : 0;
}

void spell_storm::Clear()
{
	StormSpells().clear();
	storm_debug::Reset();
}

void openblack::magic::RegisterStormSpell()
{
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = CostToMaintain,
	                    .closeDown = base::CloseDown, // the base spell's
	                    .toBeDeleted = StormToBeDeleted,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::StormAndTornado, ops);
}
