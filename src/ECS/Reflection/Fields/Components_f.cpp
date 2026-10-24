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

#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FilmCreature.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/FixedOutline.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_f(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_f(entt::meta_ctx& context)
{
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
	    .Field<&components::Field::height>("height")
	    .Field<&components::Field::farmers>("farmers");
	Reflect<components::FilmCreature>(context)
	    .Field<&components::FilmCreature::clock>("clock")
	    .Field<&components::FilmCreature::look>("look")
	    .Field<&components::FilmCreature::pace>("pace")
	    .Field<&components::FilmCreature::shown>("shown")
	    .Field<&components::FilmCreature::stars>("stars")
	    .Field<&components::FilmCreature::starsStarted>("starsStarted")
	    .Field<&components::FilmCreature::starsOn>("starsOn")
	    .Field<&components::FilmCreature::burst>("burst")
	    .Field<&components::FilmCreature::burstOn>("burstOn")
	    .Field<&components::FilmCreature::burstAlpha>("burstAlpha")
	    .Field<&components::FilmCreature::effectsPace>("effectsPace")
	    .Field<&components::FilmCreature::starSprites>("starSprites")
	    .Field<&components::FilmCreature::fans>("fans");
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
	Reflect<components::FixedOutline>(context).Field<&components::FixedOutline::outline>("outline");
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
	    .Field<&components::Flock::town>("town")
	    .Field<&components::Flock::most>("most")
	    .Field<&components::Flock::temple>("temple")
	    .Field<&components::Flock::shepherd>("shepherd")
	    .Field<&components::Flock::savedPlace>("savedPlace")
	    .Field<&components::Flock::keepsTogether>("keepsTogether");
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
	Reflect<components::Footpath>(context).Field<&components::Footpath::nodes>("nodes").Field<&components::Footpath::walkable>(
	    "walkable");
	Reflect<components::FootpathLink>(context)
	    .Field<&components::FootpathLink::position>("position")
	    .Field<&components::FootpathLink::footpaths>("footpaths");
	Reflect<components::FootpathWalk>(context)
	    .Field<&components::FootpathWalk::footpath>("footpath")
	    .Field<&components::FootpathWalk::towardsStart>("towardsStart")
	    .Field<&components::FootpathWalk::node>("node")
	    .Field<&components::FootpathWalk::then>("then");
	Reflect<components::FootpathFinder>(context)
	    .Field<&components::FootpathFinder::from>("from")
	    .Field<&components::FootpathFinder::to>("to")
	    .Field<&components::FootpathFinder::footpath>("footpath")
	    .Field<&components::FootpathFinder::plan>("plan");
	Reflect<components::FootpathObstacleChange>(context)
	    .Field<&components::FootpathObstacleChange::put>("put")
	    .Field<&components::FootpathObstacleChange::radius>("radius")
	    .Field<&components::FootpathObstacleChange::place>("place");
	Reflect<components::BigForest>(context).Field<&components::BigForest::type>("type").Field<&components::BigForest::wood>(
	    "wood");
	Reflect<components::Forest>(context).Field<&components::Forest::type>("type");
	Reflect<components::ForestMember>(context)
	    .Field<&components::ForestMember::forest>("forest")
	    .Field<&components::ForestMember::growing>("growing")
	    .Field<&components::ForestMember::listed>("listed");
}
