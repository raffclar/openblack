/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugThingActRuns.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/DeveloperToolsGate.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "Editor/EditorEntities.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/ParticleDrawPath.h"
#include "Sound/SoundRequestInterface.h"

namespace openblack::debug_thing_acts
{

using player_commands::DebugParticle;
using player_commands::DebugPhysics;
using player_commands::DebugTempleVillager;
using player_commands::DebugVillagerPath;

namespace
{

[[nodiscard]] float LandHeightAt(glm::vec2 place)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(place) : 0.0f;
}

/// The world of the game for the acts of the windows
class GameThings final: public ThingTarget
{
public:
	bool Throw(const DebugPhysics& act, PlayerNames player) override
	{
		if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return false;
		}
		auto& registry = Locator::entitiesRegistry::value();
		auto object = act.object;
		if (object == entt::null || !registry.Valid(object))
		{
			// A new static thing comes on the land at the place, as high as the window asks
			const glm::vec3 at(act.place.x, LandHeightAt(act.place), act.place.y);
			object = ecs::archetypes::MobileStaticArchetype::Create(at, static_cast<MobileStaticInfo>(act.type), act.height,
			                                                        0.0f, 0.0f, 0.0f, act.scale);
		}
		const auto started =
		    Locator::dynamicsSystem::value().InitialisePhysics(object, {.velocity = act.velocity, .player = player});
		return started.entry != nullptr;
	}

	uint32_t StartEffect(const DebugParticle& act) override
	{
		if (!Locator::particleRuleSystem::has_value())
		{
			return ecs::systems::ParticleRuleSystemInterface::k_NoEffect;
		}
		auto& particles = Locator::particleRuleSystem::value();
		const auto id = act.file.empty()
		                    ? particles.Start(static_cast<ParticleType>(act.type), act.position, act.magnitude, true)
		                    : particles.Start(act.file, act.position, act.magnitude, true);
		if (id == ecs::systems::ParticleRuleSystemInterface::k_NoEffect)
		{
			return id;
		}
		particles.SetPlayer(id, act.player);
		particles.SetDrawPath(id, static_cast<particles::draw::DrawPath>(act.drawPath));
		GiveNearestTargets(id, act.position, act.targets);
		return id;
	}

	void SetEffectsPaused(bool paused) override
	{
		if (Locator::particleRuleSystem::has_value())
		{
			Locator::particleRuleSystem::value().SetPaused(paused);
		}
	}

	void CloseAllEffects() override
	{
		if (Locator::particleRuleSystem::has_value())
		{
			auto& particles = Locator::particleRuleSystem::value();
			for (const auto& effect : particles.GetEffects())
			{
				particles.CloseDown(effect.id);
			}
		}
	}

	void DeleteAllEffects() override
	{
		if (Locator::particleRuleSystem::has_value())
		{
			auto& particles = Locator::particleRuleSystem::value();
			for (const auto& effect : particles.GetEffects())
			{
				particles.Delete(effect.id);
			}
		}
	}

	void CloseEffect(uint32_t effect) override
	{
		if (Locator::particleRuleSystem::has_value())
		{
			Locator::particleRuleSystem::value().CloseDown(effect);
		}
	}

	void DeleteEffect(uint32_t effect) override
	{
		if (Locator::particleRuleSystem::has_value())
		{
			Locator::particleRuleSystem::value().Delete(effect);
		}
	}

	bool SetTownShare(entt::entity town, float share) override
	{
		// The share goes as the town's totem sets it, which sends people or calls them back
		if (!IsTown(town) || !Locator::villageTotemSystem::has_value())
		{
			return false;
		}
		Locator::villageTotemSystem::value().SetTownShare(town, share);
		return true;
	}

	bool AdjustWorshippers(entt::entity town, int32_t count) override
	{
		if (!IsTown(town))
		{
			return false;
		}
		ecs::villager_worship::AdjustWorshippers(town, count, true, false);
		return true;
	}

