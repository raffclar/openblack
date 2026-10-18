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
	    .id = "footpaths.town_joins_site",
	    .name = "A town that joins its worship site gets a footpath to it",
	    .facet = Facet::Movement,
	    .description = "The Celtic town of the temple scenarios with a built storage pit north of it. The town's worship "
	                   "site is put up built, no one is sent to worship, and the Footpaths debug lines show the footpaths.",
	    .expected = "No footpath shows at first. A few turns after the site is put up, a footpath from the pit's door to "
	                "the site's gate shows. It goes round the town's buildings and the temple's spokes. Its ends are moved "
	                "out of the pit and the site.",
	    .environment = {.dispenserGrid = false, .drawFootpaths = true},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2(30.0f), k_Centre + glm::vec2(20.0f, 50.0f)},
	                .distance = 0.6f},
	    .objects =
	        [] {
		        auto objects = Town();
		        objects.push_back({.type = AbodeInfo::CelticStoragePit, .offset = k_Centre + glm::vec2(0.0f, 40.0f)});
		        return objects;
	        }(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
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

	// The hand gives the man to the altar while a tiger on the learning leash watches it
	auto copying = offer(k_Man, 4.0f);
	copying.insert(copying.begin(),
	               Command {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Rope});
	all.push_back({
	    .id = "learning.copy_sacrifice",
	    .name = "The creature copies a sacrifice at the altar",
	    .facet = Facet::Mind,
	    .description = "The same town and temple, the town's worship site put up built and nobody sent to worship. A grown "
	                   "tiger of the player's on the learning leash stands north of the altar and looks at it. The "
	                   "player's hand takes a man of the town and gives him to the altar with the Action button.",
	    .expected = "The tiger sees the sacrifice and copies it: it picks up a villager or an animal near it, carries it "
	                "to the altar, stops three of its heights away, faces the altar and puts the living thing down. The "
	                "log shows that the tiger copies the player with the Sacrifice action.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, 35.0f}, .look = {10.0f, 0.0f, -2.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {10.0f, 30.0f}}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = copying,
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
	all.push_back({
	    .id = "mind.altar_temple_part",
	    .name = "The altar of a worship site is a part of the temple",
	    .facet = Facet::Mind,
	    .description = "The same town and temple, the town's worship site put up built and nobody sent to worship. A grown "
	                   "tiger of the player's stands between the town and the site and faces the site. Its mind runs.",
	    .expected = "Within a few seconds the tiger's beliefs (the Mind panel) have the site's altar as a temple part of "
	                "the player, apart from the site, and both are parts of the temple. The altar is a thing that the "
	                "tiger can act on, as the heart of the temple is.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, 35.0f}, .look = {10.0f, 0.0f, -2.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = k_Watcher}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});
	all.push_back({
	    .id = "movement.past_altar",
	    .name = "A creature walks past the altar of a worship site",
	    .facet = Facet::Movement,
	    .description = "The same town and temple, the town's worship site put up built and nobody sent to worship. A grown "
	                   "tiger of the player's stands west of the site's altar and faces it. Its mind does not run.",
	    .expected = "The map_cell query of the altar's cell lists the altar in front of its site. Send the tiger east, past "
	                "the altar, with the creatures.move query. It goes round the circles of the site's model, which hold "
	                "the smaller circles of the altar, thus creatures.route keeps only the site's circles.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, 30.0f}, .look = {0.0f, 0.0f, -2.0f}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_Altar + glm::vec2(-18.0f, 0.0f),
	                   .facingDegrees = 90.0f,
	                   .pauseMind = true}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});
	all.push_back({
	    .id = "miracles.lightning_unbuilt_altar",
	    .name = "Lightning at the altar of a worship site not yet built",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple, the town's worship site just started and not yet built, so its altar is "
	                   "not shown. A lightning bolt is held from a hand eight metres from the altar's place, looking at it, "
	                   "for four seconds in every eight.",
	    .expected = "Some of the forks strike the altar's place, as it is in the map although not shown. After a moment "
	                "electric arcs crawl over the shape of the unseen altar, standing in the air above the empty ground. "
	                "The altar takes no harm.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, 30.0f}, .look = {0.0f, 0.0f, -2.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = k_Altar,
	                  .handOffset = k_Altar + glm::vec2(0.0f, 8.0f),
	                  .handHeight = 15.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.lightning_temple_town",
	    .name = "Lightning at a temple whose player has a town",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple, the town's worship site put up built. A lightning bolt is held from a "
	                   "hand above the temple's heart, looking at it, for four seconds in every eight.",
	    .expected = "The bolts that strike the temple and its worship site do not harm them. The heart gives the harm to "
	                "a building of the town. A beam goes from the top of the temple to the top of that building, at most "
	                "once every two seconds, with a sound. The building loses life and can catch fire.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {40.0f, 45.0f, 60.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = k_Temple,
	                  .handOffset = k_Temple + glm::vec2(0.0f, 20.0f),
	                  .handHeight = 25.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.0f,
	});
	all.push_back({
	    .id = "miracles.lightning_temple_alone",
	    .name = "Lightning at a temple whose player has no town",
	    .facet = Facet::Miracles,
	    .description = "A temple of the player alone, with no town. A lightning bolt is held from a hand above the "
	                   "temple's heart, looking at it, for four seconds in every eight.",
	    .expected = "The heart has nothing to give the harm to, thus it takes the harm itself and loses some life. A beam "
	                "goes from the top of the temple to a point on its own walls, at most once every two seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {40.0f, 45.0f, 30.0f}, .look = {0.0f, 0.0f, -30.0f}},
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = k_Temple,
	                  .handOffset = k_Temple + glm::vec2(0.0f, 20.0f),
	                  .handHeight = 25.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	});
	all.push_back({
	    .id = "miracles.lightning_flowers_dispenser",
	    .name = "Lightning at flowers and a miracle dispenser",
	    .facet = Facet::Miracles,
	    .description = "Two patches of flowers and a water dispenser of the player's town, 8 m apart. A lightning bolt "
	                   "is held from a hand above them, looking between them, for four seconds in every eight.",
	    .expected = "The flowers take no harm and stay. The dispenser loses life to the bolts that reach it. Its town "
	                "sends builders to mend it, and with no life left it goes as a ghost.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 25.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = FlowersInfo::WhiteOne, .offset = {-4.0f, 0.0f}},
	                {.type = FlowersInfo::Pink, .offset = {-4.0f, 4.0f}}},
	    .dispensers = {{.type = MagicType::Water, .offset = {4.0f, 0.0f}, .reward = true}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = {0.0f, 1.0f},
	                  .handOffset = {0.0f, 12.0f},
	                  .handHeight = 15.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	// The town centre close up, under miracles: from south east of it, above its huts
	const Framing closeOnCentre {.shot = Shot::Placed, .eye = {22.0f, 16.0f, 18.0f}, .look = {0.0f, 3.0f, 40.0f}};
	const MiracleCast lightningAtCentre {.type = MagicType::LightningBolt,
	                                     .target = MiracleCast::Target::Point,
	                                     .point = k_Centre,
	                                     .handOffset = k_Centre + glm::vec2(0.0f, -12.0f),
	                                     .handHeight = 18.0f,
	                                     .delaySeconds = 8.0f,
	                                     .holdSeconds = 4.0f,
	                                     .repeatSeconds = 8.0f};
	all.push_back({
	    .id = "miracles.fireball_town_centre",
	    .name = "Fireball at a town centre",
	    .facet = Facet::Miracles,
	    .description = "The Celtic town centre with its two huts and people, and no temple. A fireball is thrown from the "
	                   "south at the town centre.",
	    .expected = "The ball strikes the town centre and goes off. The town centre catches fire, with flames and smoke "
	                "on it, and loses life as it burns. Its people come out and the town cries out.",
	    .environment = {.dispenserGrid = false},
	    .framing = closeOnCentre,
	    .objects = Town(),
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = k_Centre,
	                  .handOffset = k_Centre + glm::vec2(0.0f, -14.0f),
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 12.0f},
	                  .delaySeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.lightning_town_centre",
	    .name = "Lightning at a town centre",
	    .facet = Facet::Miracles,
	    .description = "The Celtic town centre with its two huts and people, and no temple. A lightning bolt is held from "
	                   "a hand south of the town centre, looking at it, for four seconds in every eight.",
	    .expected = "The bolts strike the town centre and arcs crawl over it. It catches fire and loses life. Its people "
	                "come out and the town cries out.",
	    .environment = {.dispenserGrid = false},
	    .framing = closeOnCentre,
	    .objects = Town(),
	    .miracles = {lightningAtCentre},
	    .townBeliefInPlayer = k_TownBelief,
	});
	auto hurtTown = Town();
	hurtTown.front().life = 0.15f;
	all.push_back({
	    .id = "miracles.town_centre_destroyed",
	    .name = "A town centre destroyed by lightning",
	    .facet = Facet::Miracles,
	    .description = "The Celtic town centre at 15 percent of its life, with its two huts and people, and no temple. A "
	                   "lightning bolt is held from a hand south of the town centre, for four seconds in every eight.",
	    .expected = "The bolts take the last of its life. The town centre flickers out as a ghost and goes, with its totem "
	                "and its stone hands. Its town plans it again where it stood.",
	    .environment = {.dispenserGrid = false},
	    .framing = closeOnCentre,
	    .objects = hurtTown,
	    .miracles = {lightningAtCentre},
	    .townBeliefInPlayer = k_TownBelief,
	});
	// The town with its crèche and workshop to the south, and lanterns along the way to them
	constexpr glm::vec2 k_Creche {-14.0f, 12.0f};
	constexpr glm::vec2 k_Workshop {14.0f, 12.0f};
	constexpr glm::vec2 k_Lantern {-6.0f, 20.0f};
	// A lantern away from the town, for the lightning
	constexpr glm::vec2 k_LoneLantern {0.0f, -20.0f};
	const auto civicTown = [&](float crecheLife) {
		auto objects = Town();
		objects.push_back({.type = AbodeInfo::CelticCreche, .offset = k_Creche, .yawDegrees = 180.0f, .life = crecheLife});
		objects.push_back({.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f});
		objects.push_back({.type = MobileStaticInfo::StreetLantern, .offset = k_Lantern});
		objects.push_back({.type = MobileStaticInfo::StreetLantern, .offset = {6.0f, 20.0f}});
		objects.push_back({.type = MobileStaticInfo::CountryLantern, .offset = {0.0f, 4.0f}});
		return objects;
	};
	const Framing onCivic {.shot = Shot::Placed, .eye = {0.0f, 22.0f, -14.0f}, .look = {0.0f, 0.0f, 20.0f}};
	all.push_back({
	    .id = "advisors.building_tip_signs",
	    .name = "Tip signs at a crèche and a workshop",
	    .facet = Facet::Advisors,
	    .description = "The Celtic town with its crèche and workshop south of the town centre, and three lanterns. It is "
	                   "noon.",
	    .expected = "A sign stands in front of the crèche and one in front of the workshop, at the corner of each "
	                "building. Tap a sign and it gives the tip of its building. The town centre, the huts and the "
	                "lanterns have no sign.",
	    .environment = {.dispenserGrid = false},
	    .framing = onCivic,
	    .objects = civicTown(1.0f),
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.lanterns_at_night",
	    .name = "Street lanterns at night",
	    .facet = Facet::Miracles,
	    .description = "The Celtic town with its crèche, workshop, two street lanterns and a country lantern. The clock "
	                   "stands at ten at night.",
	    .expected = "Each lantern burns with two flames in a glow, which flicker. The land round each lantern is lit. "
	                "The lanterns crackle.",
	    .environment = {.hour = 22.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = onCivic,
	    .objects = civicTown(1.0f),
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.lightning_lantern",
	    .name = "Lightning at a street lantern at night",
	    .facet = Facet::Miracles,
	    .description = "A street lantern at 15 percent of its life and a country lantern 6 m east of it, away from any "
	                   "town, at ten at night. A lightning bolt is held from a hand above the street lantern for four "
	                   "seconds in every eight.",
	    .expected = "The bolts strike the lantern and it loses life. With no life left it goes at once, with no ghost, "
	                "and its flames, glow, light and crackle go with it. The country lantern stays.",
	    .environment = {.hour = 22.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 9.0f, -36.0f}, .look = {3.0f, 2.0f, -20.0f}},
	    .objects = {{.type = MobileStaticInfo::StreetLantern, .offset = k_LoneLantern, .life = 0.15f},
	                {.type = MobileStaticInfo::CountryLantern, .offset = k_LoneLantern + glm::vec2(6.0f, 0.0f)}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = k_LoneLantern,
	                  .handOffset = k_LoneLantern + glm::vec2(0.0f, -10.0f),
	                  .handHeight = 15.0f,
	                  .delaySeconds = 4.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.creche_destroyed_with_sign",
	    .name = "A crèche destroyed with its tip sign",
	    .facet = Facet::Miracles,
	    .description = "The Celtic town with its crèche at 40 percent of its life. After ten seconds, a lightning bolt is "
	                   "held from a hand above the crèche for four seconds in every eight.",
	    .expected = "The bolts take the last of the crèche's life. It flickers out as a ghost and goes, and its tip sign "
	                "goes with it. The workshop keeps its sign. When the town puts the crèche up again, a new sign "
	                "stands in front of the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-10.0f, 14.0f, -8.0f}, .look = {-14.0f, 0.0f, 12.0f}},
	    .objects = civicTown(0.4f),
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = k_Creche,
	                  .handOffset = k_Creche + glm::vec2(0.0f, -12.0f),
	                  .handHeight = 18.0f,
	                  .delaySeconds = 10.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	all.push_back({
	    .id = "miracles.lightning_fish_farm",
	    .name = "Lightning at a fish farm",
	    .facet = Facet::Miracles,
	    .description = "The Celtic town and a fish farm of the town 30 m west of its town centre. A lightning bolt is held "
	                   "from a hand above the farm for four seconds in every eight.",
	    .expected = "The bolts that reach the farm take its life, and the town cries out at the attack. With no life left "
	                "the farm goes, and the fishermen at it decide again what to do.",
	    .environment = {.dispenserGrid = false},
	    .framing = closeOnCentre,
	    .objects =
	        [] {
		        auto objects = Town();
		        objects.push_back({.type = FishFarmInfo::Normal, .offset = k_Centre + glm::vec2(-30.0f, 0.0f)});
		        return objects;
	        }(),
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .target = MiracleCast::Target::Point,
	                  .point = k_Centre + glm::vec2(-30.0f, 0.0f),
	                  .handOffset = k_Centre + glm::vec2(-30.0f, -10.0f),
	                  .handHeight = 15.0f,
	                  .delaySeconds = 4.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 8.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
}
