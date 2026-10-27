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

#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/TownCreatureAttitude.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureAttitudeRules.h"
#include "ECS/FixedOutlines.h"
#include "ECS/Heartbeat.h"
#include "ECS/HighDetailRules.h"
#include "ECS/PosedModel.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/RegistryContext.h"
#include "ECS/RewardRules.h"
#include "ECS/TownAggression.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerDrawRules.h"
#include "ECS/VillagerEyes.h"
#include "Enums.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_e(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 8> k_PlayerNamesNames {"PLAYER_ONE",  "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
                                                              "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "NEUTRAL"};
static_assert(static_cast<size_t>(openblack::PlayerNames::_COUNT) == k_PlayerNamesNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Values_e(entt::meta_ctx& context)
{
	Reflect<openblack::ecs::RegistryContext>(context, ValueOnly {})
	    .Field<&openblack::ecs::RegistryContext::scriptFootpaths>("scriptFootpaths")
	    .Field<&openblack::ecs::RegistryContext::scriptThingMadeLast>("scriptThingMadeLast")
	    .Field<&openblack::ecs::RegistryContext::footpathObstacleChanges>("footpathObstacleChanges")
	    .Field<&openblack::ecs::RegistryContext::footpathObstacles>("footpathObstacles")
	    .Field<&openblack::ecs::RegistryContext::streams>("streams")
	    .Field<&openblack::ecs::RegistryContext::towns>("towns")
	    .Field<&openblack::ecs::RegistryContext::villageLightFlames>("villageLightFlames")
	    .Field<&openblack::ecs::RegistryContext::mapScriptGlobals>("mapScriptGlobals")
	    .Field<&openblack::ecs::RegistryContext::nextHandRockTap>("nextHandRockTap")
	    .Field<&openblack::ecs::RegistryContext::nextCreatureRockSmash>("nextCreatureRockSmash")
	    .Field<&openblack::ecs::RegistryContext::scriptLetsTempleBeEntered>("scriptLetsTempleBeEntered")
	    .Field<&openblack::ecs::RegistryContext::templeEntrancesTakeCursor>("templeEntrancesTakeCursor", {k_PlayerNamesNames})
	    .Field<&openblack::ecs::RegistryContext::gameOver>("gameOver")
	    .Field<&openblack::ecs::RegistryContext::skirmish>("skirmish")
	    .Field<&openblack::ecs::RegistryContext::slotsWaitForHumans>("slotsWaitForHumans")
	    .Field<&openblack::ecs::RegistryContext::gameEnd>("gameEnd")
	    .Field<&openblack::ecs::RegistryContext::partnersAlwaysAgree>("partnersAlwaysAgree")
	    .Field<&openblack::ecs::RegistryContext::nextHeartBeamSound>("nextHeartBeamSound")
	    .Field<&openblack::ecs::RegistryContext::nextDamageBeamSound>("nextDamageBeamSound")
	    .Field<&openblack::ecs::RegistryContext::creaturesMade>("creaturesMade")
	    .Field<&openblack::ecs::RegistryContext::nextTownGained>("nextTownGained")
	    .Field<&openblack::ecs::RegistryContext::peopleCountedInTurn>("peopleCountedInTurn")
	    .Field<&openblack::ecs::RegistryContext::landAvoid>("landAvoid")
	    .Field<&openblack::ecs::RegistryContext::creatureBoneReach>("creatureBoneReach")
	    .Field<&openblack::ecs::RegistryContext::namedVillagersUsed>("namedVillagersUsed")
	    .Field<&openblack::ecs::RegistryContext::thingsMade>("thingsMade")
	    .Field<&openblack::ecs::RegistryContext::livingMade>("livingMade")
	    .Field<&openblack::ecs::RegistryContext::listedMade>("listedMade")
	    .Field<&openblack::ecs::RegistryContext::villagerNamesShown>("villagerNamesShown")
	    .Field<&openblack::ecs::RegistryContext::villagerDetailsShown>("villagerDetailsShown")
	    .Field<&openblack::ecs::RegistryContext::impressiveMiracleCastTurn>("impressiveMiracleCastTurn")
	    .Field<&openblack::ecs::RegistryContext::explorationMap>("explorationMap")
	    .Field<&openblack::ecs::RegistryContext::globalStatistics>("globalStatistics");
	Reflect<openblack::ecs::components::Animal::DrawnPlace>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Animal::DrawnPlace::position>("position")
	    .Field<&openblack::ecs::components::Animal::DrawnPlace::rotation>("rotation");
	Reflect<openblack::ecs::components::CarriedByTornado::Drawn>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CarriedByTornado::Drawn::position>("position")
	    .Field<&openblack::ecs::components::CarriedByTornado::Drawn::rotation>("rotation");
	Reflect<openblack::ecs::components::ChimneySmoke::Puff>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::position>("position")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::velocity>("velocity")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::age>("age")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::angle>("angle")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::clockwise>("clockwise")
	    .Field<&openblack::ecs::components::ChimneySmoke::Puff::hidden>("hidden");
	Reflect<openblack::ecs::components::CreatureAnimation::DrawnPlace>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureAnimation::DrawnPlace::position>("position")
	    .Field<&openblack::ecs::components::CreatureAnimation::DrawnPlace::rotation>("rotation")
	    .Field<&openblack::ecs::components::CreatureAnimation::DrawnPlace::slots>("slots");
	Reflect<openblack::ecs::components::CreatureAnimation::PairBlend>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureAnimation::PairBlend::from>("from")
	    .Field<&openblack::ecs::components::CreatureAnimation::PairBlend::to>("to")
	    .Field<&openblack::ecs::components::CreatureAnimation::PairBlend::weight>("weight")
	    .Field<&openblack::ecs::components::CreatureAnimation::PairBlend::animation>("animation");
	Reflect<openblack::ecs::components::CreatureAnimation::Slot>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::animation>("animation")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::timeMs>("timeMs")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::weight>("weight")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::mirrored>("mirrored")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::blendedWith>("blendedWith")
	    .Field<&openblack::ecs::components::CreatureAnimation::Slot::blendWeight>("blendWeight");
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
	Reflect<openblack::ecs::components::CreatureDance::Place>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureDance::Place::dancer>("dancer")
	    .Field<&openblack::ecs::components::CreatureDance::Place::position>("position");
	Reflect<openblack::ecs::components::CreatureDrips::Drip>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureDrips::Drip::subMesh>("subMesh")
	    .Field<&openblack::ecs::components::CreatureDrips::Drip::firstIndex>("firstIndex")
	    .Field<&openblack::ecs::components::CreatureDrips::Drip::s>("s")
	    .Field<&openblack::ecs::components::CreatureDrips::Drip::t>("t")
	    .Field<&openblack::ecs::components::CreatureDrips::Drip::ms>("ms");
	Reflect<openblack::ecs::components::CreatureEyes::Eye>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureEyes::Eye::eyeball>("eyeball")
	    .Field<&openblack::ecs::components::CreatureEyes::Eye::eyelid>("eyelid");
	Reflect<openblack::ecs::components::CreatureHair::Group>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureHair::Group::strands>("strands")
	    .Field<&openblack::ecs::components::CreatureHair::Group::colour>("colour")
	    .Field<&openblack::ecs::components::CreatureHair::Group::halfWidth>("halfWidth")
	    .Field<&openblack::ecs::components::CreatureHair::Group::textured>("textured");
	Reflect<openblack::ecs::components::CreatureHandGlows::Placed>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureHandGlows::Placed::model>("model")
	    .Field<&openblack::ecs::components::CreatureHandGlows::Placed::zoom>("zoom")
	    .Field<&openblack::ecs::components::CreatureHandGlows::Placed::slide>("slide");
	Reflect<openblack::ecs::components::CreatureLeash::Order>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLeash::Order::serial>("serial")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::fight>("fight")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::object>("object")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::point>("point")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::ring>("ring")
	    .Field<&openblack::ecs::components::CreatureLeash::Order::footprint>("footprint");
	Reflect<openblack::ecs::components::CreatureLeash::Worn>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::type>("type")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::holder>("holder")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::works>("works")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::tiedTo>("tiedTo")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::tiedTurn>("tiedTurn")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::slackLength>("slackLength")
	    .Field<&openblack::ecs::components::CreatureLeash::Worn::maxLength>("maxLength");
	Reflect<openblack::ecs::components::CreatureLeashRope::HeldRopes>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLeashRope::HeldRopes::toCollar>("toCollar")
	    .Field<&openblack::ecs::components::CreatureLeashRope::HeldRopes::toTied>("toTied");
	Reflect<openblack::ecs::components::CreatureLocomotion::GaveUp>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLocomotion::GaveUp::destination>("destination")
	    .Field<&openblack::ecs::components::CreatureLocomotion::GaveUp::met>("met");
	Reflect<openblack::ecs::components::CreatureLocomotion::Move>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::animations>("animations")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::timeMs>("timeMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::durationMs>("durationMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::displacement>("displacement")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::blendDurationMs>("blendDurationMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::side>("side")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Move::routeHeading>("routeHeading");
	Reflect<openblack::ecs::components::CreatureLocomotion::RoutePoint>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLocomotion::RoutePoint::point>("point")
	    .Field<&openblack::ecs::components::CreatureLocomotion::RoutePoint::timeMs>("timeMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::RoutePoint::ready>("ready")
	    .Field<&openblack::ecs::components::CreatureLocomotion::RoutePoint::mirrored>("mirrored")
	    .Field<&openblack::ecs::components::CreatureLocomotion::RoutePoint::ending>("ending");
	Reflect<openblack::ecs::components::CreatureLocomotion::Track>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::animation>("animation")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::fromMs>("fromMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::advanceMs>("advanceMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::durationMs>("durationMs")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::weight>("weight")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::looping>("looping")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::breathing>("breathing")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::blendedWith>("blendedWith")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::blendWeight>("blendWeight")
	    .Field<&openblack::ecs::components::CreatureLocomotion::Track::mirrored>("mirrored");
	Reflect<openblack::ecs::components::CreatureMindState::ActionUnderway>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureMindState::ActionUnderway::serial>("serial")
	    .Field<&openblack::ecs::components::CreatureMindState::ActionUnderway::action>("action");
	Reflect<openblack::ecs::components::CreatureMindState::Feedback>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureMindState::Feedback::value>("value")
	    .Field<&openblack::ecs::components::CreatureMindState::Feedback::activity>("activity");
	Reflect<openblack::ecs::components::CreatureMindState::LeashCast>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::CreatureMindState::LeashCast::thing>("thing")
	    .Field<&openblack::ecs::components::CreatureMindState::LeashCast::miracle>("miracle");
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
	Reflect<openblack::ecs::components::DrawnIntoVortex::Flight>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::DrawnIntoVortex::Flight::velocity>("velocity")
	    .Field<&openblack::ecs::components::DrawnIntoVortex::Flight::position>("position")
	    .Field<&openblack::ecs::components::DrawnIntoVortex::Flight::rotation>("rotation");
	Reflect<openblack::ecs::components::Footpath::Node>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Footpath::Node::position>("position")
	    .Field<&openblack::ecs::components::Footpath::Node::place>("place")
	    .Field<&openblack::ecs::components::Footpath::Node::planned>("planned")
	    .Field<&openblack::ecs::components::Footpath::Node::hidden>("hidden")
	    .Field<&openblack::ecs::components::Footpath::Node::marker>("marker");
	Reflect<openblack::ecs::components::FootpathFinder::Plan>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::FootpathFinder::Plan::obstacles>("obstacles")
	    .Field<&openblack::ecs::components::FootpathFinder::Plan::search>("search")
	    .Field<&openblack::ecs::components::FootpathFinder::Plan::start>("start")
	    .Field<&openblack::ecs::components::FootpathFinder::Plan::goal>("goal")
	    .Field<&openblack::ecs::components::FootpathFinder::Plan::live>("live");
	Reflect<openblack::ecs::components::GuidanceHeard::SeenThing>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::GuidanceHeard::SeenThing::thing>("thing")
	    .Field<&openblack::ecs::components::GuidanceHeard::SeenThing::since>("since");
	Reflect<openblack::ecs::components::HandGrab::Throw>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::HandGrab::Throw::velocity>("velocity")
	    .Field<&openblack::ecs::components::HandGrab::Throw::springVelocity>("springVelocity")
	    .Field<&openblack::ecs::components::HandGrab::Throw::from>("from");
	Reflect<openblack::ecs::components::HandMorph::Skin>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::HandMorph::Skin::id>("id")
	    .Field<&openblack::ecs::components::HandMorph::Skin::texels>("texels");
	Reflect<openblack::ecs::components::HeldByCreature::Drawn>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::HeldByCreature::Drawn::position>("position")
	    .Field<&openblack::ecs::components::HeldByCreature::Drawn::rotation>("rotation");
	Reflect<openblack::ecs::components::InHand::Drawn>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::InHand::Drawn::position>("position")
	    .Field<&openblack::ecs::components::InHand::Drawn::rotation>("rotation");
	Reflect<openblack::ecs::components::MapScriptGlobals>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::MapScriptGlobals::landNumber>("landNumber")
	    .Field<&openblack::ecs::components::MapScriptGlobals::townInfluenceMultiplier>("townInfluenceMultiplier")
	    .Field<&openblack::ecs::components::MapScriptGlobals::playerInfluenceMultiplier>("playerInfluenceMultiplier")
	    .Field<&openblack::ecs::components::MapScriptGlobals::landBalance>("landBalance")
	    .Field<&openblack::ecs::components::MapScriptGlobals::lostTownScale>("lostTownScale")
	    .Field<&openblack::ecs::components::MapScriptGlobals::lastObject>("lastObject");
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
	Reflect<openblack::ecs::components::Shark::DrawnPlace>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Shark::DrawnPlace::position>("position")
	    .Field<&openblack::ecs::components::Shark::DrawnPlace::rotation>("rotation");
	Reflect<openblack::ecs::components::TeleportStone::Traveller>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::TeleportStone::Traveller::living>("living")
	    .Field<&openblack::ecs::components::TeleportStone::Traveller::destination>("destination")
	    .Field<&openblack::ecs::components::TeleportStone::Traveller::finalState>("finalState");
	Reflect<openblack::ecs::components::TownCreatureAttitude::Watched>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::TownCreatureAttitude::Watched::creature>("creature")
	    .Field<&openblack::ecs::components::TownCreatureAttitude::Watched::feeling>("feeling");
	Reflect<openblack::ecs::components::VillageLightFlames>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::VillageLightFlames::clock>("clock")
	    .Field<&openblack::ecs::components::VillageLightFlames::starts>("starts");
	Reflect<openblack::ecs::components::Vortex::Arrival>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Vortex::Arrival::town>("town")
	    .Field<&openblack::ecs::components::Vortex::Arrival::gatheringPlace>("gatheringPlace")
	    .Field<&openblack::ecs::components::Vortex::Arrival::flockDistance>("flockDistance")
	    .Field<&openblack::ecs::components::Vortex::Arrival::flockRadius>("flockRadius")
	    .Field<&openblack::ecs::components::Vortex::Arrival::flock>("flock");
	Reflect<openblack::ecs::components::Vortex::ThrowingOut>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::Vortex::ThrowingOut::lines>("lines")
	    .Field<&openblack::ecs::components::Vortex::ThrowingOut::nextLine>("nextLine")
	    .Field<&openblack::ecs::components::Vortex::ThrowingOut::reading>("reading")
	    .Field<&openblack::ecs::components::Vortex::ThrowingOut::villagersOut>("villagersOut")
	    .Field<&openblack::ecs::components::Vortex::ThrowingOut::thrown>("thrown")
	    .Field<&openblack::ecs::components::Vortex::ThrowingOut::animalFlock>("animalFlock");
	Reflect<openblack::ecs::components::WorshipSite::BuildRequest>(context, ValueOnly {})
	    .Field<&openblack::ecs::components::WorshipSite::BuildRequest::town>("town")
	    .Field<&openblack::ecs::components::WorshipSite::BuildRequest::desireBoost>("desireBoost");
	Reflect<openblack::ecs::creature_attitude::Feeling>(context, ValueOnly {})
	    .Field<&openblack::ecs::creature_attitude::Feeling::attitude>("attitude")
	    .Field<&openblack::ecs::creature_attitude::Feeling::shownTurn>("shownTurn")
	    .Field<&openblack::ecs::creature_attitude::Feeling::lapsedTurn>("lapsedTurn");
	Reflect<openblack::ecs::fixed_outlines::Circle>(context, ValueOnly {})
	    .Field<&openblack::ecs::fixed_outlines::Circle::centre>("centre")
	    .Field<&openblack::ecs::fixed_outlines::Circle::radius>("radius")
	    .Field<&openblack::ecs::fixed_outlines::Circle::radiusSquared>("radiusSquared");
	Reflect<openblack::ecs::fixed_outlines::Outline>(context, ValueOnly {})
	    .Field<&openblack::ecs::fixed_outlines::Outline::bounds>("bounds")
	    .Field<&openblack::ecs::fixed_outlines::Outline::parts>("parts");
	Reflect<openblack::ecs::heartbeat::State>(context, ValueOnly {})
	    .Field<&openblack::ecs::heartbeat::State::stress>("stress")
	    .Field<&openblack::ecs::heartbeat::State::rate>("rate")
	    .Field<&openblack::ecs::heartbeat::State::phase>("phase")
	    .Field<&openblack::ecs::heartbeat::State::pulse>("pulse")
	    .Field<&openblack::ecs::heartbeat::State::previousPulse>("previousPulse")
	    .Field<&openblack::ecs::heartbeat::State::believerShare>("believerShare")
	    .Field<&openblack::ecs::heartbeat::State::influenceShare>("influenceShare")
	    .Field<&openblack::ecs::heartbeat::State::destructionBeats>("destructionBeats");
	Reflect<openblack::ecs::high_detail_rules::DrawOrders>(context, ValueOnly {})
	    .Field<&openblack::ecs::high_detail_rules::DrawOrders::followIntroHand>("followIntroHand")
	    .Field<&openblack::ecs::high_detail_rules::DrawOrders::turnAtOnce>("turnAtOnce");
	Reflect<openblack::ecs::posed_model::SkinPoint>(context, ValueOnly {})
	    .Field<&openblack::ecs::posed_model::SkinPoint::subMesh>("subMesh")
	    .Field<&openblack::ecs::posed_model::SkinPoint::firstIndex>("firstIndex")
	    .Field<&openblack::ecs::posed_model::SkinPoint::s>("s")
	    .Field<&openblack::ecs::posed_model::SkinPoint::t>("t");
	Reflect<openblack::ecs::reward::DustSprite>(context, ValueOnly {})
	    .Field<&openblack::ecs::reward::DustSprite::offset>("offset")
	    .Field<&openblack::ecs::reward::DustSprite::frame>("frame")
	    .Field<&openblack::ecs::reward::DustSprite::rgb>("rgb");
	Reflect<openblack::ecs::town_aggression::Harm>(context, ValueOnly {})
	    .Field<&openblack::ecs::town_aggression::Harm::burn>("burn")
	    .Field<&openblack::ecs::town_aggression::Harm::crush>("crush")
	    .Field<&openblack::ecs::town_aggression::Harm::hit>("hit");
	Reflect<openblack::ecs::town_aggression::Record>(context, ValueOnly {})
	    .Field<&openblack::ecs::town_aggression::Record::aggression>("aggression", {k_PlayerNamesNames})
	    .Field<&openblack::ecs::town_aggression::Record::lastTurns>("lastTurns", {k_PlayerNamesNames})
	    .Field<&openblack::ecs::town_aggression::Record::lastAggressor>("lastAggressor")
	    .Field<&openblack::ecs::town_aggression::Record::lastTurn>("lastTurn")
	    .Field<&openblack::ecs::town_aggression::Record::protectionMultiplier>("protectionMultiplier")
	    .Field<&openblack::ecs::town_aggression::Record::mercyMultiplier>("mercyMultiplier")
	    .Field<&openblack::ecs::town_aggression::Record::protection>("protection")
	    .Field<&openblack::ecs::town_aggression::Record::mercy>("mercy")
	    .Field<&openblack::ecs::town_aggression::Record::lastHarms>("lastHarms", {k_PlayerNamesNames});
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
	    .Field<&openblack::ecs::village_totem::Ease::moving>("moving")
	    .Field<&openblack::ecs::village_totem::Ease::clockMs>("clockMs");
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
}
