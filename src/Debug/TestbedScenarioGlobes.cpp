/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the miracles' globes and dispensers, close up: the swirl under a dispenser, what floats in
// each globe, and the rings round the extreme ones

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
void AddDispensers(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.dispenser_vortex",
	    .name = "Dispenser swirl and its globe",
	    .facet = Facet::Miracles,
	    .description = "Three miracle dispensers close up: fire, lightning and food. The fire and lightning dispensers "
	                   "start charged, a testbed set-up step; the food dispenser is put down as in the game.",
	    .expected = "Under each dispenser a flat disk of starry light six units across lies a little above the land, "
	                "added to what is under it: dark at its middle, brightest a little out, fading at its rim, its stars "
	                "creeping slowly round and quickly outwards. It is the same for every miracle and makes no sound. The "
	                "fire and lightning dispensers float a globe a little above themselves at once; the food dispenser has "
	                "none until 30 seconds after it is put down, when its globe appears with a puff of sparkles.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, 20.0f}, {12.0f, 30.0f}}, .distance = 0.5f},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {-10.0f, 25.0f}},
	                   {.type = MagicType::LightningBolt, .offset = {0.0f, 25.0f}},
	                   {.type = MagicType::Food, .offset = {10.0f, 25.0f}, .charged = false}},
	});

	all.push_back({
	    .id = "miracles.dispenser_swirl_frames",
	    .name = "A dispenser swirl close up, stepped in the drawn frames",
	    .facet = Facet::Miracles,
	    .description = "One fire dispenser close up, with the camera still. Its swirl moves on in each frame that draws "
	                   "the dispenser, no more often than 60 times a second, not once a turn.",
	    .expected = "The stars of the disk under the dispenser creep round and outwards smoothly, with no jerk ten times a "
	                "second. They stop when the dispenser is off the screen and the camera does not move. Their random "
	                "numbers are only this computer's, so the world stays the same.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 9.0f, 14.0f}, .look = {0.0f, 0.0f, 25.0f}},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {0.0f, 25.0f}}},
	});
}

/// The miracles that the challenges of the first land give as dispensers, a row of them this far apart
constexpr float k_RewardSpacing = 10.0f;
/// The belief of the scenario's town in the player, enough to keep the town the player's
constexpr float k_TownBelief = 1000.0f;

