/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the villagers around the player's temple, with their debug window open, and of the
// town's worship site going up

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include "TempleVillagers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town centre stands
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the player's temple stands, near enough for the town's people to walk to its worship site
constexpr glm::vec2 k_Temple {0.0f, -30.0f};
/// The town's belief in the player, so that it stays theirs rather than going back to the neutral player
constexpr float k_TownBelief = 1000.0f;
/// Where the hand pours wood: at the edge of the town's worship site as it goes up, on its side away from the site's
/// wood pile, 33 m from the temple's middle but 15 m from the site's own middle, within its reach (its radius on the
/// ground, 14.1 m, times 1.2); then a little further out, 21 m from the site's middle, beyond it
constexpr glm::vec2 k_SiteEdge {-15.0f, -1.0f};
constexpr glm::vec2 k_BeyondTheSite {-21.0f, -1.0f};
/// How much wood each pour lets go of
constexpr size_t k_Handful = 300;
/// The main room of the temple, by the temple's rooms
constexpr size_t k_MainRoom = 3;
/// The town's first woman and first man, by their place in the scenario's objects
constexpr size_t k_Woman = 3;
constexpr size_t k_Man = 4;
/// A sheep and a birch beside the site, after the town in the scenario's objects
constexpr size_t k_Sheep = 11;
constexpr size_t k_Birch = 12;
/// A dead birch beside the site, after the town in the scenario's objects
constexpr size_t k_DeadTree = 11;
/// Where the creature that watches the hand's sacrifice stands: between the town and the site, facing the site
constexpr glm::vec2 k_Watcher {10.0f, 15.0f};
/// Where the altar of the town's worship site stands
constexpr glm::vec2 k_Altar {0.0f, -1.0f};
/// The Action button is the mouse's right button
constexpr size_t k_ActionButton = 3;

/// Where the town centre stands for the sacrifice, far enough north of the temple for a long walk to its altar
constexpr glm::vec2 k_FarCentre {0.0f, 90.0f};
/// The first of the town's people, by its place in the scenario's objects
constexpr size_t k_FirstVillager = 3;

/// A Celtic town centre of the player's, two huts and eight of its people about it
std::vector<ObjectSetup> Town(glm::vec2 centre = k_Centre)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = centre},
	    {.type = AbodeInfo::CelticHut, .offset = centre + glm::vec2(-16.0f, 6.0f), .yawDegrees = 160.0f},
	    {.type = AbodeInfo::CelticHut, .offset = centre + glm::vec2(16.0f, 6.0f), .yawDegrees = 200.0f},
	};
	constexpr std::array<VillagerInfo, 4> k_People {VillagerInfo::CelticHousewifeFemale, VillagerInfo::CelticFarmerMale,
	                                                VillagerInfo::CelticForesterMale, VillagerInfo::CelticShepherdMale};
	for (size_t i = 0; i < 8; ++i)
	{
		const auto across = static_cast<float>(i % 4) * 5.0f - 7.5f;
		const auto down = i < 4 ? -10.0f : -15.0f;
		objects.push_back(
		    {.type = k_People.at(i % k_People.size()), .offset = centre + glm::vec2(across, down), .joinTown = true});
	}
	return objects;
}
} // namespace

