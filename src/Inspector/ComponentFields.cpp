/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "3D/CameraTrack.h"
#include "3D/FieldCrop.h"
#include "3D/HandMorph.h"
#include "3D/LandLightFrame.h"
#include "3D/Light.h"
#include "3D/Lightning.h"
#include "3D/MapCoords.h"
#include "3D/SkeletalAnimation.h"
#include "3D/SkyDome.h"
#include "Animals/AnimalMove.h"
#include "Animals/FishShoal.h"
#include "Audio/AnimEffectKeys.h"
#include "Common/VirtualInfluence.h"
#include "Common/Zoomer.h"
#include "ComponentReflection.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureFizz.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureSway.h"
#include "Creature/CreatureTattoo.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashRope.h"
#include "Creature/LeashRules.h"
#include "Creature/PerceivedDesires.h"
#include "Creature/TempleLeashes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/GripLandscapeParticle.h"
#include "ECS/Components/GroundMark.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGlow.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandMiracleFx.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/IntroHand.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingPhysics.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/ObjectGlow.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/ResourceLastTaken.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptSpotVisual.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SkinOverride.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Translucent.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unlit.h"
#include "ECS/Components/Velocity.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/HighDetailRules.h"
#include "ECS/RewardRules.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/TownAggression.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerDrawRules.h"
#include "ECS/VillagerEyes.h"
#include "Enums.h"
#include "Fire/FireGraphic.h"
#include "Fire/FireModel.h"
#include "Graphics/GraphicsHandle.h"
#include "Graphics/Moon.h"
#include "Graphics/Sun.h"
#include "Hand/HandGrabRules.h"
#include "Magic/DispenserRules.h"
#include "Magic/FlockMiracleRules.h"
#include "Magic/MiracleVisuals.h"
#include "Magic/ResourcePiles.h"
#include "Magic/ShieldRules.h"
#include "Magic/SpellChants.h"
#include "Magic/TownBelief.h"
#include "Particles/ParticleSpellLink.h"
#include "Physics/DamageMesh.h"
#include "ScriptHeaders/ScriptTimers.h"

namespace components = openblack::ecs::components;

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 10> k_DeathReasonNames {
    "None",      "Starving",   "Spell", "Animal", "Chant", "PlayerInteraction", "PlayerInteractionDrown",
    "Sacrifice", "Exhaustion", "OldAge"};
static_assert(static_cast<size_t>(openblack::DeathReason::_COUNT) == k_DeathReasonNames.size());
constexpr std::array<std::string_view, 8> k_PlayerNamesNames {"PLAYER_ONE",  "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
                                                              "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "NEUTRAL"};
static_assert(static_cast<size_t>(openblack::PlayerNames::_COUNT) == k_PlayerNamesNames.size());
constexpr std::array<std::string_view, 40> k_CreatureDesiresDesireNames {"Impress",
                                                                         "Compassion",
                                                                         "Anger",
                                                                         "Play",
                                                                         "Hunger",
                                                                         "Fear",
                                                                         "Curiosity",
                                                                         "Poo",
                                                                         "Tiredness",
                                                                         "IdleWithPlayer",
                                                                         "Wanderlust",
                                                                         "Puke",
                                                                         "BuildHome",
                                                                         "BringStuffHome",
                                                                         "Water",
                                                                         "RestoreHealth",
                                                                         "BeFriends",
                                                                         "AttractAttention",
                                                                         "ManifestState",
                                                                         "GetWarmer",
                                                                         "GetColder",
                                                                         "Scratch",
                                                                         "RunAwayFromPlayer",
                                                                         "Rest",
                                                                         "ObeyPlayer",
                                                                         "Illness",
                                                                         "ObeyCreature",
                                                                         "Sadness",
                                                                         "StayNearHome",
                                                                         "TellPlayer",
                                                                         "PlayWithPlayer",
                                                                         "TellCreature",
                                                                         "EducateFriend",
                                                                         "FollowPlayerDesire",
                                                                         "GetHigh",
                                                                         "HangAroundAtHome",
                                                                         "MentalIllness",
                                                                         "MissFriend",
                                                                         "LookAround",
                                                                         "Steal"};
static_assert(static_cast<size_t>(openblack::creature_desires::Desire::_Count) == k_CreatureDesiresDesireNames.size());
constexpr std::array<std::string_view, 16> k_CreatureSpellsSpellNames {
    "Freeze", "Small", "Big",    "Weak",       "Strong", "Fat", "Thin",    "Invisible",
    "Nice",   "Nasty", "Hungry", "Frightened", "Tired",  "Ill", "Thirsty", "Itchy"};
static_assert(static_cast<size_t>(openblack::creature_spells::Spell::_Count) == k_CreatureSpellsSpellNames.size());
constexpr std::array<std::string_view, 23> k_CreatureTreeAttributeNames {"Allegiance",
                                                                         "Origin",
                                                                         "Animate",
                                                                         "PlayerNumber",
                                                                         "HarderThanMe",
                                                                         "CreatureType",
                                                                         "Type",
                                                                         "Life",
                                                                         "Tribe",
                                                                         "TownReligiousBelief",
                                                                         "TownNeedsMost",
                                                                         "TownSize",
                                                                         "DominantDesire",
                                                                         "Height",
                                                                         "SpellKnowledge",
                                                                         "Carrying",
                                                                         "ForestSize",
                                                                         "VillagerJob",
                                                                         "Sex",
                                                                         "MobileObjectType",
                                                                         "AbodeType",
                                                                         "AbodeBeingBuilt",
                                                                         "OnFire"};
static_assert(static_cast<size_t>(openblack::creature_tree::Attribute::_Count) == k_CreatureTreeAttributeNames.size());
constexpr std::array<std::string_view, 3> k_EcsComponentsLivingActionIndexNames {"Top", "Final", "Previous"};
static_assert(static_cast<size_t>(openblack::ecs::components::LivingAction::Index::_Count) ==
              k_EcsComponentsLivingActionIndexNames.size());

} // namespace

