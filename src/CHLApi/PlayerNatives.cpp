/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/ForcedTurnPoint.h"
#include "ECS/GamePlayers.h"
#include "Sound/GameMusic.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"
#include "Creature/LeashRules.h"
#include "ECS/AllyInfluence.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/DiscipleRules.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainerWalk.h"
#include "ECS/ScriptCreatureCounts.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownDesire.h"
#include "ECS/TownPlanning.h"
#include "ECS/VillagerAfterDeath.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "Hand/HandGrabRules.h"
#include "Help/HelpProfile.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptInfluence.h"
#include "ScriptHeaders/ScriptPlayers.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "ScriptHeaders/ScriptTownHealth.h"
#include "Temple/TempleEntrancePoint.h"

namespace openblack::chlapi
{

using openblack::Locator;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;

/// Whether a player that a script names is a player of this game. The players of a land are the 8 of the game. A
/// network game with more gods has the gods after the seventh too: the script numbers 9 to 33 name them (deviation 165).
[[nodiscard]] static bool ScriptNamesPlayer(PlayerNames player)
{
	return static_cast<size_t>(player) < openblack::ecs::game_players::Count();
}

/// The player that these natives name by its place in the game's player list, from 0. None past the last player.
std::optional<PlayerNames> ScriptPlayerByIndex(uint32_t index)
{
	if (index >= ecs::game_players::Count())
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(index);
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	const auto player = ScriptPlayerByIndex(Pop().uintVal);
	// The game has no player past the last one and stops; here the script gets a neutral alignment
	if (!player.has_value() || !Locator::alignmentSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(Locator::alignmentSystem::value().GetPlayerAlignment(*player));
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	const auto player = ScriptPlayerByIndex(Pop().uintVal);
	const auto change = Pop().floatVal;
	// In spite of its name, the native adds the amount to the alignment at once
	if (!ecs::systems::alignment::ScriptChangeInRange(change))
	{
		ScriptMessage("Alignment out of range");
		return;
	}
	// The game has no player past the last one and stops; here nothing changes
	if (!player.has_value() || !Locator::alignmentSystem::has_value())
	{
		return;
	}
	Locator::alignmentSystem::value().AddPlayerAlignment(*player, change);
}

/// An influence a script makes for a player, at a place or going about with an object: within it the player has
/// influence, or none at all for an anti-influence. It takes its place in the scripts' table as made by a script; none for
/// a player there isn't.
static entt::entity CreateScriptInfluence(uint32_t player, float radius, bool anti, glm::vec3 position, entt::entity follows)
{
	if (player >= ecs::game_players::Count())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not make influence ring!");
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto ring = registry.Create();
	registry.Assign<Transform>(ring, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::InfluenceSource>(ring, static_cast<PlayerNames>(player), radius, anti, follows);
	RegisterCreated(ring);
	return ring;
}

void InfluenceObject() // 060 INFLUENCE_OBJECT
{
	const auto anti = Pop().intVal != 0;
	// The game's own player number, not the scripts'
	const auto player = Pop().uintVal;
	const auto radius = Popf();
	const auto target = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = target != entt::null && registry.Valid(target) ? registry.TryGet<Transform>(target) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "thing not valid");
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not make influence ring!");
		PushObject(entt::null);
		return;
	}
	PushObject(CreateScriptInfluence(player, radius, anti, transform->position, target));
}

void InfluencePosition() // 061 INFLUENCE_POSITION
{
	const auto anti = Pop().intVal != 0;
	const auto player = Pop().uintVal;
	const auto radius = Popf();
	const auto position = PopVec();
	PushObject(CreateScriptInfluence(player, radius, anti, position, entt::null));
}

void GetInfluence() // 062 GET_INFLUENCE
{
	const auto position = PopVec();
	const auto raw = Pop().intVal != 0;
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// A player who isn't in the game has no influence anywhere
	std::optional<float> influence;
	if (ScriptNamesPlayer(player) && Locator::playerSystem::has_value() && Locator::influenceSystem::has_value() &&
	    Locator::entitiesRegistry::value().Valid(Locator::playerSystem::value().FindPlayer(player).value_or(entt::null)))
	{
		influence =
		    Locator::influenceSystem::value().PlayerOwnInfluence(player, map_coords::FromMetres({position.x, position.z}));
	}
	// Where the player has none, the allies that lend their influence to the player lend it
	std::vector<float> lent;
	if (influence.has_value())
	{
		lent = ecs::ally_influence::LentInfluence(player, map_coords::FromMetres({position.x, position.z}));
	}
	Pushf(script::influence::Answer(influence, raw, lent));
}

void BuildBuilding() // 130 BUILD_BUILDING
{
	const auto desire = Popf();
	const auto position = PopVec();
	// Every town starts what it planned there
	if (ecs::town_planning::StartPlannedAt(position, desire).empty())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "BUILD_BUILDING: no planned building at ({}, {})", position.x,
		                    position.z);
	}
}

