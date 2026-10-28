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

#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/ResourceLastTaken.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/RisingSoul.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Enums.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_r(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 8> k_PlayerNamesNames {"PLAYER_ONE",  "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
                                                              "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "NEUTRAL"};
static_assert(static_cast<size_t>(openblack::PlayerNames::_COUNT) == k_PlayerNamesNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Components_r(entt::meta_ctx& context)
{
	Reflect<components::ReportedHand>(context)
	    .Field<&components::ReportedHand::collidePoint>("collidePoint")
	    .Field<&components::ReportedHand::handPoint>("handPoint")
	    .Field<&components::ReportedHand::camera>("camera")
	    .Field<&components::ReportedHand::focus>("focus")
	    .Field<&components::ReportedHand::pickedObject>("pickedObject")
	    .Field<&components::ReportedHand::viewReported>("viewReported")
	    .Field<&components::ReportedHand::viewEye>("viewEye")
	    .Field<&components::ReportedHand::viewFocus>("viewFocus")
	    .Field<&components::ReportedHand::viewLens>("viewLens")
	    .Field<&components::ReportedHand::viewNearClip>("viewNearClip")
	    .Field<&components::ReportedHand::viewScreenWidth>("viewScreenWidth")
	    .Field<&components::ReportedHand::viewScreenHeight>("viewScreenHeight")
	    .Field<&components::ReportedHand::viewAspect>("viewAspect")
	    .Field<&components::ReportedHand::inclusionDistance>("inclusionDistance")
	    .Field<&components::ReportedHand::cameraOrders>("cameraOrders")
	    .Field<&components::ReportedHand::cameraArrived>("cameraArrived")
	    .Field<&components::ReportedHand::gripsLand>("gripsLand")
	    .Field<&components::ReportedHand::otherButton>("otherButton");
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
	    .Field<&components::Reward::drawn>("drawn")
	    .Field<&components::Reward::dustMilliseconds>("dustMilliseconds")
	    .Field<&components::Reward::dust>("dust")
	    .Field<&components::Reward::opened>("opened")
	    .Field<&components::Reward::turnsLeft>("turnsLeft")
	    .Field<&components::Reward::clipPlace>("clipPlace");
	Reflect<components::RewardOnLand> {context};
	Reflect<components::RewardSeed>(context)
	    .Field<&components::RewardSeed::seed>("seed")
	    .Field<&components::RewardSeed::middle>("middle")
	    .Field<&components::RewardSeed::spin>("spin")
	    .Field<&components::RewardSeed::phialFrame>("phialFrame")
	    .Field<&components::RewardSeed::phialPhase>("phialPhase");
	Reflect<components::RisingSoul>(context)
	    .Field<&components::RisingSoul::mesh>("mesh")
	    .Field<&components::RisingSoul::clip>("clip")
	    .Field<&components::RisingSoul::timeMs>("timeMs")
	    .Field<&components::RisingSoul::alpha>("alpha")
	    .Field<&components::RisingSoul::bones>("bones")
	    .Field<&components::RisingSoul::turns>("turns")
	    .Field<&components::RisingSoul::shownOut>("shownOut");
}