void AddRewardDispensers(std::vector<Scenario>& all)
{
	std::vector<DispenserSetup> rewards;
	float east = -3.0f * k_RewardSpacing;
	for (const auto type : {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::LightningBolt, MagicType::Food,
	                        MagicType::Water, MagicType::CreatureSpellStrong, MagicType::CreatureSpellCompassion})
	{
		rewards.push_back({.type = type, .offset = {east, 25.0f}, .charged = false, .reward = true});
		east += k_RewardSpacing;
	}
	all.push_back({
	    .id = "miracles.dispenser_rewards",
	    .name = "The first land's reward dispensers",
	    .facet = Facet::Miracles,
	    .description = "The seven dispensers that the first land's challenges give, made as the reward script makes them, "
	                   "from the left: heal, heal powered up (the Ogre after a heal), lightning (the Pied Piper's evil "
	                   "way), food (the Singing Stones), water (the Missionaries and the Hermit), the creature spell "
	                   "strong and the creature spell nice. Each belongs to the player's town.",
	    .expected = "Each dispenser floats its globe at once, with a puff of sparkles, a little above itself. Each globe "
	                "holds its miracle's seed or phial, and the powered-up heal globe has a ring round it. "
	                "A tap on a globe with the action button puts its miracle in the hand, and the globe pops. The "
	                "dispenser makes a new globe when its period has passed after the tap.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-32.0f, 20.0f}, {32.0f, 30.0f}}, .distance = 0.5f},
	    .dispensers = std::move(rewards),
	    .townBeliefInPlayer = k_TownBelief,
	});

	// A town with builders, its dispenser damaged, and a damaged dispenser of no town
	std::vector<ObjectSetup> town {
	    {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 60.0f}},
	    {.type = AbodeInfo::CelticStoragePit, .offset = {-30.0f, 40.0f}, .storedFood = 5000, .storedWood = 5000},
	    {.type = AbodeInfo::CelticHut, .offset = {25.0f, 45.0f}, .yawDegrees = 180.0f},
	};
	for (int i = 0; i < 3; ++i)
	{
		const float x = static_cast<float>(i - 1) * 3.0f;
		town.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 30.0f}, .joinTown = true});
		town.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 27.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "miracles.dispenser_mending",
	    .name = "Damaged dispensers, of a town and of none",
	    .facet = Facet::Miracles,
	    .description = "Two heal dispensers, their first globes taken, each damaged to half its life by a blow: on the "
	                   "left a dispenser of the player's town, a Celtic town with a full storage pit and six villagers; "
	                   "on the right a dispenser of no town. Best watched at game speed 4.",
	    .expected = "Neither dispenser makes a globe while it is damaged. The town opens a site at its dispenser, and "
	                "builders fetch wood from the pit and mend it. Once it is whole again, it counts its period from "
	                "the start and then floats a new globe with a puff of sparkles. The dispenser of no town is never "
	                "mended and never makes a globe again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {40.0f, 60.0f}}, .distance = 0.6f},
	    .objects = std::move(town),
	    .dispensers = {{.type = MagicType::Heal,
	                    .offset = {-12.0f, 5.0f},
	                    .charged = false,
	                    .reward = true,
	                    .life = 0.5f,
	                    .globeTaken = true},
	                   {.type = MagicType::Heal,
	                    .offset = {30.0f, 5.0f},
	                    .charged = false,
	                    .reward = true,
	                    .noTown = true,
	                    .life = 0.5f,
	                    .globeTaken = true}},
	    .townBeliefInPlayer = k_TownBelief,
	});

	// The player puts a held miracle into the player's own dispenser
	using Kind = Command::Kind;
	all.push_back({
	    .id = "miracles.dispenser_store",
	    .name = "A held miracle put into a dispenser",
	    .facet = Facet::Miracles,
	    .description = "A heal dispenser of the player's town, its first globe taken. A lightning seed is put in the "
	                   "hand, the pointer goes to the dispenser, and the action button is pressed and let go there.",
	    .expected = "Over the dispenser the hand offers \"Cast\". When the button is let go, the seed leaves the hand and "
	                "a lightning globe appears above the dispenser with a puff of sparkles; no lightning strikes. The "
	                "dispenser continues to make its own heal globes as before.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 9.0f, 8.0f}, .look = {0.0f, 3.0f, 25.0f}},
	    .dispensers =
	        {{.type = MagicType::Heal, .offset = {0.0f, 25.0f}, .charged = false, .reward = true, .globeTaken = true}},
	    .commands = {{.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::LightningBolt)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}},
	    .townBeliefInPlayer = k_TownBelief,
	});
}

/// The sixteen creature spells' dispensers in four rows of four, this far apart, the first row nearest
std::vector<DispenserSetup> CreatureSpellDispensers()
{
	constexpr float k_Spacing = 12.0f;
	constexpr float k_FirstRow = 16.0f;
	constexpr int k_PerRow = 4;
	constexpr auto k_First = static_cast<int>(MagicType::CreatureSpellFreeze);
	constexpr auto k_Last = static_cast<int>(MagicType::CreatureSpellItchy);
	std::vector<DispenserSetup> dispensers;
	for (int type = k_First; type <= k_Last; ++type)
	{
		const int place = type - k_First;
		const auto column = static_cast<float>(place % k_PerRow) - static_cast<float>(k_PerRow - 1) * 0.5f;
		const auto row = static_cast<float>(place / k_PerRow);
		dispensers.push_back(
		    {.type = static_cast<MagicType>(type), .offset = {column * k_Spacing, k_FirstRow + row * k_Spacing}});
	}
	return dispensers;
}

