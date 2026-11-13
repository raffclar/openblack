/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios of the hand under the boxes of the front end. While a box takes the mouse, the front end's
// pointer shows and the hand is away. Each scenario in the land checks that the hand shows before the box, and not
// under it. The temple holds the turns of the world, so the scenarios in the temple have no checks.

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

using Kind = Command::Kind;

/// A check that the hand shows, or does not show, from a turn until a later turn. A turn is a tenth of a second.
checks::Check HandShownCheck(std::string_view label, bool shown, uint32_t afterTurns, uint32_t byTurn)
{
	return {.label = label,
	        .quantity = checks::Quantity::HandShown,
	        .afterTurns = afterTurns,
	        .byTurn = byTurn,
	        .atLeast = shown ? 1.0 : 0.0,
	        .atMost = shown ? 1.0 : 0.0};
}

} // namespace

void testbed_scenarios::AddFrontEndHandScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "hand.away_under_tattoo_editor",
	    .name = "The hand is away while the tattoo editor shows",
	    .facet = Facet::Hand,
	    .description = "The pointer rests in the middle of the screen. After one second, the tattoo editor opens on the "
	                   "player's tiger, as a click on it in the Creature Cave does.",
	    .expected = "Before the editor opens, the hand shows under the pointer. While the editor shows, no part of the "
	                "hand shows: only the turning arrow of the front end shows at the pointer.",
	    .framing = {.shot = Shot::Overview},
	    .creatures =
	        {{.label = "yours", .species = CreatureType::Tiger, .offset = {0.0f, 40.0f}, .owner = PlayerNames::PLAYER_ONE}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::OpenTattooEditor, .creature = 0, .delaySeconds = 1.0f}},
	    .checks = {HandShownCheck("the hand shows before the editor", true, 5, 9),
	               HandShownCheck("the hand is away under the editor", false, 30, 40)},
	});
	all.push_back({
	    .id = "hand.away_under_staff_question",
	    .name = "The hand is away while a question of the staff shows",
	    .facet = Facet::Hand,
	    .description = "The pointer rests in the middle of the screen. After one second, a ping server of the testbed "
	                   "sends a question from the admin of the server, with a Yes and a No.",
	    .expected = "Before the question, the hand shows under the pointer. While the box of the question shows, no part "
	                "of the hand shows: only the turning arrow of the front end shows at the pointer.",
	    .environment = {.players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        {{.kind = Kind::PointerTo, .delaySeconds = 0.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::Chat, .delaySeconds = 1.0f, .value = k_ChatStaffSays, .action = "!SaMuElGRDo you like the game?"}},
	    .checks = {HandShownCheck("the hand shows before the question", true, 5, 9),
	               HandShownCheck("the hand is away under the question", false, 25, 35)},
	});
	all.push_back({
	    .id = "hand.away_over_incoming_message",
	    .name = "The hand is away while the pointer is on the incoming message",
	    .facet = Facet::Hand,
	    .description = "A message of the real weather comes in after half a second. One second later the pointer goes "
	                   "onto its button at the bottom left of the screen. One second after that, it goes back to the "
	                   "middle of the screen.",
	    .expected = "While the pointer is on the button, no part of the hand shows: only the turning arrow of the front "
	                "end shows at the pointer. In the middle of the screen the hand shows again under the pointer.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        {{.kind = Kind::PointerTo, .delaySeconds = 0.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::AddIncomingMessage, .delaySeconds = 0.5f, .value = 4, .action = "Guildford (Surrey,Great Britain)"},
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.01f, 0.98f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}}},
	    .checks = {HandShownCheck("the hand is away on the button", false, 18, 24),
	               HandShownCheck("the hand shows again in the middle", true, 30, 40)},
	});
	all.push_back({
	    .id = "hand.away_over_story_history_buttons",
	    .name = "The hand is away while the pointer is on a button of the story history",
	    .facet = Facet::Hand,
	    .description = "A script says a line. Then the player goes through the temple's main room into the library, and "
	                   "the camera looks at the Story History scroll. The pointer goes onto the Latest Line button at "
	                   "the bottom middle of the screen, and one second later back to the middle of the screen.",
	    .expected = "While the pointer is on Latest Line, no part of the hand shows: only the turning arrow of the front "
	                "end shows at the pointer. Off the buttons, the hand shows under the pointer, also while the buttons "
	                "show.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::RunText, .delaySeconds = 0.5f, .value = 4411},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 2},
	                 {.kind = Kind::LookAtTempleScroll, .delaySeconds = 4.0f, .value = 6},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.842f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}}},
	});
	all.push_back({
	    .id = "hand.away_under_save_room_question",
	    .name = "The hand is away while a question of the save game room shows",
	    .facet = Facet::Hand,
	    .description = "The game is saved into slot 0. Then the player goes through the temple's main room into the save "
	                   "game room, and the camera comes close to the picture of slot 0. Click the delete button (the red "
	                   "cross) under the picture, then move the pointer to the left.",
	    .expected = "Before the click, the hand shows under the pointer. The question \"Are you sure you want to delete this "
	                "game?\" shows with a Yes and a No. While it shows, no part of the hand shows: only the turning arrow "
	                "of the front end shows at the pointer.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = 6},
	                 {.kind = Kind::LookAtSaveGamePicture, .delaySeconds = 4.0f, .value = 0}},
	});
}
