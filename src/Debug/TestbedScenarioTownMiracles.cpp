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
/// Where the storage pit stands from the town centre
constexpr glm::vec2 k_StoragePit {25.0f, -5.0f};
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

/// The second land, the first land with worship sites in a normal game
constexpr int32_t k_SecondLand = 2;
/// The object type and the seed with which a script puts a one-shot heal miracle into the hand
constexpr float k_OneShotSpellInHand = 31.0f;
constexpr float k_HealSeed = static_cast<float>(SpellSeedType::Heal);
/// The plain fireball, the miracle that a challenge script gives the town
constexpr float k_Fireball = static_cast<float>(MagicType::Fireball);
/// Where a food miracle falls on the land away from every store, west of the town centre
constexpr glm::vec2 k_FoodDrop {-40.0f, 40.0f};
/// A point between the altar and the food pot of the worship site that the town builds by the temple
constexpr glm::vec2 k_SiteMiddle {-23.0f, -36.0f};

/// Worship site icons, the seeds that they make and the gifts of food on the second land, as a story land plays them
void AddSecondLandIconScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	using Argument = ScriptArgument;
	const Framing icons {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}};
	all.push_back({
	    .id = "worship.icons_charge_second_land",
	    .name = "Worship site icons charge, share the prayer power and give their miracle on the second land",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given on the second land, with the site built. The "
	                   "battery of the site gets the cost of the heal miracle, and a tap starts the heal icon. In the "
	                   "same turn a tap starts the water icon. After the first turn of the charge, a second tap on the "
	                   "heal icon stops it.",
	    .expected = "Each tap clicks: the click of the water icon is higher, because it stands further along the "
	                "site. At the next turn both icons charge, and each takes the same share of the battery. The second "
	                "tap on the heal icon clicks, stops it, and gives its store back to the battery (worship.sites). The "
	                "water icon is full at the turn after: the water miracle jumps into the hand and the announcer names "
	                "it. The heal icon keeps no store.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand},
	    .framing = icons,
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::TapSiteIcon,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(SpellSeedType::Heal),
	                  .amount = 1.0f},
	                 {.kind = Kind::TapSiteIcon, .value = static_cast<size_t>(SpellSeedType::Water)},
	                 {.kind = Kind::TapSiteIcon, .delaySeconds = 0.15f, .value = static_cast<size_t>(SpellSeedType::Heal)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "worship.one_shot_seed_from_icon",
	    .name = "A one-shot miracle that a script puts in the hand comes from the worship site icon",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given on the second land, with the site built and "
	                   "each icon a fifth charged. A script puts a one-shot heal miracle into the hand. Three seconds "
	                   "later a scribble shakes it off.",
	    .expected = "The heal seed is in the hand, charged full. The inspector (town.spells) shows the seed as a seed "
	                "out of the heal icon, so a power-up gesture can charge it there. The scribble shakes the hand, and "
	                "the battery of the site (worship.sites) goes up by the full cost of the heal miracle.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand},
	    .framing = icons,
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 0.2f},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = k_OneShotSpellInHand},
	                                      {.kind = Argument::Kind::Integer, .number = k_HealSeed},
	                                      {.kind = Argument::Kind::Position, .point = {9.0f, -19.0f}}}},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Scribble)}},
	    .hand = HandHold {.offset = {9.0f, -25.0f}, .height = 6.0f},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "advisors.food_gift_answer_second_land",
	    .name = "The town near a new pile of the player's food answers the gift on the second land",
	    .facet = Facet::Advisors,
	    .description = "The town of town.miracle_debug on the second land. Every kind of the advisors' guidance is taken "
	                   "as heard long ago. The player casts the food miracle on the land west of the town, away from "
	                   "every store, for three seconds.",
	    .expected = "The grains make a new pile of food. The people of the town call out how much they wanted the food, "
	                "heard near the pile: a pleased answer when the town wants food much, a displeased answer when it "
	                "wants it little, and no answer between. Only the first new pile gets an answer for a while. On the "
	                "first land no answer comes.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand},
	    .framing = {.shot = Shot::Overview, .include = {k_FoodDrop - glm::vec2 {20.0f}, k_Centre + glm::vec2 {20.0f}}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::Food,
	                  .point = k_FoodDrop,
	                  .handOffset = k_FoodDrop,
	                  .handHeight = 20.0f,
	                  .delaySeconds = 2.0f,
	                  .holdSeconds = 3.0f}},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "worship.site_pot_takes_food",
	    .name = "Food that the hand pours on a worship site goes into its food pot",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug on the second land, with the site built. Two seconds "
	                   "later the player casts the food miracle over the site, between its altar and its food pot, for "
	                   "three seconds.",
	    .expected = "The grains make no pile on the land. Their food goes into the food pot by the gate of the site, and "
	                "the inspector shows the food in the pot (game_entity of the food_pot of worship.sites). The site "
	                "takes food out to 1.2 times its radius from its middle.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand},
	    .framing = icons,
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::Food,
	                  .point = k_SiteMiddle,
	                  .handOffset = k_SiteMiddle,
	                  .handHeight = 20.0f,
	                  .delaySeconds = 5.0f,
	                  .holdSeconds = 3.0f}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "scripts.set_magic_in_town",
	    .name = "A challenge script gives a town a miracle, and refuses an object that is not a town",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given on the second land. A script gives the town the "
	                   "fireball miracle, as the fireball challenge does. Two seconds later the script tries to give the "
	                   "fireball to the town's hut.",
	    .expected = "The village centre gets a stone hand with the fireball seed, and the worship site gets a fireball "
	                "icon. The second call changes nothing, and the log says that the object should be a town.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_MAGIC_IN_OBJECT",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 1.0f},
	                                      {.kind = Argument::Kind::Integer, .number = k_Fireball},
	                                      {.kind = Argument::Kind::Town}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "SET_MAGIC_IN_OBJECT",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 1.0f},
	                                      {.kind = Argument::Kind::Integer, .number = k_Fireball},
	                                      {.kind = Argument::Kind::Object, .index = 1}}}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "worship.converted_town_adds_icons",
	    .name = "A town that the player converts adds its miracles to the player's worship site icons",
	    .facet = Facet::Miracles,
	    .description = "The town of town.miracle_debug_given on the second land, but the second player's, with the heal "
	                   "and water miracles. The player's temple stands to the south. The town believes 1 in the player and "
	                   "2 in the second player. A second in, the town stops its belief in the second player.",
	    .expected = "At the town's next turn the player takes the town. The town's Norse worship site goes up by the "
	                "player's temple with an icon of the heal miracle and an icon of the water miracle, and the player "
	                "may now cast both miracles.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::SetTownBeliefInRival, .delaySeconds = 1.0f, .amount = 0.0f}},
	    .townBeliefInPlayer = 1.0f,
	    .townBeliefInRival = 2.0f,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	    .townOwner = PlayerNames::PLAYER_TWO,
	});
}
} // namespace

