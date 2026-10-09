/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VortexSystem.h"

#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/VortexArchetype.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/VortexRules.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellLink.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::Vortex;

namespace
{
/// The map's cells run 0 to 511 along each side
bool OnMap(glm::ivec2 cell)
{
	return cell.x >= 0 && cell.y >= 0 && cell.x < LandIslandInterface::k_MapCellsPerSide &&
	       cell.y < LandIslandInterface::k_MapCellsPerSide;
}

/// Seconds since a vortex's state began, now
double SecondsInState(const Vortex& vortex)
{
	const auto& time = Locator::time::value();
	const auto turn = time.GetTurn();
	const auto turns = turn >= vortex.stateStartTurn ? turn - vortex.stateStartTurn : 0;
	return vortex::ElapsedSeconds(turns, time.GetTurnFraction(),
	                              static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
}

const GVortexInfo* InfoOf(VortexType type)
{
	const auto& tables = Locator::infoConstants::value().vortex;
	const auto row = static_cast<size_t>(type);
	return row < tables.size() ? &tables.at(row) : nullptr;
}

/// The vortex's swirl and its effect over the land are stepped by the vortex itself; no miracle hears from them
class NoMiracle final: public particles::SpellSink
{
public:
	bool SpellEvent(const particles::SpellEventInfo& /*event*/) override { return false; }
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
};
NoMiracle g_noMiracle;
} // namespace

entt::entity VortexSystem::Create(glm::vec3 position, VortexType type, float altitude)
{
	const auto* info = InfoOf(type);
	if (info == nullptr)
	{
		return entt::null;
	}
	const auto ground = Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	const glm::vec3 centre {position.x, ground + altitude, position.z};
	const auto entity = archetypes::VortexArchetype::Create(centre, type, info->initialState, Locator::time::value().GetTurn());
	StartEffects(Locator::entitiesRegistry::value().Get<Vortex>(entity));
	return entity;
}

void VortexSystem::StartEffects(Vortex& vortex)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	const auto* info = InfoOf(vortex.type);
	auto& particles = Locator::particleSystem::value();
	// The swirl starts on the ground under the middle, the others at the middle itself
	const auto ground = Locator::terrainSystem::value().GetHeightAt({vortex.centre.x, vortex.centre.z});
	const glm::vec3 onGround {vortex.centre.x, ground, vortex.centre.z};
	const auto start = [&particles](ParticleType type, glm::vec3 origin) {
		return type == ParticleType::None ? ParticleSystemInterface::k_NoEffect : particles.Start(type, origin, 1.0f);
	};
	const auto startStepped = [&particles](ParticleType type, glm::vec3 origin) {
		return type == ParticleType::None ? ParticleSystemInterface::k_NoEffect
		                                  : particles.StartForSpell(type, origin, glm::vec3(0.0f), 1.0f, g_noMiracle);
	};
	vortex.objectMoverEffect = start(info->particleTypeObjectMover, vortex.centre);
	vortex.beforeLandEffect = startStepped(info->particleTypePreLandscape, onGround);
	vortex.afterLandEffect = startStepped(info->particleTypePostLandscape, vortex.centre);
	vortex.lightMapEffect = start(info->particleTypeLightMap, vortex.centre);
	// The swirl is drawn before the land, which covers it but where the vortex opens its hole
	if (vortex.beforeLandEffect != ParticleSystemInterface::k_NoEffect)
	{
		particles.SetDrawPath(vortex.beforeLandEffect, particles::draw::DrawPath::BeforeLand);
	}
}

void VortexSystem::DeleteEffects(const Vortex& vortex)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	for (const auto effect : {vortex.objectMoverEffect, vortex.beforeLandEffect, vortex.afterLandEffect, vortex.lightMapEffect})
	{
		if (effect != ParticleSystemInterface::k_NoEffect)
		{
			particles.Delete(effect);
		}
	}
}