void openblack::inspector::reflection::RegisterComponentFields(entt::meta_ctx& context)
{
	Reflect<components::Abode>(context)
	    .Field<&components::Abode::type>("type")
	    .Field<&components::Abode::townId>("townId")
	    .Field<&components::Abode::foodAmount>("foodAmount")
	    .Field<&components::Abode::woodAmount>("woodAmount")
	    .Field<&components::Abode::inhabitants>("inhabitants")
	    .Field<&components::Abode::presentAtHome>("presentAtHome")
	    .Field<&components::Abode::info>("info");
	Reflect<components::Alignment>(context)
	    .Field<&components::Alignment::value>("value")
	    .Field<&components::Alignment::pending>("pending");
	Reflect<components::Animal>(context)
	    .Field<&components::Animal::type>("type")
	    .Field<&components::Animal::owner>("owner")
	    .Field<&components::Animal::flock>("flock")
	    .Field<&components::Animal::fleeing>("fleeing")
	    .Field<&components::Animal::fleeReaction>("fleeReaction")
	    .Field<&components::Animal::state>("state")
	    .Field<&components::Animal::finalState>("finalState")
	    .Field<&components::Animal::afterClip>("afterClip")
	    .Field<&components::Animal::turnsInState>("turnsInState")
	    .Field<&components::Animal::move>("move")
	    .Field<&components::Animal::goalHeight>("goalHeight")
	    .Field<&components::Animal::height>("height")
	    .Field<&components::Animal::bank>("bank")
	    .Field<&components::Animal::hunger>("hunger")
	    .Field<&components::Animal::animation>("animation")
	    .Field<&components::Animal::clipPlace>("clipPlace")
	    .Field<&components::Animal::previousPosition>("previousPosition")
	    .Field<&components::Animal::position>("position")
	    .Field<&components::Animal::previousHeading>("previousHeading")
	    .Field<&components::Animal::heading>("heading")
	    .Field<&components::Animal::life>("life")
	    .Field<&components::Animal::deadTurns>("deadTurns");
	Reflect<components::TempleBirds>(context)
	    .Field<&components::TempleBirds::flock>("flock")
	    .Field<&components::TempleBirds::look>("look");
	Reflect<components::SpellAnimal>(context)
	    .Field<&components::SpellAnimal::spell>("spell")
	    .Field<&components::SpellAnimal::fade>("fade")
	    .Field<&components::SpellAnimal::previous>("previous");
	Reflect<components::SpellWolf>(context)
	    .Field<&components::SpellWolf::finalDestination>("finalDestination")
	    .Field<&components::SpellWolf::corridor>("corridor")
	    .Field<&components::SpellWolf::prey>("prey")
	    .Field<&components::SpellWolf::huntStart>("huntStart")
	    .Field<&components::SpellWolf::remembered>("remembered")
	    .Field<&components::SpellWolf::food>("food")
	    .Field<&components::SpellWolf::eatCount>("eatCount");
	Reflect<components::AnimalPose>(context)
	    .Field<&components::AnimalPose::bones>("bones")
	    .Field<&components::AnimalPose::light>("light")
	    .Field<&components::AnimalPose::colour>("colour")
	    .Field<&components::AnimalPose::alpha>("alpha")
	    .Field<&components::AnimalPose::cutBelow>("cutBelow");
	Reflect<components::BeingEaten>(context)
	    .Field<&components::BeingEaten::hunter>("hunter")
	    .Field<&components::BeingEaten::turns>("turns")
	    .Field<&components::BeingEaten::eatenFrom>("eatenFrom")
	    .Field<&components::BeingEaten::left>("left");
	Reflect<components::AnimatedStatic>(context)
	    .Field<&components::AnimatedStatic::type>("type")
	    .Field<&components::AnimatedStatic::openState>("openState")
	    .Field<&components::AnimatedStatic::gateStones>("gateStones");
	Reflect<components::AnimatedStaticPose>(context)
	    .Field<&components::AnimatedStaticPose::place>("place")
	    .Field<&components::AnimatedStaticPose::restingPlace>("restingPlace")
	    .Field<&components::AnimatedStaticPose::inDrawList>("inDrawList")
	    .Field<&components::AnimatedStaticPose::onScreen>("onScreen")
	    .Field<&components::AnimatedStaticPose::bones>("bones")
	    .Field<&components::AnimatedStaticPose::stones>("stones");
	Reflect<components::PlinthStone>(context)
	    .Field<&components::PlinthStone::plinth>("plinth")
	    .Field<&components::PlinthStone::pickable>("pickable");
	Reflect<components::AtHome>(context).Field<&components::AtHome::beenToBed>("beenToBed");
	Reflect<components::AudioEmitter>(context)
	    .Field<&components::AudioEmitter::sourceId>("sourceId")
	    .Field<&components::AudioEmitter::soundId>("soundId")
	    .Field<&components::AudioEmitter::priority>("priority")
	    .Field<&components::AudioEmitter::spatial>("spatial")
	    .Field<&components::AudioEmitter::position>("position")
	    .Field<&components::AudioEmitter::gain>("gain")
	    .Field<&components::AudioEmitter::volume>("volume")
	    .Field<&components::AudioEmitter::pitchPercent>("pitchPercent")
	    .Field<&components::AudioEmitter::minDistance>("minDistance")
	    .Field<&components::AudioEmitter::maxDistance>("maxDistance")
	    .Field<&components::AudioEmitter::distanceScale>("distanceScale")
	    .Field<&components::AudioEmitter::loop>("loop")
	    .Field<&components::AudioEmitter::state>("state")
	    .Field<&components::AudioEmitter::music>("music")
	    .Field<&components::AudioEmitter::owner>("owner")
	    .Field<&components::AudioEmitter::bank>("bank")
	    .Field<&components::AudioEmitter::group>("group");
	Reflect<components::Ball>(context).Field<&components::Ball::destination>("destination");
	Reflect<components::BuildingDamage>(context)
	    .Field<&components::BuildingDamage::mesh>("mesh")
	    .Field<&components::BuildingDamage::lastHitter>("lastHitter")
	    .Field<&components::BuildingDamage::drawMesh>("drawMesh")
	    .Field<&components::BuildingDamage::version>("version");
	Reflect<components::BuildingPiece>(context)
	    .Field<&components::BuildingPiece::mesh>("mesh")
	    .Field<&components::BuildingPiece::parent>("parent")
	    .Field<&components::BuildingPiece::sourceMesh>("sourceMesh")
	    .Field<&components::BuildingPiece::turnsLeft>("turnsLeft")
	    .Field<&components::BuildingPiece::drawMesh>("drawMesh");
	Reflect<components::CameraBookmark>(context)
	    .Field<&components::CameraBookmark::number>("number")
	    .Field<&components::CameraBookmark::animationTime>("animationTime")
	    .Field<&components::CameraBookmark::savedOrigin>("savedOrigin");
	Reflect<components::CarriedByTornado>(context).Field<&components::CarriedByTornado::carried>("carried");
	Reflect<components::CaughtByTornado> {context};
	Reflect<components::ChimneySmoke>(context)
	    .Field<&components::ChimneySmoke::chimney>("chimney")
	    .Field<&components::ChimneySmoke::rgb>("rgb")
	    .Field<&components::ChimneySmoke::state>("state")
	    .Field<&components::ChimneySmoke::ageRemainder>("ageRemainder")
	    .Field<&components::ChimneySmoke::puffs>("puffs");
	Reflect<components::Cloud>(context).Field<&components::Cloud::track>("track").Field<&components::Cloud::pinned>("pinned");
	Reflect<components::PlannedTemple>(context)
	    .Field<&components::PlannedTemple::townId>("townId")
	    .Field<&components::PlannedTemple::owner>("owner")
	    .Field<&components::PlannedTemple::yAngle>("yAngle");
	Reflect<components::PlannedAbode>(context)
	    .Field<&components::PlannedAbode::townId>("townId")
	    .Field<&components::PlannedAbode::info>("info");
	Reflect<components::BuildingSite>(context)
	    .Field<&components::BuildingSite::desire>("desire")
	    .Field<&components::BuildingSite::workers>("workers")
	    .Field<&components::BuildingSite::workerCount>("workerCount")
	    .Field<&components::BuildingSite::places>("places")
	    .Field<&components::BuildingSite::piles>("piles");
	Reflect<components::Creature>(context)
	    .Field<&components::Creature::owner>("owner")
	    .Field<&components::Creature::leashable>("leashable")
	    .Field<&components::Creature::species>("species")
	    .Field<&components::Creature::mind>("mind")
	    .Field<&components::Creature::alignment>("alignment")
	    .Field<&components::Creature::pendingAlignment>("pendingAlignment")
	    .Field<&components::Creature::fatness>("fatness")
	    .Field<&components::Creature::strength>("strength")
	    .Field<&components::Creature::size>("size")
	    .Field<&components::Creature::penSize>("penSize")
	    .Field<&components::Creature::autoScale>("autoScale")
	    .Field<&components::Creature::objectsDestroyed>("objectsDestroyed")
	    .Field<&components::Creature::canDie>("canDie");
	Reflect<components::CreatureArena>(context)
	    .Field<&components::CreatureArena::place>("place")
	    .Field<&components::CreatureArena::radius>("radius")
	    .Field<&components::CreatureArena::temporary>("temporary")
	    .Field<&components::CreatureArena::fightOn>("fightOn")
	    .Field<&components::CreatureArena::first>("first")
	    .Field<&components::CreatureArena::second>("second")
	    .Field<&components::CreatureArena::ring>("ring");
	Reflect<components::CreatureAudio>(context)
	    .Field<&components::CreatureAudio::last>("last")
	    .Field<&components::CreatureAudio::recent>("recent")
	    .Field<&components::CreatureAudio::clockMs>("clockMs");
	Reflect<components::CreatureMorph>(context)
	    .Field<&components::CreatureMorph::shownFatness>("shownFatness")
	    .Field<&components::CreatureMorph::drawn>("drawn")
	    .Field<&components::CreatureMorph::revision>("revision");
	Reflect<components::CreatureAnimation>(context)
	    .Field<&components::CreatureAnimation::body>("body")
	    .Field<&components::CreatureAnimation::slots>("slots")
	    .Field<&components::CreatureAnimation::face>("face")
	    .Field<&components::CreatureAnimation::gesture>("gesture")
	    .Field<&components::CreatureAnimation::wobble>("wobble")
	    .Field<&components::CreatureAnimation::sway>("sway")
	    .Field<&components::CreatureAnimation::lookAt>("lookAt")
	    .Field<&components::CreatureAnimation::yaw>("yaw")
	    .Field<&components::CreatureAnimation::pitch>("pitch")
	    .Field<&components::CreatureAnimation::playbackScale>("playbackScale")
	    .Field<&components::CreatureAnimation::breathPhase>("breathPhase")
	    .Field<&components::CreatureAnimation::breathPeriod>("breathPeriod")
	    .Field<&components::CreatureAnimation::builtRevision>("builtRevision")
	    .Field<&components::CreatureAnimation::skeleton>("skeleton")
	    .Field<&components::CreatureAnimation::animations>("animations")
	    .Field<&components::CreatureAnimation::mirror>("mirror")
	    .Field<&components::CreatureAnimation::boneMatrices>("boneMatrices");
	Reflect<components::CreatureEyes>(context)
	    .Field<&components::CreatureEyes::mode>("mode")
	    .Field<&components::CreatureEyes::openness>("openness")
	    .Field<&components::CreatureEyes::blink>("blink")
	    .Field<&components::CreatureEyes::lookAt>("lookAt")
	    .Field<&components::CreatureEyes::look>("look")
	    .Field<&components::CreatureEyes::lookStarted>("lookStarted")
	    .Field<&components::CreatureEyes::drawn>("drawn")
	    .Field<&components::CreatureEyes::lidColour>("lidColour");
	Reflect<components::CreatureCasting>(context)
	    .Field<&components::CreatureCasting::move>("move")
	    .Field<&components::CreatureCasting::outcome>("outcome")
	    .Field<&components::CreatureCasting::target>("target")
	    .Field<&components::CreatureCasting::keep>("keep")
	    .Field<&components::CreatureCasting::settleSeconds>("settleSeconds")
	    .Field<&components::CreatureCasting::phase>("phase")
	    .Field<&components::CreatureCasting::turns>("turns")
	    .Field<&components::CreatureCasting::holdTurns>("holdTurns")
	    .Field<&components::CreatureCasting::stuckCheck>("stuckCheck")
	    .Field<&components::CreatureCasting::destination>("destination")
	    .Field<&components::CreatureCasting::fizzle>("fizzle");
	Reflect<components::CreatureFighting>(context)
	    .Field<&components::CreatureFighting::stage>("stage")
	    .Field<&components::CreatureFighting::opponent>("opponent")
	    .Field<&components::CreatureFighting::arena>("arena")
	    .Field<&components::CreatureFighting::madeArena>("madeArena")
	    .Field<&components::CreatureFighting::arenaEntity>("arenaEntity")
	    .Field<&components::CreatureFighting::fighter>("fighter")
	    .Field<&components::CreatureFighting::reaches>("reaches")
	    .Field<&components::CreatureFighting::hitTimesMs>("hitTimesMs")
	    .Field<&components::CreatureFighting::measured>("measured")
	    .Field<&components::CreatureFighting::movedShare>("movedShare")
	    .Field<&components::CreatureFighting::stageSeconds>("stageSeconds")
	    .Field<&components::CreatureFighting::faceSeconds>("faceSeconds")
	    .Field<&components::CreatureFighting::played>("played")
	    .Field<&components::CreatureFighting::taunted>("taunted")
	    .Field<&components::CreatureFighting::ended>("ended");
	Reflect<components::CreatureFightRecord>(context)
	    .Field<&components::CreatureFightRecord::tendency>("tendency")
	    .Field<&components::CreatureFightRecord::foughtBefore>("foughtBefore")
	    .Field<&components::CreatureFightRecord::fights>("fights")
	    .Field<&components::CreatureFightRecord::wins>("wins")
	    .Field<&components::CreatureFightRecord::secondsSinceFight>("secondsSinceFight")
	    .Field<&components::CreatureFightRecord::control>("control")
	    .Field<&components::CreatureFightRecord::health>("health");
	Reflect<components::CreatureKnockedOut>(context)
	    .Field<&components::CreatureKnockedOut::stage>("stage")
	    .Field<&components::CreatureKnockedOut::seconds>("seconds")
	    .Field<&components::CreatureKnockedOut::rest>("rest")
	    .Field<&components::CreatureKnockedOut::permanent>("permanent")
	    .Field<&components::CreatureKnockedOut::home>("home")
	    .Field<&components::CreatureKnockedOut::fizzTurns>("fizzTurns");
	Reflect<components::CreatureFizz>(context).Field<&components::CreatureFizz::fizz>("fizz");
	Reflect<components::CreatureHair>(context)
	    .Field<&components::CreatureHair::groups>("groups")
	    .Field<&components::CreatureHair::started>("started");
	Reflect<components::CreatureLeash>(context)
	    .Field<&components::CreatureLeash::known>("known")
	    .Field<&components::CreatureLeash::worn>("worn")
	    .Field<&components::CreatureLeash::selected>("selected")
	    .Field<&components::CreatureLeash::drawn>("drawn")
	    .Field<&components::CreatureLeash::control>("control")
	    .Field<&components::CreatureLeash::pull>("pull")
	    .Field<&components::CreatureLeash::pulls>("pulls")
	    .Field<&components::CreatureLeash::turnsWithOther>("turnsWithOther")
	    .Field<&components::CreatureLeash::confinementCentre>("confinementCentre")
	    .Field<&components::CreatureLeash::confinementRadius>("confinementRadius")
	    .Field<&components::CreatureLeash::home>("home")
	    .Field<&components::CreatureLeash::returning>("returning")
	    .Field<&components::CreatureLeash::order>("order")
	    .Field<&components::CreatureLeash::help>("help")
	    .Field<&components::CreatureLeash::helpDesire>("helpDesire");
	Reflect<components::LeashMarker>(context).Field<&components::LeashMarker::creature>("creature");
	Reflect<components::LeashPost>(context)
	    .Field<&components::LeashPost::type>("type")
	    .Field<&components::LeashPost::owner>("owner")
	    .Field<&components::LeashPost::selected>("selected")
	    .Field<&components::LeashPost::point>("point")
	    .Field<&components::LeashPost::look>("look")
	    .Field<&components::LeashPost::hung>("hung")
	    .Field<&components::LeashPost::glow>("glow");
	Reflect<components::CreatureLocomotion>(context)
	    .Field<&components::CreatureLocomotion::motion>("motion")
	    .Field<&components::CreatureLocomotion::started>("started")
	    .Field<&components::CreatureLocomotion::heading>("heading")
	    .Field<&components::CreatureLocomotion::targetHeading>("targetHeading")
	    .Field<&components::CreatureLocomotion::speed>("speed")
	    .Field<&components::CreatureLocomotion::fraction>("fraction")
	    .Field<&components::CreatureLocomotion::speeds>("speeds")
	    .Field<&components::CreatureLocomotion::scale>("scale")
	    .Field<&components::CreatureLocomotion::radius>("radius")
	    .Field<&components::CreatureLocomotion::destination>("destination")
	    .Field<&components::CreatureLocomotion::ring>("ring")
	    .Field<&components::CreatureLocomotion::planner>("planner")
	    .Field<&components::CreatureLocomotion::route>("route")
	    .Field<&components::CreatureLocomotion::routeReady>("routeReady")
	    .Field<&components::CreatureLocomotion::planningMs>("planningMs")
	    .Field<&components::CreatureLocomotion::failed>("failed")
	    .Field<&components::CreatureLocomotion::facingOnly>("facingOnly")
	    .Field<&components::CreatureLocomotion::following>("following")
	    .Field<&components::CreatureLocomotion::followDistance>("followDistance")
	    .Field<&components::CreatureLocomotion::move>("move")
	    .Field<&components::CreatureLocomotion::walkTimeMs>("walkTimeMs")
	    .Field<&components::CreatureLocomotion::distance>("distance")
	    .Field<&components::CreatureLocomotion::fidgetMs>("fidgetMs")
	    .Field<&components::CreatureLocomotion::fidgeted>("fidgeted")
	    .Field<&components::CreatureLocomotion::fromPosition>("fromPosition")
	    .Field<&components::CreatureLocomotion::toPosition>("toPosition")
	    .Field<&components::CreatureLocomotion::fromHeading>("fromHeading")
	    .Field<&components::CreatureLocomotion::toHeading>("toHeading")
	    .Field<&components::CreatureLocomotion::tracks>("tracks");
	Reflect<components::CreatureMindState>(context)
	    .Field<&components::CreatureMindState::desires>("desires")
	    .Field<&components::CreatureMindState::idle>("idle")
	    .Field<&components::CreatureMindState::scriptPlay>("scriptPlay")
	    .Field<&components::CreatureMindState::look>("look")
	    .Field<&components::CreatureMindState::lookingAbout>("lookingAbout")
	    .Field<&components::CreatureMindState::secondsAlone>("secondsAlone")
	    .Field<&components::CreatureMindState::feedbackSeconds>("feedbackSeconds")
	    .Field<&components::CreatureMindState::feedbackWasStroke>("feedbackWasStroke")
	    .Field<&components::CreatureMindState::attitudeToPlayer>("attitudeToPlayer")
	    .Field<&components::CreatureMindState::averageFeedback>("averageFeedback")
	    .Field<&components::CreatureMindState::perceivedDesires>("perceivedDesires")
	    .Field<&components::CreatureMindState::lastFeedback>("lastFeedback")
	    .Field<&components::CreatureMindState::interactionMagnitude>("interactionMagnitude")
	    .Field<&components::CreatureMindState::actionCounts>("actionCounts")
	    .Field<&components::CreatureMindState::underway>("underway")
	    .Field<&components::CreatureMindState::developmentPhase>("developmentPhase")
	    .Field<&components::CreatureMindState::desiresPhase>("desiresPhase")
	    .Field<&components::CreatureMindState::paused>("paused")
	    .Field<&components::CreatureMindState::learnt>("learnt")
	    .Field<&components::CreatureMindState::planner>("planner")
	    .Field<&components::CreatureMindState::townCompassion>("townCompassion")
	    .Field<&components::CreatureMindState::planActive>("planActive")
	    .Field<&components::CreatureMindState::satisfiedByEffect>("satisfiedByEffect")
	    .Field<&components::CreatureMindState::desireSeenTo>("desireSeenTo")
	    .Field<&components::CreatureMindState::planSerial>("planSerial")
	    .Field<&components::CreatureMindState::agendaSeen>("agendaSeen")
	    .Field<&components::CreatureMindState::stepTurns>("stepTurns")
	    .Field<&components::CreatureMindState::turn>("turn")
	    .Field<&components::CreatureMindState::plannedTurn>("plannedTurn")
	    .Field<&components::CreatureMindState::trainer>("trainer")
	    .Field<&components::CreatureMindState::pendingFile>("pendingFile")
	    .Field<&components::CreatureMindState::resourceChecked>("resourceChecked")
	    .Field<&components::CreatureMindState::pendingTeaching>("pendingTeaching")
	    .Field<&components::CreatureMindState::leash>("leash")
	    .Field<&components::CreatureMindState::heldKinds>("heldKinds");
	Reflect<components::CreatureNeeds>(context)
	    .Field<&components::CreatureNeeds::started>("started")
	    .Field<&components::CreatureNeeds::needs>("needs")
	    .Field<&components::CreatureNeeds::kept>("kept")
	    .Field<&components::CreatureNeeds::rest>("rest")
	    .Field<&components::CreatureNeeds::restTurns>("restTurns")
	    .Field<&components::CreatureNeeds::rested>("rested")
	    .Field<&components::CreatureNeeds::faint>("faint")
	    .Field<&components::CreatureNeeds::carriedWeight>("carriedWeight")
	    .Field<&components::CreatureNeeds::moving>("moving");
	Reflect<components::CreaturePukeDrop>(context)
	    .Field<&components::CreaturePukeDrop::velocity>("velocity")
	    .Field<&components::CreaturePukeDrop::seconds>("seconds");
	Reflect<components::CreatureObjectAction>(context)
	    .Field<&components::CreatureObjectAction::kind>("kind")
	    .Field<&components::CreatureObjectAction::phase>("phase")
	    .Field<&components::CreatureObjectAction::status>("status")
	    .Field<&components::CreatureObjectAction::target>("target")
	    .Field<&components::CreatureObjectAction::point>("point")
	    .Field<&components::CreatureObjectAction::animations>("animations")
	    .Field<&components::CreatureObjectAction::weights>("weights")
	    .Field<&components::CreatureObjectAction::animationCount>("animationCount")
	    .Field<&components::CreatureObjectAction::mirrored>("mirrored")
	    .Field<&components::CreatureObjectAction::timeMs>("timeMs")
	    .Field<&components::CreatureObjectAction::durationMs>("durationMs")
	    .Field<&components::CreatureObjectAction::eventMs>("eventMs")
	    .Field<&components::CreatureObjectAction::eventDone>("eventDone")
	    .Field<&components::CreatureObjectAction::holdMs>("holdMs")
	    .Field<&components::CreatureObjectAction::pointSeconds>("pointSeconds")
	    .Field<&components::CreatureObjectAction::reach>("reach")
	    .Field<&components::CreatureObjectAction::maxReach>("maxReach")
	    .Field<&components::CreatureObjectAction::attempts>("attempts")
	    .Field<&components::CreatureObjectAction::flightSeconds>("flightSeconds")
	    .Field<&components::CreatureObjectAction::givenFlightSeconds>("givenFlightSeconds")
	    .Field<&components::CreatureObjectAction::waitsForLanding>("waitsForLanding")
	    .Field<&components::CreatureObjectAction::thrown>("thrown")
	    .Field<&components::CreatureObjectAction::catchHands>("catchHands")
	    .Field<&components::CreatureObjectAction::catching>("catching")
	    .Field<&components::CreatureObjectAction::catchTurned>("catchTurned")
	    .Field<&components::CreatureObjectAction::catchHeight>("catchHeight")
	    .Field<&components::CreatureObjectAction::failure>("failure");
	Reflect<components::CreatureDroppedObject>(context).Field<&components::CreatureDroppedObject::object>("object");
	Reflect<components::CreatureHeldObject>(context)
	    .Field<&components::CreatureHeldObject::object>("object")
	    .Field<&components::CreatureHeldObject::mirrored>("mirrored")
	    .Field<&components::CreatureHeldObject::bone>("bone")
	    .Field<&components::CreatureHeldObject::rotation>("rotation")
	    .Field<&components::CreatureHeldObject::middle>("middle");
	Reflect<components::PendingCatch>(context).Field<&components::PendingCatch::object>("object");
	Reflect<components::CatchHands>(context).Field<&components::CatchHands::hands>("hands");
	Reflect<components::HeldByCreature>(context).Field<&components::HeldByCreature::creature>("creature");
	Reflect<components::CreatureTownAttitude>(context)
	    .Field<&components::CreatureTownAttitude::attitude>("attitude")
	    .Field<&components::CreatureTownAttitude::secondsLeft>("secondsLeft");
	Reflect<components::CreatureTattoos>(context)
	    .Field<&components::CreatureTattoos::slots>("slots")
	    .Field<&components::CreatureTattoos::revision>("revision");
	Reflect<components::CreatureMarks>(context)
	    .Field<&components::CreatureMarks::marks>("marks")
	    .Field<&components::CreatureMarks::revision>("revision");
	Reflect<components::CreatureSkin>(context)
	    .Field<&components::CreatureSkin::skins>("skins")
	    .Field<&components::CreatureSkin::painted>("painted")
	    .Field<&components::CreatureSkin::revision>("revision");
	Reflect<components::CreatureSpells>(context)
	    .Field<&components::CreatureSpells::spells>("spells")
	    .Field<&components::CreatureSpells::freeze>("freeze")
	    .Field<&components::CreatureSpells::fizz>("fizz")
	    .Field<&components::CreatureSpells::staticScroll>("staticScroll")
	    .Field<&components::CreatureSpells::invisible>("invisible")
	    .Field<&components::CreatureSpells::pausedMind>("pausedMind")
	    .Field<&components::CreatureSpells::smallestSize>("smallestSize")
	    .Field<&components::CreatureSpells::largestSize>("largestSize")
	    .Field<&components::CreatureSpells::cheat>("cheat");
	Reflect<components::DanceMove>(context)
	    .Field<&components::DanceMove::action>("action")
	    .Field<&components::DanceMove::first>("first")
	    .Field<&components::DanceMove::second>("second")
	    .Field<&components::DanceMove::end>("end");
	Reflect<components::DanceGroup>(context)
	    .Field<&components::DanceGroup::name>("name")
	    .Field<&components::DanceGroup::dancers>("dancers")
	    .Field<&components::DanceGroup::limited>("limited")
	    .Field<&components::DanceGroup::quota>("quota")
	    .Field<&components::DanceGroup::weight>("weight")
	    .Field<&components::DanceGroup::limitedDancers>("limitedDancers")
	    .Field<&components::DanceGroup::danceType>("danceType")
	    .Field<&components::DanceGroup::sexes>("sexes")
	    .Field<&components::DanceGroup::formation>("formation")
	    .Field<&components::DanceGroup::inFormation>("inFormation")
	    .Field<&components::DanceGroup::flag>("flag")
	    .Field<&components::DanceGroup::shape>("shape")
	    .Field<&components::DanceGroup::radius>("radius")
	    .Field<&components::DanceGroup::offset>("offset")
	    .Field<&components::DanceGroup::rotation>("rotation")
	    .Field<&components::DanceGroup::spin>("spin")
	    .Field<&components::DanceGroup::move>("move")
	    .Field<&components::DanceGroup::spinRate>("spinRate")
	    .Field<&components::DanceGroup::rotationRate>("rotationRate")
	    .Field<&components::DanceGroup::radiusRate>("radiusRate")
	    .Field<&components::DanceGroup::offsetRate>("offsetRate")
	    .Field<&components::DanceGroup::moved>("moved")
	    .Field<&components::DanceGroup::parent>("parent")
	    .Field<&components::DanceGroup::indexInParent>("indexInParent")
	    .Field<&components::DanceGroup::children>("children");
	Reflect<components::DanceGroups>(context)
	    .Field<&components::DanceGroups::all>("all")
	    .Field<&components::DanceGroups::limited>("limited")
	    .Field<&components::DanceGroups::shared>("shared")
	    .Field<&components::DanceGroups::round>("round")
	    .Field<&components::DanceGroups::roundLength>("roundLength");
	Reflect<components::Dance>(context)
	    .Field<&components::Dance::type>("type")
	    .Field<&components::Dance::place>("place")
	    .Field<&components::Dance::angle>("angle")
	    .Field<&components::Dance::owner>("owner")
	    .Field<&components::Dance::state>("state")
	    .Field<&components::Dance::autostart>("autostart")
	    .Field<&components::Dance::madeByScript>("madeByScript")
	    .Field<&components::Dance::speed>("speed")
	    .Field<&components::Dance::rate>("rate")
	    .Field<&components::Dance::dancingRate>("dancingRate")
	    .Field<&components::Dance::clock>("clock")
	    .Field<&components::Dance::loopLength>("loopLength")
	    .Field<&components::Dance::duration>("duration")
	    .Field<&components::Dance::startTurn>("startTurn")
	    .Field<&components::Dance::dancers>("dancers")
	    .Field<&components::Dance::onTheirWay>("onTheirWay")
	    .Field<&components::Dance::firstDancerTurn>("firstDancerTurn")
	    .Field<&components::Dance::groups>("groups")
	    .Field<&components::Dance::file>("file");
	Reflect<components::Dancer>(context).Field<&components::Dancer::dance>("dance").Field<&components::Dancer::group>("group");
	Reflect<components::DeadTree>(context)
	    .Field<&components::DeadTree::type>("type")
	    .Field<&components::DeadTree::woodMultiplier>("woodMultiplier")
	    .Field<&components::DeadTree::felled>("felled");
	Reflect<components::DestructionGhost>(context)
	    .Field<&components::DestructionGhost::mesh>("mesh")
	    .Field<&components::DestructionGhost::model>("model")
	    .Field<&components::DestructionGhost::millisecondsLeft>("millisecondsLeft")
	    .Field<&components::DestructionGhost::shown>("shown");
	Reflect<components::DetailMeshes>(context)
	    .Field<&components::DetailMeshes::meshes>("meshes")
	    .Field<&components::DetailMeshes::importance>("importance");
	Reflect<components::DropsRoots> {context};
	Reflect<components::FallingRoots>(context)
	    .Field<&components::FallingRoots::seconds>("seconds")
	    .Field<&components::FallingRoots::startHeight>("startHeight")
	    .Field<&components::FallingRoots::restHeight>("restHeight");
	Reflect<components::ShownRoots>(context).Field<&components::ShownRoots::place>("place");
	Reflect<components::Feature>(context).Field<&components::Feature::type>("type");
	Reflect<components::Field>(context)
	    .Field<&components::Field::town>("town")
	    .Field<&components::Field::type>("type")
	    .Field<&components::Field::crop>("crop")
	    .Field<&components::Field::growthTurn>("growthTurn")
	    .Field<&components::Field::height>("height");
	Reflect<components::Fire>(context)
	    .Field<&components::Fire::state>("state")
	    .Field<&components::Fire::source>("source")
	    .Field<&components::Fire::player>("player")
	    .Field<&components::Fire::hasPlayer>("hasPlayer")
	    .Field<&components::Fire::root>("root")
	    .Field<&components::Fire::createdTurn>("createdTurn")
	    .Field<&components::Fire::waits>("waits")
	    .Field<&components::Fire::reaction>("reaction")
	    .Field<&components::Fire::lastSteam>("lastSteam");
	Reflect<components::FireGroup>(context)
	    .Field<&components::FireGroup::members>("members")
	    .Field<&components::FireGroup::firemen>("firemen");
	Reflect<components::FireLook>(context)
	    .Field<&components::FireLook::graphic>("graphic")
	    .Field<&components::FireLook::lightsLand>("lightsLand");
	Reflect<components::FireProofing>(context)
	    .Field<&components::FireProofing::cannotBeSetOnFire>("cannotBeSetOnFire")
	    .Field<&components::FireProofing::notHurtByFire>("notHurtByFire");
	Reflect<components::VillagerFireState>(context)
	    .Field<&components::VillagerFireState::fire>("fire")
	    .Field<&components::VillagerFireState::reactionTarget>("reactionTarget")
	    .Field<&components::VillagerFireState::reaction>("reaction")
	    .Field<&components::VillagerFireState::walkTarget>("walkTarget");
	Reflect<components::ObjectLife>(context).Field<&components::ObjectLife::life>("life");
	Reflect<components::FireflyDrift>(context)
	    .Field<&components::FireflyDrift::flightSpeed>("flightSpeed")
	    .Field<&components::FireflyDrift::quickSpeed>("quickSpeed")
	    .Field<&components::FireflyDrift::phases>("phases");
	Reflect<components::Firefly>(context)
	    .Field<&components::Firefly::state>("state")
	    .Field<&components::Firefly::at>("at")
	    .Field<&components::Firefly::previous>("previous")
	    .Field<&components::Firefly::home>("home")
	    .Field<&components::Firefly::hover>("hover")
	    .Field<&components::Firefly::drawn>("drawn")
	    .Field<&components::Firefly::clock>("clock")
	    .Field<&components::Firefly::amplitude>("amplitude")
	    .Field<&components::Firefly::progress>("progress")
	    .Field<&components::Firefly::flightSeconds>("flightSeconds")
	    .Field<&components::Firefly::drift>("drift")
	    .Field<&components::Firefly::hidden>("hidden");
	Reflect<components::FishFarm>(context)
	    .Field<&components::FishFarm::town>("town")
	    .Field<&components::FishFarm::place>("place")
	    .Field<&components::FishFarm::fish>("fish")
	    .Field<&components::FishFarm::fishermen>("fishermen")
	    .Field<&components::FishFarm::shoal>("shoal")
	    .Field<&components::FishFarm::shownAlpha>("shownAlpha");
	Reflect<components::Fixed>(context)
	    .Field<&components::Fixed::boundingCenter>("boundingCenter")
	    .Field<&components::Fixed::boundingRadius>("boundingRadius");
	Reflect<components::FloatingNumber>(context)
	    .Field<&components::FloatingNumber::text>("text")
	    .Field<&components::FloatingNumber::position>("position")
	    .Field<&components::FloatingNumber::colour>("colour")
	    .Field<&components::FloatingNumber::life>("life");
	Reflect<components::Flock>(context)
	    .Field<&components::Flock::place>("place")
	    .Field<&components::Flock::members>("members")
	    .Field<&components::Flock::domainRadius>("domainRadius")
	    .Field<&components::Flock::flockDistance>("flockDistance")
	    .Field<&components::Flock::calm>("calm")
	    .Field<&components::Flock::followState>("followState")
	    .Field<&components::Flock::followMode>("followMode")
	    .Field<&components::Flock::afterMove>("afterMove")
	    .Field<&components::Flock::turnsOnLeg>("turnsOnLeg")
	    .Field<&components::Flock::height>("height")
	    .Field<&components::Flock::scriptId>("scriptId")
	    .Field<&components::Flock::made>("made")
	    .Field<&components::Flock::temple>("temple");
	Reflect<components::FlockMember>(context)
	    .Field<&components::FlockMember::flock>("flock")
	    .Field<&components::FlockMember::order>("order");
	Reflect<components::FlockSpell>(context)
	    .Field<&components::FlockSpell::flock>("flock")
	    .Field<&components::FlockSpell::created>("created")
	    .Field<&components::FlockSpell::emitted>("emitted")
	    .Field<&components::FlockSpell::lastSpawn>("lastSpawn")
	    .Field<&components::FlockSpell::lastSpawnHeight>("lastSpawnHeight")
	    .Field<&components::FlockSpell::castEffect>("castEffect")
	    .Field<&components::FlockSpell::castEffectStarted>("castEffectStarted")
	    .Field<&components::FlockSpell::evil>("evil")
	    .Field<&components::FlockSpell::closedDown>("closedDown");
	Reflect<components::Flowers>(context).Field<&components::Flowers::type>("type");
	Reflect<components::Footpath>(context).Field<&components::Footpath::nodes>("nodes");
	Reflect<components::FootpathLink>(context)
	    .Field<&components::FootpathLink::position>("position")
	    .Field<&components::FootpathLink::footpaths>("footpaths");
	Reflect<components::BigForest>(context).Field<&components::BigForest::type>("type").Field<&components::BigForest::worth>(
	    "worth");
	Reflect<components::Forest>(context).Field<&components::Forest::type>("type");
	Reflect<components::ForestMember>(context)
	    .Field<&components::ForestMember::forest>("forest")
	    .Field<&components::ForestMember::growing>("growing")
	    .Field<&components::ForestMember::listed>("listed");
	Reflect<components::GripLandscapeParticle>(context)
	    .Field<&components::GripLandscapeParticle::centre>("centre")
	    .Field<&components::GripLandscapeParticle::offset>("offset")
	    .Field<&components::GripLandscapeParticle::age>("age")
	    .Field<&components::GripLandscapeParticle::frame>("frame");
	Reflect<components::GroundMark>(context)
	    .Field<&components::GroundMark::millisecondsLeft>("millisecondsLeft")
	    .Field<&components::GroundMark::alpha>("alpha");
	Reflect<components::Hand>(context)
	    .Field<&components::Hand::rightHanded>("rightHanded")
	    .Field<&components::Hand::renderType>("renderType")
	    .Field<&components::Hand::boneMatrices>("boneMatrices");
	Reflect<components::HandClicked>(context)
	    .Field<&components::HandClicked::thing>("thing")
	    .Field<&components::HandClicked::thingTurn>("thingTurn")
	    .Field<&components::HandClicked::place>("place")
	    .Field<&components::HandClicked::placeTurn>("placeTurn");
	Reflect<components::HandGlow>(context).Field<&components::HandGlow::rgb>("rgb");
	Reflect<components::InHand>(context).Field<&components::InHand::hand>("hand");
	Reflect<components::CannotBePickedUp> {context};
	Reflect<components::HandGrab>(context)
	    .Field<&components::HandGrab::state>("state")
	    .Field<&components::HandGrab::object>("object")
	    .Field<&components::HandGrab::pressMs>("pressMs")
	    .Field<&components::HandGrab::pressTurn>("pressTurn")
	    .Field<&components::HandGrab::waits>("waits")
	    .Field<&components::HandGrab::pullSeconds>("pullSeconds")
	    .Field<&components::HandGrab::pulling>("pulling")
	    .Field<&components::HandGrab::tug>("tug")
	    .Field<&components::HandGrab::stretch>("stretch")
	    .Field<&components::HandGrab::holdDistance>("holdDistance")
	    .Field<&components::HandGrab::pullPlanePoint>("pullPlanePoint")
	    .Field<&components::HandGrab::pullPlaneNormal>("pullPlaneNormal")
	    .Field<&components::HandGrab::hold>("hold")
	    .Field<&components::HandGrab::lowering>("lowering")
	    .Field<&components::HandGrab::rise>("rise")
	    .Field<&components::HandGrab::spring>("spring")
	    .Field<&components::HandGrab::springOn>("springOn")
	    .Field<&components::HandGrab::springPending>("springPending")
	    .Field<&components::HandGrab::lastTarget>("lastTarget")
	    .Field<&components::HandGrab::scoopSource>("scoopSource")
	    .Field<&components::HandGrab::scoopTurns>("scoopTurns")
	    .Field<&components::HandGrab::scoopStream>("scoopStream")
	    .Field<&components::HandGrab::scoopAnchor>("scoopAnchor")
	    .Field<&components::HandGrab::released>("released")
	    .Field<&components::HandGrab::releaseSpinMs>("releaseSpinMs")
	    .Field<&components::HandGrab::pourEffect>("pourEffect")
	    .Field<&components::HandGrab::pourSeconds>("pourSeconds")
	    .Field<&components::HandGrab::scoopStreamSeconds>("scoopStreamSeconds")
	    .Field<&components::HandGrab::handSize>("handSize")
	    .Field<&components::HandGrab::handPoint>("handPoint")
	    .Field<&components::HandGrab::lastPickedUp>("lastPickedUp")
	    .Field<&components::HandGrab::lastDropped>("lastDropped");
	Reflect<components::HandMiracleFx>(context)
	    .Field<&components::HandMiracleFx::bands>("bands")
	    .Field<&components::HandMiracleFx::glowing>("glowing")
	    .Field<&components::HandMiracleFx::glowFrame>("glowFrame");
	Reflect<components::HandMorph>(context)
	    .Field<&components::HandMorph::state>("state")
	    .Field<&components::HandMorph::point>("point")
	    .Field<&components::HandMorph::pointInInfluence>("pointInInfluence")
	    .Field<&components::HandMorph::skins>("skins")
	    .Field<&components::HandMorph::revision>("revision");
	Reflect<components::HandOnCreature>(context)
	    .Field<&components::HandOnCreature::creature>("creature")
	    .Field<&components::HandOnCreature::sum>("sum")
	    .Field<&components::HandOnCreature::onBodyMs>("onBodyMs")
	    .Field<&components::HandOnCreature::sinceStrokeMs>("sinceStrokeMs")
	    .Field<&components::HandOnCreature::sinceSlapMs>("sinceSlapMs")
	    .Field<&components::HandOnCreature::lastPart>("lastPart")
	    .Field<&components::HandOnCreature::lastPoint>("lastPoint")
	    .Field<&components::HandOnCreature::lastCursor>("lastCursor")
	    .Field<&components::HandOnCreature::speed>("speed")
	    .Field<&components::HandOnCreature::slapShowMs>("slapShowMs")
	    .Field<&components::HandOnCreature::byCommand>("byCommand")
	    .Field<&components::HandOnCreature::heldMs>("heldMs")
	    .Field<&components::HandOnCreature::strokedOrSlapped>("strokedOrSlapped");
	Reflect<components::HandLastFeedback>(context).Field<&components::HandLastFeedback::sum>("sum");
	Reflect<components::HiddenByState> {context};
	Reflect<components::HighDetail>(context)
	    .Field<&components::HighDetail::usualModel>("usualModel")
	    .Field<&components::HighDetail::usualDetailModels>("usualDetailModels")
	    .Field<&components::HighDetail::face>("face")
	    .Field<&components::HighDetail::orders>("orders")
	    .Field<&components::HighDetail::heldAt>("heldAt")
	    .Field<&components::HighDetail::eyes>("eyes")
	    .Field<&components::HighDetail::drawnEyes>("drawnEyes");
	Reflect<components::Indestructible> {context};
	Reflect<components::InfluenceSource>(context)
	    .Field<&components::InfluenceSource::player>("player")
	    .Field<&components::InfluenceSource::radius>("radius")
	    .Field<&components::InfluenceSource::anti>("anti")
	    .Field<&components::InfluenceSource::follows>("follows");
	Reflect<components::TownInfluence>(context)
	    .Field<&components::TownInfluence::radius>("radius")
	    .Field<&components::TownInfluence::drawnRadius>("drawnRadius");
	Reflect<components::CitadelInfluence>(context)
	    .Field<&components::CitadelInfluence::reach>("reach")
	    .Field<&components::CitadelInfluence::drawnRadius>("drawnRadius");
	Reflect<components::VirtualInfluence>(context)
	    .Field<&components::VirtualInfluence::state>("state")
	    .Field<&components::VirtualInfluence::hum>("hum");
	Reflect<components::IntroHand>(context)
	    .Field<&components::IntroHand::clip>("clip")
	    .Field<&components::IntroHand::time>("time")
	    .Field<&components::IntroHand::yaw>("yaw")
	    .Field<&components::IntroHand::scale>("scale");
	Reflect<components::LandForest>(context)
	    .Field<&components::LandForest::id>("id")
	    .Field<&components::LandForest::bigForest>("bigForest")
	    .Field<&components::LandForest::scenic>("scenic")
	    .Field<&components::LandForest::made>("made");
	Reflect<components::ForestTurns>(context)
	    .Field<&components::ForestTurns::id>("id")
	    .Field<&components::ForestTurns::made>("made")
	    .Field<&components::ForestTurns::emptyCountdown>("emptyCountdown")
	    .Field<&components::ForestTurns::spreadCounter>("spreadCounter");
	Reflect<components::TownForests>(context).Field<&components::TownForests::forests>("forests");
	Reflect<components::LightBeam>(context).Field<&components::LightBeam::cone>("cone");
	Reflect<components::LivingAction>(context)
	    .Field<&components::LivingAction::states>("states", {k_EcsComponentsLivingActionIndexNames})
	    .Field<&components::LivingAction::turnsUntilStateChange>("turnsUntilStateChange")
	    .Field<&components::LivingAction::turnsSinceStateChange>("turnsSinceStateChange");
	Reflect<components::VillagerClip>(context)
	    .Field<&components::VillagerClip::state>("state")
	    .Field<&components::VillagerClip::clip>("clip");
	Reflect<components::LastInteractingPlayer>(context).Field<&components::LastInteractingPlayer::player>("player");
	Reflect<components::WatchedFlyingObject>(context).Field<&components::WatchedFlyingObject::object>("object");
	Reflect<components::LivingReaction>(context)
	    .Field<&components::LivingReaction::reaction>("reaction")
	    .Field<&components::LivingReaction::type>("type")
	    .Field<&components::LivingReaction::startTurn>("startTurn")
	    .Field<&components::LivingReaction::creatureMemory>("creatureMemory")
	    .Field<&components::LivingReaction::previousState>("previousState");
	Reflect<components::MagicFireBall>(context)
	    .Field<&components::MagicFireBall::strength>("strength")
	    .Field<&components::MagicFireBall::radius>("radius")
	    .Field<&components::MagicFireBall::affectedByRain>("affectedByRain")
	    .Field<&components::MagicFireBall::player>("player")
	    .Field<&components::MagicFireBall::hasPlayer>("hasPlayer")
	    .Field<&components::MagicFireBall::lastTurn>("lastTurn")
	    .Field<&components::MagicFireBall::handTarget>("handTarget");
	Reflect<components::MagicForest>(context)
	    .Field<&components::MagicForest::spell>("spell")
	    .Field<&components::MagicForest::player>("player")
	    .Field<&components::MagicForest::centre>("centre")
	    .Field<&components::MagicForest::trees>("trees");
	Reflect<components::MagicTree>(context)
	    .Field<&components::MagicTree::forest>("forest")
	    .Field<&components::MagicTree::woodMultiplier>("woodMultiplier")
	    .Field<&components::MagicTree::impressiveValue>("impressiveValue");
	Reflect<components::MagicPile>(context)
	    .Field<&components::MagicPile::resource>("resource")
	    .Field<&components::MagicPile::player>("player");
	Reflect<components::MagicShield>(context)
	    .Field<&components::MagicShield::kind>("kind")
	    .Field<&components::MagicShield::spell>("spell")
	    .Field<&components::MagicShield::player>("player")
	    .Field<&components::MagicShield::radius>("radius")
	    .Field<&components::MagicShield::town>("town")
	    .Field<&components::MagicShield::reaction>("reaction")
	    .Field<&components::MagicShield::struckReaction>("struckReaction");
	Reflect<components::ShieldDome>(context)
	    .Field<&components::ShieldDome::shape>("shape")
	    .Field<&components::ShieldDome::state>("state")
	    .Field<&components::ShieldDome::pose>("pose")
	    .Field<&components::ShieldDome::previous>("previous")
	    .Field<&components::ShieldDome::age>("age")
	    .Field<&components::ShieldDome::solidScale>("solidScale")
	    .Field<&components::ShieldDome::alpha>("alpha")
	    .Field<&components::ShieldDome::mesh>("mesh")
	    .Field<&components::ShieldDome::halfExtent>("halfExtent")
	    .Field<&components::ShieldDome::effect>("effect")
	    .Field<&components::ShieldDome::hull>("hull")
	    .Field<&components::ShieldDome::hullOrigin>("hullOrigin")
	    .Field<&components::ShieldDome::hullScale>("hullScale");
	Reflect<components::AntiInfluence>(context)
	    .Field<&components::AntiInfluence::owner>("owner")
	    .Field<&components::AntiInfluence::radius>("radius");
	Reflect<components::VillagerShieldReaction>(context)
	    .Field<&components::VillagerShieldReaction::previous>("previous")
	    .Field<&components::VillagerShieldReaction::reaction>("reaction")
	    .Field<&components::VillagerShieldReaction::type>("type")
	    .Field<&components::VillagerShieldReaction::shield>("shield")
	    .Field<&components::VillagerShieldReaction::lookAt>("lookAt")
	    .Field<&components::VillagerShieldReaction::animation>("animation");
	Reflect<components::VillagerReactionMemory>(context).Field<&components::VillagerReactionMemory::memory>("memory");
	Reflect<components::MapCellResident>(context)
	    .Field<&components::MapCellResident::placement>("placement")
	    .Field<&components::MapCellResident::coversOutline>("coversOutline")
	    .Field<&components::MapCellResident::moves>("moves")
	    .Field<&components::MapCellResident::cells>("cells")
	    .Field<&components::MapCellResident::filedAt>("filedAt");
	Reflect<components::MapCellMover> {context};
	Reflect<components::Mesh>(context)
	    .Field<&components::Mesh::id>("id")
	    .Field<&components::Mesh::submeshId>("submeshId")
	    .Field<&components::Mesh::bbSubmeshId>("bbSubmeshId");
	Reflect<components::TownImpression>(context)
	    .Field<&components::TownImpression::belief>("belief")
	    .Field<&components::TownImpression::boredom>("boredom")
	    .Field<&components::TownImpression::lastImpression>("lastImpression");
	Reflect<components::CreatureImpression>(context)
	    .Field<&components::CreatureImpression::byOwnPlayer>("byOwnPlayer")
	    .Field<&components::CreatureImpression::byOtherCreatures>("byOtherCreatures");
	Reflect<components::Mist>(context)
	    .Field<&components::Mist::size>("size")
	    .Field<&components::Mist::colour>("colour")
	    .Field<&components::Mist::shrinksEdgeOn>("shrinksEdgeOn")
	    .Field<&components::Mist::edgeShrink>("edgeShrink")
	    .Field<&components::Mist::counter>("counter")
	    .Field<&components::Mist::counterRemainder>("counterRemainder");
	Reflect<components::MistDome>(context)
	    .Field<&components::MistDome::dome>("dome")
	    .Field<&components::MistDome::uvOffset>("uvOffset")
	    .Field<&components::MistDome::colour>("colour");
	Reflect<components::Mobile>(context).Field<&components::Mobile::dummy>("dummy");
	Reflect<components::MobileStatic>(context).Field<&components::MobileStatic::type>("type");
	Reflect<components::MobileObject>(context).Field<&components::MobileObject::type>("type");
	Reflect<components::Rock> {context};
	Reflect<components::MorphWithTerrain>(context).Field<&components::MorphWithTerrain::dummy>("dummy");
	Reflect<components::ObjectGlow>(context)
	    .Field<&components::ObjectGlow::rgb>("rgb")
	    .Field<&components::ObjectGlow::turnsUnset>("turnsUnset");
	Reflect<components::OneOffSpellSeed>(context)
	    .Field<&components::OneOffSpellSeed::seedType>("seedType")
	    .Field<&components::OneOffSpellSeed::magicType>("magicType")
	    .Field<&components::OneOffSpellSeed::position>("position")
	    .Field<&components::OneOffSpellSeed::powerUp>("powerUp")
	    .Field<&components::OneOffSpellSeed::multiplier>("multiplier")
	    .Field<&components::OneOffSpellSeed::middle>("middle")
	    .Field<&components::OneOffSpellSeed::spin>("spin")
	    .Field<&components::OneOffSpellSeed::ringSpin>("ringSpin")
	    .Field<&components::OneOffSpellSeed::glintFrame>("glintFrame")
	    .Field<&components::OneOffSpellSeed::phialFrame>("phialFrame")
	    .Field<&components::OneOffSpellSeed::phialPhase>("phialPhase")
	    .Field<&components::OneOffSpellSeed::seedPlacement>("seedPlacement")
	    .Field<&components::OneOffSpellSeed::dispenser>("dispenser");
	Reflect<components::InPhysics> {context};
	Reflect<components::Immovable> {context};
	Reflect<components::BuildProgress>(context).Field<&components::BuildProgress::built>("built");
	Reflect<components::RepairSite>(context).Field<&components::RepairSite::startLife>("startLife");
	Reflect<components::PhysicsDrawPose>(context)
	    .Field<&components::PhysicsDrawPose::axes>("axes")
	    .Field<&components::PhysicsDrawPose::origin>("origin")
	    .Field<&components::PhysicsDrawPose::underSea>("underSea");
	Reflect<components::Player>(context)
	    .Field<&components::Player::name>("name")
	    .Field<&components::Player::damageFrom>("damageFrom")
	    .Field<&components::Player::lastCast>("lastCast")
	    .Field<&components::Player::castsOfType>("castsOfType")
	    .Field<&components::Player::windResistance>("windResistance")
	    .Field<&components::Player::villagersLost>("villagersLost")
	    .Field<&components::Player::villagersKilled>("villagersKilled")
	    .Field<&components::Player::sacrifices>("sacrifices")
	    .Field<&components::Player::totalChantsUsed>("totalChantsUsed")
	    .Field<&components::Player::miracles>("miracles");
	Reflect<components::PlayerCreatures>(context).Field<&components::PlayerCreatures::acquired>("acquired");
	Reflect<components::Poisoned>(context).Field<&components::Poisoned::dummy>("dummy");
	Reflect<components::CreatureMiracleOpinion>(context)
	    .Field<&components::CreatureMiracleOpinion::lastTurn>("lastTurn")
	    .Field<&components::CreatureMiracleOpinion::changed>("changed");
	Reflect<components::Pot>(context)
	    .Field<&components::Pot::amount>("amount")
	    .Field<&components::Pot::maxAmount>("maxAmount")
	    .Field<&components::Pot::type>("type")
	    .Field<&components::Pot::poisoned>("poisoned")
	    .Field<&components::Pot::town>("town");
	Reflect<components::PrayerPower>(context)
	    .Field<&components::PrayerPower::chants>("chants")
	    .Field<&components::PrayerPower::infinite>("infinite");
	Reflect<components::ResourceLastTaken>(context).Field<&components::ResourceLastTaken::turn>(
	    "turn", {k_PlayerNamesNames, std::span<const std::string_view> {}});
	Reflect<components::ResourcePile>(context)
	    .Field<&components::ResourcePile::rise>("rise")
	    .Field<&components::ResourcePile::height>("height")
	    .Field<&components::ResourcePile::shownAmount>("shownAmount")
	    .Field<&components::ResourcePile::risen>("risen")
	    .Field<&components::ResourcePile::speedUp>("speedUp")
	    .Field<&components::ResourcePile::speedUpVisual>("speedUpVisual");
	Reflect<components::Reward>(context)
	    .Field<&components::Reward::type>("type")
	    .Field<&components::Reward::player>("player")
	    .Field<&components::Reward::town>("town")
	    .Field<&components::Reward::state>("state")
	    .Field<&components::Reward::seconds>("seconds")
	    .Field<&components::Reward::landingPoint>("landingPoint")
	    .Field<&components::Reward::madeAt>("madeAt")
	    .Field<&components::Reward::goesIntoMap>("goesIntoMap")
	    .Field<&components::Reward::dustMilliseconds>("dustMilliseconds")
	    .Field<&components::Reward::dust>("dust");
	Reflect<components::RewardOnLand> {context};
	Reflect<components::ScriptAnimation>(context)
	    .Field<&components::ScriptAnimation::clip>("clip")
	    .Field<&components::ScriptAnimation::playsLeft>("playsLeft");
	Reflect<components::InScript> {context};
	Reflect<components::ScriptControlled> {context};
	Reflect<components::ScriptMarker> {context};
	Reflect<components::ScriptHighlight>(context)
	    .Field<&components::ScriptHighlight::kind>("kind")
	    .Field<&components::ScriptHighlight::scriptId>("scriptId")
	    .Field<&components::ScriptHighlight::category>("category")
	    .Field<&components::ScriptHighlight::active>("active")
	    .Field<&components::ScriptHighlight::drawHeight>("drawHeight")
	    .Field<&components::ScriptHighlight::heightAbove>("heightAbove")
	    .Field<&components::ScriptHighlight::yAngle>("yAngle")
	    .Field<&components::ScriptHighlight::glints>("glints")
	    .Field<&components::ScriptHighlight::activeEffect>("activeEffect")
	    .Field<&components::ScriptHighlight::glow>("glow")
	    .Field<&components::ScriptHighlight::centre>("centre")
	    .Field<&components::ScriptHighlight::radius>("radius")
	    .Field<&components::ScriptHighlight::sparks>("sparks")
	    .Field<&components::ScriptHighlight::sparkSprites>("sparkSprites");
	Reflect<components::ScriptHighlightGlow>(context).Field<&components::ScriptHighlightGlow::highlight>("highlight");
	Reflect<components::ScriptSpotVisual>(context).Field<&components::ScriptSpotVisual::effect>("effect");
	Reflect<components::ScriptTimer>(context).Field<&components::ScriptTimer::timer>("timer");
	Reflect<components::SeeThrough>(context).Field<&components::SeeThrough::alpha>("alpha");
	Reflect<components::Shark>(context)
	    .Field<&components::Shark::position>("position")
	    .Field<&components::Shark::turnStart>("turnStart")
	    .Field<&components::Shark::heading>("heading")
	    .Field<&components::Shark::clipPlace>("clipPlace");
	Reflect<components::SkinOverride>(context)
	    .Field<&components::SkinOverride::texture>("texture")
	    .Field<&components::SkinOverride::uvOffset>("uvOffset");
	Reflect<components::SkyDome>(context)
	    .Field<&components::SkyDome::meshId>("meshId")
	    .Field<&components::SkyDome::textureId>("textureId")
	    .Field<&components::SkyDome::follow>("follow")
	    .Field<&components::SkyDome::frameRows>("frameRows")
	    .Field<&components::SkyDome::overcast>("overcast")
	    .Field<&components::SkyDome::landLight>("landLight");
	Reflect<components::DayNightCycle>(context).Field<&components::DayNightCycle::clock>("clock");
	Reflect<components::CelestialBody>(context)
	    .Field<&components::CelestialBody::meshId>("meshId")
	    .Field<&components::CelestialBody::textureId>("textureId")
	    .Field<&components::CelestialBody::alphaTextureId>("alphaTextureId");
	Reflect<components::CelestialGlow>(context)
	    .Field<&components::CelestialGlow::textureId>("textureId")
	    .Field<&components::CelestialGlow::alphaTextureId>("alphaTextureId");
	Reflect<components::Sun>(context)
	    .Field<&components::Sun::placement>("placement")
	    .Field<&components::Sun::colour>("colour")
	    .Field<&components::Sun::strength>("strength");
	Reflect<components::Moon>(context)
	    .Field<&components::Moon::phase>("phase")
	    .Field<&components::Moon::date>("date")
	    .Field<&components::Moon::dateReadAt>("dateReadAt")
	    .Field<&components::Moon::dateOverride>("dateOverride")
	    .Field<&components::Moon::placement>("placement")
	    .Field<&components::Moon::colour>("colour")
	    .Field<&components::Moon::strength>("strength");
	Reflect<components::SoundTag>(context)
	    .Field<&components::SoundTag::sound>("sound")
	    .Field<&components::SoundTag::offset>("offset")
	    .Field<&components::SoundTag::active>("active")
	    .Field<&components::SoundTag::emitter>("emitter")
	    .Field<&components::SoundTag::point>("point")
	    .Field<&components::SoundTag::delayed>("delayed")
	    .Field<&components::SoundTag::turns>("turns");
	Reflect<components::SpellCaster>(context)
	    .Field<&components::SpellCaster::kind>("kind")
	    .Field<&components::SpellCaster::player>("player")
	    .Field<&components::SpellCaster::entity>("entity")
	    .Field<&components::SpellCaster::withoutIcon>("withoutIcon")
	    .Field<&components::SpellCaster::worshipSite>("worshipSite");
	Reflect<components::Spell>(context)
	    .Field<&components::Spell::magicType>("magicType")
	    .Field<&components::Spell::spellClass>("spellClass")
	    .Field<&components::Spell::caster>("caster")
	    .Field<&components::Spell::chants>("chants")
	    .Field<&components::Spell::age>("age")
	    .Field<&components::Spell::duration>("duration")
	    .Field<&components::Spell::magnitude>("magnitude")
	    .Field<&components::Spell::position>("position")
	    .Field<&components::Spell::castPosition>("castPosition")
	    .Field<&components::Spell::originalCastPosition>("originalCastPosition")
	    .Field<&components::Spell::direction>("direction")
	    .Field<&components::Spell::movement>("movement")
	    .Field<&components::Spell::processInfo>("processInfo")
	    .Field<&components::Spell::effect>("effect")
	    .Field<&components::Spell::castEffect>("castEffect")
	    .Field<&components::Spell::seed>("seed")
	    .Field<&components::Spell::target>("target")
	    .Field<&components::Spell::closedDown>("closedDown")
	    .Field<&components::Spell::hasParticleType>("hasParticleType")
	    .Field<&components::Spell::fromLocalHand>("fromLocalHand")
	    .Field<&components::Spell::castFromHand>("castFromHand")
	    .Field<&components::Spell::reaction>("reaction")
	    .Field<&components::Spell::humanCasting>("humanCasting")
	    .Field<&components::Spell::maxObjectsToCreate>("maxObjectsToCreate")
	    .Field<&components::Spell::resourceFirstDone>("resourceFirstDone")
	    .Field<&components::Spell::lastRipple>("lastRipple")
	    .Field<&components::Spell::forestPlanted>("forestPlanted")
	    .Field<&components::Spell::objectCount>("objectCount");
	Reflect<components::SpellDispenser>(context)
	    .Field<&components::SpellDispenser::magicType>("magicType")
	    .Field<&components::SpellDispenser::timer>("timer")
	    .Field<&components::SpellDispenser::orb>("orb")
	    .Field<&components::SpellDispenser::orbPosition>("orbPosition")
	    .Field<&components::SpellDispenser::effect>("effect")
	    .Field<&components::SpellDispenser::building>("building");
	Reflect<components::TestbedDispenser> {context};
	Reflect<components::SpellSeed>(context)
	    .Field<&components::SpellSeed::seedType>("seedType")
	    .Field<&components::SpellSeed::powerUp>("powerUp")
	    .Field<&components::SpellSeed::player>("player")
	    .Field<&components::SpellSeed::chantStore>("chantStore")
	    .Field<&components::SpellSeed::learnedFrom>("learnedFrom")
	    .Field<&components::SpellSeed::storedChants>("storedChants")
	    .Field<&components::SpellSeed::storedAge>("storedAge")
	    .Field<&components::SpellSeed::storedMaxObjects>("storedMaxObjects")
	    .Field<&components::SpellSeed::castMultiplier>("castMultiplier")
	    .Field<&components::SpellSeed::power>("power")
	    .Field<&components::SpellSeed::origin>("origin")
	    .Field<&components::SpellSeed::hasIcon>("hasIcon")
	    .Field<&components::SpellSeed::worshipSite>("worshipSite")
	    .Field<&components::SpellSeed::ready>("ready")
	    .Field<&components::SpellSeed::holdType>("holdType")
	    .Field<&components::SpellSeed::followsSpell>("followsSpell")
	    .Field<&components::SpellSeed::turnsInHand>("turnsInHand")
	    .Field<&components::SpellSeed::spell>("spell")
	    .Field<&components::SpellSeed::hasCast>("hasCast")
	    .Field<&components::SpellSeed::handEffect>("handEffect");
	Reflect<components::Sprite>(context)
	    .Field<&components::Sprite::texture>("texture")
	    .Field<&components::Sprite::uvMin>("uvMin")
	    .Field<&components::Sprite::uvExtent>("uvExtent")
	    .Field<&components::Sprite::tint>("tint")
	    .Field<&components::Sprite::additive>("additive")
	    .Field<&components::Sprite::facesCamera>("facesCamera")
	    .Field<&components::Sprite::alpha>("alpha");
	Reflect<components::StoragePit>(context)
	    .Field<&components::StoragePit::woodPiles>("woodPiles")
	    .Field<&components::StoragePit::foodPile>("foodPile");
	Reflect<components::Stream>(context).Field<&components::Stream::id>("id").Field<&components::Stream::points>("points");
	Reflect<components::StreamSegment> {context};
	Reflect<components::StreetLantern>(context).Field<&components::StreetLantern::country>("country");
	Reflect<components::Swayable>(context).Field<&components::Swayable::swaySlot>("swaySlot");
	Reflect<components::TeleportStone>(context)
	    .Field<&components::TeleportStone::player>("player")
	    .Field<&components::TeleportStone::spell>("spell")
	    .Field<&components::TeleportStone::reaction>("reaction")
	    .Field<&components::TeleportStone::pool>("pool")
	    .Field<&components::TeleportStone::serial>("serial")
	    .Field<&components::TeleportStone::travellers>("travellers");
	Reflect<components::TeleportTraveller>(context)
	    .Field<&components::TeleportTraveller::stone>("stone")
	    .Field<&components::TeleportTraveller::destination>("destination")
	    .Field<&components::TeleportTraveller::finalState>("finalState")
	    .Field<&components::TeleportTraveller::previousState>("previousState")
	    .Field<&components::TeleportTraveller::jumped>("jumped")
	    .Field<&components::TeleportTraveller::arrival>("arrival")
	    .Field<&components::TeleportTraveller::transportTurns>("transportTurns")
	    .Field<&components::TeleportTraveller::transported>("transported")
	    .Field<&components::TeleportTraveller::fadingIn>("fadingIn");
	Reflect<components::TempleInteriorPart>(context)
	    .Field<&components::TempleInteriorPart::room>("room")
	    .Field<&components::TempleInteriorPart::mesh>("mesh");
	Reflect<components::Temple>(context)
	    .Field<&components::Temple::owner>("owner")
	    .Field<&components::Temple::yAngle>("yAngle")
	    .Field<&components::Temple::lastHitTurn>("lastHitTurn")
	    .Field<&components::Temple::beamTarget>("beamTarget")
	    .Field<&components::Temple::beamTurn>("beamTurn")
	    .Field<&components::Temple::beamSource>("beamSource")
	    .Field<&components::Temple::destroying>("destroying")
	    .Field<&components::Temple::destructionClock>("destructionClock")
	    .Field<&components::Temple::destructionGlow>("destructionGlow")
	    .Field<&components::Temple::destructionLoops>("destructionLoops")
	    .Field<&components::Temple::town>("town")
	    .Field<&components::Temple::destructionBeamClock>("destructionBeamClock")
	    .Field<&components::Temple::destructionBeamSource>("destructionBeamSource");
	Reflect<components::TempleEntrance>(context).Field<&components::TempleEntrance::temple>("temple");
	Reflect<components::TempleExterior>(context)
	    .Field<&components::TempleExterior::alignment>("alignment")
	    .Field<&components::TempleExterior::alignmentTarget>("alignmentTarget")
	    .Field<&components::TempleExterior::size>("size")
	    .Field<&components::TempleExterior::sizeTarget>("sizeTarget")
	    .Field<&components::TempleExterior::morphed>("morphed")
	    .Field<&components::TempleExterior::drawnBuilt>("drawnBuilt");
	Reflect<components::Town>(context)
	    .Field<&components::Town::id>("id")
	    .Field<&components::Town::owner>("owner")
	    .Field<&components::Town::beliefs>("beliefs")
	    .Field<&components::Town::uninhabitable>("uninhabitable")
	    .Field<&components::Town::homelessVillagers>("homelessVillagers")
	    .Field<&components::Town::abodes>("abodes")
	    .Field<&components::Town::gained>("gained")
	    .Field<&components::Town::injured>("injured")
	    .Field<&components::Town::deathsByKiller>("deathsByKiller", {std::span<const std::string_view> {}, k_DeathReasonNames})
	    .Field<&components::Town::scenicForest>("scenicForest")
	    .Field<&components::Town::scenicForestCentre>("scenicForestCentre")
	    .Field<&components::Town::playthings>("playthings")
	    .Field<&components::Town::worshipShare>("worshipShare")
	    .Field<&components::Town::worshipSite>("worshipSite")
	    .Field<&components::Town::cannotHaveWorshipSite>("cannotHaveWorshipSite")
	    .Field<&components::Town::congregationPos>("congregationPos")
	    .Field<&components::Town::emergencyTurn>("emergencyTurn");
	Reflect<components::TownAggression>(context).Field<&components::TownAggression::record>("record");
	Reflect<components::TownArtefact>(context)
	    .Field<&components::TownArtefact::town>("town")
	    .Field<&components::TownArtefact::player>("player")
	    .Field<&components::TownArtefact::value>("value");
	Reflect<components::DesireSort>(context)
	    .Field<&components::DesireSort::boosts>("boosts")
	    .Field<&components::DesireSort::value>("value")
	    .Field<&components::DesireSort::index>("index");
	Reflect<components::TownResourceTally>(context)
	    .Field<&components::TownResourceTally::foodCarried>("foodCarried")
	    .Field<&components::TownResourceTally::woodCarried>("woodCarried")
	    .Field<&components::TownResourceTally::woodAtSites>("woodAtSites")
	    .Field<&components::TownResourceTally::woodUsed>("woodUsed");
	Reflect<components::TownStats>(context)
	    .Field<&components::TownStats::adults>("adults")
	    .Field<&components::TownStats::children>("children")
	    .Field<&components::TownStats::abodesWithPlaces>("abodesWithPlaces")
	    .Field<&components::TownStats::adultPlaces>("adultPlaces")
	    .Field<&components::TownStats::childPlaces>("childPlaces")
	    .Field<&components::TownStats::totalPlaces>("totalPlaces")
	    .Field<&components::TownStats::civicBuildings>("civicBuildings")
	    .Field<&components::TownStats::disciples>("disciples")
	    .Field<&components::TownStats::foodForDinner>("foodForDinner")
	    .Field<&components::TownStats::foodCarried>("foodCarried")
	    .Field<&components::TownStats::woodCarried>("woodCarried")
	    .Field<&components::TownStats::woodAtSites>("woodAtSites")
	    .Field<&components::TownStats::abodesByNumber>("abodesByNumber");
	Reflect<components::TownDesire>(context)
	    .Field<&components::TownDesire::boostA>("boostA")
	    .Field<&components::TownDesire::boost>("boost")
	    .Field<&components::TownDesire::desire>("desire")
	    .Field<&components::TownDesire::population>("population")
	    .Field<&components::TownDesire::raw>("raw")
	    .Field<&components::TownDesire::sorted>("sorted")
	    .Field<&components::TownDesire::sortedRaw>("sortedRaw")
	    .Field<&components::TownDesire::doingNow>("doingNow")
	    .Field<&components::TownDesire::doingNowCount>("doingNowCount")
	    .Field<&components::TownDesire::doingNowAtStart>("doingNowAtStart")
	    .Field<&components::TownDesire::doingNowCountAtStart>("doingNowCountAtStart");
	Reflect<components::Transform>(context)
	    .Field<&components::Transform::position>("position")
	    .Field<&components::Transform::rotation>("rotation")
	    .Field<&components::Transform::scale>("scale");
	Reflect<components::Translucent>(context).Field<&components::Translucent::share>("share");
	Reflect<components::Tree>(context)
	    .Field<&components::Tree::type>("type")
	    .Field<&components::Tree::maxSize>("maxSize")
	    .Field<&components::Tree::growthCountdown>("growthCountdown")
	    .Field<&components::Tree::madeToGrow>("madeToGrow");
	Reflect<components::Unlit> {context};
	Reflect<components::Velocity>(context)
	    .Field<&components::Velocity::dX>("dX")
	    .Field<&components::Velocity::dY>("dY")
	    .Field<&components::Velocity::dZ>("dZ");
	Reflect<components::VillageLight>(context)
	    .Field<&components::VillageLight::kind>("kind")
	    .Field<&components::VillageLight::flickerTimer>("flickerTimer")
	    .Field<&components::VillageLight::flicker>("flicker")
	    .Field<&components::VillageLight::glowSize>("glowSize");
	Reflect<components::VillageLightSprite>(context)
	    .Field<&components::VillageLightSprite::light>("light")
	    .Field<&components::VillageLightSprite::index>("index");
	Reflect<components::VillageTotem>(context)
	    .Field<&components::VillageTotem::townCentre>("townCentre")
	    .Field<&components::VillageTotem::icon>("icon")
	    .Field<&components::VillageTotem::restY>("restY")
	    .Field<&components::VillageTotem::ease>("ease")
	    .Field<&components::VillageTotem::held>("held")
	    .Field<&components::VillageTotem::gripped>("gripped")
	    .Field<&components::VillageTotem::ghost>("ghost")
	    .Field<&components::VillageTotem::ghostIcon>("ghostIcon")
	    .Field<&components::VillageTotem::lastShownRise>("lastShownRise")
	    .Field<&components::VillageTotem::quietOnce>("quietOnce");
	Reflect<components::Villager>(context)
	    .Field<&components::Villager::life>("life")
	    .Field<&components::Villager::birthTurn>("birthTurn")
	    .Field<&components::Villager::food>("food")
	    .Field<&components::Villager::lifeStage>("lifeStage")
	    .Field<&components::Villager::sex>("sex")
	    .Field<&components::Villager::tribe>("tribe")
	    .Field<&components::Villager::number>("number")
	    .Field<&components::Villager::task>("task")
	    .Field<&components::Villager::town>("town")
	    .Field<&components::Villager::abode>("abode")
	    .Field<&components::Villager::lastCheckTurn>("lastCheckTurn")
	    .Field<&components::Villager::carried>("carried")
	    .Field<&components::Villager::transitionPlaying>("transitionPlaying")
	    .Field<&components::Villager::intoClipDue>("intoClipDue")
	    .Field<&components::Villager::woken>("woken")
	    .Field<&components::Villager::foodHeld>("foodHeld")
	    .Field<&components::Villager::woodHeld>("woodHeld")
	    .Field<&components::Villager::woodGraphic>("woodGraphic")
	    .Field<&components::Villager::buildingSite>("buildingSite")
	    .Field<&components::Villager::buildPlace>("buildPlace");
	Reflect<components::VillagerDeath>(context)
	    .Field<&components::VillagerDeath::turnsLeft>("turnsLeft")
	    .Field<&components::VillagerDeath::skeleton>("skeleton")
	    .Field<&components::VillagerDeath::reason>("reason")
	    .Field<&components::VillagerDeath::killer>("killer");
	Reflect<components::VillagerPose>(context)
	    .Field<&components::VillagerPose::clip>("clip")
	    .Field<&components::VillagerPose::place>("place")
	    .Field<&components::VillagerPose::bones>("bones")
	    .Field<&components::VillagerPose::turnStart>("turnStart")
	    .Field<&components::VillagerPose::drawnAt>("drawnAt")
	    .Field<&components::VillagerPose::drawnHeading>("drawnHeading")
	    .Field<&components::VillagerPose::easedHeading>("easedHeading")
	    .Field<&components::VillagerPose::detailedHeading>("detailedHeading")
	    .Field<&components::VillagerPose::clipBlend>("clipBlend");
	Reflect<components::Vortex>(context)
	    .Field<&components::Vortex::type>("type")
	    .Field<&components::Vortex::state>("state")
	    .Field<&components::Vortex::stateStartTurn>("stateStartTurn")
	    .Field<&components::Vortex::centre>("centre")
	    .Field<&components::Vortex::levelApplied>("levelApplied")
	    .Field<&components::Vortex::groundHeights>("groundHeights")
	    .Field<&components::Vortex::groundAverage>("groundAverage")
	    .Field<&components::Vortex::beforeLandEffect>("beforeLandEffect")
	    .Field<&components::Vortex::afterLandEffect>("afterLandEffect")
	    .Field<&components::Vortex::objectMoverEffect>("objectMoverEffect")
	    .Field<&components::Vortex::lightMapEffect>("lightMapEffect");
	Reflect<components::WalkPath>(context)
	    .Field<&components::WalkPath::number>("number")
	    .Field<&components::WalkPath::track>("track")
	    .Field<&components::WalkPath::walk>("walk")
	    .Field<&components::WalkPath::living>("living")
	    .Field<&components::WalkPath::speed>("speed");
	Reflect<components::WallHugObjectReference>(context)
	    .Field<&components::WallHugObjectReference::stepsAway>("stepsAway")
	    .Field<&components::WallHugObjectReference::entity>("entity")
	    .Field<&components::WallHugObjectReference::centre>("centre")
	    .Field<&components::WallHugObjectReference::radius>("radius")
	    .Field<&components::WallHugObjectReference::entryDistance>("entryDistance");
	Reflect<components::WallHug>(context)
	    .Field<&components::WallHug::goal>("goal")
	    .Field<&components::WallHug::step>("step")
	    .Field<&components::WallHug::yAngle>("yAngle")
	    .Field<&components::WallHug::speed>("speed")
	    .Field<&components::WallHug::gameAngle>("gameAngle")
	    .Field<&components::WallHug::turnsUntilStepRebuild>("turnsUntilStepRebuild")
	    .Field<&components::WallHug::position>("position")
	    .Field<&components::WallHug::placedAt>("placedAt");
	Reflect<components::WeatherInfo>(context)
	    .Field<&components::WeatherInfo::temperature>("temperature")
	    .Field<&components::WeatherInfo::rain>("rain")
	    .Field<&components::WeatherInfo::snow>("snow")
	    .Field<&components::WeatherInfo::overcast>("overcast")
	    .Field<&components::WeatherInfo::windX>("windX")
	    .Field<&components::WeatherInfo::windZ>("windZ")
	    .Field<&components::WeatherInfo::snowCover>("snowCover")
	    .Field<&components::WeatherInfo::stamp>("stamp");
	Reflect<components::Climate>(context)
	    .Field<&components::Climate::index>("index")
	    .Field<&components::Climate::global>("global")
	    .Field<&components::Climate::info>("info")
	    .Field<&components::Climate::cellX>("cellX")
	    .Field<&components::Climate::cellZ>("cellZ")
	    .Field<&components::Climate::height>("height")
	    .Field<&components::Climate::innerRadius>("innerRadius")
	    .Field<&components::Climate::outerRadius>("outerRadius")
	    .Field<&components::Climate::rainDesire>("rainDesire")
	    .Field<&components::Climate::dryDays>("dryDays")
	    .Field<&components::Climate::rainingDays>("rainingDays")
	    .Field<&components::Climate::raining>("raining")
	    .Field<&components::Climate::temperature>("temperature")
	    .Field<&components::Climate::targetTemperature>("targetTemperature")
	    .Field<&components::Climate::windX>("windX")
	    .Field<&components::Climate::windZ>("windZ")
	    .Field<&components::Climate::windAngle>("windAngle")
	    .Field<&components::Climate::maxStorms>("maxStorms")
	    .Field<&components::Climate::stormCloudHeight>("stormCloudHeight")
	    .Field<&components::Climate::stormSpeed>("stormSpeed")
	    .Field<&components::Climate::stormLightning>("stormLightning")
	    .Field<&components::Climate::stormOvercast>("stormOvercast")
	    .Field<&components::Climate::storms>("storms");
	Reflect<components::Storm>(context)
	    .Field<&components::Storm::position>("position")
	    .Field<&components::Storm::destination>("destination")
	    .Field<&components::Storm::speed>("speed")
	    .Field<&components::Storm::arrived>("arrived")
	    .Field<&components::Storm::innerRadius>("innerRadius")
	    .Field<&components::Storm::outerRadius>("outerRadius")
	    .Field<&components::Storm::fadeTime>("fadeTime")
	    .Field<&components::Storm::lastsFor>("lastsFor")
	    .Field<&components::Storm::strength>("strength")
	    .Field<&components::Storm::cloudHeight>("cloudHeight")
	    .Field<&components::Storm::rainSpeed>("rainSpeed")
	    .Field<&components::Storm::age>("age")
	    .Field<&components::Storm::currentPosition>("currentPosition")
	    .Field<&components::Storm::currentInnerRadius>("currentInnerRadius")
	    .Field<&components::Storm::currentStrength>("currentStrength")
	    .Field<&components::Storm::effect>("effect")
	    .Field<&components::Storm::thunderWait>("thunderWait")
	    .Field<&components::Storm::boltWait>("boltWait")
	    .Field<&components::Storm::thunderTimer>("thunderTimer")
	    .Field<&components::Storm::boltTimer>("boltTimer")
	    .Field<&components::Storm::flash>("flash")
	    .Field<&components::Storm::strikesNearCamera>("strikesNearCamera")
	    .Field<&components::Storm::serial>("serial")
	    .Field<&components::Storm::dead>("dead")
	    .Field<&components::Storm::deadTurns>("deadTurns")
	    .Field<&components::Storm::climate>("climate");
	Reflect<components::WeatherThing>(context).Field<&components::WeatherThing::storm>("storm");
	Reflect<components::WorshipChants>(context)
	    .Field<&components::WorshipChants::battery>("battery")
	    .Field<&components::WorshipChants::available>("available")
	    .Field<&components::WorshipChants::used>("used")
	    .Field<&components::WorshipChants::requested>("requested")
	    .Field<&components::WorshipChants::chantsPerDancer>("chantsPerDancer")
	    .Field<&components::WorshipChants::danceIntensity>("danceIntensity")
	    .Field<&components::WorshipChants::strain>("strain")
	    .Field<&components::WorshipChants::infinite>("infinite")
	    .Field<&components::WorshipChants::freeMaintenance>("freeMaintenance");
	Reflect<components::CitadelWorship>(context)
	    .Field<&components::CitadelWorship::sites>("sites")
	    .Field<&components::CitadelWorship::facing>("facing")
	    .Field<&components::CitadelWorship::cannotMakeSites>("cannotMakeSites")
	    .Field<&components::CitadelWorship::standing>("standing");
	Reflect<components::WorshipSite>(context)
	    .Field<&components::WorshipSite::temple>("temple")
	    .Field<&components::WorshipSite::player>("player")
	    .Field<&components::WorshipSite::tribe>("tribe")
	    .Field<&components::WorshipSite::place>("place")
	    .Field<&components::WorshipSite::facing>("facing")
	    .Field<&components::WorshipSite::towns>("towns")
	    .Field<&components::WorshipSite::altar>("altar")
	    .Field<&components::WorshipSite::dance>("dance")
	    .Field<&components::WorshipSite::foodPot>("foodPot")
	    .Field<&components::WorshipSite::buildRequests>("buildRequests");
	Reflect<components::WorshipAltar>(context).Field<&components::WorshipAltar::site>("site");

	// The values the components' fields hold
	Reflect<openblack::LandLightInputs>(context, ValueOnly {})
	    .Field<&openblack::LandLightInputs::skyType>("skyType")
	    .Field<&openblack::LandLightInputs::alignment>("alignment")
	    .Field<&openblack::LandLightInputs::overcast>("overcast")
	    .Field<&openblack::LandLightInputs::flash>("flash");
	Reflect<openblack::LightCone>(context, ValueOnly {})
	    .Field<&openblack::LightCone::transform>("transform")
	    .Field<&openblack::LightCone::colour>("colour")
	    .Field<&openblack::LightCone::nearRadius>("nearRadius")
	    .Field<&openblack::LightCone::length>("length")
	    .Field<&openblack::LightCone::angle>("angle");
	Reflect<openblack::Zoomer3>(context, ValueOnly {})
	    .Field<&openblack::Zoomer3::x>("x")
	    .Field<&openblack::Zoomer3::y>("y")
	    .Field<&openblack::Zoomer3::z>("z");
	Reflect<openblack::animals::Move>(context, ValueOnly {})
	    .Field<&openblack::animals::Move::position>("position")
	    .Field<&openblack::animals::Move::goal>("goal")
	    .Field<&openblack::animals::Move::step>("step")
	    .Field<&openblack::animals::Move::angle>("angle")
	    .Field<&openblack::animals::Move::speed>("speed")
	    .Field<&openblack::animals::Move::stage>("stage");
	Reflect<openblack::audio::AnimEffectKeys>(context, ValueOnly {})
	    .Field<&openblack::audio::AnimEffectKeys::size>("size")
	    .Field<&openblack::audio::AnimEffectKeys::alignment>("alignment")
	    .Field<&openblack::audio::AnimEffectKeys::object>("object")
	    .Field<&openblack::audio::AnimEffectKeys::surface>("surface")
	    .Field<&openblack::audio::AnimEffectKeys::action>("action");
	Reflect<openblack::camera_track::Walk>(context, ValueOnly {})
	    .Field<&openblack::camera_track::Walk::runner>("runner")
	    .Field<&openblack::camera_track::Walk::to>("to")
	    .Field<&openblack::camera_track::Walk::forward>("forward")
	    .Field<&openblack::camera_track::Walk::current>("current")
	    .Field<&openblack::camera_track::Walk::step>("step");
	Reflect<openblack::creature_audio::Layers>(context, ValueOnly {})
	    .Field<&openblack::creature_audio::Layers::body>("body")
	    .Field<&openblack::creature_audio::Layers::gesture>("gesture")
	    .Field<&openblack::creature_audio::Layers::face>("face")
	    .Field<&openblack::creature_audio::Layers::slots>("slots")
	    .Field<&openblack::creature_audio::Layers::soundingSlot>("soundingSlot")
	    .Field<&openblack::creature_audio::Layers::faceLooped>("faceLooped");
	Reflect<openblack::creature_audio::Played>(context, ValueOnly {})
	    .Field<&openblack::creature_audio::Played::animation>("animation")
	    .Field<&openblack::creature_audio::Played::timeMs>("timeMs");
	Reflect<openblack::creature_desires::DesireState>(context, ValueOnly {})
	    .Field<&openblack::creature_desires::DesireState::activated>("activated")
	    .Field<&openblack::creature_desires::DesireState::value>("value")
	    .Field<&openblack::creature_desires::DesireState::max>("max")
	    .Field<&openblack::creature_desires::DesireState::decay>("decay")
	    .Field<&openblack::creature_desires::DesireState::increaseSeconds>("increaseSeconds")
	    .Field<&openblack::creature_desires::DesireState::suppressedTurns>("suppressedTurns")
	    .Field<&openblack::creature_desires::DesireState::weight>("weight")
	    .Field<&openblack::creature_desires::DesireState::carriedOut>("carriedOut")
	    .Field<&openblack::creature_desires::DesireState::sources>("sources");
	Reflect<openblack::creature_desires::Desires>(context, ValueOnly {})
	    .Field<&openblack::creature_desires::Desires::desires>("desires", {k_CreatureDesiresDesireNames})
	    .Field<&openblack::creature_desires::Desires::sum>("sum");
	Reflect<openblack::creature_desires::Source>(context, ValueOnly {})
	    .Field<&openblack::creature_desires::Source::type>("type")
	    .Field<&openblack::creature_desires::Source::value>("value")
	    .Field<&openblack::creature_desires::Source::threshold>("threshold")
	    .Field<&openblack::creature_desires::Source::multiplier>("multiplier")
	    .Field<&openblack::creature_desires::Source::drive>("drive")
	    .Field<&openblack::creature_desires::Source::clearedWhenSatisfied>("clearedWhenSatisfied");
	Reflect<openblack::creature_eyes::Blink>(context, ValueOnly {})
	    .Field<&openblack::creature_eyes::Blink::state>("state")
	    .Field<&openblack::creature_eyes::Blink::timerMs>("timerMs")
	    .Field<&openblack::creature_eyes::Blink::intervalMs>("intervalMs");
	Reflect<openblack::creature_fight::Arena>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Arena::centre>("centre")
	    .Field<&openblack::creature_fight::Arena::radius>("radius");
	Reflect<openblack::creature_fight::Fighter>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Fighter::state>("state")
	    .Field<&openblack::creature_fight::Fighter::animation>("animation")
	    .Field<&openblack::creature_fight::Fighter::timeMs>("timeMs")
	    .Field<&openblack::creature_fight::Fighter::speed>("speed")
	    .Field<&openblack::creature_fight::Fighter::mirrored>("mirrored")
	    .Field<&openblack::creature_fight::Fighter::special>("special")
	    .Field<&openblack::creature_fight::Fighter::landed>("landed")
	    .Field<&openblack::creature_fight::Fighter::spell>("spell")
	    .Field<&openblack::creature_fight::Fighter::queue>("queue")
	    .Field<&openblack::creature_fight::Fighter::health>("health")
	    .Field<&openblack::creature_fight::Fighter::stamina>("stamina")
	    .Field<&openblack::creature_fight::Fighter::control>("control")
	    .Field<&openblack::creature_fight::Fighter::computerWaitMs>("computerWaitMs")
	    .Field<&openblack::creature_fight::Fighter::tendency>("tendency");
	Reflect<openblack::creature_fight::Move>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Move::kind>("kind")
	    .Field<&openblack::creature_fight::Move::value>("value");
	Reflect<openblack::creature_fight::QueuedMove>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::QueuedMove::move>("move")
	    .Field<&openblack::creature_fight::QueuedMove::chargeMs>("chargeMs");
	Reflect<openblack::creature_fight::Reach>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Reach::animation>("animation")
	    .Field<&openblack::creature_fight::Reach::reach>("reach")
	    .Field<&openblack::creature_fight::Reach::height>("height");
	Reflect<openblack::creature_fizz::Fizz>(context, ValueOnly {})
	    .Field<&openblack::creature_fizz::Fizz::now>("now")
	    .Field<&openblack::creature_fizz::Fizz::target>("target")
	    .Field<&openblack::creature_fizz::Fizz::perSecond>("perSecond")
	    .Field<&openblack::creature_fizz::Fizz::goesForGood>("goesForGood");
	Reflect<openblack::creature_hair::Strand>(context, ValueOnly {})
	    .Field<&openblack::creature_hair::Strand::positions>("positions")
	    .Field<&openblack::creature_hair::Strand::velocities>("velocities");
	Reflect<openblack::creature_layers::BodyAction>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::BodyAction::kind>("kind")
	    .Field<&openblack::creature_layers::BodyAction::phase>("phase")
	    .Field<&openblack::creature_layers::BodyAction::animations>("animations")
	    .Field<&openblack::creature_layers::BodyAction::timeMs>("timeMs")
	    .Field<&openblack::creature_layers::BodyAction::mirrored>("mirrored")
	    .Field<&openblack::creature_layers::BodyAction::endWanted>("endWanted")
	    .Field<&openblack::creature_layers::BodyAction::holdLoop>("holdLoop")
	    .Field<&openblack::creature_layers::BodyAction::timedByPlayer>("timedByPlayer");
	Reflect<openblack::creature_layers::FaceLayer>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::FaceLayer::current>("current")
	    .Field<&openblack::creature_layers::FaceLayer::timeMs>("timeMs")
	    .Field<&openblack::creature_layers::FaceLayer::wanted>("wanted")
	    .Field<&openblack::creature_layers::FaceLayer::remainingMs>("remainingMs")
	    .Field<&openblack::creature_layers::FaceLayer::cue>("cue");
	Reflect<openblack::creature_layers::GestureLayer>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::GestureLayer::animation>("animation")
	    .Field<&openblack::creature_layers::GestureLayer::timeMs>("timeMs");
	Reflect<openblack::creature_layers::LookAxis>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::LookAxis::angle>("angle")
	    .Field<&openblack::creature_layers::LookAxis::velocity>("velocity");
	Reflect<openblack::creature_learning::Context>(context, ValueOnly {})
	    .Field<&openblack::creature_learning::Context::action>("action")
	    .Field<&openblack::creature_learning::Context::desire>("desire")
	    .Field<&openblack::creature_learning::Context::object>("object")
	    .Field<&openblack::creature_learning::Context::belief>("belief")
	    .Field<&openblack::creature_learning::Context::used>("used")
	    .Field<&openblack::creature_learning::Context::usedBelief>("usedBelief")
	    .Field<&openblack::creature_learning::Context::running>("running")
	    .Field<&openblack::creature_learning::Context::secondsSince>("secondsSince")
	    .Field<&openblack::creature_learning::Context::learnable>("learnable")
	    .Field<&openblack::creature_learning::Context::windowSeconds>("windowSeconds")
	    .Field<&openblack::creature_learning::Context::credited>("credited");
	Reflect<openblack::creature_learning::CreatureAttitude>(context, ValueOnly {})
	    .Field<&openblack::creature_learning::CreatureAttitude::creature>("creature")
	    .Field<&openblack::creature_learning::CreatureAttitude::howNice>("howNice")
	    .Field<&openblack::creature_learning::CreatureAttitude::howImpressive>("howImpressive")
	    .Field<&openblack::creature_learning::CreatureAttitude::attention>("attention");
	Reflect<openblack::creature_leash::Lesson>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::Lesson::desire>("desire")
	    .Field<&openblack::creature_leash::Lesson::change>("change");
	Reflect<openblack::creature_leash::MindHooks>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::MindHooks::forcedDesire>("forcedDesire")
	    .Field<&openblack::creature_leash::MindHooks::forcedValue>("forcedValue")
	    .Field<&openblack::creature_leash::MindHooks::obeying>("obeying")
	    .Field<&openblack::creature_leash::MindHooks::learningInHand>("learningInHand")
	    .Field<&openblack::creature_leash::MindHooks::miracleSightingWeight>("miracleSightingWeight")
	    .Field<&openblack::creature_leash::MindHooks::shown>("shown")
	    .Field<&openblack::creature_leash::MindHooks::actOn>("actOn")
	    .Field<&openblack::creature_leash::MindHooks::attitudes>("attitudes");
	Reflect<openblack::creature_leash::MindHooks::Attitude>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::MindHooks::Attitude::creature>("creature")
	    .Field<&openblack::creature_leash::MindHooks::Attitude::change>("change");
	Reflect<openblack::creature_leash::MindHooks::Shown>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::MindHooks::Shown::object>("object")
	    .Field<&openblack::creature_leash::MindHooks::Shown::type>("type")
	    .Field<&openblack::creature_leash::MindHooks::Shown::lessons>("lessons");
	Reflect<openblack::creature_leash::PullMemory>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::PullMemory::counts>("counts", {k_CreatureDesiresDesireNames});
	Reflect<openblack::creature_locomotion::Pair>(context, ValueOnly {})
	    .Field<&openblack::creature_locomotion::Pair::from>("from")
	    .Field<&openblack::creature_locomotion::Pair::to>("to")
	    .Field<&openblack::creature_locomotion::Pair::weight>("weight");
	Reflect<openblack::creature_locomotion::Ring>(context, ValueOnly {})
	    .Field<&openblack::creature_locomotion::Ring::min>("min")
	    .Field<&openblack::creature_locomotion::Ring::max>("max");
	Reflect<openblack::creature_locomotion::Speeds>(context, ValueOnly {})
	    .Field<&openblack::creature_locomotion::Speeds::walk>("walk")
	    .Field<&openblack::creature_locomotion::Speeds::run>("run");
	Reflect<openblack::creature_look::Target>(context, ValueOnly {})
	    .Field<&openblack::creature_look::Target::id>("id")
	    .Field<&openblack::creature_look::Target::kind>("kind")
	    .Field<&openblack::creature_look::Target::point>("point")
	    .Field<&openblack::creature_look::Target::watchedTurns>("watchedTurns");
	Reflect<openblack::creature_marks::Mark>(context, ValueOnly {})
	    .Field<&openblack::creature_marks::Mark::u>("u")
	    .Field<&openblack::creature_marks::Mark::v>("v")
	    .Field<&openblack::creature_marks::Mark::skin>("skin")
	    .Field<&openblack::creature_marks::Mark::age>("age")
	    .Field<&openblack::creature_marks::Mark::type>("type")
	    .Field<&openblack::creature_marks::Mark::column>("column");
	Reflect<openblack::creature_marks::Marks>(context, ValueOnly {})
	    .Field<&openblack::creature_marks::Marks::wounds>("wounds")
	    .Field<&openblack::creature_marks::Marks::blood>("blood")
	    .Field<&openblack::creature_marks::Marks::counts>("counts");
	Reflect<openblack::creature_mind::CastOrder>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::CastOrder::magicType>("magicType")
	    .Field<&openblack::creature_mind::CastOrder::object>("object");
	Reflect<openblack::creature_mind::Gaze>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Gaze::object>("object")
	    .Field<&openblack::creature_mind::Gaze::bottom>("bottom")
	    .Field<&openblack::creature_mind::Gaze::point>("point")
	    .Field<&openblack::creature_mind::Gaze::camera>("camera");
	Reflect<openblack::creature_mind::IdleMind>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::IdleMind::activity>("activity")
	    .Field<&openblack::creature_mind::IdleMind::agenda>("agenda")
	    .Field<&openblack::creature_mind::IdleMind::step>("step")
	    .Field<&openblack::creature_mind::IdleMind::stepStarted>("stepStarted")
	    .Field<&openblack::creature_mind::IdleMind::stepSeconds>("stepSeconds")
	    .Field<&openblack::creature_mind::IdleMind::sitEnding>("sitEnding")
	    .Field<&openblack::creature_mind::IdleMind::faceSeconds>("faceSeconds")
	    .Field<&openblack::creature_mind::IdleMind::faceVariety>("faceVariety")
	    .Field<&openblack::creature_mind::IdleMind::showDesireSeconds>("showDesireSeconds")
	    .Field<&openblack::creature_mind::IdleMind::shown>("shown")
	    .Field<&openblack::creature_mind::IdleMind::wakeWanted>("wakeWanted")
	    .Field<&openblack::creature_mind::IdleMind::serial>("serial")
	    .Field<&openblack::creature_mind::IdleMind::gaveUp>("gaveUp")
	    .Field<&openblack::creature_mind::IdleMind::castDone>("castDone")
	    .Field<&openblack::creature_mind::IdleMind::castTurns>("castTurns");
	Reflect<openblack::creature_mind::Movement>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Movement::kind>("kind")
	    .Field<&openblack::creature_mind::Movement::point>("point")
	    .Field<&openblack::creature_mind::Movement::object>("object")
	    .Field<&openblack::creature_mind::Movement::run>("run")
	    .Field<&openblack::creature_mind::Movement::minDistance>("minDistance")
	    .Field<&openblack::creature_mind::Movement::maxDistance>("maxDistance")
	    .Field<&openblack::creature_mind::Movement::giveUpIfUnreachable>("giveUpIfUnreachable");
	Reflect<openblack::creature_mind::ObjectOrder>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::ObjectOrder::kind>("kind")
	    .Field<&openblack::creature_mind::ObjectOrder::object>("object")
	    .Field<&openblack::creature_mind::ObjectOrder::point>("point")
	    .Field<&openblack::creature_mind::ObjectOrder::animation>("animation")
	    .Field<&openblack::creature_mind::ObjectOrder::pointHeight>("pointHeight")
	    .Field<&openblack::creature_mind::ObjectOrder::seconds>("seconds");
	Reflect<openblack::creature_mind::Step>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Step::kind>("kind")
	    .Field<&openblack::creature_mind::Step::seconds>("seconds")
	    .Field<&openblack::creature_mind::Step::animation>("animation")
	    .Field<&openblack::creature_mind::Step::sleepyEyes>("sleepyEyes")
	    .Field<&openblack::creature_mind::Step::movement>("movement")
	    .Field<&openblack::creature_mind::Step::sequence>("sequence")
	    .Field<&openblack::creature_mind::Step::untilRested>("untilRested")
	    .Field<&openblack::creature_mind::Step::holdLoop>("holdLoop")
	    .Field<&openblack::creature_mind::Step::closedEyes>("closedEyes")
	    .Field<&openblack::creature_mind::Step::effect>("effect")
	    .Field<&openblack::creature_mind::Step::object>("object")
	    .Field<&openblack::creature_mind::Step::order>("order")
	    .Field<&openblack::creature_mind::Step::cast>("cast")
	    .Field<&openblack::creature_mind::Step::face>("face")
	    .Field<&openblack::creature_mind::Step::gaze>("gaze");
	Reflect<openblack::creature_mind_model::Learnt>(context, ValueOnly {})
	    .Field<&openblack::creature_mind_model::Learnt::opinions>("opinions")
	    .Field<&openblack::creature_mind_model::Learnt::turnsSinceDone>("turnsSinceDone")
	    .Field<&openblack::creature_mind_model::Learnt::episodes>("episodes")
	    .Field<&openblack::creature_mind_model::Learnt::trees>("trees")
	    .Field<&openblack::creature_mind_model::Learnt::contexts>("contexts")
	    .Field<&openblack::creature_mind_model::Learnt::creatures>("creatures")
	    .Field<&openblack::creature_mind_model::Learnt::knowledge>("knowledge")
	    .Field<&openblack::creature_mind_model::Learnt::mimicry>("mimicry")
	    .Field<&openblack::creature_mind_model::Learnt::thoughts>("thoughts")
	    .Field<&openblack::creature_mind_model::Learnt::initialThresholds>("initialThresholds")
	    .Field<&openblack::creature_mind_model::Learnt::name>("name")
	    .Field<&openblack::creature_mind_model::Learnt::alignment>("alignment")
	    .Field<&openblack::creature_mind_model::Learnt::developmentTimer>("developmentTimer")
	    .Field<&openblack::creature_mind_model::Learnt::file>("file");
	Reflect<openblack::creature_morph::Morph>(context, ValueOnly {})
	    .Field<&openblack::creature_morph::Morph::evilGood>("evilGood")
	    .Field<&openblack::creature_morph::Morph::thinFat>("thinFat")
	    .Field<&openblack::creature_morph::Morph::weakStrong>("weakStrong");
	Reflect<openblack::creature_perceived_desires::PerceivedDesires>(context, ValueOnly {})
	    .Field<&openblack::creature_perceived_desires::PerceivedDesires::player>("player")
	    .Field<&openblack::creature_perceived_desires::PerceivedDesires::town>("town");
	Reflect<openblack::creature_physiology::Kept>(context, ValueOnly {})
	    .Field<&openblack::creature_physiology::Kept::age>("age")
	    .Field<&openblack::creature_physiology::Kept::turns>("turns")
	    .Field<&openblack::creature_physiology::Kept::energy>("energy")
	    .Field<&openblack::creature_physiology::Kept::exhaustion>("exhaustion");
	Reflect<openblack::creature_physiology::Needs>(context, ValueOnly {})
	    .Field<&openblack::creature_physiology::Needs::age>("age")
	    .Field<&openblack::creature_physiology::Needs::turns>("turns")
	    .Field<&openblack::creature_physiology::Needs::warmth>("warmth")
	    .Field<&openblack::creature_physiology::Needs::energy>("energy")
	    .Field<&openblack::creature_physiology::Needs::itchiness>("itchiness")
	    .Field<&openblack::creature_physiology::Needs::poo>("poo")
	    .Field<&openblack::creature_physiology::Needs::exhaustion>("exhaustion")
	    .Field<&openblack::creature_physiology::Needs::dehydration>("dehydration")
	    .Field<&openblack::creature_physiology::Needs::life>("life")
	    .Field<&openblack::creature_physiology::Needs::meals>("meals");
	Reflect<openblack::creature_planner::Plan>(context, ValueOnly {})
	    .Field<&openblack::creature_planner::Plan::desire>("desire")
	    .Field<&openblack::creature_planner::Plan::action>("action")
	    .Field<&openblack::creature_planner::Plan::object>("object")
	    .Field<&openblack::creature_planner::Plan::about>("about")
	    .Field<&openblack::creature_planner::Plan::goalUsefulness>("goalUsefulness")
	    .Field<&openblack::creature_planner::Plan::actionPriority>("actionPriority")
	    .Field<&openblack::creature_planner::Plan::priority>("priority")
	    .Field<&openblack::creature_planner::Plan::activityObject>("activityObject")
	    .Field<&openblack::creature_planner::Plan::instrument>("instrument");
	Reflect<openblack::creature_planner::PlannerState>(context, ValueOnly {})
	    .Field<&openblack::creature_planner::PlannerState::best>("best", {k_CreatureDesiresDesireNames})
	    .Field<&openblack::creature_planner::PlannerState::nextGoal>("nextGoal")
	    .Field<&openblack::creature_planner::PlannerState::current>("current");
	Reflect<openblack::creature_route::Route>(context, ValueOnly {})
	    .Field<&openblack::creature_route::Route::points>("points")
	    .Field<&openblack::creature_route::Route::segment>("segment")
	    .Field<&openblack::creature_route::Route::travelled>("travelled");
	Reflect<openblack::creature_script_play::Request>(context, ValueOnly {})
	    .Field<&openblack::creature_script_play::Request::animation>("animation")
	    .Field<&openblack::creature_script_play::Request::plays>("plays");
	Reflect<openblack::creature_spell_mind::Cheat>(context, ValueOnly {})
	    .Field<&openblack::creature_spell_mind::Cheat::desire>("desire")
	    .Field<&openblack::creature_spell_mind::Cheat::turns>("turns")
	    .Field<&openblack::creature_spell_mind::Cheat::seconds>("seconds");
	Reflect<openblack::creature_spells::Slot>(context, ValueOnly {})
	    .Field<&openblack::creature_spells::Slot::phase>("phase")
	    .Field<&openblack::creature_spells::Slot::turnsLeft>("turnsLeft")
	    .Field<&openblack::creature_spells::Slot::holdTurns>("holdTurns")
	    .Field<&openblack::creature_spells::Slot::miracle>("miracle")
	    .Field<&openblack::creature_spells::Slot::before>("before");
	Reflect<openblack::creature_spells::Spells>(context, ValueOnly {})
	    .Field<&openblack::creature_spells::Spells::slots>("slots", {k_CreatureSpellsSpellNames})
	    .Field<&openblack::creature_spells::Spells::waiting>("waiting")
	    .Field<&openblack::creature_spells::Spells::startDelayTurns>("startDelayTurns")
	    .Field<&openblack::creature_spells::Spells::reversion>("reversion");
	Reflect<openblack::creature_spells::Waiting>(context, ValueOnly {})
	    .Field<&openblack::creature_spells::Waiting::spell>("spell")
	    .Field<&openblack::creature_spells::Waiting::holdTurns>("holdTurns")
	    .Field<&openblack::creature_spells::Waiting::miracle>("miracle");
	Reflect<openblack::creature_sway::Sway>(context, ValueOnly {})
	    .Field<&openblack::creature_sway::Sway::lowerVelocity>("lowerVelocity")
	    .Field<&openblack::creature_sway::Sway::upperVelocity>("upperVelocity")
	    .Field<&openblack::creature_sway::Sway::lowerOffset>("lowerOffset")
	    .Field<&openblack::creature_sway::Sway::upperOffset>("upperOffset")
	    .Field<&openblack::creature_sway::Sway::drive>("drive")
	    .Field<&openblack::creature_sway::Sway::active>("active")
	    .Field<&openblack::creature_sway::Sway::leashDrag>("leashDrag")
	    .Field<&openblack::creature_sway::Sway::frameSeconds>("frameSeconds");
	Reflect<openblack::creature_tattoo::Slot>(context, ValueOnly {})
	    .Field<&openblack::creature_tattoo::Slot::design>("design")
	    .Field<&openblack::creature_tattoo::Slot::site>("site")
	    .Field<&openblack::creature_tattoo::Slot::colour>("colour");
	Reflect<openblack::creature_town_compassion::State>(context, ValueOnly {})
	    .Field<&openblack::creature_town_compassion::State::desire>("desire")
	    .Field<&openblack::creature_town_compassion::State::index>("index")
	    .Field<&openblack::creature_town_compassion::State::remembered>("remembered")
	    .Field<&openblack::creature_town_compassion::State::lastTurn>("lastTurn")
	    .Field<&openblack::creature_town_compassion::State::timesKept>("timesKept")
	    .Field<&openblack::creature_town_compassion::State::choosesFreely>("choosesFreely");
	Reflect<openblack::creature_tree::Belief>(context, ValueOnly {})
	    .Field<&openblack::creature_tree::Belief::type>("type")
	    .Field<&openblack::creature_tree::Belief::values>("values", {k_CreatureTreeAttributeNames});
	Reflect<openblack::creature_tree::Episode>(context, ValueOnly {})
	    .Field<&openblack::creature_tree::Episode::belief>("belief")
	    .Field<&openblack::creature_tree::Episode::feedback>("feedback")
	    .Field<&openblack::creature_tree::Episode::saved>("saved");
	Reflect<openblack::creature_tree::Node>(context, ValueOnly {})
	    .Field<&openblack::creature_tree::Node::test>("test")
	    .Field<&openblack::creature_tree::Node::children>("children")
	    .Field<&openblack::creature_tree::Node::bucket>("bucket")
	    .Field<&openblack::creature_tree::Node::examples>("examples");
	Reflect<openblack::creature_tree::Tree>(context, ValueOnly {}).Field<&openblack::creature_tree::Tree::nodes>("nodes");
	Reflect<openblack::creature_watching::Knowledge>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::Knowledge::skillsSeen>("skillsSeen")
	    .Field<&openblack::creature_watching::Knowledge::miraclesSeen>("miraclesSeen")
	    .Field<&openblack::creature_watching::Knowledge::skillsKnown>("skillsKnown")
	    .Field<&openblack::creature_watching::Knowledge::miraclesKnown>("miraclesKnown");
	Reflect<openblack::creature_watching::Mimicry>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::Mimicry::rule>("rule")
	    .Field<&openblack::creature_watching::Mimicry::stage>("stage")
	    .Field<&openblack::creature_watching::Mimicry::stepsLeft>("stepsLeft")
	    .Field<&openblack::creature_watching::Mimicry::object>("object");
	Reflect<openblack::creature_watching::Sighting>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::Sighting::count>("count")
	    .Field<&openblack::creature_watching::Sighting::turn>("turn");
	Reflect<openblack::ecs::components::ChimneySmoke::Puff>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::position>("position")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::velocity>("velocity")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::age>("age")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::angle>("angle")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::clockwise>("clockwise")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::hidden>("hidden");
	Reflect<openblack::ecs::components::CreatureAnimation::Slot>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::animation>("animation")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::timeMs>("timeMs")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::weight>("weight")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::mirrored>("mirrored");
	Reflect<openblack::ecs::components::CreatureAudio::Heard>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::atMs>("atMs")
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::kind>("kind")
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::keys>("keys")
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::bank>("bank")
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::sample>("sample")
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::played>("played")
	    .Field<&openblack::ecs::components::CreatureAudio::Heard::note>("note");
	Reflect<openblack::ecs::components::CreatureCasting::Fizzle>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureCasting::Fizzle::target>("target")
	    .Field<&openblack::ecs::components::CreatureCasting::Fizzle::magicType>("magicType");
	Reflect<openblack::ecs::components::CreatureEyes::Eye>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureEyes::Eye::eyeball>("eyeball")
	    .Field<&openblack::ecs::components::CreatureEyes::Eye::eyelid>("eyelid");
	Reflect<openblack::ecs::components::CreatureHair::Group>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureHair::Group::strands>("strands")
	    .Field<&openblack::ecs::components::CreatureHair::Group::colour>("colour")
	    .Field<&openblack::ecs::components::CreatureHair::Group::halfWidth>("halfWidth")
	    .Field<&openblack::ecs::components::CreatureHair::Group::textured>("textured");
	Reflect<openblack::ecs::components::CreatureLeash::Order>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLeash::Order::serial>("serial")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::fight>("fight")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::object>("object")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::point>("point")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::sparkles>("sparkles")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::ring>("ring")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::footprint>("footprint");
	Reflect<openblack::ecs::components::CreatureLeash::Worn>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::type>("type")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::holder>("holder")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::works>("works")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::tiedTo>("tiedTo")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::tiedTurn>("tiedTurn")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::rope>("rope")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::ropeStarted>("ropeStarted");
	Reflect<openblack::ecs::components::CreatureLocomotion::Move>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::animations>("animations")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::timeMs>("timeMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::durationMs>("durationMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::displacement>("displacement")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::startHeading>("startHeading")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::startRouteHeading>("startRouteHeading");
	Reflect<openblack::ecs::components::CreatureLocomotion::Track>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::animation>("animation")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::fromMs>("fromMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::advanceMs>("advanceMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::durationMs>("durationMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::weight>("weight")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::looping>("looping")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::breathing>("breathing");
	Reflect<openblack::ecs::components::CreatureMindState::ActionUnderway>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureMindState::ActionUnderway::serial>("serial")
	    .Field<&openblack::ecs::components::CreatureMindState::ActionUnderway::action>("action");
	Reflect<openblack::ecs::components::CreatureMindState::Feedback>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureMindState::Feedback::value>("value")
	    .Field<&openblack::ecs::components::CreatureMindState::Feedback::activity>("activity");
	Reflect<openblack::ecs::components::CreatureMindState::Teaching>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureMindState::Teaching::type>("type")
	    .Field<&openblack::ecs::components::CreatureMindState::Teaching::action>("action")
	    .Field<&openblack::ecs::components::CreatureMindState::Teaching::knows>("knows");
	Reflect<openblack::ecs::components::CreatureSkin::Painted>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureSkin::Painted::baseMesh>("baseMesh")
	    .Field<&openblack::ecs::components::CreatureSkin::Painted::variantMesh>("variantMesh")
	    .Field<&openblack::ecs::components::CreatureSkin::Painted::weight>("weight")
	    .Field<&openblack::ecs::components::CreatureSkin::Painted::tattoos>("tattoos")
	    .Field<&openblack::ecs::components::CreatureSkin::Painted::marks>("marks")
	    .Field<&openblack::ecs::components::CreatureSkin::Painted::art>("art");
	Reflect<openblack::ecs::components::CreatureSkin::Skin>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureSkin::Skin::id>("id")
	    .Field<&openblack::ecs::components::CreatureSkin::Skin::texels>("texels");
	Reflect<openblack::ecs::components::Footpath::Node>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Footpath::Node::position>("position");
	Reflect<openblack::ecs::components::HandMorph::Skin>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::HandMorph::Skin::id>("id")
	    .Field<&openblack::ecs::components::HandMorph::Skin::texels>("texels");
	Reflect<openblack::ecs::components::Player::Cast>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Player::Cast::position>("position")
	    .Field<&openblack::ecs::components::Player::Cast::type>("type")
	    .Field<&openblack::ecs::components::Player::Cast::turn>("turn");
	Reflect<openblack::ecs::components::Player::Miracles>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Player::Miracles::enabled>("enabled")
	    .Field<&openblack::ecs::components::Player::Miracles::everEnabled>("everEnabled")
	    .Field<&openblack::ecs::components::Player::Miracles::allEnabled>("allEnabled")
	    .Field<&openblack::ecs::components::Player::Miracles::tribalPower>("tribalPower")
	    .Field<&openblack::ecs::components::Player::Miracles::maxTribalPower>("maxTribalPower");
	Reflect<openblack::ecs::components::TeleportStone::Traveller>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::TeleportStone::Traveller::living>("living")
	    .Field<&openblack::ecs::components::TeleportStone::Traveller::destination>("destination")
	    .Field<&openblack::ecs::components::TeleportStone::Traveller::finalState>("finalState");
	Reflect<openblack::ecs::components::WorshipSite::BuildRequest>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::WorshipSite::BuildRequest::town>("town")
	    .Field<&openblack::ecs::components::WorshipSite::BuildRequest::desireBoost>("desireBoost");
	Reflect<openblack::ecs::high_detail_rules::DrawOrders>(context, ValueOnly {})
	    .Field<&openblack::ecs::high_detail_rules::DrawOrders::followIntroHand>("followIntroHand")
	    .Field<&openblack::ecs::high_detail_rules::DrawOrders::turnAtOnce>("turnAtOnce");
	Reflect<openblack::ecs::reward::DustSprite>(context, ValueOnly {})
	    .Field<&openblack::ecs::reward::DustSprite::offset>("offset")
	    .Field<&openblack::ecs::reward::DustSprite::frame>("frame")
	    .Field<&openblack::ecs::reward::DustSprite::rgb>("rgb");
	Reflect<openblack::ecs::script_highlights::Spark>(context, ValueOnly {})
	    .Field<&openblack::ecs::script_highlights::Spark::ageMilliseconds>("ageMilliseconds")
	    .Field<&openblack::ecs::script_highlights::Spark::angle>("angle");
	Reflect<openblack::ecs::script_highlights::Sparks>(context, ValueOnly {})
	    .Field<&openblack::ecs::script_highlights::Sparks::sparks>("sparks")
	    .Field<&openblack::ecs::script_highlights::Sparks::firstPicture>("firstPicture");
	Reflect<openblack::ecs::town_aggression::Record>(context, ValueOnly {})
	    .Field<&openblack::ecs::town_aggression::Record::aggression>("aggression", {k_PlayerNamesNames})
	    .Field<&openblack::ecs::town_aggression::Record::lastTurns>("lastTurns", {k_PlayerNamesNames})
	    .Field<&openblack::ecs::town_aggression::Record::lastAggressor>("lastAggressor")
	    .Field<&openblack::ecs::town_aggression::Record::lastTurn>("lastTurn")
	    .Field<&openblack::ecs::town_aggression::Record::protectionMultiplier>("protectionMultiplier")
	    .Field<&openblack::ecs::town_aggression::Record::mercyMultiplier>("mercyMultiplier")
	    .Field<&openblack::ecs::town_aggression::Record::protection>("protection")
	    .Field<&openblack::ecs::town_aggression::Record::mercy>("mercy");
	Reflect<openblack::ecs::village_totem::Ease>(context, ValueOnly {})
	    .Field<&openblack::ecs::village_totem::Ease::share>("share")
	    .Field<&openblack::ecs::village_totem::Ease::speed>("speed")
	    .Field<&openblack::ecs::village_totem::Ease::target>("target")
	    .Field<&openblack::ecs::village_totem::Ease::elapsed>("elapsed")
	    .Field<&openblack::ecs::village_totem::Ease::duration>("duration")
	    .Field<&openblack::ecs::village_totem::Ease::startShare>("startShare")
	    .Field<&openblack::ecs::village_totem::Ease::startSpeed>("startSpeed")
	    .Field<&openblack::ecs::village_totem::Ease::acceleration>("acceleration")
	    .Field<&openblack::ecs::village_totem::Ease::jerk>("jerk")
	    .Field<&openblack::ecs::village_totem::Ease::snap>("snap")
	    .Field<&openblack::ecs::village_totem::Ease::moving>("moving");
	Reflect<openblack::ecs::villager_draw::ClipBlend>(context, ValueOnly {})
	    .Field<&openblack::ecs::villager_draw::ClipBlend::from>("from")
	    .Field<&openblack::ecs::villager_draw::ClipBlend::fromPlace>("fromPlace")
	    .Field<&openblack::ecs::villager_draw::ClipBlend::remaining>("remaining")
	    .Field<&openblack::ecs::villager_draw::ClipBlend::weight>("weight");
	Reflect<openblack::ecs::villager_draw::ClipBlendTrack>(context, ValueOnly {})
	    .Field<&openblack::ecs::villager_draw::ClipBlendTrack::lastClip>("lastClip")
	    .Field<&openblack::ecs::villager_draw::ClipBlendTrack::lastPlace>("lastPlace")
	    .Field<&openblack::ecs::villager_draw::ClipBlendTrack::blend>("blend");
	Reflect<openblack::ecs::villager_eyes::Blink>(context, ValueOnly {})
	    .Field<&openblack::ecs::villager_eyes::Blink::blinking>("blinking")
	    .Field<&openblack::ecs::villager_eyes::Blink::untilNext>("untilNext")
	    .Field<&openblack::ecs::villager_eyes::Blink::into>("into")
	    .Field<&openblack::ecs::villager_eyes::Blink::closedFor>("closedFor");
	Reflect<openblack::ecs::villager_eyes::DrawnEye>(context, ValueOnly {})
	    .Field<&openblack::ecs::villager_eyes::DrawnEye::eyeball>("eyeball")
	    .Field<&openblack::ecs::villager_eyes::DrawnEye::upperLid>("upperLid")
	    .Field<&openblack::ecs::villager_eyes::DrawnEye::lowerLid>("lowerLid")
	    .Field<&openblack::ecs::villager_eyes::DrawnEye::shade>("shade");
	Reflect<openblack::ecs::villager_eyes::DrawnEyes>(context, ValueOnly {})
	    .Field<&openblack::ecs::villager_eyes::DrawnEyes::eyes>("eyes")
	    .Field<&openblack::ecs::villager_eyes::DrawnEyes::closed>("closed")
	    .Field<&openblack::ecs::villager_eyes::DrawnEyes::standing>("standing");
	Reflect<openblack::ecs::villager_eyes::Eyes>(context, ValueOnly {})
	    .Field<&openblack::ecs::villager_eyes::Eyes::blink>("blink")
	    .Field<&openblack::ecs::villager_eyes::Eyes::roll>("roll");
	Reflect<openblack::field_crop::Crop>(context, ValueOnly {})
	    .Field<&openblack::field_crop::Crop::timesSown>("timesSown")
	    .Field<&openblack::field_crop::Crop::age>("age")
	    .Field<&openblack::field_crop::Crop::food>("food");
	Reflect<openblack::field_crop::Settle>(context, ValueOnly {})
	    .Field<&openblack::field_crop::Settle::position>("position")
	    .Field<&openblack::field_crop::Settle::speed>("speed");
	Reflect<openblack::fire::State>(context, ValueOnly {})
	    .Field<&openblack::fire::State::temperature>("temperature")
	    .Field<&openblack::fire::State::previous>("previous")
	    .Field<&openblack::fire::State::charring>("charring")
	    .Field<&openblack::fire::State::flags>("flags");
	Reflect<openblack::fire::graphic::Graphic>(context, ValueOnly {})
	    .Field<&openblack::fire::graphic::Graphic::kinds>("kinds")
	    .Field<&openblack::fire::graphic::Graphic::maxFlames>("maxFlames")
	    .Field<&openblack::fire::graphic::Graphic::localScale>("localScale")
	    .Field<&openblack::fire::graphic::Graphic::flameAccumulator>("flameAccumulator")
	    .Field<&openblack::fire::graphic::Graphic::flameCount>("flameCount")
	    .Field<&openblack::fire::graphic::Graphic::flared>("flared")
	    .Field<&openblack::fire::graphic::Graphic::flames>("flames")
	    .Field<&openblack::fire::graphic::Graphic::steamStart>("steamStart")
	    .Field<&openblack::fire::graphic::Graphic::steamAccumulator>("steamAccumulator")
	    .Field<&openblack::fire::graphic::Graphic::steamCount>("steamCount")
	    .Field<&openblack::fire::graphic::Graphic::steamTemperature>("steamTemperature")
	    .Field<&openblack::fire::graphic::Graphic::steam>("steam")
	    .Field<&openblack::fire::graphic::Graphic::smokeStart>("smokeStart")
	    .Field<&openblack::fire::graphic::Graphic::smokeAccumulator>("smokeAccumulator")
	    .Field<&openblack::fire::graphic::Graphic::smokeCount>("smokeCount")
	    .Field<&openblack::fire::graphic::Graphic::smokePoint>("smokePoint")
	    .Field<&openblack::fire::graphic::Graphic::smoke>("smoke")
	    .Field<&openblack::fire::graphic::Graphic::drawnTurn>("drawnTurn")
	    .Field<&openblack::fire::graphic::Graphic::drawnFraction>("drawnFraction");
	Reflect<openblack::fire::graphic::Kinds>(context, ValueOnly {})
	    .Field<&openblack::fire::graphic::Kinds::flames>("flames")
	    .Field<&openblack::fire::graphic::Kinds::smoke>("smoke")
	    .Field<&openblack::fire::graphic::Kinds::steam>("steam")
	    .Field<&openblack::fire::graphic::Kinds::lightMap>("lightMap")
	    .Field<&openblack::fire::graphic::Kinds::followsLand>("followsLand");
	Reflect<openblack::fire::graphic::Sprite>(context, ValueOnly {})
	    .Field<&openblack::fire::graphic::Sprite::position>("position")
	    .Field<&openblack::fire::graphic::Sprite::scale>("scale")
	    .Field<&openblack::fire::graphic::Sprite::age>("age")
	    .Field<&openblack::fire::graphic::Sprite::alpha>("alpha")
	    .Field<&openblack::fire::graphic::Sprite::velocity>("velocity")
	    .Field<&openblack::fire::graphic::Sprite::baseScale>("baseScale");
	Reflect<openblack::fish_shoal::Fish>(context, ValueOnly {})
	    .Field<&openblack::fish_shoal::Fish::position>("position")
	    .Field<&openblack::fish_shoal::Fish::heading>("heading")
	    .Field<&openblack::fish_shoal::Fish::speed>("speed")
	    .Field<&openblack::fish_shoal::Fish::turnRate>("turnRate")
	    .Field<&openblack::fish_shoal::Fish::phase>("phase")
	    .Field<&openblack::fish_shoal::Fish::panic>("panic")
	    .Field<&openblack::fish_shoal::Fish::size>("size")
	    .Field<&openblack::fish_shoal::Fish::frame>("frame");
	Reflect<openblack::fish_shoal::Shoal>(context, ValueOnly {})
	    .Field<&openblack::fish_shoal::Shoal::centre>("centre")
	    .Field<&openblack::fish_shoal::Shoal::target>("target")
	    .Field<&openblack::fish_shoal::Shoal::retargetSeconds>("retargetSeconds")
	    .Field<&openblack::fish_shoal::Shoal::fullness>("fullness")
	    .Field<&openblack::fish_shoal::Shoal::fish>("fish");
	Reflect<openblack::graphics::TextureHandle>(context, ValueOnly {}).Field<&openblack::graphics::TextureHandle::id>("id");
	Reflect<openblack::graphics::moon::Placement>(context, ValueOnly {})
	    .Field<&openblack::graphics::moon::Placement::offset>("offset")
	    .Field<&openblack::graphics::moon::Placement::alpha>("alpha");
	Reflect<openblack::graphics::sun::Placement>(context, ValueOnly {})
	    .Field<&openblack::graphics::sun::Placement::position>("position")
	    .Field<&openblack::graphics::sun::Placement::alpha>("alpha");
	Reflect<openblack::hand_grab::HoldFacts>(context, ValueOnly {})
	    .Field<&openblack::hand_grab::HoldFacts::type>("type")
	    .Field<&openblack::hand_grab::HoldFacts::loweringMultiplier>("loweringMultiplier")
	    .Field<&openblack::hand_grab::HoldFacts::holdRadius>("holdRadius");
	Reflect<openblack::hand_grab::Tug>(context, ValueOnly {})
	    .Field<&openblack::hand_grab::Tug::axes>("axes")
	    .Field<&openblack::hand_grab::Tug::base>("base")
	    .Field<&openblack::hand_grab::Tug::spin>("spin")
	    .Field<&openblack::hand_grab::Tug::pullVelocity>("pullVelocity");
	Reflect<openblack::hand_morph::State>(context, ValueOnly {})
	    .Field<&openblack::hand_morph::State::target>("target")
	    .Field<&openblack::hand_morph::State::drawn>("drawn")
	    .Field<&openblack::hand_morph::State::inInfluence>("inInfluence");
	Reflect<openblack::leash_rope::Look>(context, ValueOnly {})
	    .Field<&openblack::leash_rope::Look::halfWidth>("halfWidth")
	    .Field<&openblack::leash_rope::Look::v0>("v0")
	    .Field<&openblack::leash_rope::Look::v1>("v1")
	    .Field<&openblack::leash_rope::Look::uScale>("uScale");
	Reflect<openblack::leash_rope::Node>(context, ValueOnly {})
	    .Field<&openblack::leash_rope::Node::position>("position")
	    .Field<&openblack::leash_rope::Node::velocity>("velocity")
	    .Field<&openblack::leash_rope::Node::stretch>("stretch");
	Reflect<openblack::leash_rope::Rope>(context, ValueOnly {})
	    .Field<&openblack::leash_rope::Rope::start>("start")
	    .Field<&openblack::leash_rope::Rope::end>("end")
	    .Field<&openblack::leash_rope::Rope::nodes>("nodes")
	    .Field<&openblack::leash_rope::Rope::slackLength>("slackLength")
	    .Field<&openblack::leash_rope::Rope::maxLength>("maxLength")
	    .Field<&openblack::leash_rope::Rope::look>("look")
	    .Field<&openblack::leash_rope::Rope::tension>("tension");
	Reflect<openblack::lightning::Flash>(context, ValueOnly {})
	    .Field<&openblack::lightning::Flash::position>("position")
	    .Field<&openblack::lightning::Flash::radius>("radius")
	    .Field<&openblack::lightning::Flash::strength>("strength")
	    .Field<&openblack::lightning::Flash::time>("time")
	    .Field<&openblack::lightning::Flash::active>("active");
	Reflect<openblack::magic::DispenserTimer>(context, ValueOnly {})
	    .Field<&openblack::magic::DispenserTimer::tick>("tick")
	    .Field<&openblack::magic::DispenserTimer::period>("period")
	    .Field<&openblack::magic::DispenserTimer::active>("active");
	Reflect<openblack::magic::SpellChants>(context, ValueOnly {})
	    .Field<&openblack::magic::SpellChants::chants>("chants")
	    .Field<&openblack::magic::SpellChants::initialChants>("initialChants")
	    .Field<&openblack::magic::SpellChants::strengthMultiplier>("strengthMultiplier")
	    .Field<&openblack::magic::SpellChants::free>("free");
	Reflect<openblack::magic::flock::Corridor>(context, ValueOnly {})
	    .Field<&openblack::magic::flock::Corridor::normal>("normal")
	    .Field<&openblack::magic::flock::Corridor::offset>("offset")
	    .Field<&openblack::magic::flock::Corridor::halfWidth>("halfWidth")
	    .Field<&openblack::magic::flock::Corridor::along>("along");
	Reflect<openblack::magic::piles::Rise>(context, ValueOnly {})
	    .Field<&openblack::magic::piles::Rise::start>("start")
	    .Field<&openblack::magic::piles::Rise::speed>("speed")
	    .Field<&openblack::magic::piles::Rise::acceleration>("acceleration")
	    .Field<&openblack::magic::piles::Rise::jerk>("jerk")
	    .Field<&openblack::magic::piles::Rise::snap>("snap")
	    .Field<&openblack::magic::piles::Rise::target>("target")
	    .Field<&openblack::magic::piles::Rise::time>("time")
	    .Field<&openblack::magic::piles::Rise::duration>("duration")
	    .Field<&openblack::magic::piles::Rise::offset>("offset")
	    .Field<&openblack::magic::piles::Rise::currentSpeed>("currentSpeed");
	Reflect<openblack::magic::shield::DomePose>(context, ValueOnly {})
	    .Field<&openblack::magic::shield::DomePose::scale>("scale")
	    .Field<&openblack::magic::shield::DomePose::angle>("angle")
	    .Field<&openblack::magic::shield::DomePose::height>("height")
	    .Field<&openblack::magic::shield::DomePose::drawn>("drawn")
	    .Field<&openblack::magic::shield::DomePose::gone>("gone");
	Reflect<openblack::magic::shield::DomeShape>(context, ValueOnly {})
	    .Field<&openblack::magic::shield::DomeShape::finalScale>("finalScale")
	    .Field<&openblack::magic::shield::DomeShape::startScale>("startScale")
	    .Field<&openblack::magic::shield::DomeShape::startSpin>("startSpin")
	    .Field<&openblack::magic::shield::DomeShape::endSpin>("endSpin")
	    .Field<&openblack::magic::shield::DomeShape::raiseWithScale>("raiseWithScale")
	    .Field<&openblack::magic::shield::DomeShape::shieldHeight>("shieldHeight")
	    .Field<&openblack::magic::shield::DomeShape::bobMagnitude>("bobMagnitude");
	Reflect<openblack::magic::shield::DomeState>(context, ValueOnly {})
	    .Field<&openblack::magic::shield::DomeState::angle>("angle")
	    .Field<&openblack::magic::shield::DomeState::bob>("bob")
	    .Field<&openblack::magic::shield::DomeState::dieTime>("dieTime")
	    .Field<&openblack::magic::shield::DomeState::dying>("dying");
	Reflect<openblack::magic::town_belief::Belief>(context, ValueOnly {})
	    .Field<&openblack::magic::town_belief::Belief::belief>("belief", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::pending>("pending", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::recent>("recent", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::cap>("cap", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::scale>("scale");
	Reflect<openblack::magic::visuals::HandBand>(context, ValueOnly {})
	    .Field<&openblack::magic::visuals::HandBand::kind>("kind")
	    .Field<&openblack::magic::visuals::HandBand::index>("index")
	    .Field<&openblack::magic::visuals::HandBand::delay>("delay")
	    .Field<&openblack::magic::visuals::HandBand::age>("age")
	    .Field<&openblack::magic::visuals::HandBand::duration>("duration")
	    .Field<&openblack::magic::visuals::HandBand::alphaFrom>("alphaFrom")
	    .Field<&openblack::magic::visuals::HandBand::alphaTo>("alphaTo")
	    .Field<&openblack::magic::visuals::HandBand::spin>("spin")
	    .Field<&openblack::magic::visuals::HandBand::done>("done");
	Reflect<openblack::magic::visuals::HandBands>(context, ValueOnly {})
	    .Field<&openblack::magic::visuals::HandBands::bracelets>("bracelets")
	    .Field<&openblack::magic::visuals::HandBands::flying>("flying");
	Reflect<openblack::map_coords::MapCoords>(context, ValueOnly {})
	    .Field<&openblack::map_coords::MapCoords::x>("x")
	    .Field<&openblack::map_coords::MapCoords::z>("z")
	    .Field<&openblack::map_coords::MapCoords::altitude>("altitude");
	Reflect<openblack::particles::ProcessInfo>(context, ValueOnly {})
	    .Field<&openblack::particles::ProcessInfo::handPosition>("handPosition")
	    .Field<&openblack::particles::ProcessInfo::cameraForward>("cameraForward")
	    .Field<&openblack::particles::ProcessInfo::direction>("direction")
	    .Field<&openblack::particles::ProcessInfo::power>("power")
	    .Field<&openblack::particles::ProcessInfo::enabled>("enabled")
	    .Field<&openblack::particles::ProcessInfo::spin>("spin");
	Reflect<openblack::physics::damage::Corner>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Corner::position>("position")
	    .Field<&openblack::physics::damage::Corner::uv>("uv");
	Reflect<openblack::physics::damage::Mesh>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Mesh::primitives>("primitives")
	    .Field<&openblack::physics::damage::Mesh::trianglesAtCreation>("trianglesAtCreation")
	    .Field<&openblack::physics::damage::Mesh::remaining>("remaining")
	    .Field<&openblack::physics::damage::Mesh::snowLevel>("snowLevel")
	    .Field<&openblack::physics::damage::Mesh::snowFrozen>("snowFrozen");
	Reflect<openblack::physics::damage::Primitive>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Primitive::material>("material")
	    .Field<&openblack::physics::damage::Primitive::triangles>("triangles");
	Reflect<openblack::physics::damage::Triangle>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Triangle::corners>("corners")
	    .Field<&openblack::physics::damage::Triangle::neighbours>("neighbours")
	    .Field<&openblack::physics::damage::Triangle::group>("group")
	    .Field<&openblack::physics::damage::Triangle::sizeClass>("sizeClass")
	    .Field<&openblack::physics::damage::Triangle::countedClass>("countedClass");
	Reflect<openblack::script::timers::Timer>(context, ValueOnly {})
	    .Field<&openblack::script::timers::Timer::setTurn>("setTurn")
	    .Field<&openblack::script::timers::Timer::turns>("turns");
	Reflect<openblack::skeletal_animation::Animation>(context, ValueOnly {})
	    .Field<&openblack::skeletal_animation::Animation::duration>("duration")
	    .Field<&openblack::skeletal_animation::Animation::looping>("looping")
	    .Field<&openblack::skeletal_animation::Animation::rotatedJoints>("rotatedJoints")
	    .Field<&openblack::skeletal_animation::Animation::translatedJoints>("translatedJoints")
	    .Field<&openblack::skeletal_animation::Animation::frames>("frames")
	    .Field<&openblack::skeletal_animation::Animation::displacement>("displacement");
	Reflect<openblack::skeletal_animation::Animation::Frame>(context, ValueOnly {})
	    .Field<&openblack::skeletal_animation::Animation::Frame::eulerAngles>("eulerAngles")
	    .Field<&openblack::skeletal_animation::Animation::Frame::translations>("translations");
	Reflect<openblack::skeletal_animation::Skeleton>(context, ValueOnly {})
	    .Field<&openblack::skeletal_animation::Skeleton::parents>("parents")
	    .Field<&openblack::skeletal_animation::Skeleton::restRotations>("restRotations")
	    .Field<&openblack::skeletal_animation::Skeleton::inverseRestRotations>("inverseRestRotations");
	Reflect<openblack::sky_dome::FrameRows>(context, ValueOnly {})
	    .Field<&openblack::sky_dome::FrameRows::rows>("rows")
	    .Field<&openblack::sky_dome::FrameRows::count>("count");
	Reflect<openblack::sky_dome::Rows>(context, ValueOnly {})
	    .Field<&openblack::sky_dome::Rows::skyType>("skyType")
	    .Field<&openblack::sky_dome::Rows::first>("first")
	    .Field<&openblack::sky_dome::Rows::count>("count");
	Reflect<openblack::temple_leashes::Look>(context, ValueOnly {})
	    .Field<&openblack::temple_leashes::Look::scroll>("scroll")
	    .Field<&openblack::temple_leashes::Look::pitch>("pitch")
	    .Field<&openblack::temple_leashes::Look::roll>("roll")
	    .Field<&openblack::temple_leashes::Look::glow>("glow");
	Reflect<openblack::virtual_influence::State>(context, ValueOnly {})
	    .Field<&openblack::virtual_influence::State::anchor>("anchor")
	    .Field<&openblack::virtual_influence::State::turnHand>("turnHand")
	    .Field<&openblack::virtual_influence::State::lastInsideTurn>("lastInsideTurn")
	    .Field<&openblack::virtual_influence::State::fraction>("fraction")
	    .Field<&openblack::virtual_influence::State::soundFraction>("soundFraction")
	    .Field<&openblack::virtual_influence::State::soundStarted>("soundStarted")
	    .Field<&openblack::virtual_influence::State::disabled>("disabled")
	    .Field<&openblack::virtual_influence::State::manaPathStart>("manaPathStart")
	    .Field<&openblack::virtual_influence::State::manaPathEmission>("manaPathEmission");
}