/// The thing a resource native is given, if it is a thing of the world that can hold resources: an error is told
/// for none or for a container
std::optional<entt::entity> ResourceThing(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("No thing for resource");
		return std::nullopt;
	}
	if (!registry.AllOf<ecs::components::Transform>(object) || IsScriptContainer(registry, object))
	{
		ScriptMessage("Not object for resource");
		return std::nullopt;
	}
	return object;
}

/// A script's quantity, a number made whole towards none and taken as unsigned
uint32_t ResourceQuantity(float quantity)
{
	return static_cast<uint32_t>(static_cast<int32_t>(quantity));
}

void GetResource() // 133 GET_RESOURCE
{
	const auto object = PopObject();
	const auto type = static_cast<ResourceType>(Pop().intVal);
	const auto thing = ResourceThing(object);
	if (!thing.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(Locator::resourceStoreSystem::value().GetResource(*thing, type)));
}

void AddResource() // 134 ADD_RESOURCE
{
	const auto object = PopObject();
	const auto quantity = ResourceQuantity(Popf());
	const auto type = static_cast<ResourceType>(Pop().intVal);
	const auto thing = ResourceThing(object);
	if (!thing.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(Locator::resourceStoreSystem::value().AddResource(*thing, type, quantity, std::nullopt)));
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	const auto object = PopObject();
	const auto quantity = ResourceQuantity(Popf());
	const auto type = static_cast<ResourceType>(Pop().intVal);
	const auto thing = ResourceThing(object);
	if (!thing.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(Locator::resourceStoreSystem::value().RemoveResource(*thing, type, quantity)));
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	// The player's nearest town to the place within the radius, by the game's map distance. The towns go in the order
	// of the player's own list, and the last of equals wins.
	const auto radius = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	// The game has no player for a number past its players and stops. This search then finds no town.
	if (ScriptNamesPlayer(player))
	{
		const auto from = openblack::map_coords::FromMetres({position.x, position.z});
		std::vector<openblack::ecs::player_towns::Distance> towns;
		for (const auto town : openblack::ecs::player_towns::InOrder(registry, player))
		{
			if (const auto* transform = registry.TryGet<const Transform>(town))
			{
				const auto at = openblack::map_coords::FromMetres({transform->position.x, transform->position.z});
				towns.push_back({.town = town, .metres = openblack::gutils::GetDistanceInMetres(from, at)});
			}
		}
		found = openblack::ecs::player_towns::NearestOfPlayer(towns, radius);
	}
	if (found == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No town of the player near enough");
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SetAttackOwnTown() // 228 SET_ATTACK_OWN_TOWN
{
	// The creature can be angry with the things of its own player, or it cannot
	const auto object = PopObject();
	const auto allow = Pop().uintVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not found!");
		return;
	}
	if (auto* mind = registry.TryGet<CreatureMindState>(object))
	{
		mind->mayAttackOwnPlayer = allow;
	}
	else
	{
		ScriptMessage("Thing not creature!");
	}
}

/// The belief of a town a script names, reporting an object that is gone or isn't a town as the game does
ecs::components::TownImpression* ScriptTownImpression(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		return nullptr;
	}
	if (!registry.AllOf<ecs::components::Town>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not town!");
		return nullptr;
	}
	return registry.AnyOf<ecs::components::TownImpression>(object) ? &registry.Get<ecs::components::TownImpression>(object)
	                                                               : &registry.Assign<ecs::components::TownImpression>(object);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !ScriptNamesPlayer(player))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object or player no longer valid");
		Pushf(0.0f);
		return;
	}
	const auto* town = registry.TryGet<const ecs::components::Town>(object);
	std::optional<float> townBelief;
	if (const auto* impression = registry.TryGet<const ecs::components::TownImpression>(object);
	    town != nullptr && impression != nullptr)
	{
		townBelief = impression->belief.belief.at(static_cast<size_t>(player));
	}
	Pushf(script::property_rules::BeliefForPlayer(town != nullptr, townBelief, ecs::world_objects::PlayerOf(object), player));
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	// The player takes the town: the player it believes in most is halved, and the taker believes that most and the amount
	const auto amount = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	auto* impression = ScriptTownImpression(PopObject());
	if (impression == nullptr || !ScriptNamesPlayer(player))
	{
		return;
	}
	// The players that play and the neutral player take part. A player that nobody plays does not.
	std::vector<PlayerNames> inGame;
	for (const auto name : ecs::game_players::All())
	{
		if (name == PlayerNames::NEUTRAL ||
		    (Locator::playerSystem::has_value() && Locator::playerSystem::value().PlaysInGame(name)))
		{
			inGame.push_back(name);
		}
	}
	magic::town_belief::TakeTown(impression->belief, player, amount, inGame);
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "The hand's virtual influence for player {} is turned {}",
	                    static_cast<int>(player), enable ? "on" : "off");
	// Turned off, the player's hand keeps nothing of their influence past the border, and loses what it had
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = Locator::playerSystem::value().FindPlayer(player).value_or(entt::null);
	if (!registry.Valid(entity))
	{
		return;
	}
	auto& state =
	    (registry.AnyOf<ecs::components::VirtualInfluence>(entity) ? registry.Get<ecs::components::VirtualInfluence>(entity)
	                                                               : registry.Assign<ecs::components::VirtualInfluence>(entity))
	        .state;
	virtual_influence::Switch(state, enable);
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	const auto withFixedHeight = Pop().intVal != 0;
	const auto speed = Popf();
	const auto position = PopVec();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().MoveHand(player, position, speed, withFixedHeight);
	}
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto enable = Pop().intVal != 0;
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetActive(player, enable);
	}
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// A player with no computer mind gives the origin
	std::optional<glm::vec3> position;
	if (Locator::computerPlayerSystem::has_value())
	{
		position = Locator::computerPlayerSystem::value().HandPosition(player);
	}
	PushVec(position.value_or(glm::vec3(0.0f)));
}

