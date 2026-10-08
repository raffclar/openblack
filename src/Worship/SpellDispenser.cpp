/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellDispenser.h"

#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The dispenser's own particle effect
constexpr auto k_DispenserParticle = static_cast<ParticleType>(0x90);
/// The orb at the dispenser's height x 1.2
constexpr float k_OrbHeight = 1.2f;
/// The spot visual for a new orb; 0x1B for a seed given to a dispenser
constexpr int k_OrbSpotVisual = 9;
constexpr int k_SeedSpotVisual = 0x1B;

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

const GAbodeInfo* AbodeInfoOf(entt::entity dispenser)
{
	const auto* abode = Registry().TryGet<const ecs::components::Abode>(dispenser);
	if (abode == nullptr)
	{
		return nullptr;
	}
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber == abode->type)
		{
			return &info;
		}
	}
	return nullptr;
}

/// 2 x the mesh's half height x scale
float HeightOf(entt::entity entity)
{
	return ecs::object::GetHeight(entity);
}

/// Where the dispenser makes its orb
glm::vec3 OrbPosition(entt::entity dispenser)
{
	return Registry().Get<const Transform>(dispenser).position + glm::vec3(0.0f, HeightOf(dispenser) * k_OrbHeight, 0.0f);
}

/// The orb touches the dispenser (within 0.001): (inferred) the orb still stands where the dispenser made it.
/// (inferred): the touch test itself is not ported; the extra 0.5 m and the xz-only test are openblack's
bool OrbStillThere(entt::entity dispenser, entt::entity orb)
{
	auto& registry = Registry();
	if (orb == entt::null || !registry.Valid(orb) || !registry.AllOf<OneOffSpellSeed, Transform>(orb))
	{
		return false;
	}
	const auto& at = registry.Get<const Transform>(orb).position;
	const auto spawn = OrbPosition(dispenser);
	return glm::distance(glm::vec2(at.x, at.z), glm::vec2(spawn.x, spawn.z)) <= 0.001f + 0.5f;
}

/// No town -> the head of the first player's town list (the oldest town of PLAYER_ONE, new towns go at the tail;
/// ecs::map_cells::TownsOf)
uint32_t TownIdFor(int townId)
{
	if (townId >= 0)
	{
		// the script's number -> the town's key; none -> no town
		const auto town = ecs::town_queries::FindTownWithID(static_cast<uint32_t>(townId));
		return town != entt::null ? Registry().Get<const Town>(town).id : 0xFFFFFFFFu;
	}
	const auto towns = ecs::map_cells::TownsOf(PlayerNames::PLAYER_ONE);
	return towns.empty() ? 0xFFFFFFFFu : Registry().Get<const Town>(towns.front()).id;
}
} // namespace

entt::entity dispenser::Create(const glm::vec3& position, AbodeInfo type, int townId, float yAngle, float scale)
{
	auto& registry = Registry();
	const auto entity = ecs::archetypes::AbodeArchetype::Create(TownIdFor(townId), position, type, yAngle, scale, 0, 0);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& component = registry.Assign<SpellDispenser>(entity); // all 0
	const auto* info = AbodeInfoOf(entity);
	component.period = info != nullptr ? static_cast<uint32_t>(info->timeEachMobileObjectTakesToProduce) : 0;
	component.magicType = MagicType::None;
	if (component.period == 0)
	{
		SetActive(entity, false);
	}
	// Its effect at the land under it
	const auto file = psys::ParticleTypeFile(k_DispenserParticle);
	if (!file.empty())
	{
		auto ground = registry.Get<const Transform>(entity).position;
		ground.y = GroundAt(ground);
		registry.Get<SpellDispenser>(entity).psys = psys::manager::Start(std::string(file), ground, 1.0f);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: spell dispenser {} ({}) at ({:.1f}, {:.1f})",
	                   static_cast<uint32_t>(entity), static_cast<int>(type), position.x, position.z);
	return entity;
}

bool dispenser::IsDispenser(entt::entity entity)
{
	return entity != entt::null && ecs::IsAvailable(entity) && Registry().AllOf<SpellDispenser>(entity);
}

void dispenser::SetActive(entt::entity dispenser, bool active)
{
	if (!IsDispenser(dispenser))
	{
		return;
	}
	Registry().Get<SpellDispenser>(dispenser).active = active;
	if (active)
	{
		CreateOneOffSpellSeed(dispenser);
	}
}

