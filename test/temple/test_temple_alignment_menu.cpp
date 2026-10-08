/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Temple debug window's alignment part: which players it offers, what its slider writes, where the temple's
// outside heads and how the look reads. All on fakes of the services.

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Debug/TempleAlignmentModel.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"

using namespace openblack;
using namespace openblack::debug::temple_alignment;

namespace
{
/// Keeps every player's alignment as it is given, and records each write
class FakeAlignment final: public ecs::systems::AlignmentSystemInterface
{
public:
	struct SetCall
	{
		PlayerNames player;
		float value;
	};

	[[nodiscard]] float GetPlayerAlignment(PlayerNames player) const override { return values.at(static_cast<size_t>(player)); }
	void SetPlayerAlignment(PlayerNames player, float alignment) override
	{
		sets.push_back({player, alignment});
		values.at(static_cast<size_t>(player)) = alignment;
	}
	void AddPlayerAlignment(PlayerNames, float) override { ++adds; }
	void UpdateTurn() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	[[nodiscard]] float GetCameraAlignment() const override { return 0.0f; }
	[[nodiscard]] float GetSkyAlignment() const override { return 0.0f; }

	std::array<float, static_cast<size_t>(PlayerNames::_COUNT)> values {};
	std::vector<SetCall> sets;
	int adds {0};
};

/// A player is in the game when it has been given an entity
class FakePlayers final: public ecs::systems::PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity) override {}
	[[nodiscard]] entt::entity GetPlayer(PlayerNames name) const override { return entities.at(static_cast<size_t>(name)); }
	[[nodiscard]] PlayerNames LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	void ClearPlayers() override {}
	[[nodiscard]] ecs::components::Alignment& Alignment(PlayerNames) override { return alignment; }
	[[nodiscard]] ecs::components::PlayerMagic& MagicWithoutEntity(PlayerNames) override { return magic; }
	void ClearMagicWithoutEntity() override {}

	std::array<entt::entity, static_cast<size_t>(PlayerNames::_COUNT)> entities {
	    entt::null, entt::null, entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	ecs::components::Alignment alignment {};
	ecs::components::PlayerMagic magic {};
};

/// Records the snaps it is asked for
class FakeExterior final: public ecs::systems::TempleExteriorSystemInterface
{
public:
	struct SnapCall
	{
		entt::entity heart;
		float target;
	};

	void Create(entt::entity) override {}
	[[nodiscard]] bool Step(entt::entity, float, float) override { return false; }
	void Blend(entt::entity) override {}
	[[nodiscard]] std::optional<TempleExteriorMorph::State> GetLook(entt::entity) const override { return std::nullopt; }
	void SnapAlignment(entt::entity heart, float alignmentTarget) override { snaps.push_back({heart, alignmentTarget}); }

	std::vector<SnapCall> snaps;
};
} // namespace

TEST(TempleAlignmentMenu, offersThePlayersInTheGameInPlayerOrder)
{
	FakePlayers players;
	const auto entityOf = [&players](PlayerNames player) { return players.GetPlayer(player); };
	EXPECT_TRUE(PlayersInGame(entityOf).empty());
	players.entities.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)) = entt::entity {4};
	players.entities.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = entt::entity {9};
	EXPECT_EQ(PlayersInGame(entityOf), (std::vector {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO}));
}

TEST(TempleAlignmentMenu, aPlayerTheLandHasNotMadeIsLeftOutWithoutAskingTheListThatThrows)
{
	// The lookup is asked for every player slot: one that is not on the land answers null and is left out (the
	// player system's own GetPlayer throws for it, which crashed the window)
	std::vector<PlayerNames> asked;
	const auto entityOf = [&asked](PlayerNames player) {
		asked.push_back(player);
		return player == PlayerNames::PLAYER_THREE ? entt::entity {2} : entt::entity {entt::null};
	};
	EXPECT_EQ(PlayersInGame(entityOf), (std::vector {PlayerNames::PLAYER_THREE}));
	EXPECT_EQ(asked.size(), static_cast<size_t>(PlayerNames::_COUNT));
}

TEST(TempleAlignmentMenu, theSliderSetsThatPlayersAlignmentHeldFromEvilToGood)
{
	FakeAlignment alignment;
	SetAlignment(alignment, PlayerNames::PLAYER_TWO, 0.25f);
	SetAlignment(alignment, PlayerNames::PLAYER_ONE, -3.0f);
	SetAlignment(alignment, PlayerNames::PLAYER_ONE, 2.0f);
	ASSERT_EQ(alignment.sets.size(), 3u);
	EXPECT_EQ(alignment.sets[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(alignment.sets[0].value, 0.25f);
	EXPECT_EQ(alignment.sets[1].value, -1.0f);
	EXPECT_EQ(alignment.sets[2].value, 1.0f);
	EXPECT_EQ(alignment.adds, 0);
}

TEST(TempleAlignmentMenu, theOutsideHeadsForThePlayersAlignmentTakenFromZeroToJustShortOfOne)
{
	FakeAlignment alignment;
	alignment.values.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = -1.0f;
	alignment.values.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)) = 0.5f;
	alignment.values.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)) = 1.0f;
	EXPECT_EQ(AlignmentTargetNow(alignment, PlayerNames::PLAYER_ONE), 0.0f);
	EXPECT_EQ(AlignmentTargetNow(alignment, PlayerNames::PLAYER_TWO), 0.75f);
	EXPECT_EQ(AlignmentTargetNow(alignment, PlayerNames::PLAYER_THREE), 0.9999f);
}

TEST(TempleAlignmentMenu, aSnapTakesTheHeartToThePlayersTargetNow)
{
	FakeAlignment alignment;
	FakeExterior exterior;
	alignment.values.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)) = -0.5f;
	SnapOutside(alignment, exterior, entt::entity {7}, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(exterior.snaps.size(), 1u);
	EXPECT_EQ(exterior.snaps[0].heart, entt::entity {7});
	EXPECT_EQ(exterior.snaps[0].target, 0.25f);
	EXPECT_TRUE(alignment.sets.empty());
}

TEST(TempleAlignmentMenu, theTurnsToGoAreTheGamesStepsOf0016)
{
	EXPECT_EQ(TurnsToReach(0.5f, 0.5f), 0u);
	EXPECT_EQ(TurnsToReach(0.5f, 0.5005f), 1u);
	EXPECT_EQ(TurnsToReach(0.5f, 0.51f), 1u);
	EXPECT_EQ(TurnsToReach(0.5f, 0.531f), 2u);
	EXPECT_EQ(TurnsToReach(0.0f, 0.9999f), 63u);
	EXPECT_EQ(TurnsToReach(0.9999f, 0.0f), 63u);
}

TEST(TempleAlignmentMenu, theLookReadsWhereItIsWhereItHeadsAndWhatWasBlended)
{
	TempleExteriorMorph::State look {};
	EXPECT_EQ(LookStatus(look, 0.5f), "Outside alignment 0.500, heading to 0.500\nMesh blended for 0.500, there");
	look.alignment = 0.516f;
	look.alignmentTarget = 0.9999f;
	EXPECT_EQ(LookStatus(look, 0.9999f), "Outside alignment 0.516, heading to 1.000\nMesh blended for 0.500, 31 turns to go");
	// the player's alignment changed since the last turn: the next turn takes the new target
	EXPECT_EQ(LookStatus(look, 0.25f),
	          "Outside alignment 0.516, heading to 1.000 (0.250 from the next turn)\nMesh blended for 0.500, 17 turns to go");
}
