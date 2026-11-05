/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "Debug/TempleVillagersSurvey.h"
#include "Debug/TownMiracles.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "GameProviderShared.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Sound/GameMusic.h"
#include "WorldProviders.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// Miracles, fire and what reacts to them

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> MagicProvider()
{
	auto provider = std::make_unique<FunctionProvider>("magic");
	auto townMiracle =
	    Query("town_miracle",
	          "Debug only: gives a town a miracle or takes it away, through the same steps as the land's scripts. A town given "
	          "one without a worship site can have its site, which goes up for its builders.",
	          {IdParameter("The town (town.list)"),
	           {.name = "magic",
	            .type = "integer|string",
	            .description = "The magic type, by number or by its info name",
	            .required = true},
	           {.name = "enable", .type = "boolean", .description = "Give it (default) or take it away", .required = false},
	           {.name = "with_plain",
	            .type = "boolean",
	            .description = "Give a power-up level's plain miracle too, as a land file line does (default true)",
	            .required = false},
	           {.name = "all_levels",
	            .type = "boolean",
	            .description = "Take the plain miracle and every level, as a creature that steals it does (default false)",
	            .required = false},
	           {.name = "open_site",
	            .type = "boolean",
	            .description = "Let a town without a worship site have one, as the scripts do (default true)",
	            .required = false},
	           {.name = "any_land",
	            .type = "boolean",
	            .description = "Also on the first land, which never has worship sites (default true)",
	            .required = false}});
	townMiracle.writes = true;
	provider->Add(std::move(townMiracle), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr)
		{
			return QueryResult::Error("there are no towns without a land");
		}
		const auto town = EntityParam(context.params);
		if (!town.has_value() || !registry->AllOf<Town>(*town))
		{
			return QueryResult::Error("give a town's id (town.list)");
		}
		const auto& params = context.params;
		std::optional<MagicType> type;
		if (const auto magic = params.find("magic"); magic != params.end())
		{
			if (magic->is_number_integer())
			{
				type = static_cast<MagicType>(magic->get<int>());
			}
			else if (magic->is_string() && Locator::infoConstants::has_value())
			{
				type = magic::FindMagicTypeByName(Locator::infoConstants::value(), magic->get<std::string>());
			}
		}
		if (!type.has_value())
		{
			return QueryResult::Error("give magic as a magic type number or an info name");
		}
		const auto flag = [&params](const char* name, bool fallback) {
			const auto found = params.find(name);
			return found != params.end() && found->is_boolean() ? found->get<bool>() : fallback;
		};
		const auto outcome = debug::town_miracles::Apply({.town = *town,
		                                                  .type = *type,
		                                                  .give = flag("enable", true),
		                                                  .withPlain = flag("with_plain", true),
		                                                  .allLevels = flag("all_levels", false),
		                                                  .openSite = flag("open_site", true),
		                                                  .anyLand = flag("any_land", true)});
		if (!outcome.error.empty())
		{
			return QueryResult::Error(outcome.error);
		}
		Json changed = Json::array();
		for (const auto changedType : outcome.changed)
		{
			changed.push_back(static_cast<int>(changedType));
		}
		Json held = Json::array();
		for (const auto heldType : debug::town_miracles::HeldBy(*town))
		{
			held.push_back(static_cast<int>(heldType));
		}
		return QueryResult::Value(
		    {{"changed", std::move(changed)},
		     {"magic_held", std::move(held)},
		     {"worship_site", Id(outcome.site)},
		     {"site_is_new", outcome.siteIsNew},
		     {"no_site",
		      outcome.noSite.has_value() ? Json(std::string(town_miracle_tool::Describe(*outcome.noSite))) : Json(nullptr)}});
	});
	provider->Add(
	    Query("state", "The hand's miracle, the spells and dispensers there are, and the other miracles' "
	                   "systems: shields, tornadoes, explosions, the camera shakes and the piles of roots"),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json result = {{"spells", magic.GetSpells().size()},
		                   {"dispensers", magic.GetDispensers().size()},
		                   {"hand_busy", magic.IsHandBusy()},
		                   {"held_seed", Id(magic.GetHeldSeed())},
		                   {"last_hand_result", static_cast<int>(magic.GetLastHandResult())},
		                   {"ignoring_influence", magic.IsIgnoringInfluence()}};
		    if (Locator::magicShieldSystem::has_value())
		    {
			    result["shields"] = Locator::magicShieldSystem::value().GetDomes(0.0f).size();
		    }
		    if (Locator::tornadoSystem::has_value())
		    {
			    result["carried_by_tornadoes"] = Locator::tornadoSystem::value().CarriedCount();
		    }
		    if (Locator::explosionSystem::has_value())
		    {
			    result["shaking"] = Locator::explosionSystem::value().IsShaking();
			    result["shakes"] = ShakeItems(Locator::explosionSystem::value().Shakes());
			    result["ghosts"] = Locator::explosionSystem::value().Ghosts().size();
			    // The piles of roots that this computer draws, with the milliseconds that stay for each
			    auto piles = Json::array();
			    for (const auto& pile : Locator::explosionSystem::value().RootsPiles())
			    {
				    piles.push_back({{"position", {pile.position.x, pile.position.y, pile.position.z}},
				                     {"scale", pile.scale},
				                     {"ms_left", pile.millisecondsLeft},
				                     {"alpha", pile.alpha.has_value() ? Json(*pile.alpha) : Json(nullptr)}});
			    }
			    result["roots_piles"] = std::move(piles);
		    }
		    if (Locator::floatingNumberSystem::has_value())
		    {
			    result["floating_numbers"] = Locator::floatingNumberSystem::value().Numbers().size();
		    }
		    result["miracle_fx"] = Locator::miracleFxSystem::has_value();
		    return result;
	    }));
	provider->Add(
	    Query("spells", "The spells cast, nearest first when searched about a point", {}, ResultKind::List),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& spell : magic.GetSpells())
		    {
			    items.push_back({{"id", Id(spell.entity)},
			                     {"type", static_cast<int>(spell.magicType)},
			                     {"player", static_cast<int>(spell.player)},
			                     {"position", Point(spell.position)},
			                     {"age", spell.age},
			                     {"duration", spell.duration},
			                     {"strength", spell.strength},
			                     {"closing", spell.closing}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("dispensers", "The miracle dispensers", {}, ResultKind::List),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& dispenser : magic.GetDispensers())
		    {
			    items.push_back({{"id", Id(dispenser.entity)},
			                     {"type", static_cast<int>(dispenser.magicType)},
			                     {"position", Point(dispenser.position)},
			                     {"has_orb", dispenser.hasOrb},
			                     {"active", dispenser.active},
			                     {"tick", dispenser.tick},
			                     {"period", dispenser.period}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("teleport", "A player's teleport stones",
	          {{.name = "player", .type = "integer", .description = "The player's number, 0 by default", .required = false}},
	          ResultKind::List),
	    Serve<Locator::teleportSystem>(
	        "the teleports", [](const ecs::systems::TeleportSystemInterface& teleports, const QueryContext& context) {
		        const auto* registry = Registry();
		        Json items = Json::array();
		        for (const auto stone : teleports.GetStones(PlayerParam(context.params)))
		        {
			        items.push_back(registry != nullptr ? Listed(*registry, stone) : Json {{"id", ToId(stone)}});
		        }
		        return items;
	        }));
	provider->Add(Query("vortices",
	                    "The vortices between the lands: their kind, state, openness and how far they levelled the ground", {},
	                    ResultKind::List),
	              Serve<Locator::vortexSystem>(
	                  "the vortices", [](const ecs::systems::VortexSystemInterface& vortices, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  if (auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const ecs::components::Vortex>(
			                      [&items, &vortices](entt::entity entity, const ecs::components::Vortex& vortex) {
				                      items.push_back({{"id", Id(entity)},
				                                       {"type", static_cast<int>(vortex.type)},
				                                       {"state", static_cast<int>(vortex.state)},
				                                       {"state_start_turn", vortex.stateStartTurn},
				                                       {"centre", Point(vortex.centre)},
				                                       {"openness", vortices.GetOpenness(entity)},
				                                       {"level_applied", vortex.levelApplied},
				                                       {"before_land_effect", vortex.beforeLandEffect},
				                                       {"after_land_effect", vortex.afterLandEffect},
				                                       {"object_mover_effect", vortex.objectMoverEffect},
				                                       {"light_map_effect", vortex.lightMapEffect}});
			                      });
		                  }
		                  return items;
	                  }));
	provider->Add(Query("reactions", "The reactions going on, nearest first when searched about a point", {}, ResultKind::List),
	              Serve<Locator::reactionSystem>(
	                  "the reactions", [](const ecs::systems::ReactionSystemInterface& reactions, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  for (const auto& reaction : reactions.GetReactions())
		                  {
			                  items.push_back({{"id", reaction.id},
			                                   {"type", static_cast<int>(reaction.source.type)},
			                                   {"initiator", Id(reaction.source.initiator)},
			                                   {"player", static_cast<int>(reaction.source.player)},
			                                   {"position", Point(reaction.source.position)},
			                                   {"age", reaction.age},
			                                   {"reach", reaction.reach}});
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("fires", "The fires burning, nearest first when searched about a point", {}, ResultKind::List),
	    Serve<Locator::fireSystem>("the fires", [](const ecs::systems::FireSystemInterface& fires, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& fire : fires.GetFires())
		    {
			    items.push_back({{"id", Id(fire.object)},
			                     {"position", Point(fire.position)},
			                     {"temperature", fire.temperature},
			                     {"burning", fire.burning},
			                     {"charring", fire.charring},
			                     {"life", fire.life},
			                     {"root", Id(fire.root)},
			                     {"blaze_size", fire.blazeSize},
			                     {"firemen", fire.firemen}});
		    }
		    return items;
	    }));
	return provider;
}

} // namespace openblack::inspector::game_providers
