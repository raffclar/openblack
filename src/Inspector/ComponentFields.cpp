/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include "ComponentReflection.h"
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
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Fixed.h"
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
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Influence.h"
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
#include "ECS/Components/ScriptControl.h"
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
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Weather.h"

namespace components = openblack::ecs::components;

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
	Reflect<components::Flock>(context)
	    .Field<&components::Flock::members>("members")
	    .Field<&components::Flock::centre>("centre")
	    .Field<&components::Flock::domainRadius>("domainRadius")
	    .Field<&components::Flock::flockDistance>("flockDistance")
	    .Field<&components::Flock::followState>("followState")
	    .Field<&components::Flock::followMode>("followMode")
	    .Field<&components::Flock::afterMove>("afterMove")
	    .Field<&components::Flock::turnsOnLeg>("turnsOnLeg")
	    .Field<&components::Flock::height>("height")
	    .Field<&components::Flock::scriptId>("scriptId")
	    .Field<&components::Flock::made>("made")
	    .Field<&components::Flock::temple>("temple");
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
	    .Field<&components::AnimalPose::alpha>("alpha");
	Reflect<components::BeingEaten>(context)
	    .Field<&components::BeingEaten::hunter>("hunter")
	    .Field<&components::BeingEaten::turns>("turns")
	    .Field<&components::BeingEaten::eatenFrom>("eatenFrom")
	    .Field<&components::BeingEaten::left>("left");
	Reflect<components::AnimatedStatic>(context)
	    .Field<&components::AnimatedStatic::type>("type")
	    .Field<&components::AnimatedStatic::openState>("openState")
	    .Field<&components::AnimatedStatic::plinthState>("plinthState")
	    .Field<&components::AnimatedStatic::plinthFull>("plinthFull");
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
	    .Field<&components::CreatureFighting::ended>("ended")
	    .Field<&components::CreatureFighting::startPosition>("startPosition");
	Reflect<components::CreatureFightRecord>(context)
	    .Field<&components::CreatureFightRecord::tendency>("tendency")
	    .Field<&components::CreatureFightRecord::foughtBefore>("foughtBefore")
	    .Field<&components::CreatureFightRecord::fights>("fights")
	    .Field<&components::CreatureFightRecord::wins>("wins")
	    .Field<&components::CreatureFightRecord::secondsSinceFight>("secondsSinceFight");
	Reflect<components::CreatureKnockedOut>(context)
	    .Field<&components::CreatureKnockedOut::stage>("stage")
	    .Field<&components::CreatureKnockedOut::seconds>("seconds")
	    .Field<&components::CreatureKnockedOut::rest>("rest")
	    .Field<&components::CreatureKnockedOut::permanent>("permanent")
	    .Field<&components::CreatureKnockedOut::home>("home");
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
	    .Field<&components::CreatureMindState::look>("look")
	    .Field<&components::CreatureMindState::lookingAbout>("lookingAbout")
	    .Field<&components::CreatureMindState::secondsAlone>("secondsAlone")
	    .Field<&components::CreatureMindState::feedbackSeconds>("feedbackSeconds")
	    .Field<&components::CreatureMindState::feedbackWasStroke>("feedbackWasStroke")
	    .Field<&components::CreatureMindState::attitudeToPlayer>("attitudeToPlayer")
	    .Field<&components::CreatureMindState::averageFeedback>("averageFeedback")
	    .Field<&components::CreatureMindState::perceivedDesires>("perceivedDesires")
	    .Field<&components::CreatureMindState::lastFeedback>("lastFeedback")
	    .Field<&components::CreatureMindState::developmentPhase>("developmentPhase")
	    .Field<&components::CreatureMindState::desiresPhase>("desiresPhase")
	    .Field<&components::CreatureMindState::paused>("paused")
	    .Field<&components::CreatureMindState::learnt>("learnt")
	    .Field<&components::CreatureMindState::planner>("planner")
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
	    .Field<&components::CreatureObjectAction::reach>("reach")
	    .Field<&components::CreatureObjectAction::maxReach>("maxReach")
	    .Field<&components::CreatureObjectAction::attempts>("attempts")
	    .Field<&components::CreatureObjectAction::flightSeconds>("flightSeconds")
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
	    .Field<&components::CreatureSpells::invisible>("invisible")
	    .Field<&components::CreatureSpells::pausedMind>("pausedMind")
	    .Field<&components::CreatureSpells::smallestSize>("smallestSize")
	    .Field<&components::CreatureSpells::largestSize>("largestSize")
	    .Field<&components::CreatureSpells::cheat>("cheat");
	Reflect<components::DeadTree>(context)
	    .Field<&components::DeadTree::type>("type")
	    .Field<&components::DeadTree::woodMultiplier>("woodMultiplier")
	    .Field<&components::DeadTree::felled>("felled");
	Reflect<components::DestructionGhost>(context)
	    .Field<&components::DestructionGhost::mesh>("mesh")
	    .Field<&components::DestructionGhost::model>("model")
	    .Field<&components::DestructionGhost::millisecondsLeft>("millisecondsLeft")
	    .Field<&components::DestructionGhost::shown>("shown");
	Reflect<components::DropsRoots> {context};
	Reflect<components::FallingRoots>(context)
	    .Field<&components::FallingRoots::seconds>("seconds")
	    .Field<&components::FallingRoots::startHeight>("startHeight")
	    .Field<&components::FallingRoots::restHeight>("restHeight");
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
	Reflect<components::Fixed>(context)
	    .Field<&components::Fixed::boundingCenter>("boundingCenter")
	    .Field<&components::Fixed::boundingRadius>("boundingRadius");
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
	Reflect<components::ForestMember>(context).Field<&components::ForestMember::forest>("forest");
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
	Reflect<components::Indestructible> {context};
	Reflect<components::InfluenceSource>(context)
	    .Field<&components::InfluenceSource::player>("player")
	    .Field<&components::InfluenceSource::radius>("radius");
	Reflect<components::TownInfluence>(context)
	    .Field<&components::TownInfluence::radius>("radius")
	    .Field<&components::TownInfluence::drawnRadius>("drawnRadius");
	Reflect<components::CitadelInfluence>(context)
	    .Field<&components::CitadelInfluence::reach>("reach")
	    .Field<&components::CitadelInfluence::drawnRadius>("drawnRadius");
	Reflect<components::VirtualInfluence>(context)
	    .Field<&components::VirtualInfluence::state>("state")
	    .Field<&components::VirtualInfluence::hum>("hum");
	Reflect<components::LandForest>(context)
	    .Field<&components::LandForest::id>("id")
	    .Field<&components::LandForest::bigForest>("bigForest")
	    .Field<&components::LandForest::scenic>("scenic")
	    .Field<&components::LandForest::made>("made");
	Reflect<components::TownForests>(context).Field<&components::TownForests::forests>("forests");
	Reflect<components::LightBeam>(context).Field<&components::LightBeam::cone>("cone");
	Reflect<components::LivingAction>(context)
	    .Field<&components::LivingAction::states>("states")
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
	    .Field<&components::Player::sacrifices>("sacrifices");
	Reflect<components::PlayerCreatures>(context).Field<&components::PlayerCreatures::acquired>("acquired");
	Reflect<components::Poisoned>(context).Field<&components::Poisoned::dummy>("dummy");
	Reflect<components::CreatureMiracleOpinion>(context)
	    .Field<&components::CreatureMiracleOpinion::lastTurn>("lastTurn")
	    .Field<&components::CreatureMiracleOpinion::changed>("changed");
	Reflect<components::Pot>(context)
	    .Field<&components::Pot::amount>("amount")
	    .Field<&components::Pot::maxAmount>("maxAmount")
	    .Field<&components::Pot::type>("type")
	    .Field<&components::Pot::poisoned>("poisoned");
	Reflect<components::PrayerPower>(context)
	    .Field<&components::PrayerPower::chants>("chants")
	    .Field<&components::PrayerPower::infinite>("infinite");
	Reflect<components::ResourceLastTaken>(context).Field<&components::ResourceLastTaken::turn>("turn");
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
	Reflect<components::InScript> {context};
	Reflect<components::ScriptControlled> {context};
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
	    .Field<&components::SpellCaster::withoutIcon>("withoutIcon");
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
	    .Field<&components::Temple::lastHitTurn>("lastHitTurn")
	    .Field<&components::Temple::beamTarget>("beamTarget")
	    .Field<&components::Temple::beamTurn>("beamTurn")
	    .Field<&components::Temple::beamSource>("beamSource")
	    .Field<&components::Temple::destroying>("destroying")
	    .Field<&components::Temple::destructionClock>("destructionClock")
	    .Field<&components::Temple::destructionGlow>("destructionGlow")
	    .Field<&components::Temple::destructionLoops>("destructionLoops")
	    .Field<&components::Temple::destructionBeamClock>("destructionBeamClock")
	    .Field<&components::Temple::destructionBeamSource>("destructionBeamSource");
	Reflect<components::TempleEntrance>(context).Field<&components::TempleEntrance::temple>("temple");
	Reflect<components::TempleExterior>(context)
	    .Field<&components::TempleExterior::alignment>("alignment")
	    .Field<&components::TempleExterior::alignmentTarget>("alignmentTarget")
	    .Field<&components::TempleExterior::size>("size")
	    .Field<&components::TempleExterior::sizeTarget>("sizeTarget")
	    .Field<&components::TempleExterior::morphed>("morphed");
	Reflect<components::Town>(context)
	    .Field<&components::Town::id>("id")
	    .Field<&components::Town::owner>("owner")
	    .Field<&components::Town::beliefs>("beliefs")
	    .Field<&components::Town::uninhabitable>("uninhabitable")
	    .Field<&components::Town::homelessVillagers>("homelessVillagers")
	    .Field<&components::Town::abodes>("abodes")
	    .Field<&components::Town::gained>("gained")
	    .Field<&components::Town::injured>("injured")
	    .Field<&components::Town::deathsByKiller>("deathsByKiller")
	    .Field<&components::Town::scenicForest>("scenicForest")
	    .Field<&components::Town::scenicForestCentre>("scenicForestCentre")
	    .Field<&components::Town::playthings>("playthings")
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
	    .Field<&components::Tree::turnsToGrowth>("turnsToGrowth");
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
	    .Field<&components::Villager::woken>("woken");
	Reflect<components::VillagerDeath>(context)
	    .Field<&components::VillagerDeath::turnsLeft>("turnsLeft")
	    .Field<&components::VillagerDeath::skeleton>("skeleton")
	    .Field<&components::VillagerDeath::reason>("reason")
	    .Field<&components::VillagerDeath::killer>("killer");
	Reflect<components::VillagerPose>(context)
	    .Field<&components::VillagerPose::clip>("clip")
	    .Field<&components::VillagerPose::place>("place")
	    .Field<&components::VillagerPose::bones>("bones");
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
}
