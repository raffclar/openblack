/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ScriptHeaders/ScriptLockedInteraction.h"
#include "Temple/TempleKeys.h"

using namespace openblack;

TEST(ScriptNativesAudit, AScriptMistStandsOnTheGridOfTheMap)
{
	// The ground is 2 m high everywhere. A point between two grid points goes onto the lower grid point.
	const auto ground = [](const map_coords::MapCoords&) { return 2.0f; };
	const glm::vec3 point {100.00007f, 30.0f, 200.00007f};

	const auto place = map_coords::FromWorld(point, ground);

	EXPECT_EQ(place.x, map_coords::ToFixed(point.x));
	EXPECT_EQ(place.z, map_coords::ToFixed(point.z));
	EXPECT_NE(map_coords::ToMetres(place.x), point.x);
	EXPECT_FLOAT_EQ(map_coords::ToMetres(place.x), map_coords::Quantise(point.x));
	EXPECT_FLOAT_EQ(place.altitude, 28.0f);
}

TEST(ScriptNativesAudit, TheGroundOfAScriptMistIsTheGroundAtTheGridPoint)
{
	// The ground rises 1 m for each fixed unit along x: the altitude uses the ground at the grid point
	const auto ground = [](const map_coords::MapCoords& coords) { return static_cast<float>(coords.x); };
	const glm::vec3 point {0.0005f, 10.0f, 0.0f};

	const auto place = map_coords::FromWorld(point, ground);

	EXPECT_EQ(place.x, 3);
	EXPECT_FLOAT_EQ(place.altitude, 7.0f);
}

TEST(ScriptNativesAudit, TheScoopOfTheHandOfAnyPlayerLocksTheThing)
{
	const auto thing = static_cast<entt::entity>(5);
	const auto other = static_cast<entt::entity>(6);
	// The hand of the third player scoops from the thing, and the hand of the first player from another thing
	const auto lockedOnOf = [thing, other](PlayerNames player) -> std::optional<entt::entity> {
		if (player == PlayerNames::PLAYER_THREE)
		{
			return thing;
		}
		if (player == PlayerNames::PLAYER_ONE)
		{
			return other;
		}
		return std::nullopt;
	};

	EXPECT_TRUE(script::locked_interaction::IsLocked({}, thing, lockedOnOf));
	EXPECT_TRUE(script::locked_interaction::IsLocked({}, other, lockedOnOf));
	EXPECT_FALSE(script::locked_interaction::IsLocked({}, static_cast<entt::entity>(7), lockedOnOf));
}

TEST(ScriptNativesAudit, AHeldCreatureOrAGrippedTotemIsLockedWithNoScoop)
{
	const auto none = [](PlayerNames) -> std::optional<entt::entity> { return std::nullopt; };
	const auto thing = static_cast<entt::entity>(1);

	EXPECT_TRUE(script::locked_interaction::IsLocked({.heldCreature = true}, thing, none));
	EXPECT_TRUE(script::locked_interaction::IsLocked({.grippedTotem = true}, thing, none));
	EXPECT_TRUE(script::locked_interaction::IsLocked({.heldPuzzleTotem = true}, thing, none));
	EXPECT_FALSE(script::locked_interaction::IsLocked({}, thing, none));
}

TEST(ScriptNativesAudit, TheScriptSendsThePlayerInOrOutOfTheTemple)
{
	using temple::keys::ScriptSwitch;
	using temple::keys::ScriptSwitchOf;
	// Outside, the player goes in as a tap on the entrance does
	EXPECT_EQ(ScriptSwitchOf(true, false), ScriptSwitch::GoIn);
	// Inside already, the boxes close and the camera cuts to the main room
	EXPECT_EQ(ScriptSwitchOf(true, true), ScriptSwitch::GoToMainRoom);
	EXPECT_EQ(ScriptSwitchOf(false, true), ScriptSwitch::Leave);
	// Outside already, only the interface lock of the hand ends
	EXPECT_EQ(ScriptSwitchOf(false, false), ScriptSwitch::EndHandLock);
}