void AddCreatureSpellDispensers(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.creature_spell_dispensers",
	    .name = "The creature spells' dispensers",
	    .facet = Facet::Miracles,
	    .description = "A dispenser of each of the sixteen creature spells, close up, each floating its globe at once (a "
	                   "testbed set-up step): freeze, small, big, weak; strong, fat, thin, invisible; nice, angry, hungry, "
	                   "frightened; tired, ill, thirsty, itchy, from the nearest row back.",
	    .expected = "The spells with a globe (freeze, small, big, weak, strong, invisible, nice, angry, hungry and itchy; "
	                "the others have no one-shot seed and float none, as in the game) each hold their phial spinning "
	                "twice a second round: a glass bottle shining silver with the environment map, the red liquid inside "
	                "showing faintly through the glass and not running, its stopper and its label (the spell's own "
	                "picture) still and unshined. The phials take the land's light and colour, except the small and big "
	                "ones, which are lit in white with none of the land's colour and are as faint as their globe: the "
	                "small one shrinks away under a faint whole one, the big one has a fading copy growing out of it. The "
	                "freeze phial pulses icy, the invisible one fades in and out.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 12.0f}, {20.0f, 56.0f}}, .distance = 0.5f},
	    .dispensers = CreatureSpellDispensers(),
	});
}

/// A row of globes on their own, floating this high, this far apart, from the left
std::vector<DispenserSetup> GlobeRow(std::initializer_list<MagicType> types, float north, float spacing = 7.0f)
{
	constexpr float k_GlobeHeight = 5.0f;
	std::vector<DispenserSetup> globes;
	float east = -spacing * static_cast<float>(types.size() - 1) * 0.5f;
	for (const auto type : types)
	{
		globes.push_back({.type = type, .offset = {east, north}, .bubbleHeight = k_GlobeHeight});
		east += spacing;
	}
	return globes;
}

