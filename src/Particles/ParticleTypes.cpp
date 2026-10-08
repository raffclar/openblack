/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleTypes.h"

#include <array>

namespace
{
// The original fills the 150 names once (".\Data\Spells\ZSpellFiles\<name>.txt"); the others stay NULL.
constexpr std::array<std::string_view, openblack::psys::k_ParticleTypeCount> k_Files = {
    "",                                   // 0 None
    "",                                   // 1 Tornado
    "",                                   // 2 Firework
    "",                                   // 3 FireworkSingle
    "SF_Forest",                          // 4 Leaves
    "",                                   // 5 Magicball
    "SF_LightningStormPush",              // 6 Lightning
    "SF_LightningBolt",                   // 7 LightningBolt
    "",                                   // 8 Explosion_1
    "SF_Food",                            // 9 Food
    "SF_Food",                            // 10 FoodPoisoned
    "SF_BeamExplosionSingle",             // 11 ExplosionOne
    "SF_BeamExplosionMany",               // 12 ExplosionOnePuOne
    "SF_BeamExplosionLoads",              // 13 ExplosionOnePuTwo
    "SF_BeamExplosionCitadel",            // 14 ExplosionCitadel
    "SF_Wood",                            // 15 Wood
    "SF_Water",                           // 16 Water
    "SF_WaterPU1",                        // 17 WaterPuOne
    "SF_WaterOnHolder",                   // 18 WaterOnHolder
    "SF_WaterInHand",                     // 19 WaterInHand
    "SF_WaterInHandPU1",                  // 20 WaterInHandPuOne
    "SF_HealChakra",                      // 21 Heal
    "SF_ManaPathNew",                     // 22 ManaPath
    "SF_ExplodeObject",                   // 23 ExplodeObject
    "SF_BeliefSprite",                    // 24 BeliefSprite
    "SF_TownBelief",                      // 25 TownBelief
    "SF_FailedApply",                     // 26 SpellFail
    "",                                   // 27 SpellSucceed
    "SF_SpellSelection",                  // 28 SpellSelection
    "SF_GripLandscape",                   // 29 GripLandscape
    "SF_MagicObjectCreated",              // 30 MagicObjectCreated
    "SF_CreatureGestureChain",            // 31 CreatureGesture
    "SF_TeleportVillager",                // 32 VillagerTeleport
    "SF_CreatureTarget",                  // 33 CreatureTarget
    "SF_SimpleBeamCreatureCast",          // 34 CreatureCastVisual
    "SF_Gesture",                         // 35 Gesture
    "SF_Fireworks2",                      // 36 KbTest
    "SF_OnFire",                          // 37 OnFire
    "SF_VolFX",                           // 38 MagicFx
    "SF_VolFXArtifact",                   // 39 MagicFxOnObject
    "SF_VolFXCitadel",                    // 40 MagicFxOnCitadel
    "",                                   // 41 FireFx
    "",                                   // 42 FireFxOnObject
    "SF_FireBallThrow",                   // 43 Fireball
    "SF_FireBallInHand",                  // 44 FireballInHand
    "",                                   // 45 FireballInHandPuOne
    "",                                   // 46 FireballInHandPuTwo
    "SF_FireBallOnHolder",                // 47 FireballOnHolder
    "SF_GestureChain",                    // 48 GestureLocal
    "",                                   // 49 MagicSystem
    "SF_HealChakraInHand",                // 50 HealInHand
    "SF_HealChakraOnHolder",              // 51 HealOnHolder
    "SF_FireBallThrowPU",                 // 52 FireballPuOne
    "SF_FireBallThrowPU2",                // 53 FireballPuTwo
    "SF_HealChakraPU",                    // 54 HealPuOne
    "SF_LightningStormInHand",            // 55 LightningStormInHand
    "SF_LightningStormInHandPU1",         // 56 LightningStormInHandPuOne
    "SF_LightningStormInHandPU2",         // 57 LightningStormInHandPuTwo
    "SF_LightningStormOnHolder",          // 58 LightningStormOnHolder
    "SF_LightningBoltInHand",             // 59 LightningBoltInHand
    "SF_LightningBoltOnHolder",           // 60 LightningBoltOnHolder
    "SF_LightningStrike",                 // 61 LightningStrike
    "SF_LightningSingleStrike",           // 62 LightningSingleStrike
    "",                                   // 63 FoodInHand
    "SF_DefenseSphere",                   // 64 Shield
    "SF_DefenseSphereInHand",             // 65 ShieldInHand
    "SF_DefenseSphereOnHolder",           // 66 ShieldOnHolder
    "SF_PhysicalShieldFX",                // 67 PhysicalShieldFx
    "SF_LightningBoltPUOne",              // 68 LightningBoltPuOne
    "SF_LightningBoltPUTwo",              // 69 LightningBoltPuTwo
    "SF_LightningBoltInHandPUOne",        // 70 LightningBoltInHandPuOne
    "SF_LightningBoltInHandPUTwo",        // 71 LightningBoltInHandPuTwo
    "",                                   // 72 Teleport
    "SF_TeleportVortex",                  // 73 TeleportVortex
    "SF_TeleportOnHolder",                // 74 TeleportOnHolder
    "SF_TeleportInHand",                  // 75 TeleportInHand
    "SF_LandscapeVortexObjectMover",      // 76 LandscapeVortexObjectMover
    "SF_LandscapeVortexLightMap",         // 77 LandscapeVortexLightmap
    "SF_LandscapeVortexInBefore",         // 78 LandscapeVortexInBefore
    "SF_LandscapeVortexInAfter",          // 79 LandscapeVortexInAfter
    "SF_LandscapeVortexInBefore",         // 80 LandscapeVortexOutBefore
    "SF_LandscapeVortexOutAfter",         // 81 LandscapeVortexOutAfter
    "SF_LandscapeVolcanoBefore",          // 82 VolcanoVortexBefore
    "SF_LandscapeVolcanoAfter",           // 83 VolcanoVortexAfter
    "SF_LandscapeVolcanoLightMap",        // 84 VolcanoVortexLightmap
    "SF_CreatureSpellGeneric",            // 85 CreatureSpellPhysical
    "",                                   // 86 CreatureSpellMental
    "SF_CreatureSpellItch",               // 87 CreatureSpellItchy
    "SF_CreatureSpellItchInHand",         // 88 CreatureSpellItchyInHand
    "SF_CreatureSpellItchOnHolder",       // 89 CreatureSpellItchyOnHolder
    "SF_CreatureSpellFreeze",             // 90 CreatureSpellFreeze
    "",                                   // 91 CreatureSpellFreezeInHand
    "SF_CreatureSpellFreezeOnHolder",     // 92 CreatureSpellFreezeOnHolder
    "SF_CreatureSpellCompassion",         // 93 CreatureSpellCompassion
    "",                                   // 94 CreatureSpellCompassionInHand
    "SF_CreatureSpellCompassionOnHolder", // 95 CreatureSpellCompassionOnHolder
    "",                                   // 96 CreatureSpellWeak
    "",                                   // 97 CreatureSpellWeakInHand
    "",                                   // 98 CreatureSpellWeakOnHolder
    "SF_ScriptHighlightGlintsGold",       // 99 ScriptHighlightGoldGlints
    "SF_ScriptHighlightGlintsSilver",     // 100 ScriptHighlightSilverGlints
    "SF_ScriptHighlightGlintsBronze",     // 101 ScriptHighlightBronzeGlints
    "SF_ScriptHighlightActiveGold",       // 102 ScriptHighlightGoldActive
    "SF_ScriptHighlightActiveSilver",     // 103 ScriptHighlightSilverActive
    "",                                   // 104 FlockFlying
    "",                                   // 105 FlockGround
    "SF_StormCast",                       // 106 StormCast
    "SF_MultiPickUpFood",                 // 107 FoodPickup
    "SF_MultiPickUpFoodPoisoned",         // 108 FoodPickupPoisoned
    "SF_MultiPickUpFoodFish",             // 109 FoodPickupFish
    "SF_MultiPickUpWood",                 // 110 WoodPickup
    "SF_MultiPutDownFoodPoisoned",        // 111 FoodPutdownPoisoned
    "SF_MultiPutDownWood",                // 112 WoodPutdown
    "SF_MultiPutDownFood",                // 113 FoodPutdown
    "SF_Steam",                           // 114 Steam
    "SF_Smoke",                           // 115 Smoke
    "SF_Bonfire",                         // 116 Bonfire
    "SF_EvilSmoke",                       // 117 EvilSmoke
    "",                                   // 118 Dust
    "SF_SimpleBeam",                      // 119 MagicBeam
    "SF_SimpleBeamCitadel",               // 120 MagicBeamOnCitadel
    "SF_SimpleBeamCreatureSwap",          // 121 MagicBeamCreatureSwap
    "SF_FlockFlyingCastGood",             // 122 FlockFlyingCastGood
    "SF_FlockFlyingCastEvil",             // 123 FlockFlyingCastEvil
    "SF_FlockFlyingRainGood",             // 124 FlockFlyingRainGood
    "SF_FlockFlyingRainEvil",             // 125 FlockFlyingRainEvil
    "SF_FlockGroundDust",                 // 126 FlockGroundDust
    "SF_Butterflies",                     // 127 Butterflies
    "SF_ButterfliesOnObject",             // 128 ButterfliesOnObject
    "SF_Flies",                           // 129 Flies
    "SF_FliesOnObject",                   // 130 FliesOnObject
    "SF_MagicObjectCreated2",             // 131 ObjectAppear
    "",                                   // 132 ObjectDisappear
    "",                                   // 133 SingStonesGlow
    "SF_PlayerIconFountain",              // 134 PlayerIconFountain
    "SF_SmokeExplode",                    // 135 Bang
    "SF_HealChakra",                      // 136 HealFx
    "SF_HighlightOnObject",               // 137 HighlightOnObject
    "SF_BeamExplosionFX",                 // 138 BeamExplosionFx
    "SF_Flash",                           // 139 Flash
    "SF_TickerTape",                      // 140 TickerTape
    "SF_ForestCreated",                   // 141 ForestCreated
    "SF_SingingStonesHeal",               // 142 SingingStonesHeal
    "SF_SparklesFromObject",              // 143 PilefoodSpeedup
    "SF_SpellDispenserVortex",            // 144 SpelldispenserVortex
    "SF_SeeThisBeam",                     // 145 SeeThisBeam
    "",                                   // 146 SeeThisBeam2
    "",                                   // 147 Test
    "",                                   // 148 Test2
    "",                                   // 149 Test3
};
} // namespace

std::string_view openblack::psys::ParticleTypeFile(ParticleType type)
{
	const auto index = static_cast<size_t>(type);
	return index < k_Files.size() ? k_Files[index] : std::string_view {};
}
