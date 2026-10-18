/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerPlayerSystem.h"

#include <string>
#include <utility>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/CreatureHome.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// The game's own world, through the registry and the systems in the locator
class GameWorld final: public computer_player::World
{
public:
	[[nodiscard]] PlayerNames LocalPlayer() const override
	{
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	}

	[[nodiscard]] std::optional<entt::entity> Creature(PlayerNames player) const override
	{
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	}

	[[nodiscard]] float ShownSize(entt::entity creature) const override
	{
		const auto* body = Locator::entitiesRegistry::value().TryGet<const ecs::components::Creature>(creature);
		return body != nullptr ? ecs::components::ShownSize(*body) : 0.0f;
	}

	void SetSize(entt::entity creature, float size) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* body = registry.TryGet<ecs::components::Creature>(creature);
		if (body == nullptr)
		{
			return;
		}
		// It grows or shrinks to the size at once, as a script that sets its scale does
		body->size = size;
		if (auto* transform = registry.TryGet<Transform>(creature); transform != nullptr)
		{
			transform->scale =
			    glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(body->species, ecs::components::ShownSize(*body)));
			registry.SetDirty();
		}
	}

	[[nodiscard]] bool HasTemple(PlayerNames player) const override
	{
		bool temple = false;
		Locator::entitiesRegistry::value().Each<const Temple>(
		    [&temple, player](const Temple& building) { temple = temple || building.owner == player; });
		return temple;
	}

	[[nodiscard]] std::optional<glm::vec3> Pen(PlayerNames player) const override
	{
		auto pen = ecs::creature_home::TemplePenOf(Locator::entitiesRegistry::value(), player);
		// The creature stands on the land at the pen
		if (pen.has_value() && Locator::terrainSystem::has_value())
		{
			pen->y = Locator::terrainSystem::value().GetHeightAt({pen->x, pen->z});
		}
		return pen;
	}

	[[nodiscard]] bool Skirmish() const override { return Locator::entitiesRegistry::value().Context().skirmish; }

	[[nodiscard]] Player::Miracles Miracles(PlayerNames player) const override
	{
		Player::Miracles miracles;
		Locator::entitiesRegistry::value().Each<const Player>([&miracles, player](const Player& each) {
			if (each.name == player)
			{
				miracles = each.miracles;
			}
		});
		return miracles;
	}

	void MakeCreature(PlayerNames player, CreatureType species, const glm::vec3& position) override
	{
		// The same steps as a script that loads a creature from a mind file
		ecs::creature_from_file::Make({.owner = player,
		                               .speciesRow = script::property_rules::ScriptCreatureType(species),
		                               .fileName = std::string(computer_player::k_CreatureMindFile),
		                               .place = glm::vec2(position.x, position.z)});
	}

	void Teach(entt::entity creature, MagicType miracle) override
	{
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().SetKnowsAction(creature, CreatureActionLearningType::Magic,
			                                                    static_cast<uint32_t>(miracle), true);
		}
	}

	uint32_t GameRand(uint32_t range) override { return Locator::gameRandom::value().GameRand(range); }

	float GameFloatRand(float most) override { return Locator::gameRandom::value().GameFloatRand(most); }
};

} // namespace

ComputerPlayerSystem::ComputerPlayerSystem()
    : _world(std::make_unique<GameWorld>())
{
}

ComputerPlayerSystem::ComputerPlayerSystem(std::unique_ptr<computer_player::World> world)
    : _world(std::move(world))
{
}

ComputerPlayerSystem::~ComputerPlayerSystem() = default;

ComputerPlayer& ComputerPlayerSystem::MindOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity entity = entt::null;
	registry.Each<const Player>([&entity, player](entt::entity each, const Player& name) {
		if (name.name == player && entity == entt::null)
		{
			entity = each;
		}
	});
	// The player made here is known to the players at once, so that the creature made for it next is its creature
	if (entity == entt::null)
	{
		entity = ecs::archetypes::PlayerArchetype::Create(player);
		if (Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().AddPlayer(entity);
		}
	}
	auto* mind = registry.TryGet<ComputerPlayer>(entity);
	return mind != nullptr ? *mind : registry.Assign<ComputerPlayer>(entity);
}

void ComputerPlayerSystem::SwitchOn(PlayerNames player, bool on)
{
	computer_player::SwitchOn(MindOf(player), on);
}

void ComputerPlayerSystem::SetCreatureLike(PlayerNames player, PlayerNames like)
{
	MindOf(player).creatureLike = like;
}

void ComputerPlayerSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<PlayerNames, entt::entity>> due;
	registry.Each<const Player, const ComputerPlayer>(
	    [&due](entt::entity entity, const Player& player, const ComputerPlayer& mind) {
		    if (computer_player::DueToStart(mind))
		    {
			    due.emplace_back(player.name, entity);
		    }
	    });
	// A start can make a creature, so each mind starts after the players were found
	for (const auto& [name, entity] : due)
	{
		if (auto* mind = registry.TryGet<ComputerPlayer>(entity); mind != nullptr)
		{
			computer_player::Start(*_world, name, *mind);
		}
	}
}