void AddGlobes(std::vector<Scenario>& all)
{
	auto everySeed = GlobeRow({MagicType::StormWindRain, MagicType::Forest, MagicType::Fireball, MagicType::Food,
	                           MagicType::Shield, MagicType::PhysicalShield},
	                          34.0f);
	for (auto& row : {GlobeRow({MagicType::LightningBolt, MagicType::Heal, MagicType::Wood, MagicType::Water,
	                            MagicType::FlockFlying, MagicType::FlockGround},
	                           27.0f),
	                  GlobeRow({MagicType::CreatureSpellFreeze, MagicType::CreatureSpellBig, MagicType::CreatureSpellCompassion,
	                            MagicType::CreatureSpellItchy, MagicType::Teleport, MagicType::ExplosionOne},
	                           20.0f)})
	{
		everySeed.insert(everySeed.end(), row.begin(), row.end());
	}
	all.push_back({
	    .id = "miracles.globes_every_seed",
	    .name = "A globe of every miracle",
	    .facet = Facet::Miracles,
	    .description = "One-shot globes on their own, close up, a row at a time: storm, forest, fire, food, shield, "
	                   "physical shield; lightning, heal, wood, water, flying and ground flocks; the freeze, big, nice and "
	                   "itchy phials, teleport and the beam explosion.",
	    .expected = "Each globe is added over what is behind it, faintly, a glint running over its dome. The storm, fire, "
	                "lightning, water and teleport show only their miracle's own little effect inside (no model): the "
	                "storm cloud, the fireball, the bolt's sparks, the water mist, the teleport's swirl. Shield, heal, the "
	                "freeze, nice and itchy phials show their model with their effect too. The others show their model "
	                "spinning twice a second round, a little below the middle; food and the phials shine with the "
	                "environment map, the phials' texture running and the big phial pulsing larger; the beam's model is "
	                "added without hiding what is behind it. None has rings.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 18.0f}, {20.0f, 36.0f}}, .distance = 0.55f},
	    .dispensers = everySeed,
	});

	all.push_back({
	    .id = "miracles.globe_effect_frames",
	    .name = "A fireball globe close up, its effect stepped in the drawn frames",
	    .facet = Facet::Miracles,
	    .description = "One fireball globe close up, with the camera still. The fireball inside steps in the frames that "
	                   "draw the globe, no more often than 60 times a second.",
	    .expected = "The little fireball in the globe burns as it does at 60 frames a second, also at hundreds of frames "
	                "a second. It does not burn faster or flicker more at a high frame rate. Its random numbers are only "
	                "this computer's, so the world stays the same.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 7.0f, 15.0f}, .look = {0.0f, 5.0f, 25.0f}},
	    .dispensers = GlobeRow({MagicType::Fireball}, 25.0f),
	});

	auto extreme = GlobeRow({MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo}, 34.0f);
	for (auto& row :
	     {GlobeRow({MagicType::LightningBolt, MagicType::LightningBoltPowerUpOne, MagicType::LightningBoltPowerUpTwo}, 27.0f),
	      GlobeRow({MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado}, 20.0f)})
	{
		extreme.insert(extreme.end(), row.begin(), row.end());
	}
	all.push_back({
	    .id = "miracles.extreme_globes",
	    .name = "Extreme globes side by side",
	    .facet = Facet::Miracles,
	    .description = "Globes of fire, lightning and storm: in each row the plain miracle, then its first and its "
	                   "second power-up.",
	    .expected = "The plain globes have no rings. Each extreme globe has a faint ring in the player's colour spinning "
	                "fast round its miracle, tilted to the camera; the second power-up has two, crossing. The globes are "
	                "otherwise the same: the same dome, glint and effect inside.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-8.0f, 18.0f}, {8.0f, 36.0f}}, .distance = 0.5f},
	    .dispensers = extreme,
	});
}

