/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "Commands/TurnCommandSource.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/PlayerCommandSystem.h"

using namespace openblack;
using namespace openblack::player_commands;

namespace
{

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// A game server for the test: it keeps what the computer sent, and gives the turns that the test puts in
class FakeSource final: public TurnCommandSourceInterface
{
public:
	void Send(const Action& action) override { sent.push_back(action); }
	[[nodiscard]] bool HasTurn(uint32_t turn) const override { return turns.contains(turn); }
	[[nodiscard]] std::vector<Command> TakeTurn(uint32_t turn) override
	{
		auto found = turns.find(turn);
		if (found == turns.end())
		{
			return {};
		}
		auto commands = std::move(found->second);
		turns.erase(found);
		return commands;
	}

	std::vector<Action> sent;
	std::map<uint32_t, std::vector<Command>> turns;
};

/// The hands of the test: it keeps the players that got hands
class FakeHands final: public ecs::systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, 2> GetPlayerHands() const noexcept override { return {}; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetPlayerHandPositions() const noexcept override { return {}; }
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
	std::array<entt::entity, 2> MakeHandsOf(PlayerNames player) override
	{
		made.push_back(player);
		return {};
	}

	std::vector<PlayerNames> made;
};

/// The computer minds of the test: it keeps the humans of each join
class FakeMinds final: public ecs::systems::ComputerPlayerSystemInterface
{
public:
	void SwitchOn(PlayerNames /*player*/, bool /*on*/) override {}
	void SetCreatureLike(PlayerNames /*player*/, PlayerNames /*like*/) override {}
	void ProcessTurn() override {}
	void FillEmptySlots(std::span<const PlayerNames> /*gods*/, std::span<const PlayerNames> /*humans*/) override {}
	void WaitForHumans() override {}
	void HumansJoined(std::span<const PlayerNames> humans) override { joins.emplace_back(humans.begin(), humans.end()); }
	std::vector<std::vector<PlayerNames>> joins;
};

/// The leashes of the test: it keeps the players that went into their temple
class FakeLeash final: public ecs::systems::LeashSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float /*seconds*/) override {}
	void ShowPosts(bool /*handShown*/) override {}
	bool PressAction(PlayerNames /*player*/, const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/,
	                 glm::vec2 /*cursor*/, uint32_t /*milliseconds*/) override
	{
		return false;
	}
	bool TapThing(PlayerNames /*player*/, entt::entity /*object*/) override { return false; }
	[[nodiscard]] bool TakesTap(PlayerNames /*player*/, entt::entity /*object*/) const override { return false; }
	[[nodiscard]] std::optional<float> RopeUnderCursor(PlayerNames /*player*/, const RopeFrame& /*frame*/) override
	{
		return std::nullopt;
	}
	void SetRopeHovered(PlayerNames /*player*/, bool /*hovered*/) override {}
	[[nodiscard]] leash_snap::Hand RopeHand(PlayerNames /*player*/) const override { return leash_snap::Hand::Away; }
	bool PressRope(PlayerNames /*player*/) override { return false; }
	void LetGoOfRope(PlayerNames /*player*/, std::optional<glm::vec3> /*place*/) override {}
	void HandleInput(const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/, bool /*actionTaken*/) override {}
	[[nodiscard]] bool Knows(entt::entity /*creature*/, LeashType /*type*/) const override { return false; }
	void SetKnown(entt::entity /*creature*/, LeashType /*type*/, bool /*known*/) override {}
	[[nodiscard]] bool IsLeashable(entt::entity /*creature*/) const override { return false; }
	bool SetLeashable(entt::entity /*creature*/, bool /*leashable*/) override { return false; }
	void SetOwner(entt::entity /*creature*/, PlayerNames /*owner*/) override {}
	void ClaimOnArrival(entt::entity /*creature*/) override {}
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames /*player*/, entt::entity /*creature*/,
	                                             LeashType /*type*/) const override
	{
		return {};
	}
	[[nodiscard]] std::optional<Refused> LastRefusal() const override { return std::nullopt; }
	bool PutOn(entt::entity /*creature*/, LeashType /*type*/) override { return false; }
	void TakeOff(entt::entity /*creature*/) override {}
	bool Toggle(entt::entity /*creature*/) override { return false; }
	bool ChangeType(entt::entity /*creature*/, LeashType /*type*/) override { return false; }
	bool TieTo(entt::entity /*creature*/, entt::entity /*object*/) override { return false; }
	void UntieToHand(entt::entity /*creature*/) override {}
	void SetWorks(entt::entity /*creature*/, bool /*works*/) override {}
	void SetDrawn(bool /*drawn*/) override {}
	void ConfineToHome(entt::entity /*creature*/, float /*radius*/) override {}
	void ClearConfinement(entt::entity /*creature*/) override {}
	[[nodiscard]] bool FreeOfHome(entt::entity /*creature*/) const override { return true; }
	[[nodiscard]] bool IsLeashed(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> HolderPoint(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] LeashType TypeOf(entt::entity /*creature*/) const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames /*player*/) const override { return std::nullopt; }
	bool PressKey(PlayerNames /*player*/, creature_leash::LeashKey /*key*/) override { return false; }
	bool TapCreature(PlayerNames /*player*/, entt::entity /*creature*/) override { return false; }
	bool Shake(PlayerNames /*player*/) override { return false; }
	void PlacePosts(PlayerNames /*owner*/, const std::array<glm::vec3, 3>& /*points*/) override {}
	bool OrderAt(PlayerNames /*player*/, const glm::vec3& /*place*/) override { return false; }
	bool OrderOn(PlayerNames /*player*/, entt::entity /*object*/) override { return false; }
	[[nodiscard]] std::optional<glm::vec3> OrderTarget(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<uint32_t> ToolTip(PlayerNames /*player*/,
	                                              std::optional<entt::entity> /*hovered*/) const override
	{
		return std::nullopt;
	}
	bool TapPost(entt::entity /*post*/) override { return false; }
	bool TakeOffAtTemple(PlayerNames player) override
	{
		templeLeashOff.push_back(player);
		return true;
	}

	std::vector<PlayerNames> templeLeashOff;
};

