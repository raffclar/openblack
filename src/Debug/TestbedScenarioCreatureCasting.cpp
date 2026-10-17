/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of creatures casting miracles: a spell at another creature, a lightning bolt, a fireball, an
// explosion, a heal, food, wood, a shield, water, a forest, the casts for a town, a miracle to impress, a try that fizzles
// and one the creature is too tired for, and the one-shot miracles that it picks up or holds

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

Command RunTo(size_t creature, glm::vec2 point, bool wait)
{
	return {.kind = Kind::RunTo, .creature = creature, .delaySeconds = 0.5f, .waitUntilFree = wait, .point = point};
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

	// The particles of two creature spells: the hearts of compassion and the steam of a freeze
	all.push_back({
	    .id = "miracles.creature_spell_hearts_and_steam",
	    .name = "Creature casting: the hearts of compassion and the steam of a freeze",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the compassion and the freeze spells casts compassion at a cow sixty metres "
	                   "away, then freeze at the same cow.",
	    .expected = "Wisps fly from the tiger to the cow. Hearts come from points near the bones of the cow, four a "
	                "second, and fly out in all directions. After the freeze, steam comes from the feet of the cow and "
	                "stays above the land. The first atom of each spell stays at the place where it started.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 70.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 60.0f}, "target")},
	    .commands = {Know(MagicType::CreatureSpellCompassion), Know(MagicType::CreatureSpellFreeze),
	                 CastAtCreature(MagicType::CreatureSpellCompassion, 1, 0.5f),
	                 CastAtCreature(MagicType::CreatureSpellFreeze, 1, 12.0f)},
	});

	all.push_back({
	    .id = "miracles.creature_casts_amusing_spell",
	    .name = "Creature casting: a spell to amuse another creature",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the itchy and the fat spells is told to amuse a cow a hundred metres away.",
	    .expected = "It picks one of the two spells at random. One time in five it shows that it feels playful first. It "
	                "walks until it is within four times its height (about 60 metres) of the cow, backs off if nearer "
	                "than twice its height, turns to face it and casts the spell from the casting pose. The cow scratches "
	                "or grows fat.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 110.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster"), Other({0.0f, 100.0f}, "target")},
	    .commands = {Know(MagicType::CreatureSpellItchy),
	                 {.kind = Kind::KnowMiracle, .value = static_cast<size_t>(MagicType::CreatureSpellFat)},
	                 {.kind = Kind::DoAction, .delaySeconds = 0.5f, .action = "CastAmusingSpellOnCreature", .atCreature = 1}},
	});
	all.push_back({
	    .id = "miracles.script_casts_heal_power_up",
	    .name = "Creature casting: a script tells it to cast the heal power-up",
	    .facet = Facet::Miracles,
	    .description = "A script tells a tiger that knows the heal power-up to cast it at a hurt villager sixty metres "
	                   "away, as the guide of the first land tells the player's creature.",
	    .expected = "The tiger walks up to within twice its height of the villager, faces it and casts the heal power-up "
	                "from the casting pose. The life of the villager fills up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 70.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 60.0f}, .life = 0.33f}},
	    .commands = {Know(MagicType::HealPowerUpOne), Do("CastHealSpellPU1", 0, 0.5f)},
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
	    .id = "miracles.creature_casts_wood_by_workshop",
	    .name = "Creature casting: wood by a workshop",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the wood miracle is told to cast it by a workshop sixty metres away.",
	    .expected = "One time in five it shows that it feels kind first. It walks up to within twice its height of the "
	                "workshop, backs off if it is nearer, turns to the workshop and casts wood from the casting pose.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = AbodeInfo::CelticWorkshop, .offset = {0.0f, 60.0f}}},
	    .commands = {Know(MagicType::Wood), Do("CastMagicWoodByWorkshop", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_waters_field",
	    .name = "Creature casting: water on a field",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the water miracle is told to water a field of wheat sixty metres away.",
	    .expected = "It walks up to within twice its height of the field and turns to it. It does not draw a gesture. "
	                "It holds its arms out in the scatter pose for three seconds, and the water rains on the field.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, 60.0f}}},
	    .commands = {Know(MagicType::Water), Do("WaterField", 0, 1.0f)},
	});
	all.push_back({
	    .id = "miracles.creature_waters_tree_for_town",
	    .name = "Creature casting: water on a tree for a town",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the water miracle is told to water a tree for a town. The action is about a "
	                   "villager, and the tree, sixty metres away, is the thing that it uses.",
	    .expected = "It walks up to within twice its height of the tree, not the villager, and turns to the tree. It "
	                "holds its arms out in the scatter pose for three seconds, and the water rains on the tree.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 75.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-30.0f, 20.0f}},
	                {.type = TreeInfo::Birch, .offset = {20.0f, 60.0f}}},
	    .commands = {Know(MagicType::Water),
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "WaterTreeForTown", .with = 1}},
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
	    .id = "miracles.creature_picks_up_one_shot",
	    .name = "Creature casting: a one-shot heal that it picks up",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the heal is told to pick up a one-shot heal globe 25 metres ahead, and cast "
	                   "it at a villager hurt to a third of its life, 90 metres ahead.",
	    .expected = "It walks to the globe and picks it up. It walks to within 50 metres of the villager, then backs off "
	                "to twice its height, and turns to the villager. The globe goes from its hand. It holds the casting "
	                "pose for three seconds and the heal lights over the villager.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 95.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 90.0f}, .life = 0.33f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 25.0f}, .bubbleHeight = 0.0f}},
	    .commands = {Know(MagicType::Heal),
	                 {.kind = Kind::DoAction,
	                  .delaySeconds = 1.0f,
	                  .object = 0,
	                  .action = "PickUpAndCastOneOffSpellCompassi",
	                  .globe = 0}},
	});
	all.push_back({
	    .id = "miracles.creature_casts_one_shot_in_hand",
	    .name = "Creature casting: a one-shot lightning bolt in its hand",
	    .facet = Facet::Miracles,
	    .description = "A tiger that knows the lightning bolt is told to hold a one-shot lightning globe at its feet. Then "
	                   "it is told to cast the globe in its hand at a tree 90 metres ahead.",
	    .expected = "It walks to within 50 metres of the tree, then backs off to twice its height, and turns to the "
	                "tree. The globe goes from its hand. It holds the casting pose for three seconds and the bolt hits "
	                "the tree.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 95.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, 90.0f}}},
	    .dispensers = {{.type = MagicType::LightningBolt, .offset = {0.0f, 12.0f}, .bubbleHeight = 0.0f}},
	    .commands = {Know(MagicType::LightningBolt),
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .action = "HoldObject", .globe = 0, .onGlobe = true},
	                 {.kind = Kind::DoAction, .delaySeconds = 10.0f, .object = 0, .action = "CastOneOffSpellInHandAggressive"}},
	});
	all.push_back({
	    .id = "miracles.creature_one_shot_full_hand",
	    .name = "Creature casting: a one-shot heal with a rock in its hand",
	    .facet = Facet::Miracles,
	    .description =
	        "A tiger that knows the heal is told to hold a rock. Then it is told to pick up a one-shot heal globe and "
	        "cast it at a hurt villager, as in the game.",
	    .expected = "With its hand full it does not pick up the globe. It walks to the villager, backs off and turns. The "
	                "rock goes from its hand, it casts the heal, and the globe on the land goes after the cast.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -10.0f}, {30.0f, 95.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 90.0f}, .life = 0.33f},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, 12.0f}}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {20.0f, 25.0f}, .bubbleHeight = 0.0f}},
	    .commands = {Know(MagicType::Heal),
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "HoldObject"},
	                 {.kind = Kind::DoAction,
	                  .delaySeconds = 10.0f,
	                  .object = 0,
	                  .action = "PickUpAndCastOneOffSpellCompassi",
	                  .globe = 0}},
	});

	// The player's hand takes a one-shot globe, holds it in front of the creature and gives it. It waits first, so that the
	// creature comes across the things about it.
	const std::vector<Command> giveGlobe {{.kind = Kind::HandTake, .delaySeconds = 10.0f, .globe = 0},
	                                      {.kind = Kind::HandGive, .creature = 0, .delaySeconds = 1.5f}};
	const HandHold offering {.offset = {0.0f, -10.0f}, .height = 12.0f};
	all.push_back({
	    .id = "miracles.creature_given_heal_globe",
	    .name = "Creature casting: a one-shot heal given from the hand",
	    .facet = Facet::Miracles,
	    .description = "The player's hand takes a one-shot heal globe and gives it to a grown-up tiger that knows the heal. "
	                   "A villager of its town, hurt to a third of its life, stands 30 metres ahead.",
	    .expected = "The tiger turns to the camera, reaches out and takes the globe. The heal counts as one more sighting. "
	                "With a face of compassion it walks to within 50 metres of the thing that it finds most useful for "
	                "compassion, and looks at it. It turns to face the thing. The globe goes from its hand, it holds the "
	                "casting pose for three seconds, and the heal lights over the thing.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -35.0f}, {30.0f, 20.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -30.0f}, .joinTown = true, .life = 0.33f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {-15.0f, 15.0f}, .bubbleHeight = 0.0f}},
	    .commands = {Know(MagicType::Heal), giveGlobe[0], giveGlobe[1]},
	    .hand = offering,
	});
	all.push_back({
	    .id = "miracles.creature_given_unknown_globe",
	    .name = "Creature casting: a one-shot lightning bolt that it does not know",
	    .facet = Facet::Miracles,
	    .description = "The player's hand gives a one-shot lightning globe to a grown-up tiger that has never seen the "
	                   "lightning bolt. A villager of its town stands 30 metres ahead.",
	    .expected = "The tiger takes the globe, and the bolt counts as one sighting. It goes to the thing that it finds "
	                "most useful for anger and turns to it. The globe goes from its hand. It raises its arms, but the "
	                "bolt fizzles: it looks embarrassed, and one time in two sad. The fizzle counts as one more sighting.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -35.0f}, {30.0f, 20.0f}}},
	    .creatures = {Caster({0.0f, 0.0f}, "caster")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -30.0f}, .joinTown = true}},
	    .dispensers = {{.type = MagicType::LightningBolt, .offset = {-15.0f, 15.0f}, .bubbleHeight = 0.0f}},
	    .commands = giveGlobe,
	    .hand = offering,
	});
	auto young = Caster({0.0f, 0.0f}, "young");
	young.phase = 4;
	all.push_back({
	    .id = "miracles.creature_young_given_globe",
	    .name = "Creature casting: a one-shot heal given to a young creature",
	    .facet = Facet::Miracles,
	    .description = "The player's hand gives a one-shot heal globe to a tiger that has not grown up to stage 8.",
	    .expected = "The tiger takes the globe but does not cast it, and the heal counts as no sighting. It uses the globe "
	                "as any other gift: the first time, with no strong desire, it looks the globe over.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {young},
	    .dispensers = {{.type = MagicType::Heal, .offset = {-15.0f, 15.0f}, .bubbleHeight = 0.0f}},
	    .commands = giveGlobe,
	    .hand = offering,
	});

	// Two tigers run a short way, then each is told to cast the itchy spell at a cow far ahead. The wild tiger keeps the
	// run speed on its way to the cow. The player's tiger stops what it did, so it walks.
	CreatureSetup wild = Caster({-120.0f, 0.0f}, "wild caster");
	wild.owner = PlayerNames::NEUTRAL;
	constexpr auto k_Itchy = static_cast<size_t>(MagicType::CreatureSpellItchy);
	std::vector<Command> paces {
	    {.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = k_Itchy},
	    {.kind = Kind::KnowMiracle, .creature = 1, .delaySeconds = 0.0f, .value = k_Itchy},
	    RunTo(0, {-120.0f, 30.0f}, false),
	    RunTo(1, {120.0f, 30.0f}, true),
	    {.kind = Kind::CastMiracle, .creature = 0, .delaySeconds = 0.0f, .value = k_Itchy, .atCreature = 2},
	    {.kind = Kind::CastMiracle, .creature = 1, .delaySeconds = 0.0f, .value = k_Itchy, .atCreature = 3},
	};
	all.push_back({
	    .id = "movement.kept_pace_cast_walk",
	    .name = "Creature casting: the walk keeps the speed it got last",
	    .facet = Facet::Movement,
	    .description = "A wild tiger and the player's tiger run thirty metres. Then each is told to cast the itchy spell "
	                   "at a cow far ahead.",
	    .expected = "Both tigers run. On the way to its cow the wild tiger keeps the run speed. The player's tiger goes "
	                "back to its walk speed, because it stopped what it did. Each stops within about 75 metres of its cow "
	                "and casts the spell.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-140.0f, -10.0f}, {140.0f, 170.0f}}},
	    .creatures = {wild, Caster({120.0f, 0.0f}, "player caster"), Other({-120.0f, 160.0f}, "wild target"),
	                  Other({120.0f, 160.0f}, "player target")},
	    .commands = paces,
	});

	// Three tigers set off at once: an exhausted one and a rested one walk, and a rested one runs
	const std::vector<Command> setOff {
	    {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 1.0f, .point = {-60.0f, 150.0f}},
	    {.kind = Kind::WalkTo, .creature = 1, .delaySeconds = 0.0f, .point = {0.0f, 150.0f}},
	    {.kind = Kind::RunTo, .creature = 2, .delaySeconds = 0.0f, .point = {60.0f, 150.0f}},
	};
	all.push_back({
	    .id = "movement.exhausted_slow_walk",
	    .name = "Creature movement: an exhausted walk and the breath while it moves",
	    .facet = Facet::Movement,
	    .description = "Three tigers go 150 metres. The left tiger is exhausted and walks, the middle tiger walks and the "
	                   "right tiger runs.",
	    .expected = "The exhausted tiger walks at its slow speed. If its exhaustion goes below 0.8 on the way, it stays "
	                "slow until the walk ends. As each tiger sets off, its breath period goes to 3 seconds (walk) or 1.4 "
	                "seconds (run) for the rest of that game turn, then back to 5 seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-80.0f, -10.0f}, {80.0f, 160.0f}}},
	    .creatures = {Caster({-60.0f, 0.0f}, "exhausted walker", 0.85f), Caster({0.0f, 0.0f}, "walker"),
	                  Caster({60.0f, 0.0f}, "runner")},
	    .commands = setOff,
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