void AddHand(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	all.push_back({
	    .id = "miracles.hand_power_up_bands",
	    .name = "Bands and bracelets on the hand",
	    .facet = Facet::Miracles,
	    .description = "A lightning seed is summoned from the player's worship into the hand, powered up twice with its "
	                   "gestures, then shaken off with a scribble.",
	    .expected = "As the seed comes, five bands fly from in front of the camera onto the hand with the band's sound, "
	                "and after a moment a bracelet settles round the wrist; the hand glows in the player's colour, the glow "
	                "flowing over it. Each power-up sends five bands flying on with the sound and the announcer's voice "
	                "(power up one, then two), and puts another bracelet on further up the arm. The scribble sends a band "
	                "flying off the hand to the camera with the hand's shake; the bracelets and the glow go.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-3.0f, 27.0f}, {3.0f, 33.0f}}, .distance = 0.2f},
	    .commands =
	        {{.kind = Kind::SummonSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::LightningBolt)},
	         {.kind = Kind::DrawGesture, .delaySeconds = 4.0f, .value = static_cast<size_t>(GestureType::InverseSpiral)},
	         {.kind = Kind::DrawGesture, .delaySeconds = 4.0f, .value = static_cast<size_t>(GestureType::Spiral)},
	         {.kind = Kind::DrawGesture, .delaySeconds = 4.0f, .value = static_cast<size_t>(GestureType::Scribble)}},
	    .repeatFrom = 0,
	    .hand = HandHold {.offset = {0.0f, 25.0f}, .height = 6.0f},
	});

	all.push_back({
	    .id = "miracles.hand_hold_poses",
	    .name = "How the hand holds each seed",
	    .facet = Facet::Miracles,
	    .description = "Seeds put in the hand one after another, three seconds each: food, the forest, lightning, the ground "
	                   "flock, wood and the freeze phial; then a food seed summoned from worship.",
	    .expected =
	        "Taking each seed the hand fades over about a tenth of a second into a still pose that doesn't play: "
	        "food and wood held from the side, the hand opened round the horn of plenty or the logs hanging below "
	        "it and raised by about their height; the forest's tree held above, standing a little above the hand, "
	        "turned half round; lightning in the rest pose, the hand raised by its own height, nothing in it but "
	        "the bolt's sparks in the fingers; the wolf held above, turned a quarter round; the phial held from the side "
	        "like the horn, hanging six tenths of its height below the hand point. Moving the mouse sways "
	        "the hand and its seed together by up to about 17 degrees, rolling about the line to the camera and "
	        "pitching. The summoned food is held in the rest pose with nothing in it for a second and a half, then "
	        "snaps to the side hold with the horn showing.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-3.0f, 27.0f}, {3.0f, 33.0f}}, .distance = 0.2f},
	    .commands = {{.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::Nature)},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::LightningBolt)},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::FlockGround)},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::Wood)},
	                 {.kind = Kind::HoldSeed,
	                  .delaySeconds = 3.0f,
	                  .value = static_cast<size_t>(SpellSeedType::CreatureSpellFreeze)},
	                 {.kind = Kind::SummonSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::Food)}},
	    .repeatFrom = 0,
	    .hand = HandHold {.offset = {0.0f, 25.0f}, .height = 6.0f},
	});

	all.push_back({
	    .id = "miracles.hand_tribal_power",
	    .name = "A tribe's power behind a miracle",
	    .facet = Facet::Miracles,
	    .description = "The player's Norse power is set to twice (a testbed set-up step; worship sets it in the game). A "
	                   "wood seed comes to a hand held over the land and is poured for three seconds, once a second after "
	                   "it comes; again every eight seconds.",
	    .expected = "As the seed comes, \"Norse Power\" flies in from the camera over a second as a ring of letters in "
	                "the player's colour, closing up from trailing one another, and settles round the hand a little above "
	                "it, turning slowly. When the wood is cast the announcer says \"Norse Power\" and the ring is let go: "
	                "its letters stand up and rise from where the hand was in a widening column, turning faster and faster, "
	                "fading after three seconds and gone within five. The wood pours twice as strongly.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-6.0f, 22.0f}, {6.0f, 34.0f}}, .distance = 0.3f},
	    .miracles = {{.type = MagicType::Wood,
	                  .point = {0.0f, 28.0f},
	                  .handOffset = {0.0f, 28.0f},
	                  .handHeight = 8.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 3.0f,
	                  .repeatSeconds = 8.0f,
	                  .byHand = true,
	                  .holdBeforePress = 2.0f}},
	    .tribalPower = std::pair {Tribe::NORSE, 2.0f},
	});

	all.push_back({
	    .id = "miracles.hand_extreme_lightning",
	    .name = "An extreme lightning held by hand",
	    .facet = Facet::Miracles,
	    .description = "The lightning's second power-up summoned from worship into a hand over the land, held for six "
	                   "seconds and then cast for six; again every fourteen.",
	    .expected = "Bands fly onto the hand from the camera as the seed comes, with the band's sound and the announcer's "
	                "\"power up two\"; after a moment two bracelets settle round the wrist and arm, spinning in the "
	                "player's colour, and the hand glows, its flow running backwards over it. Once ready, its in-hand "
	                "effect is the extreme one's (more and larger sparks). When the seed goes the bracelets go.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {10.0f, 45.0f}}, .distance = 0.45f},
	    .miracles = {{.type = MagicType::LightningBoltPowerUpTwo,
	                  .point = {0.0f, 32.0f},
	                  .handOffset = {0.0f, 25.0f},
	                  .handHeight = 6.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 6.0f,
	                  .repeatSeconds = 15.0f,
	                  .byHand = true,
	                  .fromWorship = true,
	                  .holdBeforePress = 6.0f}},
	});
}
void AddPiles(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.food_speedup_and_stores",
	    .name = "Powered food, and a storage pit taking food and wood",
	    .facet = Facet::Miracles,
	    .description = "The food miracle's first power-up poured on open ground on the left; food and then wood poured "
	                   "beside an empty storage pit on the right, each for four seconds.",
	    .expected = "On the left a pile of grain rises out of the ground over a second with a thud, its grain flowing up "
	                "it as it rises, and sparkles keep coming off it for as long as it stands. On the right the food and "
	                "wood land in the pit's own piles, which rise out of the ground for what they hold, thudding as they "
	                "fill; no new pile is made beside the pit while it takes them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 30.0f}, {20.0f, 30.0f}}, .distance = 0.7f},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {20.0f, 30.0f}, .amount = 0}},
	    .miracles = {{.type = MagicType::FoodPowerUpOne,
	                  .point = {-20.0f, 30.0f},
	                  .handOffset = {-20.0f, 30.0f},
	                  .handHeight = 12.0f,
	                  .holdSeconds = 4.0f},
	                 {.type = MagicType::Food,
	                  .point = {20.0f, 33.0f},
	                  .handOffset = {20.0f, 33.0f},
	                  .handHeight = 12.0f,
	                  .holdSeconds = 4.0f},
	                 {.type = MagicType::Wood,
	                  .point = {20.0f, 27.0f},
	                  .handOffset = {20.0f, 27.0f},
	                  .handHeight = 12.0f,
	                  .delaySeconds = 6.0f,
	                  .holdSeconds = 4.0f}},
	});
}

