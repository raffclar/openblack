/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of creatures casting miracles: a spell at another creature, a lightning bolt, a fireball, an
// explosion, a heal, food, wood, a shield, water, a forest, a miracle to impress, a try that fizzles and one the creature is
// too tired for

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

Command Know(MagicType type)
{
	// Once its mind has set up what it knows, on its first turns
	return {.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(type)};
}

Command CastAtCreature(MagicType type, size_t target, float delay)
{
	return {.kind = Kind::CastMiracle, .delaySeconds = delay, .value = static_cast<size_t>(type), .atCreature = target};
}

Command CastAtObject(MagicType type, size_t object, float delay)
{
	return {.kind = Kind::CastMiracle, .delaySeconds = delay, .value = static_cast<size_t>(type), .object = object};
}

Command Do(std::string_view action, size_t object, float delay)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .object = object, .action = action};
}

CreatureSetup Caster(glm::vec2 offset, std::string_view label, float exhaustion = 0.0f)
{
	return {.label = label,
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .facingDegrees = 0.0f,
	        .needs = {.energy = 1.0f, .exhaustion = exhaustion, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

CreatureSetup Other(glm::vec2 offset, std::string_view label)
{
	return {.label = label, .species = CreatureType::Cow, .offset = offset, .facingDegrees = 180.0f, .pauseMind = true};
}
} // namespace

void testbed_scenarios::AddCreatureCastingScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.creature_casts_itchy",
	    .name = "Creature casting: itchy at another creature",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the itchy spell is told to cast it at a cow a hundred metres away.",
	    .expected = "One time in five it pulls a playful face first. It walks until it is within five times its height "
	                "(about 75 metres) of the cow, backs off if nearer than twice its height, turns to face it and "
	                "holds still a tenth of a second, then raises its arms into the casting pose; as the pose loops the "
	                "spell is cast with the sound a creature's cast makes, wisps fly to the cow and flies gather round "
	                "its head; three seconds later the tiger lowers its arms. The cow starts scratching two seconds "
	                "after the cast. The tiger tires a little.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 110.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 100.0f}, "target")},
	    .commands = {Know(MagicType::CreatureSpellItchy), CastAtCreature(MagicType::CreatureSpellItchy, 1, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_casts_lightning",
	    .name = "Creature casting: a lightning bolt at a rock",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the lightning bolt is told to cast it at a pillar of rock eighty metres away.",
	    .expected = "One time in five it shows its anger first. It walks up to within fifty metres of the rock, turns "
	                "to it, raises its arms and a bolt crackles from between its hands to the rock for three seconds, "
	                "then stops as it lowers its arms.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 90.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 80.0f}, .scale = 0.5f}},
	    .commands = {Know(MagicType::LightningBolt), CastAtObject(MagicType::LightningBolt, 0, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_casts_fireball",
	    .name = "Creature casting: a fireball at a cow",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the fireball is told to cast it at a cow sixty metres away.",
	    .expected = "It first walks to the middle of a clear area near where it stands, five times as wide as its "
	                "radius. One time in five it then shows its anger. As the cow is living, it backs off if it is "
	                "nearer than twice its height. It turns to the cow, raises its arms and a fireball flies from "
	                "between its hands to the cow.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 70.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 60.0f}, "target")},
	    .commands = {Know(MagicType::Fireball), CastAtCreature(MagicType::Fireball, 1, 0.5f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_explosion",
	    .name = "Creature casting: an explosion at a rock",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the explosion is told to cast it at a pillar of rock a hundred and forty "
	                   "metres away.",
	    .expected = "One time in three it shows its anger first. It walks up to within a hundred metres of the rock, "
	                "backs off if nearer than twice its height, turns to it, raises its arms and casts the explosion "
	                "at the rock.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 150.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 140.0f}, .scale = 0.5f}},
	    .commands = {Know(MagicType::ExplosionOne), CastAtObject(MagicType::ExplosionOne, 0, 0.5f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_heal",
	    .name = "Creature casting: a heal at a hurt villager",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal is told to cast it at a villager hurt to a third of its life, sixty "
	                   "metres away.",
	    .expected = "It walks up to within twice its height of the villager, backing off if nearer, faces it and casts: "
	                "the chakra lights over the villager, whose life fills up. The heal runs its own ten seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 70.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 60.0f}, .life = 0.33f}},
	    .commands = {Know(MagicType::Heal), CastAtObject(MagicType::Heal, 0, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.creature_casts_food",
	    .name = "Creature casting: food into a storage pit",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the food miracle is told to cast it into a storage pit sixty metres away.",
	    .expected = "It walks up to within two metres of the pit and turns to face it. It does not show how it feels "
	                "first, and it does not back off. It draws the gesture of the food miracle if the miracle has one. "
	                "Then it holds its arms out in the scatter pose for three seconds, and the food falls into the pit.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 60.0f}, .storedFood = 0}},
	    .commands = {Know(MagicType::Food), Do("CastMagicFood", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_wood",
	    .name = "Creature casting: wood into a storage pit",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the wood miracle is told to cast it into a storage pit sixty metres away.",
	    .expected = "One time in five it shows that it feels kind first. It walks up to within twice its height of the "
	                "pit, backs off if it is nearer, turns to the pit and casts wood into it from the casting pose.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 60.0f}, .storedFood = 0}},
	    .commands = {Know(MagicType::Wood), Do("CastMagicWood", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_shield",
	    .name = "Creature casting: a shield at a villager",
	    .facet = Facet::Miracles,
	    .description = "The debug tools tell a tiger that knows the shield to cast it at a villager forty metres away.",
	    .expected = "It does not walk. It turns to face the villager and casts the shield from the casting pose, and "
	                "holds it three seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 50.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {20.0f, 35.0f}}},
	    .commands = {Know(MagicType::Shield), CastAtObject(MagicType::Shield, 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_shield_idles",
	    .name = "Creature casting: a shield about nothing idles",
	    .facet = Facet::Miracles,
	    .description = "A script tells a tiger that knows the shield to cast it at a villager. A plan from a script is "
	                   "about nothing.",
	    .expected = "It does not cast the shield. It idles: two times it waits about a second and yawns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 50.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {20.0f, 35.0f}}},
	    .commands = {Know(MagicType::Shield), Do("CastShield", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_water",
	    .name = "Creature casting: water at a tree",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the water miracle is told to cast it at a tree eighty metres away.",
	    .expected = "It walks up to within twice its height of the tree and turns to it. It does not draw a gesture. "
	                "It holds its arms out in the scatter pose for three seconds, and the water rains on the tree.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 90.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, 80.0f}}},
	    .commands = {Know(MagicType::Water), CastAtObject(MagicType::Water, 0, 0.5f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_to_impress",
	    .name = "Creature casting: a miracle to impress a villager",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal is told to impress a villager sixty metres away with a miracle.",
	    .expected = "It walks up to within one and a half times its height of the villager and backs off to twice "
	                "its height. One time in two it points at the villager first. Two times in three it beckons. It "
	                "turns to the villager and casts the heal from the casting pose. If it did not point first, it "
	                "points now.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 70.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 60.0f}}},
	    .commands = {Know(MagicType::Heal), Do("CastImpressiveSpell", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_casts_forest",
	    .name = "Creature casting: a forest by a villager",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the forest miracle is told to cast a forest by a villager sixty metres away.",
	    .expected = "It walks up to within two and a half times its height of the villager and turns to it. It does not "
	                "draw a gesture. It holds its arms out in the scatter pose for four seconds, and a forest grows "
	                "at the place where the villager stood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 80.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 60.0f}}},
	    .commands = {Know(MagicType::Forest), Do("CastMagicForest", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_cast_fizzles",
	    .name = "Creature casting: a spell it hasn't learnt fizzles",
	    .facet = Facet::Miracles,
	    .description = "A tiger that has never seen the freeze spell is told to cast it at a cow.",
	    .expected = "It goes up to the cow and raises its arms, but nothing is cast: it lowers them at once, looks "
	                "embarrassed, and one time in two is sad as well. Its count of sightings of the freeze goes up by "
	                "one.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 60.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 50.0f}, "target")},
	    .commands = {CastAtCreature(MagicType::CreatureSpellFreeze, 1, 1.0f)},
	});

	all.push_back({
	    .id = "miracles.creature_too_tired_to_cast",
	    .name = "Creature casting: too tired to cast",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the freeze spell but is all but exhausted is told to cast it at a cow.",
	    .expected = "It goes up to the cow and raises its arms, but can't pay for the spell: nothing is cast, and it "
	                "wants food above everything else.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 60.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster", 0.84f), Other({0.0f, 50.0f}, "target")},
	    .commands = {Know(MagicType::CreatureSpellFreeze), CastAtCreature(MagicType::CreatureSpellFreeze, 1, 0.5f)},
	});

	all.push_back({
	    .id = "miracles.hand_itchy_seed",
	    .name = "Creature spells: the itchy spell held in the hand",
	    .facet = Facet::Miracles,
	    .description = "The player's hand is given the itchy creature spell and held still over the land.",
	    .expected = "Ten flies swarm round the hand's grasp point, circling a point that goes round it on a sphere of two "
	                "metres. The freeze and compassion spells would show nothing in the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {10.0f, 40.0f}}, .distance = 0.3f},
	    .commands = {{.kind = Kind::HoldSeed,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(SpellSeedType::CreatureSpellItchy)}},
	    .hand = HandHold {.offset = {0.0f, 30.0f}, .height = 8.0f},
	});
}