	std::optional<entt::entity> MakeVillager(const DebugTempleVillager& act, glm::vec2 place) override
	{
		if (!Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		VillagerInfo type;
		try
		{
			type = GVillagerInfo::Find(static_cast<Tribe>(act.tribe), static_cast<VillagerNumber>(act.number));
		}
		catch (const std::runtime_error&)
		{
			return std::nullopt;
		}
		auto& registry = Locator::entitiesRegistry::value();
		const glm::vec3 at(place.x, LandHeightAt(place), place.y);
		const auto villager = ecs::archetypes::VillagerArchetype::Create(act.home, at, type, act.age);
		// The villager joins the town nearest its home, which is the town of the act
		if (auto* person = registry.TryGet<ecs::components::Villager>(villager);
		    person != nullptr && IsTown(act.town) && person->town != act.town)
		{
			person->town = act.town;
			registry.TownMembersChanged();
		}
		return villager;
	}

	bool SendToWorship(entt::entity villager) override
	{
		if (!IsVillager(villager))
		{
			return false;
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* action = registry.TryGet<ecs::components::LivingAction>(villager))
		{
			ecs::villager_worship::CheckWorshipActivity(*action, false);
			return true;
		}
		return false;
	}

	bool TakeAway(entt::entity villager) override
	{
		if (!IsVillager(villager))
		{
			return false;
		}
		auto& registry = Locator::entitiesRegistry::value();
		for (auto& made : registry.Context().debugMadeVillagers)
		{
			std::erase(made, villager);
		}
		// The villager leaves its site and the dance as a worshipper that leaves its town, then the towns forget it
		ecs::villager_worship::LeaveTown(villager);
		ecs::villager_worship::Forget(villager);
		// The villager goes out of the physics and stops its sounds, then it goes out of the world as the game takes
		// things out
		if (Locator::dynamicsSystem::has_value())
		{
			Locator::dynamicsSystem::value().RemoveObject(villager, false, false);
		}
		if (Locator::sounds::has_value())
		{
			Locator::sounds::value().StopOwnedSounds(villager);
		}
		editor::Remove(villager);
		return true;
	}

	bool MoveVillager(const DebugVillagerPath& act) override
	{
		using namespace ecs::components;
		if (!IsVillager(act.villager))
		{
			return false;
		}
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(act.villager);
		auto* wallHug = registry.TryGet<WallHug>(act.villager);
		if (transform == nullptr || wallHug == nullptr)
		{
			return false;
		}
		switch (act.act)
		{
		case DebugVillagerPath::Act::Teleport:
			transform->position = act.position;
			break;
		case DebugVillagerPath::Act::MoveTo:
			wallHug->goalPlace = ecs::wall_hug::ToWhole(glm::vec2(act.position.x, act.position.z));
			registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
			                MoveStateFinalStepTag, MoveStateArrivedTag>(act.villager);
			registry.Assign<MoveStateLinearTag>(act.villager);
			break;
		case DebugVillagerPath::Act::Goal:
			wallHug->goalPlace = act.goal;
			break;
		case DebugVillagerPath::Act::Speed:
			wallHug->speed = act.speed;
			break;
		}
		return true;
	}

private:
	[[nodiscard]] static bool IsTown(entt::entity town)
	{
		if (town == entt::null || !Locator::entitiesRegistry::has_value())
		{
			return false;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		return registry.Valid(town) && registry.AllOf<ecs::components::Town>(town);
	}

	[[nodiscard]] static bool IsVillager(entt::entity villager)
	{
		if (villager == entt::null || !Locator::entitiesRegistry::has_value())
		{
			return false;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		return registry.Valid(villager) && registry.AllOf<ecs::components::Villager>(villager);
	}
};

void Submit(player_commands::Action action)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(std::move(action));
	}
}

[[nodiscard]] PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

} // namespace

bool SendsCommands()
{
	return developer_tools::NetworkSessionLive();
}

ActResult Run(const DebugPhysics& act, PlayerNames player)
{
	GameThings things;
	return Apply(things, act, player);
}

ActResult Run(const DebugParticle& act)
{
	GameThings things;
	return Apply(things, act);
}

ActResult Run(const DebugTempleVillager& act, PlayerNames player)
{
	GameThings things;
	auto result = Apply(things, act);
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& lists = registry.Context().debugMadeVillagers;
		// A villager that went from the world in another way leaves the lists
		for (auto& list : lists)
		{
			std::erase_if(list, [&registry](entt::entity villager) { return !registry.Valid(villager); });
		}
		const auto slot = static_cast<size_t>(player);
		if (slot < lists.size())
		{
			lists.at(slot).insert(lists.at(slot).end(), result.made.begin(), result.made.end());
		}
	}
	return result;
}

ActResult Run(const DebugVillagerPath& act)
{
	GameThings things;
	return Apply(things, act);
}

std::optional<ActResult> Do(const DebugPhysics& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return std::nullopt;
	}
	return Run(act, LocalPlayer());
}

std::optional<ActResult> Do(const DebugParticle& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return std::nullopt;
	}
	return Run(act);
}

std::optional<ActResult> Do(const DebugTempleVillager& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return std::nullopt;
	}
	return Run(act, LocalPlayer());
}

std::optional<ActResult> Do(const DebugVillagerPath& act)
{
	if (SendsCommands())
	{
		Submit(act);
		return std::nullopt;
	}
	return Run(act);
}

std::vector<entt::entity> MadeVillagers()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& lists = registry.Context().debugMadeVillagers;
	const auto slot = static_cast<size_t>(LocalPlayer());
	if (slot >= lists.size())
	{
		return {};
	}
	std::vector<entt::entity> made;
	std::ranges::copy_if(lists.at(slot), std::back_inserter(made),
	                     [&registry](entt::entity villager) { return registry.Valid(villager); });
	return made;
}

void GiveNearestTargets(uint32_t effect, const glm::vec3& origin, uint8_t count)
{
	if (count == 0 || !Locator::entitiesRegistry::has_value() || !Locator::particleRuleSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<float, entt::entity>> nearest;
	const auto add = [&nearest, &origin](entt::entity entity, const ecs::components::Transform& transform) {
		nearest.emplace_back(glm::distance(transform.position, origin), entity);
	};
	registry.Each<const ecs::components::Creature, const ecs::components::Transform>(
	    [&add](entt::entity entity, const ecs::components::Creature&, const ecs::components::Transform& transform) {
		    add(entity, transform);
	    });
	registry.Each<const ecs::components::Villager, const ecs::components::Transform>(
	    [&add](entt::entity entity, const ecs::components::Villager&, const ecs::components::Transform& transform) {
		    add(entity, transform);
	    });
	std::ranges::sort(nearest);
	const auto taken = std::min(nearest.size(), static_cast<size_t>(count));
	// The rules take the last given first: the nearest goes last
	for (size_t i = taken; i > 0; --i)
	{
		Locator::particleRuleSystem::value().AddTarget(effect, nearest.at(i - 1).second);
	}
}

} // namespace openblack::debug_thing_acts