void AddScriptDeletes(std::vector<Scenario>& all)
{
	// A script makes two fireball globes, as the second land's fireball challenge does, then deletes them
	using Kind = Command::Kind;
	using Argument = ScriptArgument;
	constexpr int32_t k_OneShotSpell = 30;
	constexpr int32_t k_FireballSeed = 2;
	const auto create = [](float delay, glm::vec2 point) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = delay,
		                .script = "CREATE",
		                .scriptArguments = {{.kind = Argument::Kind::Integer, .number = static_cast<float>(k_OneShotSpell)},
		                                    {.kind = Argument::Kind::Integer, .number = static_cast<float>(k_FireballSeed)},
		                                    {.kind = Argument::Kind::Position, .point = point}}};
	};
	const auto remove = [](float delay, size_t made, float mode) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = delay,
		                .script = "OBJECT_DELETE",
		                .scriptArguments = {{.kind = Argument::Kind::Made, .index = made},
		                                    {.kind = Argument::Kind::Integer, .number = mode}}};
	};
	all.push_back({
	    .id = "miracles.script_deletes_globes",
	    .name = "A script deletes fireball globes with fade and with explode",
	    .facet = Facet::Miracles,
	    .description = "One second in, a script makes a fireball globe 3 metres west of the point the camera looks at, "
	                   "and another 3 metres east. Three seconds later it deletes the west globe with fade. Three "
	                   "seconds after that it deletes the east globe with explode.",
	    .expected = "The two globes lie on the land, each with its fireball seed spinning inside. At the first delete a "
	                "ghost of the west globe's dome flickers out where it lay over half a second, and the globe is gone. "
	                "At the second delete the dome of the east globe breaks into pieces that fly out, and the globe is "
	                "gone. No globe stays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 12.0f}, .look = {0.0f, 0.0f, 25.0f}},
	    .commands = {create(1.0f, {-3.0f, 25.0f}), create(0.1f, {3.0f, 25.0f}), remove(3.0f, 0, 1.0f), remove(3.0f, 1, 2.0f)},
	});
}
} // namespace

void testbed_scenarios::AddGlobeScenarios(std::vector<Scenario>& all)
{
	AddDispensers(all);
	AddRewardDispensers(all);
	AddCreatureSpellDispensers(all);
	AddGlobes(all);
	AddHand(all);
	AddPiles(all);
	AddScriptDeletes(all);
}
