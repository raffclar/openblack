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

#include "ECS/Components/TattooMenuCreature.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownCreatureAttitude.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Translucent.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Enums.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_t(entt::meta_ctx& context);
}

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

} // namespace

void openblack::ecs::reflection::RegisterFields_Components_t(entt::meta_ctx& context)
{
	Reflect<components::TattooMenuCreature>(context)
	    .Field<&components::TattooMenuCreature::state>("state")
	    .Field<&components::TattooMenuCreature::tattooSites>("tattooSites");
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
	    .Field<&components::TeleportTraveller::jumped>("jumped")
	    .Field<&components::TeleportTraveller::arrival>("arrival")
	    .Field<&components::TeleportTraveller::arrivalOnMap>("arrivalOnMap")
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
	Reflect<components::TempleHeart>(context).Field<&components::TempleHeart::temple>("temple");
	Reflect<components::TempleLantern>(context)
	    .Field<&components::TempleLantern::temple>("temple")
	    .Field<&components::TempleLantern::light>("light");
	Reflect<components::TempleExterior>(context)
	    .Field<&components::TempleExterior::alignment>("alignment")
	    .Field<&components::TempleExterior::alignmentTarget>("alignmentTarget")
	    .Field<&components::TempleExterior::size>("size")
	    .Field<&components::TempleExterior::sizeTarget>("sizeTarget")
	    .Field<&components::TempleExterior::morphed>("morphed")
	    .Field<&components::TempleExterior::drawnBuilt>("drawnBuilt")
	    .Field<&components::TempleExterior::lanternsMade>("lanternsMade");
	Reflect<components::TownArea>(context).Field<&components::TownArea::min>("min").Field<&components::TownArea::max>("max");
	Reflect<components::Town>(context)
	    .Field<&components::Town::id>("id")
	    .Field<&components::Town::owner>("owner")
	    .Field<&components::Town::uninhabitable>("uninhabitable")
	    .Field<&components::Town::homelessVillagers>("homelessVillagers")
	    .Field<&components::Town::abodes>("abodes")
	    .Field<&components::Town::gained>("gained")
	    .Field<&components::Town::made>("made")
	    .Field<&components::Town::countedAsNew>("countedAsNew")
	    .Field<&components::Town::injured>("injured")
	    .Field<&components::Town::deathsByKiller>("deathsByKiller", {std::span<const std::string_view> {}, k_DeathReasonNames})
	    .Field<&components::Town::scenicForest>("scenicForest")
	    .Field<&components::Town::scenicForestCentre>("scenicForestCentre")
	    .Field<&components::Town::playthings>("playthings")
	    .Field<&components::Town::worshipShare>("worshipShare")
	    .Field<&components::Town::worshipSite>("worshipSite")
	    .Field<&components::Town::cannotHaveWorshipSite>("cannotHaveWorshipSite")
	    .Field<&components::Town::worshippers>("worshippers")
	    .Field<&components::Town::onWayToWorship>("onWayToWorship")
	    .Field<&components::Town::congregationPos>("congregationPos")
	    .Field<&components::Town::emergencyTurn>("emergencyTurn")
	    .Field<&components::Town::worshipShareBeforeEmergency>("worshipShareBeforeEmergency")
	    .Field<&components::Town::graveyard>("graveyard")
	    .Field<&components::Town::creche>("creche")
	    .Field<&components::Town::desiresChanged>("desiresChanged")
	    .Field<&components::Town::desiresChangedLastTurn>("desiresChangedLastTurn")
	    .Field<&components::Town::planned>("planned")
	    .Field<&components::Town::sitesOpened>("sitesOpened")
	    .Field<&components::Town::askedToBuildTurn>("askedToBuildTurn")
	    .Field<&components::Town::emptyTownTimer>("emptyTownTimer")
	    .Field<&components::Town::temporaryStores>("temporaryStores")
	    .Field<&components::Town::area>("area");
	Reflect<components::TownAggression>(context).Field<&components::TownAggression::record>("record");
	Reflect<components::TownArtefact>(context)
	    .Field<&components::TownArtefact::town>("town")
	    .Field<&components::TownArtefact::site>("site")
	    .Field<&components::TownArtefact::inTown>("inTown")
	    .Field<&components::TownArtefact::inSite>("inSite")
	    .Field<&components::TownArtefact::joined>("joined")
	    .Field<&components::TownArtefact::player>("player")
	    .Field<&components::TownArtefact::value>("value")
	    .Field<&components::TownArtefact::dance>("dance")
	    .Field<&components::TownArtefact::dancing>("dancing")
	    .Field<&components::TownArtefact::danceStarted>("danceStarted")
	    .Field<&components::TownArtefact::effect>("effect")
	    .Field<&components::TownArtefact::symbol>("symbol")
	    .Field<&components::TownArtefact::symbolMilliseconds>("symbolMilliseconds");
	Reflect<components::TownArtefactGifts>(context).Field<&components::TownArtefactGifts::lastGift>("lastGift",
	                                                                                                {k_PlayerNamesNames});
	Reflect<components::TownCreatureAttitude>(context).Field<&components::TownCreatureAttitude::creatures>("creatures");
	Reflect<components::TownDeaths>(context).Field<&components::TownDeaths::counts>("counts");
	Reflect<components::DesireSort>(context)
	    .Field<&components::DesireSort::boosts>("boosts")
	    .Field<&components::DesireSort::value>("value")
	    .Field<&components::DesireSort::index>("index");
	Reflect<components::TownResourceTally>(context)
	    .Field<&components::TownResourceTally::foodCarried>("foodCarried")
	    .Field<&components::TownResourceTally::woodCarried>("woodCarried")
	    .Field<&components::TownResourceTally::woodAtSites>("woodAtSites")
	    .Field<&components::TownResourceTally::woodUsed>("woodUsed")
	    .Field<&components::TownResourceTally::foodUsed>("foodUsed");
	Reflect<components::TownStats>(context)
	    .Field<&components::TownStats::adults>("adults")
	    .Field<&components::TownStats::children>("children")
	    .Field<&components::TownStats::men>("men")
	    .Field<&components::TownStats::women>("women")
	    .Field<&components::TownStats::abodesWithPlaces>("abodesWithPlaces")
	    .Field<&components::TownStats::adultPlaces>("adultPlaces")
	    .Field<&components::TownStats::childPlaces>("childPlaces")
	    .Field<&components::TownStats::totalPlaces>("totalPlaces")
	    .Field<&components::TownStats::adultRoom>("adultRoom")
	    .Field<&components::TownStats::childRoom>("childRoom")
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
	    .Field<&components::TownDesire::doingNowCountAtStart>("doingNowCountAtStart")
	    .Field<&components::TownDesire::turnsOverAlignmentLevel>("turnsOverAlignmentLevel");
	Reflect<components::TownDesireFlag>(context)
	    .Field<&components::TownDesireFlag::town>("town")
	    .Field<&components::TownDesireFlag::desire>("desire")
	    .Field<&components::TownDesireFlag::point>("point")
	    .Field<&components::TownDesireFlag::store>("store")
	    .Field<&components::TownDesireFlag::place>("place")
	    .Field<&components::TownDesireFlag::madeAt>("madeAt")
	    .Field<&components::TownDesireFlag::overGround>("overGround")
	    .Field<&components::TownDesireFlag::level>("level")
	    .Field<&components::TownDesireFlag::speed>("speed")
	    .Field<&components::TownDesireFlag::shown>("shown")
	    .Field<&components::TownDesireFlag::drawnPosition>("drawnPosition")
	    .Field<&components::TownDesireFlag::drawnRotation>("drawnRotation")
	    .Field<&components::TownDesireFlag::drawnScale>("drawnScale");
	Reflect<components::TownDesireFlags>(context).Field<&components::TownDesireFlags::flags>("flags");
	Reflect<components::Transform>(context)
	    .Field<&components::Transform::position>("position")
	    .Field<&components::Transform::rotation>("rotation")
	    .Field<&components::Transform::scale>("scale");
	Reflect<components::Translucent>(context).Field<&components::Translucent::share>("share");
	Reflect<components::Tree>(context)
	    .Field<&components::Tree::type>("type")
	    .Field<&components::Tree::maxSize>("maxSize")
	    .Field<&components::Tree::growthCountdown>("growthCountdown")
	    .Field<&components::Tree::madeToGrow>("madeToGrow")
	    .Field<&components::Tree::nonScenic>("nonScenic")
	    .Field<&components::Tree::bendSlot>("bendSlot");
	Reflect<components::TurnAngles>(context)
	    .Field<&components::TurnAngles::angles>("angles")
	    .Field<&components::TurnAngles::turn>("turn");
}
