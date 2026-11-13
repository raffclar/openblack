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
// under it. The temple holds the turns of the world, and the checks count the time of the frames then as turns. The
// save room scenario has no checks, because the click on its delete button is by hand.

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

/// A check that a quantity of 0 or 1 is 1, or is 0, from a turn until a later turn
checks::Check OnOffCheck(std::string_view label, checks::Quantity quantity, bool on, uint32_t afterTurns, uint32_t byTurn,
                         size_t index = 0)
{
	return {.label = label,
	        .quantity = quantity,
	        .index = index,
	        .afterTurns = afterTurns,
	        .byTurn = byTurn,
	        .atLeast = on ? 1.0 : 0.0,
	        .atMost = on ? 1.0 : 0.0};
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
	    .checks = {HandShownCheck("the hand is away on the button", false, 85, 89),
	               HandShownCheck("the hand shows again off the button", true, 95, 160)},
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
	// The hand's tooltip while the pointer is on the incoming message. The hand is away and offers no tooltip, so the
	// tooltip that it showed stays for its linger time, and then it goes.
	all.push_back({
	    .id = "hand.tooltip_goes_over_incoming_message",
	    .name = "The hand's tooltip goes while the pointer is on the incoming message",
	    .facet = Facet::Hand,
	    .description = "The pointer rests in the middle of the screen, and the hand shows its tooltip. A message of the "
	                   "real weather comes in after half a second. One second later the pointer goes onto its button at "
	                   "the bottom left of the screen for six seconds. Then it goes back to the middle of the screen.",
	    .expected = "While the pointer is on the button, the hand is away. Its tooltip stays at the place where the hand "
	                "went away for a short time, and then it goes. In the middle of the screen the hand and its tooltip "
	                "show again.",
	    .framing = {.shot = Shot::Overview},
	    .commands =
	        {{.kind = Kind::PointerTo, .delaySeconds = 0.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::AddIncomingMessage, .delaySeconds = 0.5f, .value = 4, .action = "Guildford (Surrey,Great Britain)"},
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.01f, 0.98f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.5f}}},
	    .checks = {OnOffCheck("the hand shows a tooltip before the button", checks::Quantity::HandToolTipShown, true, 10, 14),
	               OnOffCheck("the tooltip has gone on the button", checks::Quantity::HandToolTipShown, false, 70, 74),
	               OnOffCheck("the tooltip shows again in the middle", checks::Quantity::HandToolTipShown, true, 80, 110)},
	});
	// A plain pot that holds nothing, such as a creature's handful from a field that it put down, is not drawn. The cursor
	// picks only what the game draws, so it never points at such a pot or takes it.
	all.push_back({
	    .id = "hand.empty_handful_not_picked",
	    .name = "The cursor does not pick an empty handful of wheat",
	    .facet = Facet::Hand,
	    .description = "Two handfuls of wheat lie on flat land, as a creature puts them down. The left one holds nothing, "
	                   "the right one holds 300 food. The empty hand goes over the left one for four seconds, then over "
	                   "the right one for four seconds.",
	    .expected = "No pot shows at the left place, and the hand over it shows the tooltip of the land. The pot at the "
	                "right place shows, and the hand over it offers to pick it up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 6.0f}},
	    .objects = {{.type = PotInfo::WheatInHand, .offset = {0.0f, 0.0f}, .amount = 0},
	                {.type = PotInfo::WheatInHand, .offset = {0.0f, 12.0f}, .amount = 300}},
	    .commands = {{.kind = Kind::PointerOnObject, .delaySeconds = 2.0f, .object = 0, .amount = 0.3f},
	                 {.kind = Kind::PointerOnObject, .delaySeconds = 4.0f, .object = 1, .amount = 0.3f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.5f, 0.9f}}},
	    .checks = {OnOffCheck("the cursor does not pick the empty handful", checks::Quantity::ObjectPicked, false, 30, 55, 0),
	               OnOffCheck("the cursor picks the full handful", checks::Quantity::ObjectPicked, true, 70, 95, 1)},
	});
}