void SetComputerPlayerPosition() // 313 SET_COMPUTER_PLAYER_POSITION
{
	const auto withFixedHeight = Pop().intVal != 0;
	const auto position = PopVec();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetHand(player, position, withFixedHeight);
	}
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	// The town's belief in the player becomes a share of its belief in its owner
	const auto share = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto object = PopObject();
	auto* impression = ScriptTownImpression(object);
	if (impression == nullptr || !ScriptNamesPlayer(player))
	{
		return;
	}
	const auto& town = Locator::entitiesRegistry::value().Get<const ecs::components::Town>(object);
	magic::town_belief::SetRelativeToOwner(impression->belief, town.owner, player, share);
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	// A town wants one of its desires that much more, or less, until a script changes it again; its order of desires is
	// put right at once
	const auto boost = Popf();
	const auto desire = Pop().intVal;
	const auto town = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const bool isTown = town != entt::null && registry.Valid(town) && registry.AllOf<ecs::components::Town>(town);
	if (!isTown)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: object not a town");
	}
	if (!ecs::town_desire::ValidScriptBoost(desire, boost))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: desire {} or boost {} out of range", desire,
		                    boost);
		return;
	}
	if (isTown)
	{
		Locator::townDesireSystem::value().SetBoost(town, static_cast<TownDesireInfo>(desire), boost, true);
	}
}

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	Pushb(Locator::computerPlayerSystem::has_value() && Locator::computerPlayerSystem::value().Ready(player));
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto pause = Pop().intVal != 0;
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPaused(player, pause);
	}
}

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	const auto weight = Popf();
	const auto aspect = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPersonality(player, aspect, weight);
	}
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	const auto suppression = Popf();
	const auto aspect = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetSuppression(player, aspect, suppression);
	}
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	const auto second = PopObject();
	const auto first = PopObject();
	const auto action = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		std::ignore = Locator::computerPlayerSystem::value().ForceAction(player, action, first, second);
	}
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	const auto second = PopObject();
	const auto first = PopObject();
	const auto action = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().QueueAction(player, action, first, second);
	}
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	// The town the land's script made with this number, whoever owns it now
	const auto id = ecs::script_find::TownIdFromScript(Popf());
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(id);
	if (found == towns.end() || !registry.Valid(found->second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Town not found");
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found->second, false);
	Pusho(static_cast<uint32_t>(found->second));
}