void openblack::testbed_scenarios::AddTempleVillagerScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "debug.temple_villagers",
	    .name = "Villagers around the temple",
	    .facet = Facet::Movement,
	    .description = "The player's temple standing south of a Celtic town of the player's with eight people. The town's "
	                   "worship site is put up built and its totem raised to half its people, and the Temple Villagers "
	                   "debug window opens.",
	    .expected = "Half the town sets off for the worship site by the temple, through its gate, and joins the dance round "
	                "the altar. The window lists them going from on the way to worshipping, their states and walks, and "
	                "marks them, their walks and the site's places in the world. Making villagers, sending them to "
	                "worship, the share slider, pausing and stepping all work from it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2(30.0f), k_Centre + glm::vec2(20.0f)},
	                .distance = 0.6f},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.5f,
	    .debugWindow = debug::gui::k_TempleVillagersWindow,
	});
	all.push_back({
	    .id = "debug.temple_villagers_inside",
	    .name = "Villagers inside the temple",
	    .facet = Facet::Movement,
	    .description = "The same town and temple, the whole town sent to worship; once they have had time to reach the "
	                   "site the player goes into the temple's main room with the Temple Villagers debug window open.",
	    .expected = "Those worshipping come in with the player and wander the main room's floors, walking from point to "
	                "point and now and then standing a few seconds. The window's Inside the temple list shows each, "
	                "walking or standing and where it is going, and marks them and the floors they walk.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2(30.0f), k_Centre + glm::vec2(20.0f)},
	                .distance = 0.6f},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::EnterTemple, .delaySeconds = 60.0f, .value = k_MainRoom}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 1.0f,
	    .debugWindow = debug::gui::k_TempleVillagersWindow,
	});
	all.push_back({
	    .id = "town.worship_site_pour",
	    .name = "Wood poured on a worship site going up",
	    .facet = Facet::Objects,
	    .description = "The same town and temple, the town's worship site just started and not yet built. The player's "
	                   "hand pours a handful of wood (300) at the site's edge, well out from the temple's middle, then another "
	                   "a few metres further out, beyond the site.",
	    .expected = "The first pour goes into the site's wood pile for its builders (the pile grows and no loose wood is left "
	                "there), because a worship site takes what is poured within its reach of its own middle, not the "
	                "temple's. The second, beyond that reach, is left on the land as a loose pile of wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2(40.0f), k_Temple + glm::vec2(40.0f)}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::HandPour, .delaySeconds = 3.0f, .point = k_SiteEdge, .value = k_Handful},
	                 {.kind = Command::Kind::HandPour, .delaySeconds = 4.0f, .point = k_BeyondTheSite, .value = k_Handful}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "objects.creature_sacrifice",
	    .name = "The creature takes a villager to the altar",
	    .facet = Facet::Objects,
	    .description = "A grown tiger of the player's stands by a Celtic town of the player's, far north of the player's "
	                   "temple. The town's worship site stands built by the temple, with no one sent to worship. A script "
	                   "tells the tiger to sacrifice the first of the town's people.",
	    .expected = "The tiger picks the villager up with an angry face, and looks at the villager as it goes to it. It "
	                "walks south with the villager in its hand and looks ahead. It stops three of its heights from the "
	                "altar of the worship site, turns to face the altar and looks at it. Then it puts the villager down "
	                "still. The villager lands alive and walks away: as in the original game, the altar does not take it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {40.0f, 14.0f, 18.0f}, .look = {0.0f, 8.0f, 8.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = k_FarCentre + glm::vec2(0.0f, -30.0f)}},
	    .objects = Town(k_FarCentre),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::DoAction, .delaySeconds = 3.0f, .object = k_FirstVillager, .action = "Sacrifice"}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});

	// The hand takes a man, then a woman, of the town and gives each to the altar with the Action button
	using Kind = Command::Kind;
	const auto offer = [](size_t villager, float delay) {
		// The hand is first put on the altar, inside the player's influence, where it can take the villager
		return std::vector<Command> {{.kind = Kind::PointerOnAltar, .delaySeconds = delay},
		                             {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = villager},
		                             {.kind = Kind::PointerOnAltar, .delaySeconds = 1.0f},
		                             {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = k_ActionButton},
		                             {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton}};
	};
	auto commands = offer(k_Man, 3.0f);
	for (const auto thing : {k_Woman, k_Sheep, k_Birch})
	{
		std::ranges::copy(offer(thing, 4.0f), std::back_inserter(commands));
	}
	auto objects = Town();
	objects.push_back({.type = AnimalInfo::Sheep, .offset = {14.0f, 8.0f}});
	objects.push_back({.type = TreeInfo::Birch, .offset = {-14.0f, 8.0f}});
	all.push_back({
	    .id = "hand.sacrifice_at_altar",
	    .name = "The hand sacrifices villagers, an animal and a tree at the altar",
	    .facet = Facet::Hand,
	    .description = "The same town and temple, the town's worship site put up built and nobody sent to worship. The "
	                   "player's hand takes a man of the town, moves onto the site's altar and presses the Action "
	                   "button; then it does the same with a woman, a sheep and a birch.",
	    .expected = "Each villager goes from the hand at once, with no throw. The sacrifice sound plays where the hand is, "
	                "and the man screams, then the woman. For each villager a light rises from the top of the altar. A red "
	                "number, the prayer power stored at the site, floats up from the altar for each of the four. The "
	                "sheep and the birch make no cry and no light; the birch leaves a ghost that flickers out.",
	    .environment = {.dispenserGrid = false},
	    // Close on the altar, which shows right of the middle of the screen, clear of the testbed's window
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, 35.0f}, .look = {10.0f, 0.0f, -2.0f}},
	    .objects = objects,
	    .temples = {{.offset = k_Temple}},
	    .commands = commands,
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});

	// The hand gives a man and then a dead tree to the altar, then rests on the altar empty
	auto withDeadTree = Town();
	withDeadTree.push_back({.type = TreeInfo::Birch, .offset = {-14.0f, 8.0f}, .dead = true});
	auto deadTreeCommands = offer(k_Man, 3.0f);
	std::ranges::copy(offer(k_DeadTree, 4.0f), std::back_inserter(deadTreeCommands));
	deadTreeCommands.push_back({.kind = Kind::PointerOnAltar, .delaySeconds = 2.0f});
	all.push_back({
	    .id = "hand.sacrifice_dead_tree",
	    .name = "The hand sacrifices a dead tree and shows the altar's prayer energy",
	    .facet = Facet::Hand,
	    .description = "The same town and temple, the town's worship site put up built and nobody sent to worship, with a "
	                   "dead birch on the land beside the site. The player's hand gives a man of the town to the site's "
	                   "altar, then takes the dead birch, moves onto the altar and presses the Action button. Then the "
	                   "empty hand stays on the altar.",
	    .expected = "The man gives 10000 prayer power to the site. While the hand holds the dead birch over the altar, the "
	                "tooltip says \"Sacrifice\". The dead birch goes from the hand at once, with no throw: the sacrifice "
	                "sound plays and the birch leaves a ghost that flickers out. No light and no number come from the "
	                "altar, and the site's store does not change: the game's table gives a dead tree no sacrifice value. "
	                "Over the altar the empty hand shows \"Prayer Energy: 10000\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, 35.0f}, .look = {10.0f, 0.0f, -2.0f}},
	    .objects = withDeadTree,
	    .temples = {{.offset = k_Temple}},
	    .commands = deadTreeCommands,
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});

	// A tiger on the learning leash watches the hand give a man to the altar; then the leash comes off
	// The leash goes on with the hand already on the altar, so that the leash keeps the tiger near the altar. The hand
	// goes there two times: the first time, the camera has not yet come to its place.
	const std::vector<Command> copyCommands {
	    {.kind = Kind::PointerOnAltar, .delaySeconds = 3.0f},
	    {.kind = Kind::PointerOnAltar, .delaySeconds = 1.0f},
	    {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Rope},
	    {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = k_Man},
	    {.kind = Kind::PointerOnAltar, .delaySeconds = 1.0f},
	    {.kind = Kind::TurnToFace, .creature = 0, .delaySeconds = 0.2f, .point = k_Altar},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.3f, .value = k_ActionButton},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton},
	    {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 3.0f}};
	all.push_back({
	    .id = "mind.copy_sacrifice",
	    .name = "The creature copies the hand's sacrifice",
	    .facet = Facet::Mind,
	    .description = "The same town and temple, the town's worship site put up built and nobody sent to worship. A grown "
	                   "tiger of the player's stands between the town and the site, on the learning leash, and faces the "
	                   "site. The player's hand takes a man of the town, moves onto the site's altar, the tiger turns to "
	                   "face the altar, and the hand presses the Action button. Then the leash comes off.",
	    .expected = "The tiger sees the deed, if its eyes are on the altar at that time: a tiger that looks at the sky or "
	                "the sea does not see it (about one run in three). Its thoughts show \"I saw you: Sacrifice\". The "
	                "man is gone, so the tiger does not show that it saw him. About 9 seconds later it copies the deed: it "
	                "picks up a villager of the same kind with an "
	                "angry face, takes it to the altar, faces the altar and puts the villager down alive. The log shows "
	                "\"I'm copying you: Sacrifice\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, 35.0f}, .look = {10.0f, 0.0f, -2.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = k_Watcher}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = copyCommands,
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});
}
