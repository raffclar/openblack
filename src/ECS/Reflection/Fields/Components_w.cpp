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

#include "ECS/Components/WalkPath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_w(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_w(entt::meta_ctx& context)
{
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
	Reflect<components::WallHugLookahead>(context)
	    .Field<&components::WallHugLookahead::tried>("tried")
	    .Field<&components::WallHugLookahead::spareAsItGoes>("spareAsItGoes")
	    .Field<&components::WallHugLookahead::spareTurnedRound>("spareTurnedRound")
	    .Field<&components::WallHugLookahead::turnedRound>("turnedRound")
	    .Field<&components::WallHugLookahead::landingShutIn>("landingShutIn");
	Reflect<components::TurningWalker>(context)
	    .Field<&components::TurningWalker::turnAngle>("turnAngle")
	    .Field<&components::TurningWalker::speed>("speed");
	Reflect<components::WallHug>(context)
	    .Field<&components::WallHug::goalPlace>("goalPlace")
	    .Field<&components::WallHug::step>("step")
	    .Field<&components::WallHug::stepAltitude>("stepAltitude")
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
	    .Field<&components::Storm::cloudCount>("cloudCount")
	    .Field<&components::Storm::cloudShade>("cloudShade")
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
	Reflect<components::WeatherThing>(context)
	    .Field<&components::WeatherThing::storm>("storm")
	    .Field<&components::WeatherThing::lastStorm>("lastStorm")
	    .Field<&components::WeatherThing::affectedByWind>("affectedByWind");
	Reflect<components::Wonder>(context).Field<&components::Wonder::power>("power");
	Reflect<components::Workshop>(context)
	    .Field<&components::Workshop::makingTurns>("makingTurns")
	    .Field<&components::Workshop::counted>("counted")
	    .Field<&components::Workshop::places>("places")
	    .Field<&components::Workshop::scaffolds>("scaffolds")
	    .Field<&components::Workshop::woodPile>("woodPile")
	    .Field<&components::Workshop::woodWant>("woodWant")
	    .Field<&components::Workshop::flag>("flag");
	Reflect<components::WorkshopFlag>(context)
	    .Field<&components::WorkshopFlag::workshop>("workshop")
	    .Field<&components::WorkshopFlag::level>("level")
	    .Field<&components::WorkshopFlag::speed>("speed")
	    .Field<&components::WorkshopFlag::shown>("shown");
	Reflect<components::WorshipChants>(context)
	    .Field<&components::WorshipChants::battery>("battery")
	    .Field<&components::WorshipChants::available>("available")
	    .Field<&components::WorshipChants::used>("used")
	    .Field<&components::WorshipChants::requested>("requested")
	    .Field<&components::WorshipChants::chantsPerDancer>("chantsPerDancer")
	    .Field<&components::WorshipChants::danceIntensity>("danceIntensity")
	    .Field<&components::WorshipChants::strain>("strain")
	    .Field<&components::WorshipChants::infinite>("infinite")
	    .Field<&components::WorshipChants::freeMaintenance>("freeMaintenance")
	    .Field<&components::WorshipChants::sacrificed>("sacrificed");
	Reflect<components::CitadelWorship>(context)
	    .Field<&components::CitadelWorship::sites>("sites")
	    .Field<&components::CitadelWorship::facing>("facing")
	    .Field<&components::CitadelWorship::cannotMakeSites>("cannotMakeSites")
	    .Field<&components::CitadelWorship::standing>("standing")
	    .Field<&components::CitadelWorship::sitesMade>("sitesMade")
	    .Field<&components::CitadelWorship::strainSound>("strainSound");
	Reflect<components::WorshipSite>(context)
	    .Field<&components::WorshipSite::temple>("temple")
	    .Field<&components::WorshipSite::player>("player")
	    .Field<&components::WorshipSite::tribe>("tribe")
	    .Field<&components::WorshipSite::place>("place")
	    .Field<&components::WorshipSite::made>("made")
	    .Field<&components::WorshipSite::facing>("facing")
	    .Field<&components::WorshipSite::towns>("towns")
	    .Field<&components::WorshipSite::altar>("altar")
	    .Field<&components::WorshipSite::dance>("dance")
	    .Field<&components::WorshipSite::foodPot>("foodPot")
	    .Field<&components::WorshipSite::foodOnTheWay>("foodOnTheWay")
	    .Field<&components::WorshipSite::worshippers>("worshippers")
	    .Field<&components::WorshipSite::buildRequests>("buildRequests")
	    .Field<&components::WorshipSite::needs>("needs")
	    .Field<&components::WorshipSite::needFlags>("needFlags")
	    .Field<&components::WorshipSite::manaPath>("manaPath");
	Reflect<components::WorshipSiteFlag>(context)
	    .Field<&components::WorshipSiteFlag::site>("site")
	    .Field<&components::WorshipSiteFlag::need>("need")
	    .Field<&components::WorshipSiteFlag::level>("level")
	    .Field<&components::WorshipSiteFlag::speed>("speed")
	    .Field<&components::WorshipSiteFlag::shown>("shown");
	Reflect<components::Worshipper>(context)
	    .Field<&components::Worshipper::atWorship>("atWorship")
	    .Field<&components::Worshipper::onTheWay>("onTheWay");
	Reflect<components::WorshipAltar>(context)
	    .Field<&components::WorshipAltar::site>("site")
	    .Field<&components::WorshipAltar::model>("model");
}