void dispenser::SetMagicProperties(entt::entity dispenser, MagicType magic, float seconds)
{
	if (!IsDispenser(dispenser))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_MAGIC_PROPERTIES: Thing must be a dispenser");
		return;
	}
	auto& component = Registry().Get<SpellDispenser>(dispenser);
	component.magicType = magic;
	if (seconds > 0.0f)
	{
		// The seconds as game turns, truncated
		component.period = static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
	}
	else
	{
		const auto* info = AbodeInfoOf(dispenser);
		component.period = info != nullptr ? static_cast<uint32_t>(info->timeEachMobileObjectTakesToProduce) : 0;
	}
	if (component.period == 0)
	{
		SetActive(dispenser, false);
	}
}

void dispenser::SetTimerTime(entt::entity dispenser, float seconds)
{
	if (!IsDispenser(dispenser))
	{
		return;
	}
	// The seconds as game turns, truncated, kept when > 0 unsigned
	const auto period = static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
	if (period > 0)
	{
		Registry().Get<SpellDispenser>(dispenser).period = period;
	}
}

void dispenser::SetMagicAndPeriod(entt::entity dispenser, MagicType magic, uint32_t periodTurns)
{
	if (!IsDispenser(dispenser))
	{
		return;
	}
	Registry().Get<SpellDispenser>(dispenser).magicType = magic;
	SetActive(dispenser, true);
	Registry().Get<SpellDispenser>(dispenser).period = periodTurns;
	if (periodTurns == 0)
	{
		SetActive(dispenser, false);
	}
}

void dispenser::Process(entt::entity entity)
{
	auto& registry = Registry();
	// (openblack, guard) the town pass walks the town's abodes; a dispenser on the dead list is out of it in the original
	if (!IsDispenser(entity) || registry.AllOf<Unavailable>(entity))
	{
		return;
	}
	auto& component = registry.Get<SpellDispenser>(entity);
	const bool orbStillThere = component.oneShot != entt::null && OrbStillThere(entity, component.oneShot);
	if (StepTurn(component, orbStillThere) == TurnStep::MakeOrb)
	{
		CreateOneOffSpellSeed(entity);
	}
}

dispenser::TurnStep dispenser::StepTurn(SpellDispenser& component, bool orbStillThere)
{
	// An orb available and touching it (within 0.001) -> wait; else no orb and the tick back to 0
	if (component.oneShot != entt::null)
	{
		if (orbStillThere)
		{
			return TurnStep::Wait;
		}
		component.oneShot = entt::null;
		component.tick = 0;
		return TurnStep::OrbGone;
	}
	// Active, with a magic, built and repaired (openblack's abodes are both)
	if (!component.active || component.magicType == MagicType::None)
	{
		return TurnStep::Idle;
	}
	// One more tick; at the period -> an orb
	if (++component.tick >= component.period)
	{
		return TurnStep::MakeOrb;
	}
	return TurnStep::Count;
}

entt::entity dispenser::CreateOneOffSpellSeed(entt::entity dispenser)
{
	auto& registry = Registry();
	auto& component = registry.Get<SpellDispenser>(dispenser);
	const auto& tables = Locator::infoConstants::value();
	// The seed the magic's info names in the running game: the first seed of that magic type. With none the one-off seed
	// refuses it, no orb is made and the tick stays as it is.
	const auto seed = magic::GetSpellSeedOfMagicInfo(tables, component.magicType);
	if (static_cast<int>(seed) < 0)
	{
		return entt::null;
	}
	const int powerUp = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(tables, seed), component.magicType).level;
	const auto position = OrbPosition(dispenser);
	const auto orb = magic::one_off::Create(position, seed, powerUp, 1.0f);
	registry.Get<SpellDispenser>(dispenser).oneShot = orb;
	if (orb == entt::null)
	{
		return entt::null;
	}
	psys::manager::CreateSpotVisual(k_OrbSpotVisual, position, 0.0f, entt::null);
	registry.Get<SpellDispenser>(dispenser).tick = 0;
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: dispenser {} makes orb {} (seed {}, pu {})",
		                   static_cast<uint32_t>(dispenser), static_cast<uint32_t>(orb), static_cast<int>(seed), powerUp);
	}
	return orb;
}

bool dispenser::ApplySeed(entt::entity dispenser, entt::entity seedEntity)
{
	auto& registry = Registry();
	if (!IsDispenser(dispenser) || !registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return false;
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	// Only a seed that has not cast; (inferred): taken as openblack's lastMagic
	if (seed.lastMagic != MagicType::None)
	{
		return false;
	}
	const auto position = OrbPosition(dispenser);
	magic::one_off::Create(position, seed.seedType, seed.powerUp, 1.0f);
	magic::seed::SetChantStore(seed, 0.0f);
	magic::seed::ToBeDeleted(seedEntity);
	psys::manager::CreateSpotVisual(k_SeedSpotVisual, position, 0.0f, entt::null);
	return true;
}
