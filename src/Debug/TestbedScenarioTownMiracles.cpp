/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the debug tool that gives a town a miracle or takes it away

#include <vector>

#include <SDL_scancode.h>

#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town centre stands
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the player's temple stands, south of the town centre
constexpr glm::vec2 k_Temple {0.0f, -60.0f};
/// The town believes in the player, so that it stays theirs
constexpr float k_TownBelief = 1000.0f;
/// The first land, which has no worship sites in a normal game
constexpr int32_t k_FirstLand = 1;
/// The part of its miracle's charge that each worship site icon holds
constexpr float k_IconCharge = 0.45f;
/// The temple's main room, the one the entrance leads to
constexpr size_t k_MainRoom = 3;

[[nodiscard]] std::vector<ObjectSetup> Town()
{
	return {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	        {.type = AbodeInfo::NorseHut, .offset = k_Centre + glm::vec2(-20.0f, 5.0f)},
	        {.type = VillagerInfo::NorseHousewifeFemale, .offset = k_Centre + glm::vec2(10.0f, -10.0f), .joinTown = true},
	        {.type = VillagerInfo::NorseForesterMale, .offset = k_Centre + glm::vec2(-10.0f, -10.0f), .joinTown = true},
	        {.type = VillagerInfo::NorseFarmerMale, .offset = k_Centre + glm::vec2(5.0f, -15.0f), .joinTown = true}};
}

[[nodiscard]] Framing TownAndTemple()
{
	return {.shot = Shot::Overview,
	        .include = {k_Temple - glm::vec2(50.0f, 45.0f), k_Centre + glm::vec2(25.0f, 15.0f)},
	        .distance = 0.6f};
}
} // namespace