void SetDisciple() // 369 SET_DISCIPLE
{
	// Whether with a sound: the game takes it and makes none
	Pop();
	const auto number = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	if (!registry.AllOf<ecs::components::Villager>(object))
	{
		ScriptMessage("Thing must be villager");
		return;
	}
	// The villager takes up the work at once, glowing as a disciple, and sets about it when it next decides
	if (const auto type = disciple::FromScript(number); type.has_value())
	{
		ecs::villager_disciple::SetType(object, *type);
	}
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	// The mind continues and releases the hand
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPaused(player, false);
	}
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	const auto speed = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetSpeed(player, speed);
	}
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	// Each player has a computer player from its start. The script gets the player's entity, which keeps the state of
	// the computer player.
	const auto name = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().FindPlayer(name) : std::optional<entt::entity> {};
	if (!player.has_value() || !Locator::entitiesRegistry::value().Valid(*player))
	{
		ScriptMessage("Could not get computer player!");
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(*player, false);
	Pusho(static_cast<uint32_t>(*player));
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	// The newest building of the town of the kind given that is built at least as much as given and that no script
	// holds. Without "excluding scripted" the game gives the newest building of the town of any kind (vanilla-bugs 927).
	const bool excludingScripted = Pop().intVal != 0;
	const auto minBuilt = Popf();
	const auto town = PopObject();
	const auto number = static_cast<AbodeNumber>(Pop().intVal);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* data =
	    town != entt::null && registry.Valid(town) ? registry.TryGet<const ecs::components::Town>(town) : nullptr;
	if (data == nullptr)
	{
		ScriptMessage("Must be town");
		Pusho(0);
		return;
	}
	std::vector<ecs::script_containers::TownBuilding> buildings;
	for (const auto entity : data->abodes)
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		const auto* abode = registry.TryGet<const ecs::components::Abode>(entity);
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(entity);
		buildings.push_back({.entity = entity,
		                     .number = abode != nullptr ? abode->type : AbodeNumber::Invalid,
		                     .built = progress != nullptr ? progress->built : 1.0f,
		                     .inScript = registry.AllOf<ecs::components::InScript>(entity)});
	}
	const auto found = ecs::script_containers::BuildingInTown(buildings, number, minBuilt, excludingScripted);
	if (found == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	// A town or a temple is let have worship sites made, or stopped
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().SetCanHaveSites(object, enable);
	}
}

void SetComputerPlayerAttitude() // 378 SET_COMPUTER_PLAYER_ATTITUDE
{
	const auto attitude = Popf();
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The mind of the first player now thinks this of the second; a number that names no player does nothing
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetAttitude(player, other, attitude);
	}
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// A number that names no player gives 0
	float attitude = 0.0f;
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::computerPlayerSystem::has_value())
	{
		attitude = Locator::computerPlayerSystem::value().Attitude(player, other);
	}
	Pushf(attitude);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	const auto file = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The mind of the player gets the weights of the file. A missing file or a number that names no player does nothing.
	if (ScriptNamesPlayer(player) && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().LoadPersonality(player, file);
	}
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	const auto file = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The file gets the weights of the mind of the player. A number that names no player does nothing.
	if (ScriptNamesPlayer(player) && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SavePersonality(player, file);
	}
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	const auto strength = Popf();
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The two players are allies of each other with this strength; a number that names no player does nothing
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().SetAlly(other, player, strength);
	}
}

void GetPlayerAlly() // 398 GET_PLAYER_ALLY
{
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The strength that the second player keeps for the first; a number that names no player gives 0
	float strength = 0.0f;
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::playerSystem::has_value())
	{
		strength = Locator::playerSystem::value().AllyStrength(other, player);
	}
	Pushf(strength);
}