class NetworkCommands: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	ecs::systems::PlayerCommandSystem _commands;
	FakeSource _source;
	std::vector<std::pair<uint32_t, std::vector<Command>>> _heard;

	void Listen()
	{
		_commands.SetTurnListener([this](uint32_t turn, std::span<const Command> commands) {
			_heard.emplace_back(turn, std::vector<Command>(commands.begin(), commands.end()));
		});
	}
};

} // namespace

TEST_F(NetworkCommands, ASingleTurnGameAlwaysHasItsTurn)
{
	EXPECT_TRUE(_commands.TurnReady(0));
	EXPECT_TRUE(_commands.TurnReady(1234));
}

TEST_F(NetworkCommands, TheCommandsOfANetworkGameGoToTheServerAtOnce)
{
	Listen();
	_commands.Submit(HandTap {.object = Entity(1)});
	_commands.SetSource(&_source);
	// The command of the single player game does not go into the network game
	EXPECT_EQ(_commands.PendingCount(), 0u);
	_commands.Submit(HandTap {.object = Entity(2)});
	ASSERT_EQ(_source.sent.size(), 1u);
	EXPECT_EQ(std::get<HandTap>(_source.sent[0]).object, Entity(2));
	EXPECT_EQ(_commands.PendingCount(), 0u);

	// The world waits for the turn of the server, and then does only the server's commands
	EXPECT_FALSE(_commands.TurnReady(0));
	_source.turns[0] = {{.player = PlayerNames::PLAYER_TWO, .action = HandTake {.object = Entity(7)}},
	                    {.player = PlayerNames::PLAYER_ONE, .action = HandTap {.object = Entity(2)}}};
	EXPECT_TRUE(_commands.TurnReady(0));
	_commands.RunSent(0);
	ASSERT_EQ(_heard.size(), 1u);
	ASSERT_EQ(_heard[0].second.size(), 2u);
	EXPECT_EQ(_heard[0].second[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_heard[0].second[1].player, PlayerNames::PLAYER_ONE);

	_commands.SetSource(nullptr);
	EXPECT_TRUE(_commands.TurnReady(1));
}

TEST_F(NetworkCommands, TheLeaveOfTheServerDefeatsTheGodOfThePlayerAtTheTurn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto god = registry.Create();
	registry.Assign<ecs::components::Player>(god, ecs::components::Player {.name = PlayerNames::PLAYER_TWO});
	_commands.SetSource(&_source);
	// A slot that has no god on the land is no problem
	_source.turns[4] = {{.player = PlayerNames::PLAYER_SEVEN, .action = PlayerLeft {}},
	                    {.player = PlayerNames::PLAYER_TWO, .action = PlayerLeft {}}};
	_commands.RunSent(4);
	ASSERT_TRUE(registry.AllOf<ecs::components::LeftGame>(god));
	EXPECT_EQ(registry.Get<ecs::components::LeftGame>(god).turn, 4u);
	// A second leave keeps the turn of the first
	_source.turns[5] = {{.player = PlayerNames::PLAYER_TWO, .action = PlayerLeft {}}};
	_commands.RunSent(5);
	EXPECT_EQ(registry.Get<ecs::components::LeftGame>(god).turn, 4u);
	_commands.SetSource(nullptr);
}

TEST_F(NetworkCommands, TheJoinOfTheServerGivesTheGodOfThePlayerItsHandsAtTheTurn)
{
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	_commands.SetSource(&_source);
	_source.turns[0] = {{.player = PlayerNames::PLAYER_TWO, .action = PlayerJoined {}},
	                    {.player = PlayerNames::PLAYER_THREE, .action = PlayerJoined {}}};
	_commands.RunSent(0);
	EXPECT_EQ(hands.made, (std::vector {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE}));
	_commands.SetSource(nullptr);
	Locator::handSystem::reset();
}

TEST_F(NetworkCommands, AComputerIsBehindWhenTheNextTurnIsThereToo)
{
	EXPECT_FALSE(_commands.IsBehind(3));
	_commands.SetSource(&_source);
	_source.turns[3] = {};
	EXPECT_FALSE(_commands.IsBehind(3));
	_source.turns[4] = {};
	EXPECT_TRUE(_commands.IsBehind(3));
	_commands.SetSource(nullptr);
}

TEST_F(NetworkCommands, TheHumansOfTheFirstTurnJoinTogetherAfterTheCommandsOfTheTurn)
{
	auto& minds = static_cast<FakeMinds&>(Locator::computerPlayerSystem::emplace<FakeMinds>());
	_commands.SetSource(&_source);
	_source.turns[0] = {{.player = PlayerNames::PLAYER_ONE, .action = PlayerJoined {}},
	                    {.player = PlayerNames::PLAYER_TWO, .action = PlayerJoined {}}};
	_source.turns[1] = {{.player = PlayerNames::PLAYER_ONE, .action = HandTap {.object = Entity(2)}}};
	_commands.RunSent(0);
	_commands.RunSent(1);
	// One join with both humans, and none for a turn with no join
	ASSERT_EQ(minds.joins.size(), 1u);
	EXPECT_EQ(minds.joins[0], (std::vector {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO}));
	_commands.SetSource(nullptr);
	Locator::computerPlayerSystem::reset();
}

TEST_F(NetworkCommands, TheLeashOffOfTheTempleGoesToTheLeashOfTheSenderAtTheTurn)
{
	auto& leashes = static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>());
	_commands.SetSource(&_source);
	// The computer sends the command to the server and does nothing with it before the turn
	_commands.Submit(TempleLeashOff {});
	ASSERT_EQ(_source.sent.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<TempleLeashOff>(_source.sent[0]));
	EXPECT_TRUE(leashes.templeLeashOff.empty());
	_source.turns[3] = {{.player = PlayerNames::PLAYER_TWO, .action = TempleLeashOff {}}};
	_commands.RunSent(3);
	EXPECT_EQ(leashes.templeLeashOff, (std::vector {PlayerNames::PLAYER_TWO}));
	_commands.SetSource(nullptr);
	Locator::leashSystem::reset();
}
