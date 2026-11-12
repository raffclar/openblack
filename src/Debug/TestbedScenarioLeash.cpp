/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the leashes of the creature.

#include "Creature/CreatureCave.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Input/BindableActions.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddLeash(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "leash.types",
	    .name = "Led on each leash",
	    .facet = Facet::Leash,
	    .description = "A tiger is put on the learning leash, then the aggression leash, then the compassion leash, a "
	                   "while each, and the leash is taken off. Move the hand about to lead it.",
	    .expected = "A rope runs from the hand to its collar, coloured for each leash; on the aggression leash anger is "
	                "what it wants most, on the compassion leash kindness and making friends, and on the learning leash "
	                "it watches the player. Taken off the compassion leash, that mood ends at once and its other wants "
	                "come back; taken off the learning leash, nothing it wants changes.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Leash(0, LeashType::Evil, 10.0f), Leash(0, LeashType::Good, 10.0f),
	                 Act(Kind::TakeOffLeash, 0, 10.0f), Act(Kind::Stop, 0, 3.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.orders",
	    .name = "Orders given with the leash",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is told with the Action button to go to a point to the east, told "
	                   "again before it gets there, then to act on a rock, a cow, and the lake.",
	    .expected = "Each order is answered with the acknowledge sound, a nod and the tiger's voice, and marked: six pale "
	                "yellow sparkles over the place, and a throbbing yellow ring with a paw print inside, which follows "
	                "the cow. Told again it runs and then waits a few seconds; the rock it looks at, the cow it picks "
	                "up and holds, at the lake it drinks or looks at its reflection. One marker at a time.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {0.0f, k_NearShallows}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {-40.0f, 30.0f}},
	                {.type = AnimalInfo::Cow, .offset = {30.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 2.0f, false), OnObject(Kind::LeashOrderOn, 0, 0, 10.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 1, 12.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows + 10.0f}, 15.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 15.0f)},
	    .repeatFrom = 0,
	});

	// A small tiger, 6 metres high, so that eight of its heights (48 metres) fit in the view
	constexpr glm::vec2 k_ThrowNear {20.0f, 0.0f};
	constexpr glm::vec2 k_ThrowFar {-110.0f, 0.0f};
	all.push_back({
	    .id = "leash.throw_at_place",
	    .name = "A thing carried on the aggression leash is thrown at a place",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 0.4 on the aggression leash picks up a rock. With the Action button the player "
	                   "tells it to go to a place about 16 metres away. When the rock is still, the tiger picks it up "
	                   "again, and the player tells it to go to a place far to the west.",
	    .expected = "The tiger does not carry the rock to the place. For the near place, it stays where it is and throws "
	                "the rock at the place. The game sends it back to eight of its heights (48 metres) from the place, "
	                "but the walk ends at once, because the creature is already in the ring of that walk. For the far "
	                "place, it walks until it is eight of its heights from the place, then it throws the rock at the "
	                "place.",
	    .framing = {.shot = Shot::Overview, .include = {k_ThrowFar, k_ThrowNear + glm::vec2(48.0f, 0.0f)}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger, .offset = {0.0f, 0.0f}, .size = 0.4f, .needs = Content(), .hold = true}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {4.0f, -6.0f}}},
	    .commands = {Leash(0, LeashType::Evil, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Go(Kind::LeashOrderAt, 0, k_ThrowNear, 1.0f), OnObject(Kind::PickUp, 0, 0, 8.0f),
	                 Go(Kind::LeashOrderAt, 0, k_ThrowFar, 1.0f), Act(Kind::TakeOffLeash, 0, 6.0f, true)},
	    .repeatFrom = 0,
	});

	// A man held by a script out in the lake, as the drowning men of Land 1's silver scroll, and a tiger that shrinks
	// in its temple's pen first, so that it grows again on its way out to him
	constexpr glm::vec2 k_RescueTemple {-150.0f, 150.0f};
	constexpr glm::vec2 k_DrowningMan {-30.0f, k_NearShallows + 8.0f};
	constexpr glm::vec2 k_Bank {-30.0f, k_NearShallows - 20.0f};
	all.push_back({
	    .id = "leash.rescue_from_water",
	    .name = "A man carried out of the water on the leash",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 0.5 walks home into its temple's pen, where it shrinks, and then goes on the "
	                   "learning leash. The Action button of the leash orders it onto a man that a script holds still out "
	                   "in the lake, as the drowning men of Land 1, and some time later onto the bank.",
	    .expected = "It grows back to its own size as it leaves the pen, wades out, picks the man up and holds him, "
	                "facing the player. Told onto the bank, it carries him there and puts him down gently on his feet, "
	                "with no toss: he stays where its hand sets him.",
	    .framing = {.shot = Shot::Overview, .include = {k_RescueTemple, k_DrowningMan, k_Bank}},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, k_RescueTemple + glm::vec2(60.0f, -40.0f));
		    tiger.size = 0.5f;
		    return tiger;
	    }()},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_DrowningMan, .inScript = true}},
	    .temples = {{.offset = k_RescueTemple, .angle = 36.0f}},
	    .commands = {Act(Kind::WalkHome, 0, 1.0f), Leash(0, LeashType::Rope, 25.0f),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 1.0f, false), Go(Kind::LeashOrderAt, 0, k_Bank, 45.0f, false)},
	});

	// A full size tiger stops its walk to a man just inside the reach of its hand, which is less straight ahead, so it
	// turns on the spot before it reaches
	constexpr glm::vec2 k_StillMan {40.0f, -40.0f};
	all.push_back({
	    .id = "leash.pick_up_turns_into_reach",
	    .name = "A full size creature turns to get a man in reach",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 1 goes on the learning leash. The Action button of the leash orders it onto a "
	                   "man that a script holds still on the land in front of it.",
	    .expected = "It walks up to the man and stops a hand's reach from him. It turns a little on the spot, so that "
	                "the man is in front of its hand, then picks him up and holds him. Before, it reached straight "
	                "ahead with nothing in its hand.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 20.0f}, k_StillMan}},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, {0.0f, 20.0f});
		    tiger.size = 1.0f;
		    return tiger;
	    }()},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_StillMan, .inScript = true}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false)},
	});

	all.push_back({
	    .id = "objects.pick_up_from_far",
	    .name = "A creature walks a long way to a small thing and picks it up",
	    .facet = Facet::Objects,
	    .description = "A tiger of size 1 has a lump of poo 40 metres in front of it. After one second its alignment jumps "
	                   "to fully evil, so its body takes the evil shape after it first measured its reach. After one and "
	                   "a half seconds more it is told to pick up the poo.",
	    .expected = "It walks to the poo and stops on the place of the poo, as the game does: the walk goes to the place "
	                "itself, and the reach starts when the walk ends. Then it picks the poo up and holds it. Before, it "
	                "stopped 11 metres from the poo, at twice the two radii.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 60.0f}, {0.0f, 20.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .size = 1.0f}},
	    .objects = {{.type = MobileObjectInfo::LumpOfPoo, .offset = {0.0f, 20.0f}}},
	    .commands = {{.kind = Kind::SetBodyLook, .creature = 0, .delaySeconds = 1.0f, .value = 0, .amount = -1.0f},
	                 OnObject(Kind::PickUp, 0, 0, 1.5f)},
	});

	// Worn out and wanting to sleep more than anything, far from its player's temple
	CreatureSetup tired {.species = CreatureType::Tiger,
	                     .offset = {0.0f, 20.0f},
	                     .needs = {.energy = 1.0f, .exhaustion = 0.95f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                     .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}};
	all.push_back({
	    .id = "leash.tired_orders",
	    .name = "Orders given with the leash to a tired creature",
	    .facet = Facet::Leash,
	    .description = "A worn out tiger (exhaustion 0.95) that wants to sleep is on the learning leash, far from its "
	                   "player's temple. After eight seconds it is told to go to a point to the east, and after twenty "
	                   "seconds to a point to the west.",
	    .expected = "It sets off home to sleep. Each order stops that: it nods and walks to the place it is told. Once "
	                "there and the order is done, it sets off home to sleep again.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {-60.0f, 40.0f}}},
	    .creatures = {tired},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 7.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {-60.0f, 40.0f}, 12.0f, false)},
	});

	// Wholly exhausted, and as young as a creature on the first land, too young to faint
	auto exhausted = tired;
	exhausted.needs.exhaustion = 1.0f;
	exhausted.phase = 4;
	// It stays exhausted though it sleeps
	exhausted.hold = true;
	all.push_back({
	    .id = "leash.exhausted_orders",
	    .name = "Orders given with the leash to a wholly exhausted creature",
	    .facet = Facet::Leash,
	    .description = "A young, wholly exhausted tiger (exhaustion 1) is on the learning leash, far from its player's "
	                   "temple. It is too young to faint, and stays exhausted. After eight seconds it is told to go to a "
	                   "point to the east.",
	    .expected = "It takes no order: no nod, no marker, and it does not go east. The leash still keeps it near: if "
	                "it strays too far on its way to sleep, it walks back. Then it sleeps. An older creature this "
	                "exhausted faints and lies out cold.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {-60.0f, 40.0f}}},
	    .creatures = {exhausted},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 7.0f, false)},
	});

	all.push_back({
	    .id = "leash.order_hurries",
	    .name = "An order with the leash hurries what the body does",
	    .facet = Facet::Leash,
	    .description = "Two tigers sit side by side. The left tiger is on the learning leash. After eight seconds, the "
	                   "left tiger is told to go to a point to the east. At the same time, the right tiger is told to "
	                   "stand up, with no order.",
	    .expected = "The left tiger gets up at two times the usual speed, nods, and walks east. The right tiger gets up "
	                "at the usual speed. A kiss, a cast and a throw keep their usual speed when an order comes.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 20.0f}, {60.0f, 40.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-15.0f, 20.0f}), Content(CreatureType::Tiger, {15.0f, 20.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Act(Kind::SitDown, 0, 1.0f), Act(Kind::SitDown, 1, 0.0f),
	                 Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 8.0f, false), Act(Kind::StandUp, 1, 0.0f)},
	});

	all.push_back({
	    .id = "leash.marker_flat_and_slope",
	    .name = "The marker of a leash order on flat land and on a slope",
	    .facet = Facet::Leash,
	    .description = "A tiger far away on the learning leash is told with the Action button to go to a point on the "
	                   "flat land. Twenty seconds later it is told to go to a point halfway down the steep bank of the "
	                   "lake.",
	    .expected = "Each time a yellow ring hangs two metres above the point, with a paw print inside. The ring and the "
	                "print always face the camera and do not lie along the land, also on the bank. The ring throbs, its "
	                "width and its height a fifth of a second apart. The print stands on its side, with its top to the "
	                "left, and it is narrower than the ring.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, -40.0f}, {0.0f, k_NearShallows - 28.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-150.0f, -100.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {40.0f, -40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows - 28.0f}, 20.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 30.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "mind.step_looks_reflection",
	    .name = "Looks at its reflection with the faces of each step",
	    .facet = Facet::Mind,
	    .description = "A tiger that is not thirsty, on the learning leash, away from the lake, is told with the Action "
	                   "button to go to the lake.",
	    .expected = "It walks to the water of the nearest region of sea on its map of the land, its head on the place. "
	                "Then it looks down at the water four metres in front of its feet, puzzled, plays its dazed clip "
	                "with an amazed face, and looks down again, puzzled. It walks to the nearest coast with an amazed "
	                "face, looks down with a curious face, plays its dazed clip, walks back to the water amazed, and "
	                "looks down, first curious and then amazed. The coast and the water are near each other, so these "
	                "two walks are short or stop at once.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -60.0f}, {0.0f, k_NearShallows}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, -60.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows + 10.0f}, 2.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 45.0f)},
	});

	all.push_back({
	    .id = "mind.step_looks_set_fire",
	    .name = "Sets fire to a tree with the looks and faces of each step",
	    .facet = Facet::Mind,
	    .description = "A script tells a player's lion to set fire to an oak with a burning villager beside it.",
	    .expected = "It walks to the villager, its head on the villager, and picks him up. It walks to the oak, its head "
	                "on the oak. It turns to the oak with an angry face and flutters its eyelids at it, throws the villager "
	                "at it with a smile, watches him until he lands, then holds still a second with an angry face.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-60.0f, 40.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-20.0f, 10.0f}, .alight = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SetFireToObject", .with = 1}},
	});

	all.push_back({
	    .id = "senses.look_at_mover",
	    .name = "Looks at a thing that moves as it walks",
	    .facet = Facet::Senses,
	    .description = "A tiger on the learning leash is told to go to a point far to the north. Behind it, two "
	                   "villagers walk away to the west and to the east.",
	    .expected = "On its way the head looks at the point for two seconds. For the next two seconds it turns back to "
	                "a villager that walks, at the place where the tiger last saw the villager move. Then it looks from "
	                "side to side for two seconds. Near the point it looks only at the point.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 110.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticForesterMale,
	                 .offset = {-30.0f, -5.0f},
	                 .walkTo = glm::vec2(-90.0f, -5.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 30.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {30.0f, -5.0f},
	                 .walkTo = glm::vec2(90.0f, -5.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 30.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {0.0f, 110.0f}, 2.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 0.0f}, 30.0f, false)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "senses.hang_around_at_home",
	    .name = "Hangs around at home",
	    .facet = Facet::Senses,
	    .description = "A tiger kept within 40 of its home at the temple is told to hang around at home, again and again. "
	                   "A birch stands to the west as the thing that the order names.",
	    .expected = "It walks home and waits for two to five seconds while it looks about. Then, one time in ten, it yawns. "
	                "Seven times in ten, it walks 32 out from home away from the temple, faces away from the temple and "
	                "sits or points. Then it turns to the temple and sits for ten to fifteen seconds. Two times in ten, it "
	                "only turns to the temple and sits.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-60.0f, 0.0f}}},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HangAroundAtHome"},
	                 {.kind = Kind::DoAction, .delaySeconds = 40.0f, .object = 0, .action = "HangAroundAtHome"}},
	    .repeatFrom = 2,
	});

	all.push_back({
	    .id = "leash.temple_leashes",
	    .name = "The leashes hanging on the temple",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows the learning leash. The temple's three leashes are hung in a row in front of "
	                   "the camera; then it is taught the aggression and compassion leashes, the aggression leash is "
	                   "tapped, and tapped again.",
	    .expected = "At first only the learning leash hangs, a twisted rope collar in a white smoky glow, tumbling slowly "
	                "with its texture running round it. Taught the others, the spiked blade (aggression) and the rainbow "
	                "fur (compassion) hang beside it. Tapped with the click sound, the aggression leash is carried in the "
	                "hand and its glow at the temple turns orange; tapped again it hangs back in place, silently. The "
	                "hand over each names it: Leash Of Aggression, Leash Of Learning, Leash Of Compassion.",
	    .framing = {.shot = Shot::Placed, .eye = {6.0f, 6.0f, 26.0f}, .look = {9.0f, 5.0f, 40.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 70.0f})},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                 Go(Kind::HangLeashPosts, 0, {14.0f, 40.0f}, 0.5f, false),
	                 Leash(0, LeashType::Evil, 4.0f),
	                 Leash(0, LeashType::Good, 0.5f),
	                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 4.0f, .value = 0},
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 4.0f, .value = 0}},
	});

	const auto templeGlows = [](std::string_view id, std::string_view name, float hour) {
		return Scenario {
		    .id = id,
		    .name = name,
		    .facet = Facet::Leash,
		    .description = "Your tiger knows all three leashes and your temple stands in front of the camera, its leash posts "
		                   "in view. The learning leash is picked at its post, so the hand carries it.",
		    .expected = "The aggression leash (the spiked blade) and the compassion leash (the rainbow fur) tumble slowly on "
		                "their posts, each in a pale smoky glow about four metres across, half seen by day and fading at "
		                "night. By the empty learning post the glow is orange and added to what is behind it. The learning "
		                "leash, a twisted rope collar, hangs round the hand's wrist, turned with the hand, its texture "
		                "running round it. Tapping the orange glow puts the leash back on its post, silently; with the hand "
		                "put away the carried leash and its orange glow are gone.",
		    .environment = {.hour = hour, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {-262.0f, 30.0f, 120.0f}, .look = {-300.0f, 5.0f, 100.0f}},
		    .creatures = {Posed(CreatureType::Tiger, {-240.0f, 40.0f}, 90.0f)},
		    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
		    .commands = {Leash(0, LeashType::Rope, 0.5f),
		                 Leash(0, LeashType::Evil, 0.5f),
		                 Leash(0, LeashType::Good, 0.5f),
		                 Act(Kind::TakeOffLeash, 0, 0.5f),
		                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 1}},
		};
	};
	all.push_back(templeGlows("leash.temple_glows", "The glows round the temple's leashes by day", 12.0f));
	all.push_back(templeGlows("leash.temple_glows_night", "The glows round the temple's leashes at night", 23.0f));

	all.push_back({
	    .id = "leash.l_key_picks_post",
	    .name = "The leash key picks the leash at the temple",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes and your temple stands in front of the camera. The compassion "
	                   "leash is tapped at its post, then tapped again, so no leash is picked. Then the leash key puts "
	                   "the compassion leash on. Last, the leash key takes it off.",
	    .expected = "After the second tap, no glow is orange and the hand carries no leash. When the leash key puts the "
	                "leash on, the compassion leash (the rainbow fur) goes round the hand's wrist again and its glow "
	                "turns orange. When the leash comes off, the leash stays in the hand and its glow stays orange.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-262.0f, 30.0f, 120.0f}, .look = {-300.0f, 5.0f, 100.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-240.0f, 40.0f}, 90.0f)},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 2},
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 2.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "leash.pull_to_hand",
	    .name = "Pulled to the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger far off to the north is put on the learning leash. Put the hand on the land near the "
	                   "camera, well away from it.",
	    .expected = "When the tiger is further from the hand than the full length of the leash, it stops what it does "
	                "and walks back. It is faster when it is further away. It walks to the point below the hand. When it "
	                "goes out of the area again, it walks back again.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 150.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f)},
	});

	all.push_back({
	    .id = "leash.walk_back_on_the_way",
	    .name = "Walked back while on the way",
	    .facet = Facet::Leash,
	    .description = "A tiger starts in the middle of the testbed. It is kept within 400 units of where it stands, as "
	                   "a young creature is kept near its home. After one second its player orders it to look at its "
	                   "reflection, and it walks north to the shore of the lake. After six seconds, on its way, the "
	                   "area that holds it becomes 10 units wide.",
	    .expected = "When the area becomes small, the tiger is out of it. It turns at once and walks back to where it "
	                "started. It does not walk on to the lake first.",
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -20.0f}, {60.0f, 160.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 0.0f},
	    }},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 400.0f},
	                 {.kind = Kind::ForceAction, .delaySeconds = 1.0f, .action = "LookAtReflection"},
	                 {.kind = Kind::ConfineToHome, .delaySeconds = 6.0f, .radius = 10.0f}},
	});

	all.push_back({
	    .id = "leash.tied",
	    .name = "Tied to a tree and a rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and told to walk far off, untied back to the "
	                   "hand, then tied to a pillar of rock and told to walk far off again.",
	    .expected = "The rope runs from what it is tied to. The walk of the testbed moves the tiger directly, not as an "
	                "order from the player, so the tie does not stop it and the rope stretches to it. Untied, the rope "
	                "keeps its shape and swings back to the wrist of the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 60.0f}, {120.0f, 60.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Go(Kind::WalkTo, 0, {-120.0f, 60.0f}, 1.0f, false), Act(Kind::UntieLeash, 0, 8.0f),
	                 OnObject(Kind::TieLeash, 0, 1, 2.0f, false), Go(Kind::WalkTo, 0, {120.0f, 60.0f}, 1.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 8.0f)},
	    .repeatFrom = 0,
	});

	// The camera looks down at 45 degrees on a very large oak from about 150 m, the pointer on its crown. A tiger is tied
	// to the oak. The hand pulls the oak for a tenth of a second and lets go, so the oak stays drawn in the lean of the
	// pull. Then the leash key unties the leash to the hand.
	all.push_back({
	    .id = "leash.tied_tree_pulled",
	    .name = "Rope of a leash tied to a pulled tree",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a very large oak. The hand pulls the oak for a tenth of "
	                   "a second and lets go. Three seconds later, the leash key unties the leash to the hand.",
	    .expected = "The rope meets the oak halfway up the oak and moves with the lean of the pull. After the leash key, "
	                "the rope keeps its shape and swings to the hand. It does not jump straight.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 106.0f, -66.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-30.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 40.0f}, .scale = 2.5f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.42f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.1f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.8f, 0.8f}},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "leash.hand_attach_sounds",
	    .name = "Attach sounds of the leash in the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and untied back to the hand two times, every "
	                   "four seconds.",
	    .expected = "Each tie and each return to the hand plays a leash-attach sound. The two sounds play in turn. The "
	                "first return to the hand plays the first sound, and the second return plays the second sound.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 10.0f}, {0.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::UntieLeash, 0, 4.0f), OnObject(Kind::TieLeash, 0, 0, 4.0f, false),
	                 Act(Kind::UntieLeash, 0, 4.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.tied_thrown",
	    .name = "Tied to a thrown rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a rock of nobody's, which is then thrown off to the "
	                   "east, every eight seconds.",
	    .expected = "The leash stays tied to the rock as it flies and after it lands: the rope runs from the rock "
	                "throughout and never goes back to the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 10.0f}, {60.0f, 10.0f}, {0.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-30.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false)},
	    .throws = {{.object = 0, .height = 4.0f, .velocity = {12.0f, 8.0f, 0.0f}, .delaySeconds = 3.0f, .repeatSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "leash.tie_by_hand",
	    .name = "Ordering and tying with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, a tree and a pillar of rock stand ahead. With the hand: "
	                   "right click the tree once, then double click it, then double click it again; right click the "
	                   "rock, and double click the rock.",
	    .expected = "Over the tree with the leash in the hand the tip says Attach Leash. A single click sends the tiger to "
	                "act on the tree (marked); the double click ties the leash to it, the rope running from the tree, and "
	                "over the tree the tip now says Detach Leash; double clicked again it comes back to the hand. The "
	                "same for the rock. The hand never pulls the tree up on a quick click.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 60.0f}, {40.0f, 60.0f}, {0.0f, 20.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 60.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 60.0f}}},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.snap_rope",
	    .name = "Snap the tied leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger's leash is tied to a tree. Put the hand on the rope (the hand opens its fingers), hold "
	                   "the right button to take hold of it, drag it out over the land and let go.",
	    .expected = "Over the rope the hand opens its fingers to pinch it and no tip shows. Holding it, the fingers pinch "
	                "it and the rope runs as two, one from the collar and one from the tree, both to the hand. Let go, the "
	                "tiger is sent to the place under the hand (marked) and is kept within the distance from the tree to "
	                "where the hand let go.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 60.0f}, {60.0f, 20.0f}, {0.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 60.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false)},
	});

	// The hand that holds the rope of the tied leash stands straight up, also when the pointer leaves the rope
	all.push_back({
	    .id = "hand.rope_hold_stand",
	    .name = "The hand that holds the rope of the tied leash",
	    .facet = Facet::Hand,
	    .description = "Your tiger's leash is tied to a tree, and the tiger lies down to sleep. The pointer goes onto the "
	                   "rope and the Action button takes hold of it. The pointer then moves away from the rope, across "
	                   "the lake onto its far bank, and the button comes up. Then the pointer moves a little to the "
	                   "right.",
	    .expected = "Holding the rope, the hand pinches it and stands straight up, not turned to the slope of the bank, "
	                "also away from the rope. It does not offer to turn the camera. Let go, it turns to the slope when the "
	                "pointer moves across the screen.",
	    .framing = {.shot = Shot::Placed, .eye = {-20.0f, 40.0f, 20.0f}, .look = {-20.0f, 0.0f, 120.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 75.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 100.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::Sleep, 0, 0.5f),
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 5.0f},
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.05f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.12f, -0.3f}, .amount = 4.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 5.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.03f, 0.0f}, .amount = 0.5f}},
	});

	// The select button goes down while the hand holds the rope of the tied leash. Both buttons end the hold, and the
	// hand grips the land at once.
	all.push_back({
	    .id = "hand.rope_hold_both_buttons",
	    .name = "Both buttons while the hand holds the rope of the tied leash",
	    .facet = Facet::Hand,
	    .description = "Your tiger's leash is tied to a tree, and the tiger lies down to sleep. The pointer goes onto the "
	                   "rope and the Action button takes hold of it. Then the select button goes down, the pointer moves "
	                   "across the lake, and both buttons come up.",
	    .expected = "At the press of the select button the hand lets the rope fall. The rope goes back to one line from "
	                "the collar to the tree, and the tiger gets no order. The hand grips the land at once with the grip "
	                "sound, and both buttons turn and zoom the camera.",
	    .framing = {.shot = Shot::Placed, .eye = {-20.0f, 40.0f, 20.0f}, .look = {-20.0f, 0.0f, 120.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 75.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 100.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::Sleep, 0, 0.5f),
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 5.0f},
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.05f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.05f, -0.1f}, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 1},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.2f, .point = {0.12f, -0.2f}, .amount = 3.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 3.5f, .value = 1},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});

	all.push_back({
	    .id = "leash.hand",
	    .name = "Leash your creature with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger stands to the north. The hand clicks it with the right button, putting the learning "
	                   "leash on. Move the hand about to lead it; press L, right click it or shake the hand to "
	                   "take the leash off again, and right click it again to put it back on.",
	    .expected = "A rope runs from the hand to its collar; pulled taut, the tiger walks to the hand. The readout says "
	                "\"on\".",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	// The real button clicks the creature that wears the leash: the click is the leash key, so the leash comes off
	all.push_back({
	    .id = "leash.click_takes_off",
	    .name = "A click on your leashed creature takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger faces the camera in the middle of the screen and wears the learning leash. The action "
	                   "button clicks the tiger for 0.35 seconds and the hand moves to the right. Then the hand comes "
	                   "back, clicks the tiger again and moves to the right again. With the leash held loose, a click "
	                   "must be longer than the wait for a tap.",
	    .expected = "After the first click no rope follows the hand: the click took the leash off. After the second "
	                "click a rope runs from the hand to the collar again. The log shows \"The draw of the hand sends the "
	                "leash tap\" for each click, and a turn later \"Leash command: player 0 taps a creature\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.35f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.25f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 2.5f, .point = {-0.25f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.35f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.25f, 0.0f}, .amount = 1.0f}},
	});

	all.push_back({
	    .id = "audio.leash_tie_placed",
	    .name = "The tie sound of another player's leash",
	    .facet = Facet::Audio,
	    .description = "Your tiger on the left is tied to a tree. Then player two's lion on the right is tied to a "
	                   "pillar of rock.",
	    .expected = "The tie of your tiger's leash is heard in the middle, with no place. The tie of the lion's leash "
	                "is heard from the rock on the right.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-60.0f, 70.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {60.0f, 70.0f}}},
	    .commands = {OnObject(Kind::TieLeash, 0, 0, 1.0f, false), OnObject(Kind::TieLeash, 1, 1, 3.0f, false)},
	});

	all.push_back({
	    .id = "leash.someone_elses",
	    .name = "Another player's creature refuses the leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left and player two's lion on the right. Your hand taps the lion, then your "
	                   "tiger.",
	    .expected = "The lion is refused, as it belongs to another player, and the log says so; then your tiger is "
	                "leashed. Right clicking the lion yourself is refused the same way.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.only_leashable",
	    .name = "Only the leashable one of two",
	    .facet = Facet::Leash,
	    .description = "You have two tigers: the left one is the one you can lead, the right one isn't. The hand taps "
	                   "the right one, then the left one.",
	    .expected = "The right tiger is refused as it isn't the one you can lead; the left one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.switch",
	    .name = "Switch the leashable creature",
	    .facet = Facet::Leash,
	    .description = "Your left tiger is on the leash. The right tiger is made the one you can lead, then the hand "
	                   "taps the left one and the right one.",
	    .expected = "Made leashable, the right tiger takes over: the left one's leash comes off and tapping it is "
	                "refused, and the right one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::MakeLeashable, .creature = 1, .delaySeconds = 4.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.keys",
	    .name = "The leash shortcuts",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes. The keys are pressed in turn: L puts the learning leash "
	                   "on, B steps down to the aggression leash, V steps back up to the learning leash and on up to "
	                   "the compassion leash, and L takes it off, however many leashes it knows.",
	    .expected = "The rope goes on, changes colour three times, and comes off; the readout names each leash. "
	                "Pressing L, V and B yourself does the same.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::LeashKey, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_next_turn",
	    .name = "The leash key acts at the start of the next turn",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows the learning leash. The scenario presses the leash key through the game's "
	                   "controls, as a player does, and three seconds later presses it again. The key makes a command "
	                   "of the player, and the world does the command at the start of the next game turn.",
	    .expected = "The rope goes on one or two game turns after the press (a tenth to a fifth of a second), not on the "
	                "frame of the press, and comes off again after the second press. The players.commands query shows "
	                "the command that waits.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::PressKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::LEASH_UNLEASH_CREATURE)},
	                 {.kind = Kind::PressKey,
	                  .delaySeconds = 3.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::LEASH_UNLEASH_CREATURE)}},
	});

	all.push_back({
	    .id = "leash.click_or_hold",
	    .name = "Hold to stroke, right click to leash",
	    .facet = Facet::Leash,
	    .description = "The hand holds on to your tiger with the right button, stroking its head and belly, and lets go; "
	                   "then it clicks the tiger, which puts the leash on. Try it yourself: holding the right button on "
	                   "it for a second strokes it, sweeping the held hand across it slaps it, and a quick right click "
	                   "leashes it.",
	    .expected = "The hold strokes it without leashing it; the click then puts the rope on without stroking it.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f),
	                 Stroke(0, creature_feedback::BodyPart::Belly, 2.5f),
	                 Act(Kind::HandLetGo, 0, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.shake",
	    .name = "Shake the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, and a scribble is then drawn with the empty hand, as "
	                   "the game shakes a leash off. Draw it yourself (move the mouse quickly side to side a few times, "
	                   "no button held) to try it.",
	    .expected = "The rope goes on, and comes off once the scribble is recognised; the log says so.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashShake, .creature = 0, .delaySeconds = 4.0f}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_off",
	    .name = "L takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, then L is pressed.",
	    .expected = "The rope goes on, then comes off with L.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashKey, .delaySeconds = 4.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.other_gods_creatures",
	    .name = "Other gods' creatures can't be slapped",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left, a tortoise of player three (as Khazar's is) in the middle and a wolf of "
	                   "player four (as Lethys's is) on the right. The hand tries to slap the tortoise and the wolf, "
	                   "then to leash the tortoise.",
	    .expected = "Neither is slapped: the hand never takes hold of another god's creature, inside the player's "
	                "influence or out, and the log says so. Leashing the tortoise is refused too, as it isn't yours.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-40.0f, 60.0f}),
	                  [] {
		                  auto tortoise = Content(CreatureType::Tortoise, {0.0f, 60.0f});
		                  tortoise.owner = PlayerNames::PLAYER_THREE;
		                  return tortoise;
	                  }(),
	                  [] {
		                  auto wolf = Content(CreatureType::Wolf, {40.0f, 60.0f});
		                  wolf.owner = PlayerNames::PLAYER_FOUR;
		                  return wolf;
	                  }()},
	    .commands = {Slap(1, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 1, 2.0f, true),
	                 Slap(2, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 2, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.home",
	    .name = "Kept at home",
	    .facet = Facet::Leash,
	    .description = "A tiger is kept within 40 units of where it stands, as a young creature is kept near its home, "
	                   "and told to walk 120 units away.",
	    .expected = "Once it strays past the radius it turns back and walks home again.",
	    .framing = {.shot = Shot::Overview, .include = {{120.0f, 0.0f}, {-40.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 Go(Kind::WalkTo, 0, {120.0f, 0.0f}, 1.0f),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 6.0f)},
	    .repeatFrom = 1,
	});
}