/// The player a script names: its 0 is the neutral player, the others count from 1
PlayerNames ScriptPlayerName(int32_t scriptPlayer)
{
	return script::script_players::NameOf(scriptPlayer);
}

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	// How many of the town's people have died of chanting at worship; 0, with an error, for what isn't a town
	const auto object = PopObject();
	const auto* town =
	    object != entt::null ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Town>(object) : nullptr;
	if (town == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TOWN_WORSHIP_DEATHS: town invalid");
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(ecs::villager_after_death::DeathsOf(object, DeathReason::Chant)));
}

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The number of towns that the player has now
	const auto& registry = Locator::entitiesRegistry::value();
	Pushf(static_cast<float>(ecs::player_towns::Total(registry, player)));
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	// A town centre gives the totem that stands on it. Any other thing, or a centre with no totem, gives nothing.
	const auto centre = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	if (centre != entt::null && registry.Valid(centre))
	{
		registry.Each<const ecs::components::VillageTotem>(
		    [centre, &found](entt::entity totem, const ecs::components::VillageTotem& component) {
			    if (component.townCentre == centre && found == entt::null)
			    {
				    found = totem;
			    }
		    });
	}
	if (found == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SetLandBalance() // 427 SET_LAND_BALANCE
{
	// One of the eight balances of the land, as the land script sets it. The game writes outside the eight balances for
	// an index out of the range; openblack ignores such an index (vanilla-bugs 928).
	const auto value = Popf();
	const auto index = static_cast<int32_t>(Popf());
	lhscriptx::FeatureScriptCommands::SetGlobalLandBalance(index, value);
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	// The scale of the belief that a town gives to each player at its turn
	const auto scale = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	auto* impression = object != entt::null && registry.Valid(object) && registry.AllOf<ecs::components::Town>(object)
	                       ? registry.TryGet<ecs::components::TownImpression>(object)
	                       : nullptr;
	if (impression == nullptr)
	{
		ScriptMessage("Town invalid!");
		return;
	}
	impression->belief.scale = scale;
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	// The life of a town's buildings and of all its people added up: each building's people and then the building, the
	// buildings in the scripts' order, then the homeless
	const auto town = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* data = registry.Valid(town) ? registry.TryGet<const ecs::components::Town>(town) : nullptr;
	if (data == nullptr)
	{
		ScriptMessage("Invalid objects");
		Pushf(0.0f);
		return;
	}
	std::vector<script::town_health::AbodeLives> abodes;
	for (const auto abode : data->abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const ecs::components::Abode>(abode) : nullptr;
		if (building == nullptr)
		{
			continue;
		}
		auto& lives = abodes.emplace_back(script::town_health::AbodeLives {.life = ecs::world_objects::LifeOf(abode)});
		std::ranges::transform(building->inhabitants, std::back_inserter(lives.people),
		                       [](entt::entity person) { return ecs::world_objects::LifeOf(person); });
	}
	std::vector<float> homeless;
	std::ranges::transform(data->homelessVillagers, std::back_inserter(homeless),
	                       [](entt::entity person) { return ecs::world_objects::LifeOf(person); });
	Pushf(script::town_health::Total(abodes, homeless));
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	// The town gets the building site of the thing as one of its own sites, so its builders work at it too. The site
	// stays the site of the town of the building.
	const auto town = PopObject();
	const auto building = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const bool isTown = town != entt::null && registry.Valid(town) && registry.AllOf<ecs::components::Town>(town);
	if (building == entt::null || !isTown || !ecs::construction::ShareSite(building, town))
	{
		ScriptMessage("Invalid objects");
	}
}

void GetSacrificeTotal() // 449 GET_SACRIFICE_TOTAL
{
	// All the prayer power that sacrifices at a worship site's altar have stored there; 0 for anything but a site
	const auto site = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* chants = registry.Valid(site) ? registry.TryGet<const ecs::components::WorshipChants>(site) : nullptr;
	Pushf(chants != nullptr ? chants->sacrificed : 0.0f);
}

/// A player's temple, the earliest made when there are several, none when the player has none
std::optional<entt::entity> PlayersTemple(PlayerNames player)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const ecs::components::Temple>(
	    [player, &found](entt::entity entity, const ecs::components::Temple& temple) {
		    if (temple.owner == player && (!found.has_value() || entity < *found))
		    {
			    found = entity;
		    }
	    });
	return found;
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	// Where a player's temple stands, or nowhere (all zero) without one
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	glm::vec3 position {0.0f};
	if (const auto templeEntity = PlayersTemple(player); templeEntity.has_value())
	{
		position = Locator::entitiesRegistry::value().Get<const Transform>(*templeEntity).position;
	}
	PushVec(position);
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().ClearActions(player);
	}
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	// A point out from a player's temple towards its entrance, at a distance and a height, or nowhere (all zero)
	// without a temple
	const auto height = Popf();
	const auto radius = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto templeEntity = PlayersTemple(player);
	if (!templeEntity.has_value())
	{
		PushVec(glm::vec3(0.0f));
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(*templeEntity);
	// The entrance is the place its model marks for it, turned and placed with the temple, raised or lowered as the
	// land under it is from the land under the temple; a model without it has its entrance at the temple
	auto entrance = transform.position;
	if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(*templeEntity); mesh != nullptr)
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id) && meshes.Handle(mesh->id)->GetExtraMetrics().size() > openblack::temple::k_EntrancePlace)
		{
			const auto& place = meshes.Handle(mesh->id)->GetExtraMetrics().at(openblack::temple::k_EntrancePlace);
			entrance = transform.position + (transform.rotation * (transform.scale * glm::vec3(place[3])));
			const auto& land = Locator::terrainSystem::value();
			entrance.y +=
			    land.GetHeightAt({entrance.x, entrance.z}) - land.GetHeightAt({transform.position.x, transform.position.z});
		}
	}
	PushVec(openblack::temple::TowardsEntrance(transform.position, entrance, radius, height));
}

} // namespace openblack::chlapi