void VortexSystem::UpdateFrame(float gameSeconds)
{
	_groundMarks.clear();
	auto& registry = Locator::entitiesRegistry::value();
	auto* particles = Locator::particleSystem::has_value() ? &Locator::particleSystem::value() : nullptr;
	registry.Each<Vortex>([this, gameSeconds, particles](entt::entity /*entity*/, Vortex& vortex) {
		const auto seconds = static_cast<float>(SecondsInState(vortex));
		const auto openness = vortex::Openness(vortex.state, seconds);
		if (openness != 0.0f)
		{
			if (const auto* info = InfoOf(vortex.type); info != nullptr)
			{
				const auto cell = vortex::CentreCell({vortex.centre.x, vortex.centre.z});
				_groundMarks.push_back({
				    .type = vortex.type,
				    .centre = {vortex.centre.x, vortex.centre.z},
				    .block = {cell.x >> 4, cell.y >> 4},
				    .baseScale = info->baseScale,
				    .holeThreshold = vortex::GroundHoleThreshold(openness),
				});
			}
		}
		if (particles == nullptr)
		{
			return;
		}
		// The swirl and the effect over the land grow with the openness, stepped by the frame's game time; the swirl
		// rises towards the ground as the land is levelled
		const particles::ProcessInfo info {.power = openness, .enabled = true};
		if (openness != 0.0f && vortex.beforeLandEffect != ParticleSystemInterface::k_NoEffect)
		{
			const auto ground = Locator::terrainSystem::value().GetHeightAt({vortex.centre.x, vortex.centre.z});
			const auto depth = vortex::SwirlDepth(vortex::LevelAmount(vortex.state, seconds));
			particles->SetOrigin(vortex.beforeLandEffect, {vortex.centre.x, ground - depth, vortex.centre.z});
			if (!particles->ProcessForSpell(vortex.beforeLandEffect, info, gameSeconds))
			{
				vortex.beforeLandEffect = ParticleSystemInterface::k_NoEffect;
			}
		}
		if (openness != 0.0f && vortex.afterLandEffect != ParticleSystemInterface::k_NoEffect &&
		    !particles->ProcessForSpell(vortex.afterLandEffect, info, gameSeconds))
		{
			vortex.afterLandEffect = ParticleSystemInterface::k_NoEffect;
		}
		// The glow on the ground is as bright as the vortex's state has it
		if (auto* lightMap = particles->Find(vortex.lightMapEffect))
		{
			const auto glow = static_cast<double>(vortex::GlowBrightness(vortex.state, seconds));
			lightMap->SetGlobalAlpha(static_cast<float>(static_cast<int>(glow * 255.0)));
		}
	});
}

bool VortexSystem::StartFadeOut(entt::entity vortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return false;
	}
	auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return false;
	}
	component->state = VortexStateType::FadeOut;
	component->stateStartTurn = Locator::time::value().GetTurn();
	return true;
}

float VortexSystem::GetOpenness(entt::entity vortex) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return 0.0f;
	}
	const auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return 0.0f;
	}
	return vortex::Openness(component->state, static_cast<float>(SecondsInState(*component)));
}

void VortexSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> gone;
	bool levelled = false;
	registry.Each<Vortex>([&gone, &levelled](entt::entity entity, Vortex& vortex) {
		const auto seconds = SecondsInState(vortex);
		switch (vortex::Advance(vortex.state, seconds))
		{
		case vortex::Step::Open:
			vortex.state = VortexStateType::Active;
			break;
		case vortex::Step::Remove:
			vortex.state = VortexStateType::Inactive;
			gone.push_back(entity);
			return;
		case vortex::Step::Stay:
			break;
		}
		if (!vortex::LevelsGround(vortex.type))
		{
			return;
		}
		// The levelling only ever goes further
		const auto amount = vortex::LevelAmount(vortex.state, static_cast<float>(seconds));
		if (vortex.levelApplied < amount)
		{
			vortex.levelApplied = amount;
			LevelGround(vortex, amount);
			levelled = true;
		}
	});
	for (const auto entity : gone)
	{
		DeleteEffects(registry.Get<Vortex>(entity));
		registry.Destroy(entity);
	}
	if (levelled)
	{
		Locator::terrainSystem::value().CommitAltitudeChanges();
	}
}

void VortexSystem::LevelGround(Vortex& vortex, float amount)
{
	auto& island = Locator::terrainSystem::value();
	const glm::vec2 centre {vortex.centre.x, vortex.centre.z};
	const auto centreCell = vortex::CentreCell(centre);
	if (vortex.groundHeights.empty())
	{
		// The square's heights as they are now, cells off the land counting as 0
		vortex.groundHeights.resize(vortex::k_LevelCellCount);
		for (size_t i = 0; i < vortex.groundHeights.size(); ++i)
		{
			const auto cell = vortex::SquareCell(centreCell, i);
			const auto* landCell = OnMap(cell) ? island.FindCell(glm::u16vec2(cell)) : nullptr;
			vortex.groundHeights[i] = landCell != nullptr ? landCell->altitude : 0;
		}
		vortex.groundAverage = vortex::AverageAltitude(vortex.groundHeights);
	}
	const auto heights = vortex::LevelSquare(centre, vortex.groundHeights, vortex.groundAverage, amount);
	for (size_t i = 0; i < heights.size(); ++i)
	{
		const auto cell = vortex::SquareCell(centreCell, i);
		if (OnMap(cell))
		{
			island.SetCellAltitude(glm::u16vec2(cell), heights.at(i));
		}
	}
}
