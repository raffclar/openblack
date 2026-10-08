/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysRegistry.h"

#include <functional>
#include <map>
#include <string>

#include <spdlog/spdlog.h>

#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/PSysManagerState.h"

using namespace openblack::psys;

namespace
{
/// The factories, in the psys state (Locator::particleSystem)
FactoryRegistry& Raw()
{
	return openblack::Locator::particleSystem::value().GetState().factories;
}

/// Every topic's list. New rule and creator files add their Register*() call here.
void RegisterAll()
{
	RegisterCoreModifiers();
	RegisterSoundRules();
	RegisterFireballRules();
	RegisterSprinkleRules();
	RegisterMeshCreators();
	RegisterHandFollowRules();
	RegisterGestureRules();
	RegisterLightningRules();
	RegisterChainCreator();
	RegisterLightMapCreator();
	RegisterHealRules();
	RegisterShieldRules();
	RegisterSurfRevolRules();
	RegisterExplosionRules();
	RegisterKeyPointRules();
	RegisterOrientRules();
	RegisterForestRules();
	RegisterMistCreator();
	RegisterFlockRules();
	RegisterStormRules();
}

const FactoryRegistry& Filled()
{
	auto& state = openblack::Locator::particleSystem::value().GetState();
	if (!state.factoriesFilled)
	{
		RegisterAll();
		state.factoriesFilled = true;
	}
	return state.factories;
}

template <class Map, class Factory>
void Insert(Map& map, std::string_view kind, std::string_view className, Factory factory)
{
	if (const auto it = map.find(className); it != map.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} {} registered twice, the last one wins", kind, className);
		it->second = factory;
		return;
	}
	map.emplace(std::string(className), factory);
}
} // namespace

void openblack::psys::RegisterModifier(std::string_view className, ModifierFactory factory)
{
	Insert(Raw().modifiers, "modifier", className, factory);
}

void openblack::psys::RegisterCreator(std::string_view className, CreatorFactory factory)
{
	Insert(Raw().creators, "creator", className, factory);
}

void openblack::psys::RegisterCondition(std::string_view className, ConditionTest test)
{
	Insert(Raw().conditions, "condition", className, test);
}

ConditionTest openblack::psys::FindCondition(std::string_view className)
{
	const auto& conditions = Filled().conditions;
	const auto it = conditions.find(className);
	return it != conditions.end() ? it->second : nullptr;
}

ModifierFactory openblack::psys::FindModifierFactory(std::string_view className)
{
	const auto& modifiers = Filled().modifiers;
	const auto it = modifiers.find(className);
	return it != modifiers.end() ? it->second : nullptr;
}

CreatorFactory openblack::psys::FindCreatorFactory(std::string_view className)
{
	const auto& creators = Filled().creators;
	const auto it = creators.find(className);
	return it != creators.end() ? it->second : nullptr;
}