std::vector<ObjectSetup> openblack::testbed_scenarios::WorshipTown()
{
	return Town();
}

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
	    .id = "town.stone_hand_holder_effects",
	    .name = "The effects of the seeds over a village centre's stone hands",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple on the first land, given six miracles by the debug tool: heal, "
	                   "fireball, lightning, water, teleport and storm. The camera looks at the village centre from near.",
	    .expected = "Each stone hand shows the effect of its seed's holder above its palm, in the player's colours. The "
	                "fireball, lightning, water, teleport and storm seeds have no model: only their effects show. The "
	                "heal seed spins with its effect above it. The effects move only while the hands are on the screen, "
	                "and they stop while the game is paused.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Centre.x + 6.0f, 14.0f, k_Centre.y - 30.0f},
	                .look = {k_Centre.x, 5.0f, k_Centre.y}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Fireball, MagicType::LightningBolt, MagicType::Water, MagicType::Teleport,
	                     MagicType::StormWindRain},
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
	    .id = "hand.tooltips_belief_gained",
	    .name = "The belief that the towns gained shows each tenth turn",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given, with an empty Norse storage pit by the centre. "
	                   "After three seconds the hand of the player pours 300 wood into the storage pit, and the town "
	                   "gains belief in the player for the gift. Keep the hand over the land by the town.",
	    .expected = "At the first turn that is a multiple of ten after the gift, the tooltip \"Got 12 Belief\" (with "
	                "the gain in thousandths) shows at the hand at once, with no mouse button drawn. A yellow number "
	                "of the same gain, four places wide, rises from the place under the hand. In the other turns no "
	                "such tooltip or number shows.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {-25.0f, 30.0f, 0.0f}, .look = {10.0f, 2.0f, 35.0f}},
	    .objects =
	        [] {
		        auto objects = Town();
		        objects.push_back(
		            {.type = AbodeInfo::NorseStoragePit, .offset = k_Centre + k_StoragePit, .storedFood = 0, .storedWood = 0});
		        return objects;
	        }(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::HandPour, .delaySeconds = 3.0f, .point = k_Centre + k_StoragePit, .value = 300}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.scribble_stops_icon_charge",
	    .name = "A scribble of the empty hand stops the charge of a worship site's icon",
	    .facet = Facet::Miracles,
	    .description =
	        "The town and temple of town.miracle_debug_given. When the site has its heal and water icons, the site "
	        "is built and each icon holds 45% of the charge of its miracle. The hand draws a spiral, then a W, so that "
	        "the water icon starts to charge for the hand. Then the empty hand draws a scribble.",
	    .expected = "After the W, the water icon charges, and its charge goes up in its tooltip. The scribble shakes "
	                "the hand, and no trail of the scribble shows on the land. At the next turn the water icon stops "
	                "its charge, and the prayer power that it stored goes back to the site.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = k_IconCharge},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Spiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 7.0f, .value = static_cast<size_t>(GestureType::WShape)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 6.0f, .value = static_cast<size_t>(GestureType::Scribble)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "miracles.icon_charge_looks",
	    .name = "The ring, the shine and the pulse of a worship site's icons as they charge",
	    .facet = Facet::Miracles,
	    .description =
	        "The town and temple of town.miracle_debug_given. When the site has its heal and water icons, the site "
	        "is built and each icon holds 45% of the charge of its miracle. The hand draws a spiral, then a W, so that "
	        "the water icon starts to charge for the hand. The camera looks at the icons from near.",
	    .expected = "A ring of light in red stands round the foot of each icon. The ring of the heal icon is 45% full, "
	                "and the heal icon does not shine. When the water icon starts to charge, its ring falls to a part "
	                "of its height and rises again as the icon charges. The water icon flashes red for 1.5 seconds, "
	                "then shines a dim red. Once in each 3.33 seconds, a large red pulse model shrinks into the water "
	                "icon over one second. When the water miracle comes into the hand, the water icon throbs in red "
	                "and its ring is full.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = k_IconCharge},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Spiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 7.0f, .value = static_cast<size_t>(GestureType::WShape)}},
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
	all.push_back({
	    .id = "miracles.selection_creature_spell",
	    .name = "A creature spell asked for by the inverse spiral and its gesture on the second land",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given on the second land, with the freeze spell and the "
	                   "water miracle. The site is built and each icon holds the full charge of its miracle. The hand "
	                   "draws an inverse spiral, then a star.",
	    .expected = "The inverse spiral opens the selection for the creature's miracles: white sparks come out of the "
	                "hand, and the hints show the star of the freeze spell but not the W of the water miracle. The star "
	                "closes the selection, and at the next turn the freeze spell comes into the hand.",
	    .environment = {.dispenserGrid = false, .landNumber = k_SecondLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 1.0f},
	                 {.kind = Kind::DrawGesture,
	                  .delaySeconds = 3.0f,
	                  .value = static_cast<size_t>(GestureType::InverseSpiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 4.0f, .value = static_cast<size_t>(GestureType::Star)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::CreatureSpellFreeze, MagicType::Water},
	});
	all.push_back({
	    .id = "miracles.power_up_icon_charge",
	    .name = "A power-up gesture asks the worship site icon of the miracle in the hand to charge it",
	    .facet = Facet::Miracles,
	    .description = "The town and temple of town.miracle_debug_given, with the storm, its extreme storm and the tornado. "
	                   "The site is built, and a tap puts the storm miracle from its icon into the hand. The battery of "
	                   "the site gets the cost of the tornado. The hand draws the inverse spiral, then a scribble, then "
	                   "a second scribble.",
	    .expected = "After the inverse spiral, its hint goes at once and the scribble shows the storm seed with no rings, "
	                "but the miracle in the hand stays the plain storm until the storm icon has charged the extra prayer "
	                "power into it. Then the hand shows the name of the extreme storm, a bracelet more and the bands. "
	                "The first scribble shakes the hand and the storm icon charges the seed back to the plain storm: "
	                "the name of the storm shows again, and the prayer power past its cost goes back to the site. The "
	                "second scribble drops the miracle.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::TakeIconSeed,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(SpellSeedType::Storm),
	                  .amount = 1.0f},
	                 {.kind = Kind::DrawGesture,
	                  .delaySeconds = 4.0f,
	                  .value = static_cast<size_t>(GestureType::InverseSpiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 8.0f, .value = static_cast<size_t>(GestureType::Scribble)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 7.0f, .value = static_cast<size_t>(GestureType::Scribble)}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado},
	});
	all.push_back({
	    .id = "miracles.selection_help_events",
	    .name = "The help events of the miracle selection and the R",
	    .facet = Facet::Miracles,
	    .description =
	        "The town and temple of town.miracle_debug_given. When the site has its heal and water icons, the site "
	        "is built and each icon holds the full charge of its miracle. The hand draws a spiral, then a scribble. Then "
	        "it draws a spiral and a W, a scribble drops the water miracle, and the hand draws an R.",
	    .expected = "The first spiral opens the miracle selection and the scribble closes it with a shake of the hand. The "
	                "second spiral opens it again, and the W chooses the water miracle, which comes into the hand. The "
	                "inspector query help.profile then counts event 15 two times and events 16, 17 and 21 one time each. "
	                "After the R it counts event 14 one time. Each event also counts towards events 24, 43 and 48.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {9.0f, 14.0f, -40.0f}, .look = {9.0f, 2.0f, -19.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 3.0f},
	                 {.kind = Kind::FillSiteIcons, .delaySeconds = 0.5f, .amount = 1.0f},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Spiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Scribble)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::Spiral)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 3.0f, .value = static_cast<size_t>(GestureType::WShape)},
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
	AddSecondLandIconScenarios(all);
}
