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

#include <algorithm>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ComputerMindWorld.h"
#include "Creature/CreatureFileLoad.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// Where the hand of a god with no temple starts, in metres
constexpr glm::vec3 k_HandWithoutTemple {2150.0f, 0.0f, 2620.0f};

/// The entity of the player. The game makes the player when the land has none, and the player is known to the players at
/// once, so that a creature made for it next is its creature.
entt::entity PlayerEntity(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity entity = entt::null;
	registry.Each<const Player>([&entity, player](entt::entity each, const Player& name) {
		if (name.name == player && entity == entt::null)
		{
			entity = each;
		}
	});
	if (entity == entt::null)
	{
		entity = ecs::archetypes::PlayerArchetype::Create(player);
		if (Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().AddPlayer(entity);
		}
	}
	return entity;
}

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

/// The game's own world for the start of a network game
class GameSlotWorld final: public computer_player::SlotWorld
{
public:
	explicit GameSlotWorld(ComputerPlayerSystemInterface& minds)
	    : _minds(minds)
	{
	}

	[[nodiscard]] bool HasCitadel(PlayerNames player) const override
	{
		bool citadel = false;
		Locator::entitiesRegistry::value().Each<const Temple>(
		    [&citadel, player](const Temple& building) { citadel = citadel || building.owner == player; });
		return citadel;
	}

	[[nodiscard]] bool IsComputerPlayer(PlayerNames player) const override
	{
		bool computer = false;
		Locator::entitiesRegistry::value().Each<const Player, const ComputerPlayer>(
		    [&computer, player](const Player& each, const ComputerPlayer& mind) {
			    computer = computer || (each.name == player && mind.marked);
		    });
		return computer;
	}

	void MakePlayer(PlayerNames player) override { std::ignore = PlayerEntity(player); }

	void SwitchOn(PlayerNames player) override { _minds.SwitchOn(player, true); }

	void MakeSlotCreature(PlayerNames player, CreatureType species, uint32_t nameText, uint32_t stage) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto home = ecs::creature_home::CitadelCreatureHomeOf(registry, player);
		if (!home.has_value() || !map_coords::InBounds(*home) || !Locator::terrainSystem::has_value())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"),
			                   "No creature made for the computer god {}: its citadel has no home on the land",
			                   static_cast<int>(player));
			return;
		}
		const auto position = map_coords::ToWorld(Locator::terrainSystem::value(), *home);
		const auto creature = ecs::archetypes::CreatureArchetype::Create(
		    position, player, species, 0, creature_file_load::k_StartYaw,
		    ecs::archetypes::CreatureArchetype::StartScale(species), ecs::archetypes::CreatureArchetype::StartBody(species));
		if (Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().MakePrimaryCreature(creature);
		}
		registry.Get<ecs::components::Creature>(creature).name = ecs::creature_naming::HelpWords(nameText);
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().MoveToStage(creature, stage);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Made creature {} of species {} for the computer god {} at {:.1f}, {:.1f}",
		                   entt::to_integral(creature), static_cast<int>(species), static_cast<int>(player), position.x,
		                   position.z);
	}

	uint32_t GameRand(uint32_t range) override { return Locator::gameRandom::value().GameRand(range); }

private:
	ComputerPlayerSystemInterface& _minds;
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
	const auto entity = PlayerEntity(player);
	auto* mind = registry.TryGet<ComputerPlayer>(entity);
	auto& state = mind != nullptr ? *mind : registry.Assign<ComputerPlayer>(entity);
	// Every player has the tree of needs from its start, so a land script can set its weights at any time
	if (state.mind.nodes.empty())
	{
		state.mind = computer_mind::MakeMind();
	}
	return state;
}

void ComputerPlayerSystem::SwitchOn(PlayerNames player, bool on)
{
	computer_player::SwitchOn(MindOf(player), on);
}

void ComputerPlayerSystem::SetCreatureLike(PlayerNames player, PlayerNames like)
{
	MindOf(player).creatureLike = like;
}

void ComputerPlayerSystem::SetActive(PlayerNames player, bool on)
{
	MindOf(player).switchedOn = on;
}

void ComputerPlayerSystem::SetPaused(PlayerNames player, bool paused)
{
	auto& mind = MindOf(player);
	mind.mind.paused = paused;
	mind.mind.scriptHold = paused;
}

void ComputerPlayerSystem::SetPersonality(PlayerNames player, std::string_view name, float weight)
{
	computer_mind::SetPersonality(MindOf(player).mind, name, weight);
}

void ComputerPlayerSystem::SetSuppression(PlayerNames player, std::string_view name, float suppression)
{
	computer_mind::SetSuppression(MindOf(player).mind, name, suppression);
}

bool ComputerPlayerSystem::Ready(PlayerNames player)
{
	return !computer_hand::Busy(MindOf(player).hand);
}

void ComputerPlayerSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<PlayerNames, entt::entity>> minds;
	registry.Each<const Player, const ComputerPlayer>(
	    [&minds](entt::entity entity, const Player& player, const ComputerPlayer& /*mind*/) {
		    minds.emplace_back(player.name, entity);
	    });
	// The players take their turns in the order of their numbers
	std::ranges::sort(minds, {}, &std::pair<PlayerNames, entt::entity>::first);
	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	for (const auto& [name, entity] : minds)
	{
		auto* mind = registry.TryGet<ComputerPlayer>(entity);
		if (mind == nullptr)
		{
			continue;
		}
		if (mind->mind.nodes.empty())
		{
			mind->mind = computer_mind::MakeMind();
		}
		computer_world::Validate(*mind);
		if (!mind->switchedOn || mind->mind.paused)
		{
			continue;
		}
		if (computer_player::DueToStart(*mind))
		{
			// The hand starts at the temple, or at a fixed place when the god has none
			mind->hand.hand = computer_world::StartingHand(name).value_or(k_HandWithoutTemple);
			// A start can make a creature, so the mind is found again after it
			computer_player::Start(*_world, name, *mind);
			mind = registry.TryGet<ComputerPlayer>(entity);
			if (mind == nullptr)
			{
				continue;
			}
		}
		computer_world::GameMindWorld world(name, *mind);
		computer_mind::ProcessTurn(mind->mind, world, turn);
	}
}

void ComputerPlayerSystem::FillEmptySlots(std::span<const PlayerNames> gods, std::span<const PlayerNames> humans)
{
	GameSlotWorld world(*this);
	computer_player::FillEmptySlots(world, gods, humans);
}

void ComputerPlayerSystem::WaitForHumans()
{
	Locator::entitiesRegistry::value().Context().slotsWaitForHumans = true;
}

void ComputerPlayerSystem::HumansJoined(std::span<const PlayerNames> humans)
{
	// The slots fill one time on the land
	const bool waits = std::exchange(Locator::entitiesRegistry::value().Context().slotsWaitForHumans, false);
	GameSlotWorld world(*this);
	computer_player::HumansJoin(world, humans, waits);
}