void openblack::testbed_scenarios::AddTownMiracleScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "town.miracle_debug",
	    .name = "Give a town a miracle with the debug tool",
	    .facet = Facet::Miracles,
	    .description = "A Norse town of the player with three villagers, its centre, and the player's temple built to "
	                   "the south, on the first land, which has no worship sites. The town has no miracles and no "
	                   "worship site. The Magic window opens: use its Town miracles tab.",
	    .expected = "Give the town a miracle (Heal, for example). A stone hand with the heal seed rises on the village "
	                "centre. The town's Norse worship site appears by the temple as a building site going up, with an "
	                "icon of the heal miracle. Take the miracle away: the hand and the site's icon go, the site stays.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .debugWindow = "Magic",
	});
	all.push_back({
	    .id = "town.miracle_debug_given",
	    .name = "A town given two miracles by the debug tool",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple on the first land. The debug tool gives the town the heal and the water "
	                   "miracles, as a land file line gives them, then lets the town have its worship site.",
	    .expected = "The village centre shows two stone hands, heal and water, each seed spinning over the palm. The "
	                "town's Norse worship site goes up by the temple for the town's builders, with an icon of each "
	                "miracle. The site's water icon shows no power-up ring, as the player may not cast the water's "
	                "power-up.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});

	all.push_back({
	    .id = "town.icon_seeds_paused",
	    .name = "A pause with seeds over the stone hands",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple on the first land, given the heal and the water miracles by the debug "
	                   "tool. Four seconds in, the game pauses, as the pause key does. Press P to continue the game.",
	    .expected = "Before the pause, the seeds over the stone hands of the village centre spin. While the game is "
	                "paused, they stand still. When the game continues, they spin on.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::PauseGame, .delaySeconds = 4.0f, .value = 1}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "town.miracle_spares_stone_hands",
	    .name = "A fireball on a village centre with stone hands",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple on the first land, given the heal and the water miracles. After four "
	                   "seconds a fireball falls on the village centre.",
	    .expected = "The blast and its fire hurt the centre and the villagers near it, and the centre can catch fire. "
	                "The two stone hands are not hurt and do not go: they are not in the land's cells, so the blast, "
	                "the fire and a heal do not find them. Each seed still spins over its palm, and a tap on a hand "
	                "still works.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = k_Centre,
	                  .handOffset = k_Centre - glm::vec2(0.0f, 10.0f),
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 4.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	// The Help key on the things of a temple and a town that have their own texts
	using Kind = Command::Kind;
	all.push_back({
	    .id = "advisors.help_key_leashes_and_hands",
	    .name = "The Help key on the temple's leashes and a village centre's stone hands",
	    .facet = Facet::Advisors,
	    .description = "The town and temple of town.miracle_debug_given, its centre with the heal and water stone "
	                   "hands. The player's tiger is taught the three leashes, which then hang in a row in front of the "
	                   "camera. Put the hand over each leash and over a stone hand, and press F1.",
	    .expected = "Over each leash the advisors say its name in the dialogue box, as its tooltip says it: Leash Of "
	                "Aggression, Leash Of Learning, Leash Of Compassion. Over a stone hand they say the stone hand's own "
	                "text. Over the temple they say the temple's text.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {6.0f, 8.0f, 0.0f}, .look = {6.0f, 4.0f, 30.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {-30.0f, 10.0f}}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Rope},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::HangLeashPosts, .creature = 0, .delaySeconds = 0.5f, .point = glm::vec2 {12.0f, 18.0f}}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "hand.tooltips_stone_hands",
	    .name = "The hand over a village centre's stone hands",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given, its centre with the heal and water stone hands, "
	                   "seen from near the centre. Put the hand over each stone hand.",
	    .expected = "Over each stone hand the tooltip names the hand's miracle at once, in the words of the miracle's "
	                "tooltip, with no mouse button drawn. Over the centre itself it shows the belief and the "
	                "population as before.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {-25.0f, 8.0f, 25.0f}, .look = {-10.0f, 3.0f, 37.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "hand.tooltips_worship_icons",
	    .name = "The hand over a worship site's icons, part charged",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given. When the town's worship site has its heal and "
	                   "water icons, each icon holds 45% of its miracle's charge. Put the hand over each icon of the site.",
	    .expected = "Over each icon the tooltip shows at once the name of the icon's miracle, then a space, the charge "
	                "right aligned to three places and a percent sign (\"Heal Miracle  45%\"), with no mouse button drawn. "
	                "Over a stone hand on the village centre it shows only the name.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::FillSiteIcons, .delaySeconds = 3.0f, .amount = k_IconCharge}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "hand.worship_icon_seed_in_hand",
	    .name = "A worship site's icon that charges while its seed is in the hand",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given. When the site has its heal icon, a tap puts the "
	                   "heal miracle in the hand. The seed keeps 45% of its charge, and the heal icon charges again for "
	                   "the hand. The battery of the site holds a full charge. Put the hand over the heal icon.",
	    .expected = "Each turn the icon gives its share of prayer power to the seed in the hand, not to its own store. "
	                "The icon stops when the seed is full: the seed shows its power-up effect once, and no second seed "
	                "comes into the hand. Over the heal icon the tooltip shows only the name. The inspector shows the "
	                "seed's charge go up to the full cost, and the icon's store stays at zero.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::ChargeIconWithSeedHeld,
	                  .delaySeconds = 3.0f,
	                  .value = static_cast<size_t>(SpellSeedType::Heal),
	                  .amount = k_IconCharge}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "miracles.selection_request",
	    .name = "A miracle asked for by gesture from a worship site's icon, then again by the R",
	    .facet = Facet::Miracles,
	    .description =
	        "The town and temple of town.miracle_debug_given. When the site has its heal and water icons, the site "
	        "is built and each icon holds the full charge of its miracle. The hand draws a spiral, then a W. Then a scribble "
	        "drops the water miracle, and the hand draws an R.",
	    .expected = "The spiral opens the miracle selection: white sparks come out of the hand, and the hints show the "
	                "gestures of the two miracles. The W closes the selection, and at the next turn the water miracle "
	                "comes into the hand. The scribble shakes the hand and drops the miracle. The R asks the water icon "
	                "again: at the next turn the icon starts to charge, and its charge goes up in the tooltip.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 1.0f},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Spiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 7.0f, .value = static_cast<size_t>(GestureType::WShape)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 9.0f, .value = static_cast<size_t>(GestureType::Scribble)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 9.0f, .value = static_cast<size_t>(GestureType::RShape)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	// The seeds that the recorded input of the hand draws gestures on and casts, for the dedicated server's check
	// (tools/sim/hand_inputs/casts.json)
	all.push_back({
	    .id = "miracles.recorded_casts",
	    .name = "Seeds for the recorded gestures and casts of the hand",
	    .facet = Facet::Miracles,
	    .description =
	        "The town and temple of town.miracle_debug_given. The site is built and each icon holds the full "
	        "charge of its miracle. A water miracle from the icon comes into the hand, then a fireball "
	        "miracle, then a water miracle again. The dedicated server and the game both play this scenario, and the game "
	        "makes the recorded input of tools/sim/hand_inputs/casts.json: a power-up, a scribble and a second "
	        "scribble on the first water miracle, the fireball thrown from the moving hand, and the second "
	        "water miracle held on the land while the hand moves.",
	    .expected = "At the turn after each gesture, the water miracle powers up, then goes back to its plain miracle, "
	                "then drops from the hand. The fireball flies the way that the hand moved. The water rains below the "
	                "hand while the button stays down, and stops when the button comes up.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 1.0f},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Water)},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 10.5f, .value = static_cast<size_t>(SpellSeedType::Fire)},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 5.0f, .value = static_cast<size_t>(SpellSeedType::Water)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "miracles.selection_keys",
	    .name = "The M and R keys ask a worship site for a miracle",
	    .facet = Facet::Miracles,
	    .description =
	        "The town and temple of town.miracle_debug_given. When the site has its heal and water icons, the site "
	        "is built and each icon holds the full charge of its miracle. The M key is pressed on the keyboard, and the "
	        "hand draws a W. Then a scribble drops the water miracle, and the R key is pressed on the keyboard.",
	    .expected = "The M key opens the miracle selection: white sparks come out of the hand, and the hints show the "
	                "gestures of the two miracles. The W closes the selection, and at the next turn the water miracle "
	                "comes into the hand. The scribble shakes the hand and drops the miracle. The R key asks the water "
	                "icon again: at the next turn the icon starts to charge, and its charge goes up in the tooltip.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 1.0f},
	                 {.kind = Kind::PressPlainKey, .delaySeconds = 3.0f, .value = static_cast<size_t>(SDL_SCANCODE_M)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 2.0f, .value = static_cast<size_t>(GestureType::WShape)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 9.0f, .value = static_cast<size_t>(GestureType::Scribble)},
	                 {.kind = Kind::PressPlainKey, .delaySeconds = 9.0f, .value = static_cast<size_t>(SDL_SCANCODE_R)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "interface.hint_selection_globe",
	    .name = "The globe over the hint of the spiral",
	    .facet = Facet::Interface,
	    .description = "The town and temple of town.miracle_debug_given. When the site has its heal and water icons, the "
	                   "site is built and each icon holds the full charge of its miracle. The hand is empty.",
	    .expected = "The hint along the bottom right of the screen shows the spiral. Over the spiral, a small globe "
	                "shows, as the globe of a miracle looks. The R has no hint until a miracle comes into the hand.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 1.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "advisors.help_key_in_temple",
	    .name = "The Help key inside the temple",
	    .facet = Facet::Advisors,
	    .description = "The town and temple of town.miracle_debug_given. The player goes into the temple's main room, "
	                   "and the Help key is pressed on the keyboard after two seconds. Press F1 again in any room.",
	    .expected = "Inside the temple the Help key does nothing: the advisors say nothing and no dialogue box opens. "
	                "Outside the temple the Help key works as before.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = k_MainRoom},
	                 {.kind = Kind::PressKey,
	                  .delaySeconds = 2.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::HELP)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
}
