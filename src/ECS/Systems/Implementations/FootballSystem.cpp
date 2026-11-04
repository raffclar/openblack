/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FootballSystem.h"

#include <algorithm>
#include <vector>

#include <L3DFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/X87.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Football.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/RelaxationRules.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FixedOutlineSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerHome.h"
#include "VillagerMoveToObject.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace rules = openblack::ecs::football_rules;

namespace
{

/// The file of the goal model, whose box list gives the two goals
constexpr std::string_view k_GoalModel = "football.l3d";

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Turn()
{
	return Locator::time::value().GetTurn();
}

float FloatRand(float range)
{
	return Locator::gameRandom::value().GameFloatRand(range);
}

uint32_t Rand(uint32_t range)
{
	return Locator::gameRandom::value().GameRand(range);
}

glm::vec3 PositionOf(entt::entity thing)
{
	return Entities().Get<const Transform>(thing).position;
}

map_coords::MapCoords CoordsOf(glm::vec3 point)
{
	return map_coords::FromWorld(Locator::terrainSystem::value(), point);
}

glm::vec3 WorldOf(const map_coords::MapCoords& coords)
{
	return map_coords::ToWorld(Locator::terrainSystem::value(), coords);
}

map_coords::MapCoords CoordsOf(entt::entity thing)
{
	return CoordsOf(PositionOf(thing));
}

/// The distance on the ground between two points, in metres, as the game measures map positions
float Metres(glm::vec3 a, glm::vec3 b)
{
	return gutils::GetDistanceInMetres(CoordsOf(a), CoordsOf(b));
}

float Metres(entt::entity a, entt::entity b)
{
	return Metres(PositionOf(a), PositionOf(b));
}

bool IsThere(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && Entities().AllOf<Transform>(thing);
}

/// A villager that died or went no longer plays
bool IsGone(entt::entity villager)
{
	return !Entities().Valid(villager) || !Entities().AllOf<Villager, LivingAction>(villager);
}

bool IsMoving(entt::entity villager)
{
	const auto* pose = Entities().TryGet<const VillagerPose>(villager);
	return pose != nullptr && pose->movedLastTurn;
}

LivingAction& ActionOf(entt::entity villager)
{
	return Entities().Get<LivingAction>(villager);
}

const LivingActionSystemInterface& Living()
{
	return Locator::livingActionSystem::value();
}

/// A villager goes into a state as the normal change of its top state
void ChangeState(entt::entity villager, VillagerStates state)
{
	Living().VillagerChangeTopState(ActionOf(villager), state);
}

/// A villager touches the ball and stands taller than the ball is high: it can kick the ball
bool CanKick(entt::entity villager, entt::entity ball)
{
	const auto& registry = Entities();
	const float height = ecs::systems::object_measures::Height(registry, villager);
	if (ecs::systems::object_measures::DistanceFromObject(registry, villager, ball) > height)
	{
		return false;
	}
	return height > CoordsOf(ball).altitude;
}

bool Touches(entt::entity villager, entt::entity ball)
{
	const auto& registry = Entities();
	return ecs::systems::object_measures::DistanceFromObject(registry, villager, ball) <=
	       ecs::systems::object_measures::Height(registry, villager);
}

std::optional<size_t> IndexIn(const std::vector<entt::entity>& list, entt::entity villager)
{
	const auto found = std::ranges::find(list, villager);
	return found != list.end() ? std::optional<size_t>(static_cast<size_t>(found - list.begin())) : std::nullopt;
}

bool IsHome(const FootballPitch& football, entt::entity villager)
{
	return IndexIn(football.home, villager).has_value();
}

rules::Side SideOf(const FootballPitch& football, entt::entity villager)
{
	return IsHome(football, villager) ? rules::Side::Home : rules::Side::Away;
}

const std::vector<entt::entity>& TeamOf(const FootballPitch& football, rules::Side side)
{
	return side == rules::Side::Home ? football.home : football.away;
}

/// The index of a player in its team, which stands in order of distance to the ball. A player that is in no team gives
/// the largest index, as the game's unsigned test sees it.
uint32_t BallIndexOf(const FootballPitch& football, entt::entity villager)
{
	const auto index = IndexIn(TeamOf(football, SideOf(football, villager)), villager);
	return index.has_value() ? static_cast<uint32_t>(*index) : std::numeric_limits<uint32_t>::max();
}

/// The index of a player in its team's order of distance to the goal that it attacks, -1 when it is not there
int32_t GoalIndexOf(const FootballPitch& football, entt::entity villager)
{
	const auto& ranks = football.ranks.at(static_cast<size_t>(SideOf(football, villager)));
	const auto index = IndexIn(ranks, villager);
	return index.has_value() ? static_cast<int32_t>(*index) : -1;
}

VillagerStates RoleState(rules::Role role)
{
	switch (role)
	{
	case rules::Role::Attacker:
		return VillagerStates::FootballAttacker;
	case rules::Role::Goalkeeper:
		return VillagerStates::FootballGoalie;
	case rules::Role::Defender:
		return VillagerStates::FootballDefender;
	}
	return VillagerStates::FootballAttacker;
}

std::optional<rules::Role> RoleOf(VillagerStates state)
{
	switch (state)
	{
	case VillagerStates::FootballAttacker:
		return rules::Role::Attacker;
	case VillagerStates::FootballGoalie:
		return rules::Role::Goalkeeper;
	case VillagerStates::FootballDefender:
		return rules::Role::Defender;
	default:
		return std::nullopt;
	}
}

/// The largest task number of a role. A higher number means that the player has no task.
uint8_t LastTask(rules::Role role)
{
	return static_cast<uint8_t>(rules::TasksOf(role).size() - 1);
}

/// A distance in map units moved by a random part of a metre in a square of a size, as the game moves a point
int32_t ScatterBy(int32_t fixed, float size)
{
	return rules::Scatter(fixed, FloatRand(size) - size * 0.5f);
}

entt::entity FootballPitchOf(entt::entity town)
{
	const auto& registry = Entities();
	const auto& abodes = registry.Get<const Town>(town).abodes;
	const auto& table = Locator::infoConstants::value().abode;
	const auto found = std::ranges::find_if(abodes, [&registry, &table](entt::entity abode) {
		const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		return building != nullptr && building->info != AbodeInfo::None &&
		       table.at(static_cast<size_t>(building->info)).abodeType == AbodeType::FootballPitch;
	});
	return found != abodes.end() ? *found : entt::null;
}

/// Half the length of the pitch's model, along which the ball stays in play
float HalfLengthOf(entt::entity pitch)
{
	const auto* mesh = Entities().TryGet<const Mesh>(pitch);
	if (mesh == nullptr || !Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return 0.0f;
	}
	return Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox().Size().x * 0.5f;
}

glm::mat4 PitchMatrix(entt::entity pitch)
{
	const auto& transform = Entities().Get<const Transform>(pitch);
	return PlacedMatrix(transform.position, transform.rotation, transform.scale);
}

} // namespace

void FootballSystem::StartGame(bool addOnSwitch, bool multiplayer)
{
	_on = rules::IsFootballOn(addOnSwitch, multiplayer);
	_lastMatchTurn.reset();
}

bool FootballSystem::IsOn() const
{
	return _on;
}

const std::vector<FootballSystem::GoalBox>& FootballSystem::Goals()
{
	if (_goals.has_value())
	{
		return *_goals;
	}
	_goals.emplace();
	if (!Locator::filesystem::has_value())
	{
		return *_goals;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / k_GoalModel;
	l3d::L3DFile file;
	if (!fileSystem.Exists(path) || file.ReadFile(*fileSystem.GetData(path)) != l3d::L3DResult::Success)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The goal model, {}, can't be read", path.generic_string());
		return *_goals;
	}
	for (const auto& box : file.GetBoxes())
	{
		_goals->push_back({
		    .centre = {box.centre.x, box.centre.y, box.centre.z},
		    .halfSize = {box.size.x, box.size.y, box.size.z},
		});
	}
	return *_goals;
}

glm::vec3 FootballSystem::GoalPosition(entt::entity pitch, uint32_t goal)
{
	return WorldOf(GoalCoords(pitch, goal));
}

map_coords::MapCoords FootballSystem::GoalCoords(entt::entity pitch, uint32_t goal)
{
	const auto& goals = Goals();
	if (goal >= goals.size())
	{
		return CoordsOf(pitch);
	}
	const auto point = PitchMatrix(pitch) * glm::vec4(goals.at(goal).centre, 1.0f);
	return CoordsOf(glm::vec3(point));
}

void FootballSystem::ProcessTown(entt::entity town)
{
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto* own = registry.TryGet<const TownFootballPitch>(town);
	auto pitch = own != nullptr ? own->pitch : entt::null;
	if (pitch == entt::null || !IsThere(pitch) || !registry.AllOf<FootballPitch>(pitch))
	{
		// A pitch becomes the town's pitch when its building works
		const auto built = FootballPitchOf(town);
		if (built == entt::null || !ecs::villager_home::IsFunctional(built))
		{
			return;
		}
		MakeFunctional(town, built);
		pitch = built;
	}
	if (_on)
	{
		ProcessPitch(town, pitch);
	}
}

void FootballSystem::MakeFunctional(entt::entity town, entt::entity pitch)
{
	auto& registry = Entities();
	if (!registry.AllOf<FootballPitch>(pitch))
	{
		registry.Assign<FootballPitch>(pitch);
	}
	auto& football = registry.Get<FootballPitch>(pitch);
	football.functional = true;
	football.match.state = rules::PlayState::WaitingForPlayers;
	registry.AssignOrReplace<TownFootballPitch>(town, pitch);
	BallOf(pitch);
}

entt::entity FootballSystem::BallOf(entt::entity pitch)
{
	auto& registry = Entities();
	auto* football = registry.TryGet<FootballPitch>(pitch);
	if (football == nullptr)
	{
		return entt::null;
	}
	if (!IsThere(football->ball) || !registry.AllOf<Ball>(football->ball))
	{
		football->ball = ecs::archetypes::BallArchetype::Create(WorldOf(CoordsOf(pitch)));
		football->ballLastPlace.reset();
	}
	return football->ball;
}

std::optional<rules::PlayState> FootballSystem::PlayStateOf(entt::entity pitch) const
{
	const auto* football = IsThere(pitch) ? Entities().TryGet<const FootballPitch>(pitch) : nullptr;
	return football != nullptr ? std::optional(football->match.state) : std::nullopt;
}

bool FootballSystem::IsPlaying(entt::entity villager) const
{
	const auto& registry = Entities();
	if (!registry.Valid(villager) || !registry.AllOf<Villager, LivingAction>(villager))
	{
		return false;
	}
	return rules::IsMatchState(Living().VillagerGetFinalState(registry.Get<const LivingAction>(villager)));
}

std::optional<bool> FootballSystem::HomeMournsGoal(entt::entity pitch) const
{
	const auto* football = IsThere(pitch) ? Entities().TryGet<const FootballPitch>(pitch) : nullptr;
	if (football == nullptr || football->match.state != rules::PlayState::Scored)
	{
		return std::nullopt;
	}
	return football->match.homeMournedLastGoal;
}

glm::vec3 FootballSystem::AttackedGoalPosition(entt::entity pitch, bool homeTeam)
{
	return GoalPosition(pitch, rules::AttackedGoal(homeTeam ? rules::Side::Home : rules::Side::Away));
}

entt::entity FootballSystem::FirstPlayer(entt::entity pitch, bool homeTeam) const
{
	const auto* football = IsThere(pitch) ? Entities().TryGet<const FootballPitch>(pitch) : nullptr;
	if (football == nullptr)
	{
		return entt::null;
	}
	const auto& team = homeTeam ? football->home : football->away;
	return team.empty() ? entt::null : team.front();
}

bool FootballSystem::IsPitch(entt::entity thing) const
{
	const auto& registry = Entities();
	const auto* building = registry.Valid(thing) ? registry.TryGet<const Abode>(thing) : nullptr;
	if (building == nullptr || building->info == AbodeInfo::None || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& table = Locator::infoConstants::value().abode;
	const auto row = static_cast<size_t>(building->info);
	return row < table.size() && table.at(row).abodeType == AbodeType::FootballPitch;
}

entt::entity FootballSystem::TownPitch(entt::entity town) const
{
	const auto& registry = Entities();
	const auto* own = registry.Valid(town) ? registry.TryGet<const TownFootballPitch>(town) : nullptr;
	return own != nullptr && IsThere(own->pitch) ? own->pitch : entt::null;
}

FootballPitch* FootballSystem::PitchData(entt::entity pitch)
{
	if (!IsPitch(pitch) || !IsThere(pitch))
	{
		return nullptr;
	}
	auto& registry = Entities();
	if (!registry.AllOf<FootballPitch>(pitch))
	{
		registry.Assign<FootballPitch>(pitch);
	}
	return &registry.Get<FootballPitch>(pitch);
}

void FootballSystem::SendToKickOff(entt::entity pitch, entt::entity villager)
{
	// The player waits for the kick-off at a random place near the centre spot
	const auto centre = CoordsOf(pitch);
	const auto x = ScatterBy(centre.x, rules::k_KickOffScatter);
	const auto z = ScatterBy(centre.z, rules::k_KickOffScatter);
	ecs::villager_home::SetupMoveTo(ActionOf(villager), glm::ivec2(x, z), VillagerStates::FootballWaitForKickOff);
}

void FootballSystem::StopAllGames(entt::entity pitch)
{
	auto* football = PitchData(pitch);
	if (football == nullptr)
	{
		return;
	}
	const auto* building = Entities().TryGet<const Abode>(pitch);
	const auto& towns = Entities().Context().towns;
	const auto town = building != nullptr ? towns.find(building->townId) : towns.end();
	StopMatch(town != towns.end() && Entities().Valid(town->second) ? town->second : entt::null, pitch, *football);
}

bool FootballSystem::AttachToGame(entt::entity pitch, entt::entity villager, bool homeTeam)
{
	auto* football = PitchData(pitch);
	if (football == nullptr || IsGone(villager))
	{
		return false;
	}
	const auto side =
	    rules::ScriptTeam(homeTeam, static_cast<uint32_t>(football->home.size()), static_cast<uint32_t>(football->away.size()));
	if (side.has_value())
	{
		auto& team = *side == rules::Side::Home ? football->home : football->away;
		team.insert(team.begin(), villager);
		SendToKickOff(pitch, villager);
	}
	// The villager is in the match also when no team took it
	Entities().AssignOrReplace<Footballer>(villager, Footballer {.pitch = pitch});
	return side.has_value();
}

bool FootballSystem::DetachFromGame(entt::entity pitch, entt::entity villager)
{
	auto* football = PitchData(pitch);
	if (football == nullptr || IsGone(villager))
	{
		return false;
	}
	const auto drop = [villager](std::vector<entt::entity>& list) { std::erase(list, villager); };
	drop(football->home);
	drop(football->away);
	drop(football->ranks.at(0));
	drop(football->ranks.at(1));
	return true;
}

bool FootballSystem::DetachFirstPlayer(entt::entity pitch, bool homeTeam)
{
	auto* football = PitchData(pitch);
	if (football == nullptr || (football->home.empty() && football->away.empty()))
	{
		return false;
	}
	auto& team = homeTeam ? football->home : football->away;
	if (!team.empty())
	{
		const auto first = team.front();
		team.erase(team.begin());
		std::erase(football->ranks.at(0), first);
		std::erase(football->ranks.at(1), first);
	}
	return true;
}

void FootballSystem::SetOnlyForScripts(entt::entity pitch, bool onlyForScripts)
{
	if (auto* football = PitchData(pitch))
	{
		football->match.townMatch = !onlyForScripts;
	}
}

bool FootballSystem::StartWithReferee(entt::entity pitch, entt::entity referee)
{
	auto* football = PitchData(pitch);
	if (football == nullptr || IsGone(referee))
	{
		return false;
	}
	BallOf(pitch);
	const bool became = football->referee == entt::null;
	if (became)
	{
		football->referee = referee;
	}
	// The villager is in the match also when the pitch had a referee
	Entities().AssignOrReplace<Footballer>(referee, Footballer {.pitch = pitch});
	return became;
}

uint32_t FootballSystem::TeamSize(entt::entity pitch, bool homeTeam) const
{
	const auto* football = IsThere(pitch) ? Entities().TryGet<const FootballPitch>(pitch) : nullptr;
	if (football == nullptr)
	{
		return 0;
	}
	return static_cast<uint32_t>(homeTeam ? football->home.size() : football->away.size());
}

entt::entity FootballSystem::PitchOf(entt::entity villager) const
{
	const auto* player = Entities().Valid(villager) ? Entities().TryGet<const Footballer>(villager) : nullptr;
	if (player == nullptr || !IsThere(player->pitch) || !Entities().AllOf<FootballPitch>(player->pitch))
	{
		return entt::null;
	}
	return player->pitch;
}

std::optional<float> FootballSystem::RelaxationDesire(entt::entity town, entt::entity villager) const
{
	const auto& registry = Entities();
	const auto* own = registry.TryGet<const TownFootballPitch>(town);
	if (own == nullptr || !IsThere(own->pitch) || !registry.AllOf<FootballPitch>(own->pitch))
	{
		return std::nullopt;
	}
	const auto& football = registry.Get<const FootballPitch>(own->pitch);
	const auto* person = registry.TryGet<const Villager>(villager);
	const auto age = person != nullptr ? ecs::villager_age::AgeNow(*person) : 0;
	const ecs::relaxation_rules::FootballGame game {
	    .firstTeam = static_cast<uint32_t>(football.home.size()),
	    .secondTeam = static_cast<uint32_t>(football.away.size()),
	    .playing = football.referee != entt::null,
	    .lastGameTurn = football.lastMatchTurn,
	};
	return ecs::relaxation_rules::FootballDesire(age, game, Turn(), [](float range) { return FloatRand(range); });
}

bool FootballSystem::Join(entt::entity town, entt::entity villager)
{
	auto& registry = Entities();
	const auto* own = registry.TryGet<const TownFootballPitch>(town);
	if (own == nullptr || !IsThere(own->pitch) || !registry.AllOf<FootballPitch>(own->pitch))
	{
		return false;
	}
	const auto pitch = own->pitch;
	auto& football = registry.Get<FootballPitch>(pitch);
	if (!football.match.townMatch || BallOf(pitch) == entt::null)
	{
		return false;
	}
	auto& action = ActionOf(villager);
	const auto joins = rules::JoinAs(football.referee != entt::null, static_cast<uint32_t>(football.home.size()),
	                                 static_cast<uint32_t>(football.away.size()));
	const auto centre = CoordsOf(pitch);
	switch (joins)
	{
	case rules::Joins::Referee:
		football.referee = villager;
		break;
	case rules::Joins::Home:
	case rules::Joins::Away:
	{
		auto& team = joins == rules::Joins::Home ? football.home : football.away;
		team.insert(team.begin(), villager);
		SendToKickOff(pitch, villager);
		break;
	}
	case rules::Joins::Crowd:
	{
		float angle = 0.0f;
		if (football.crowdPlaces < rules::k_WavePlaces)
		{
			const auto free = std::ranges::find(football.crowd, entt::null);
			if (free != football.crowd.end())
			{
				*free = villager;
			}
			++football.crowdPlaces;
			angle = rules::CrowdPlaceAngle(football.crowdPlaces);
		}
		else
		{
			angle = FloatRand(6.2831855f);
		}
		auto place = centre;
		place +=
		    gutils::GetPosFromAngle(angle, ecs::systems::object_measures::TwoDRadius(registry, pitch) + rules::k_CrowdDistance);
		const auto x = ScatterBy(place.x, rules::k_CrowdScatter);
		const auto z = ScatterBy(place.z, rules::k_CrowdScatter);
		ecs::villager_home::SetupMoveTo(action, glm::ivec2(x, z), VillagerStates::FootballWatchMatch);
		break;
	}
	}
	if (football.home.size() + football.away.size() == 1)
	{
		football.match.turns = 0;
	}
	registry.AssignOrReplace<Footballer>(villager, Footballer {.pitch = pitch});
	return true;
}

void FootballSystem::Leave(entt::entity villager)
{
	const auto pitch = PitchOf(villager);
	if (pitch == entt::null)
	{
		return;
	}
	Entities().Get<Footballer>(villager).inMatch = false;
	auto& football = Entities().Get<FootballPitch>(pitch);
	if (football.deadBallFetcher == villager)
	{
		football.deadBallFetcher = entt::null;
	}
	const auto drop = [villager](std::vector<entt::entity>& list) { std::erase(list, villager); };
	drop(football.home);
	drop(football.away);
	drop(football.ranks.at(0));
	drop(football.ranks.at(1));
}

void FootballSystem::ProcessPitch(entt::entity town, entt::entity pitch)
{
	auto& registry = Entities();
	auto ball = BallOf(pitch);
	auto& football = registry.Get<FootballPitch>(pitch);
	// A ball that went too far away or into the water goes, and a new ball waits on the centre spot
	const bool water = Locator::fixedOutlineSystem::has_value() && Locator::fixedOutlineSystem::value().IsWater(CoordsOf(ball));
	if (Metres(ball, pitch) > rules::k_BallLostDistance || water ||
	    (registry.AllOf<InHand>(ball) && Metres(ball, pitch) > rules::k_BallInHandLostDistance))
	{
		ecs::world_objects::Remove(ball);
		football.ball = entt::null;
		BallOf(pitch);
		return;
	}

	const uint32_t turn = Turn();
	if (_lastMatchTurn != turn)
	{
		_lastMatchTurn = turn;
		if (const auto place = rules::StepWave(football.wave); place.has_value())
		{
			const auto spectator = football.crowd.at(*place);
			if (spectator != entt::null && !IsGone(spectator) && !IsMoving(spectator))
			{
				ChangeState(spectator, VillagerStates::FootballMexicanWave);
			}
		}
		auto& match = football.match;
		const auto home = static_cast<uint32_t>(football.home.size());
		const auto away = static_cast<uint32_t>(football.away.size());
		switch (match.state)
		{
		case rules::PlayState::WaitingForPlayers:
		{
			const auto step = rules::StepWaiting(match, home, away, [&football]() {
				uint32_t walking = 0;
				for (const auto* team : {&football.home, &football.away})
				{
					walking += static_cast<uint32_t>(std::ranges::count_if(*team, [](entt::entity v) { return IsMoving(v); }));
				}
				return walking;
			});
			if (step.sendPlayersHome)
			{
				SendPlayersHome(football);
			}
			if (step.placeBall && !registry.AllOf<InPhysics>(ball))
			{
				registry.Get<Transform>(ball).position = WorldOf(CoordsOf(pitch));
				if (Locator::entitiesMap::has_value())
				{
					Locator::entitiesMap::value().Moved(ball);
				}
			}
			if (step.kickOff)
			{
				RankTeams(pitch, football);
				GiveRoles(football, 0);
				GiveRoles(football, 1);
			}
			break;
		}
		case rules::PlayState::Playing:
		{
			if (!rules::PlaysOn(home, away))
			{
				match.state = rules::PlayState::WaitingForPlayers;
				break;
			}
			if (match.townMatch)
			{
				const auto wanted = Locator::townDesireSystem::has_value()
				                        ? Locator::townDesireSystem::value().GetMostWanted(town)
				                        : TownDesireInfo::ForRelaxation;
				const bool wantsPlay = wanted == TownDesireInfo::ForRelaxation || wanted == TownDesireInfo::ForPlaytime;
				if (rules::TownMatchStops(match, wantsPlay))
				{
					football.lastMatchTurn = Turn();
					StopMatch(town, pitch, football);
				}
			}
			ball = BallOf(pitch);
			if (ProcessBall(pitch, football, ball))
			{
				football.deadBallFetcher = entt::null;
				RankTeams(pitch, football);
				if (rules::RoleTimeIsOver(match))
				{
					GiveRoles(football, 0);
					GiveRoles(football, 1);
				}
				ChooseTasks(pitch, football, 0);
				ChooseTasks(pitch, football, 1);
				break;
			}
			// The ball is out of play: the first player of one team fetches it, and the others pause
			const auto homeFirst = football.home.front();
			const auto awayFirst = football.away.front();
			const auto ballCoords = CoordsOf(ball);
			const auto fetcherSide = rules::DeadBallFetcher(gutils::FastDistance(CoordsOf(homeFirst), ballCoords),
			                                                gutils::FastDistance(CoordsOf(awayFirst), ballCoords));
			const auto candidate = fetcherSide == rules::Side::Home ? homeFirst : awayFirst;
			for (const auto& team : {football.home, football.away})
			{
				for (const auto villager : team)
				{
					if (villager != candidate && villager != football.deadBallFetcher &&
					    match.state != rules::PlayState::Scored &&
					    Living().VillagerGetState(ActionOf(villager), LivingAction::Index::Top) !=
					        VillagerStates::FootballMatchPaused)
					{
						ChangeState(villager, VillagerStates::FootballMatchPaused);
					}
				}
			}
			if (football.deadBallFetcher == entt::null)
			{
				ecs::villager_move_to_object::SetupMoveToObject(ActionOf(candidate), ball,
				                                                VillagerStates::ArrivedAtPickUpBallForDeadBall);
				football.deadBallFetcher = candidate;
			}
			break;
		}
		case rules::PlayState::Scored:
			static_cast<void>(rules::StepGoalPause(match));
			break;
		case rules::PlayState::Paused:
		case rules::PlayState::Ended:
			break;
		}
	}

	// A ball that went makes the players go home
	auto& kept = registry.Get<FootballPitch>(pitch);
	if (kept.ball != entt::null && !IsThere(kept.ball))
	{
		const auto gone = kept.ball;
		kept.ball = entt::null;
		SendPlayersHome(kept);
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(gone);
		}
	}
	ForgetGone(kept);
}

void FootballSystem::SendPlayersHome(FootballPitch& football)
{
	// A copy of each team, because a villager that goes home leaves its team
	for (const auto& team : {football.home, football.away})
	{
		for (const auto villager : team)
		{
			if (!IsGone(villager))
			{
				ChangeState(villager, VillagerStates::GoHome);
			}
		}
	}
	if (football.ball != entt::null && Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().RemoveFrom(football.ball);
	}
}

void FootballSystem::ForgetGone(FootballPitch& football)
{
	if (football.referee != entt::null && IsGone(football.referee))
	{
		football.referee = entt::null;
	}
	for (auto& spectator : football.crowd)
	{
		if (spectator != entt::null && IsGone(spectator))
		{
			spectator = entt::null;
		}
	}
	for (auto* list : {&football.home, &football.away, &football.ranks.at(0), &football.ranks.at(1)})
	{
		std::erase_if(*list, [](entt::entity villager) { return IsGone(villager); });
	}
}

bool FootballSystem::ProcessBall(entt::entity pitch, FootballPitch& football, entt::entity ball)
{
	const auto now = WorldOf(CoordsOf(ball));
	const auto toPitch = glm::inverse(PitchMatrix(pitch));
	const auto local = glm::vec3(toPitch * glm::vec4(now, 1.0f));
	const auto* kicked = Entities().TryGet<const Ball>(ball);
	if (football.match.state == rules::PlayState::Playing && kicked != nullptr && kicked->canScore &&
	    football.ballLastPlace.has_value())
	{
		const auto before = glm::vec3(toPitch * glm::vec4(*football.ballLastPlace, 1.0f));
		const auto& goals = Goals();
		for (uint32_t goal = 0; goal < goals.size() && goal < 2; ++goal)
		{
			const rules::GoalMouth mouth {
			    .line = goals.at(goal).centre.x,
			    .height = goals.at(goal).halfSize.y,
			    .halfWidth = goals.at(goal).halfSize.z,
			};
			if (rules::CrossesGoal(before, local, mouth))
			{
				ScoreGoal(football, goal);
				break;
			}
		}
	}
	football.ballLastPlace = now;
	return rules::IsOnPitch(local.x, HalfLengthOf(pitch));
}

void FootballSystem::ScoreGoal(FootballPitch& football, uint32_t goal)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "GOAL!!!");
	const bool homeMourns = rules::ScoreGoal(football.match, goal);
	for (const auto villager : std::vector(football.home))
	{
		ChangeState(villager, homeMourns ? VillagerStates::FootballLostGoal : VillagerStates::FootballWonGoal);
	}
	for (const auto villager : std::vector(football.away))
	{
		ChangeState(villager, homeMourns ? VillagerStates::FootballWonGoal : VillagerStates::FootballLostGoal);
	}
}

void FootballSystem::RankTeams(entt::entity pitch, FootballPitch& football)
{
	const auto ballCoords = CoordsOf(BallOf(pitch));
	for (size_t side = 0; side < 2; ++side)
	{
		auto& team = side == 0 ? football.home : football.away;
		// The team stands in order of distance to the ball
		std::vector<int32_t> toBall;
		toBall.reserve(team.size());
		for (const auto villager : team)
		{
			toBall.push_back(gutils::FastDistance(CoordsOf(villager), ballCoords));
		}
		std::vector<entt::entity> byBall;
		byBall.reserve(team.size());
		for (const auto index : rules::OrderByDistance(toBall))
		{
			byBall.push_back(team.at(index));
		}
		team = std::move(byBall);
		// The attacker nearest to the ball, and a defender nearer than it, are the players that the other team marks
		std::vector<rules::Role> roles;
		std::vector<uint32_t> distances;
		for (const auto villager : team)
		{
			const auto role = RoleOf(Living().VillagerGetState(ActionOf(villager), LivingAction::Index::Top));
			roles.push_back(role.value_or(rules::Role::Goalkeeper));
			distances.push_back(static_cast<uint32_t>(gutils::FastDistance(CoordsOf(villager), ballCoords)));
		}
		const auto marks = rules::FindMarks(roles, distances);
		if (marks.attacker.has_value())
		{
			football.attackerMarks.at(side) = team.at(*marks.attacker);
		}
		if (marks.defender.has_value())
		{
			football.defenderMarks.at(side) = team.at(*marks.defender);
		}
	}
	// Each team also stands in order of distance to the goal that it attacks
	for (size_t side = 0; side < 2; ++side)
	{
		const auto& team = side == 0 ? football.home : football.away;
		const auto goal = CoordsOf(GoalPosition(pitch, rules::AttackedGoal(static_cast<rules::Side>(side))));
		std::vector<int32_t> toGoal;
		toGoal.reserve(team.size());
		for (const auto villager : team)
		{
			toGoal.push_back(gutils::FastDistance(CoordsOf(villager), goal));
		}
		auto& ranks = football.ranks.at(side);
		ranks.clear();
		for (const auto index : rules::OrderByDistance(toGoal))
		{
			ranks.push_back(team.at(index));
		}
	}
}

void FootballSystem::GiveRoles(FootballPitch& football, size_t side)
{
	const auto ranks = football.ranks.at(side);
	const auto count = static_cast<uint32_t>(ranks.size());
	for (uint32_t rank = 0; rank < count; ++rank)
	{
		const auto villager = ranks.at(rank);
		auto* found = Entities().TryGet<Footballer>(villager);
		if (found == nullptr)
		{
			continue;
		}
		auto& player = *found;
		const auto given = rules::RoleForRank(rank, count);
		if (given.role != rules::Role::Goalkeeper)
		{
			player.number = given.number;
		}
		const auto state = RoleState(given.role);
		auto& action = ActionOf(villager);
		if (Living().VillagerGetState(action, LivingAction::Index::Top) != state)
		{
			ChangeState(villager, state);
			action.turnsUntilStateChange = rules::k_NewRoleTurns;
			player.chooseTask = true;
		}
		if (player.pitch != entt::null)
		{
			ChooseTask(player.pitch, villager);
		}
	}
}

void FootballSystem::ChooseTasks(entt::entity pitch, const FootballPitch& football, size_t side)
{
	const auto ranks = football.ranks.at(side);
	for (const auto villager : ranks)
	{
		ChooseTask(pitch, villager);
	}
}

void FootballSystem::ChooseTask(entt::entity pitch, entt::entity villager)
{
	auto* player = Entities().TryGet<Footballer>(villager);
	if (player == nullptr || !player->chooseTask)
	{
		return;
	}
	const auto role = RoleOf(Living().VillagerGetFinalState(ActionOf(villager)));
	if (!role.has_value())
	{
		return;
	}
	player->task = rules::ChooseTask(
	    *role, [this, pitch, villager](rules::Test test) { return Passes(pitch, villager, test); },
	    [this, pitch, villager, role](uint8_t task) { return Priority(pitch, villager, *role, task); });
	// The tests and priorities can change the components, so the player is read again
	auto& chosen = Entities().Get<Footballer>(villager);
	chosen.task = player->task;
	chosen.chooseTask = false;
	chosen.lookTurns = 0;
}

void FootballSystem::StopMatch(entt::entity town, entt::entity pitch, FootballPitch& football)
{
	// A copy of each team, because a villager that goes home can leave its team
	for (const auto& team : {football.home, football.away})
	{
		for (const auto villager : team)
		{
			Entities().Get<Footballer>(villager).inMatch = false;
			auto& action = ActionOf(villager);
			Living().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo, true);
			ecs::villager_home::GoHome(action);
		}
	}
	for (const auto spectator : football.crowd)
	{
		if (spectator != entt::null && !IsGone(spectator))
		{
			ecs::villager_home::GoHome(ActionOf(spectator));
		}
	}
	// The game measures the match against the town's match length, but nothing counts that measure, so the belief is 0
	const auto& townInfo = Locator::infoConstants::value().town;
	const float belief = rules::MatchBelief(townInfo.beliefForFooballMatch, 0, townInfo.footballMatchDuration);
	if (Locator::reactionSystem::has_value() && town != entt::null && Entities().AllOf<Town>(town))
	{
		Locator::reactionSystem::value().GiveTownBelief(town, Entities().Get<const Town>(town).owner, belief, pitch,
		                                                GuidanceAlignment::Good);
	}
	// The pitch forgets its match and its ball. The old ball stays in the world, and the pitch makes a new ball.
	const auto lastMatch = football.lastMatchTurn;
	const bool functional = football.functional;
	football = FootballPitch {};
	football.functional = functional;
	football.lastMatchTurn = lastMatch;
}

glm::vec3 FootballSystem::PlaceOf(entt::entity pitch, entt::entity villager)
{
	return WorldOf(PlaceCoordsOf(pitch, villager));
}

map_coords::MapCoords FootballSystem::PlaceCoordsOf(entt::entity pitch, entt::entity villager)
{
	const auto& football = Entities().Get<const FootballPitch>(pitch);
	const auto side = SideOf(football, villager);
	const auto role = RoleOf(Living().VillagerGetFinalState(ActionOf(villager))).value_or(rules::Role::Attacker);
	if (role == rules::Role::Goalkeeper)
	{
		return GoalCoords(pitch, rules::KeptGoal(side));
	}
	const auto& player = Entities().Get<const Footballer>(villager);
	const auto count = static_cast<uint32_t>(football.ranks.at(static_cast<size_t>(side)).size());
	const auto place =
	    role == rules::Role::Attacker ? rules::AttackerPlace(player.number, count) : rules::DefenderPlace(player.number, count);
	const auto centre = PositionOf(pitch);
	const auto goal = GoalPosition(pitch, rules::AttackedGoal(side));
	const auto spot = WorldOf(CoordsOf(centre));
	const auto point = rules::PlacePoint(glm::xz(spot), glm::xz(goal), place);
	// The place has the world height of the centre spot, which the game then measures from the land at the place
	return CoordsOf(glm::vec3(point.x, spot.y, point.y));
}

glm::vec3 FootballSystem::Perturb(glm::vec3 target, entt::entity villager)
{
	auto coords = CoordsOf(target);
	const float half = Metres(target, PositionOf(villager)) * 0.5f;
	const float range = half + half;
	coords.x = rules::Scatter(coords.x, FloatRand(range) - half);
	coords.z = rules::Scatter(coords.z, FloatRand(range) - half);
	return WorldOf(coords);
}

void FootballSystem::Kick(entt::entity /*pitch*/, entt::entity ball, glm::vec3 target, float speed, bool canScore)
{
	if (!Locator::dynamicsSystem::has_value())
	{
		return;
	}
	if (ecs::object_physics::KickBall(Locator::dynamicsSystem::value(), ball, target, speed))
	{
		Entities().Get<Ball>(ball).canScore = canScore;
	}
}

uint32_t FootballSystem::PlayRole(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto pitch = PitchOf(villager);
	if (pitch == entt::null || BallOf(pitch) == entt::null)
	{
		return ecs::villager_home::DecideWhatToDo(action);
	}
	if (action.turnsUntilStateChange < 1)
	{
		++action.turnsUntilStateChange;
		return 1;
	}
	const auto state = Living().VillagerGetState(action, LivingAction::Index::Top);
	const auto role = RoleOf(state).value_or(rules::Role::Attacker);
	const auto task = registry.Get<const Footballer>(villager).task;
	if (!task.has_value() || *task > LastTask(role))
	{
		return 1;
	}
	DoTask(pitch, villager, state, *task);
	if (!registry.Valid(villager) || !registry.AllOf<Footballer>(villager) || PitchOf(villager) == entt::null)
	{
		return 1;
	}
	if (!Passes(pitch, villager, rules::TasksOf(role)[*task].first))
	{
		registry.Get<Footballer>(villager).chooseTask = true;
	}
	return 1;
}

void FootballSystem::RestartDeadBall(entt::entity villager)
{
	const auto pitch = PitchOf(villager);
	if (pitch == entt::null)
	{
		return;
	}
	const auto ball = BallOf(pitch);
	if (CanKick(villager, ball))
	{
		Kick(pitch, ball, WorldOf(CoordsOf(pitch)), rules::k_DeadBallKickSpeed, false);
	}
	ecs::villager_move_to_object::SetupMoveToObject(ActionOf(villager), ball, VillagerStates::ArrivedAtPickUpBallForDeadBall);
	auto& match = Entities().Get<FootballPitch>(pitch).match;
	match.state = rules::PlayState::Playing;
	match.roleTurns = rules::k_ReRoleTurns;
}

bool FootballSystem::Passes(entt::entity pitch, entt::entity villager, rules::Test test)
{
	const auto ball = BallOf(pitch);
	const auto& football = Entities().Get<const FootballPitch>(pitch);
	switch (test)
	{
	case rules::Test::TouchesBall:
		return Touches(villager, ball);
	case rules::Test::DoesNotTouchBall:
		return !Touches(villager, ball);
	case rules::Test::NearestToBall:
		return BallIndexOf(football, villager) == 0;
	case rules::Test::NotNearestToBall:
		return BallIndexOf(football, villager) > 0;
	case rules::Test::AtPlace:
		return Metres(PositionOf(villager), PlaceOf(pitch, villager)) < rules::k_AtPlaceDistance;
	case rules::Test::NotAtPlace:
		return !(Metres(PositionOf(villager), PlaceOf(pitch, villager)) < rules::k_AtPlaceDistance);
	case rules::Test::QuiteNearBall:
		return Metres(villager, ball) <= rules::k_QuiteNearBallDistance;
	case rules::Test::NotQuiteNearBall:
		return !(Metres(villager, ball) <= rules::k_QuiteNearBallDistance);
	}
	return false;
}

float FootballSystem::Priority(entt::entity pitch, entt::entity villager, rules::Role role, uint8_t task)
{
	const auto& football = Entities().Get<const FootballPitch>(pitch);
	const auto side = SideOf(football, villager);
	const auto me = PositionOf(villager);
	const auto toGoal = [&]() { return Metres(me, GoalPosition(pitch, rules::AttackedGoal(side))); };
	const auto toPlace = [&]() { return Metres(me, PlaceOf(pitch, villager)); };
	const auto toBall = [&]() { return Metres(villager, BallOf(pitch)); };
	const auto pass = [&]() { return GoalIndexOf(football, villager) - 1 >= 0 ? FloatRand(0.2f) + 0.8f : 0.0f; };
	switch (role)
	{
	case rules::Role::Attacker:
		switch (static_cast<rules::AttackerTask>(task))
		{
		case rules::AttackerTask::Shoot:
			return rules::ShootPriority(toGoal());
		case rules::AttackerTask::Pass:
			return pass();
		case rules::AttackerTask::LobNearGoal:
		case rules::AttackerTask::GoToBall:
			return 0.0f;
		case rules::AttackerTask::Dribble:
			return rules::DribblePriority(toGoal());
		case rules::AttackerTask::RunForPass:
			return rules::RunForPassPriority(toBall());
		case rules::AttackerTask::GoToPlace:
			return rules::GoToPlacePriority(toPlace());
		case rules::AttackerTask::Idle:
		{
			const float ball = toBall();
			return rules::IdlePriority(ball, toPlace());
		}
		}
		break;
	case rules::Role::Defender:
		switch (static_cast<rules::DefenderTask>(task))
		{
		case rules::DefenderTask::Save:
			return FloatRand(0.3f) + 0.7f;
		case rules::DefenderTask::Clear:
			return FloatRand(0.4f) + 0.6f;
		case rules::DefenderTask::Dribble:
			return rules::DribblePriority(toGoal());
		case rules::DefenderTask::Pass:
			return pass();
		case rules::DefenderTask::Mark:
			return FloatRand(1.0f);
		case rules::DefenderTask::GoToBall:
			return 0.0f;
		case rules::DefenderTask::GoToPlace:
			return rules::GoToPlacePriority(toPlace());
		case rules::DefenderTask::Idle:
			return 1.0f - rules::GoToPlacePriority(toPlace());
		}
		break;
	case rules::Role::Goalkeeper:
		switch (static_cast<rules::GoalkeeperTask>(task))
		{
		case rules::GoalkeeperTask::Save:
		case rules::GoalkeeperTask::Clear:
		case rules::GoalkeeperTask::Look:
			return FloatRand(1.0f);
		case rules::GoalkeeperTask::Pass:
			return pass();
		case rules::GoalkeeperTask::GoToBall:
			return 0.0f;
		case rules::GoalkeeperTask::GoToPlace:
			return rules::GoToPlacePriority(toPlace());
		case rules::GoalkeeperTask::Idle:
			return 1.0f - rules::GoToPlacePriority(toPlace());
		}
		break;
	}
	return 0.0f;
}

void FootballSystem::DoTask(entt::entity pitch, entt::entity villager, VillagerStates role, uint8_t task)
{
	auto& registry = Entities();
	const auto ball = BallOf(pitch);
	auto& action = ActionOf(villager);
	const auto& football = registry.Get<const FootballPitch>(pitch);
	const auto side = SideOf(football, villager);
	const auto finishKick = [&]() {
		action.turnsUntilStateChange = 0;
		registry.Get<Footballer>(villager).chooseTask = true;
	};
	// The kicks at the goal: a shot, a lob near the goal, and a save or clear to the far end
	const auto kickAtGoal = [&](float scatter, bool perturb, float speedBase, float speedRange) {
		if (!CanKick(villager, ball))
		{
			GoToBall(pitch, villager, role);
			return;
		}
		auto goal = CoordsOf(GoalPosition(pitch, rules::AttackedGoal(side)));
		if (scatter > 0.0f)
		{
			goal.x = ScatterBy(goal.x, scatter);
			goal.z = ScatterBy(goal.z, scatter);
		}
		auto target = WorldOf(goal);
		if (perturb)
		{
			target = Perturb(target, villager);
		}
		Kick(pitch, ball, target, FloatRand(speedRange) + speedBase, true);
		finishKick();
	};
	const auto dribble = [&]() {
		if (!CanKick(villager, ball))
		{
			GoToBall(pitch, villager, role);
			return;
		}
		const auto me = WorldOf(CoordsOf(villager));
		const auto goal = GoalPosition(pitch, rules::AttackedGoal(side));
		auto towards = goal - me;
		const float length = std::sqrt(towards.y * towards.y + towards.x * towards.x + towards.z * towards.z);
		if (towards.x != 0.0f || towards.y != 0.0f || towards.z != 0.0f)
		{
			towards *= 1.0f / length;
		}
		const auto target = Perturb(WorldOf(CoordsOf(me + towards * (length * 0.2f))), villager);
		Kick(pitch, ball, target, FloatRand(1.0f) + 6.0f, true);
		finishKick();
	};
	const auto goToPlace = [&]() {
		ecs::villager_home::SetupMoveTo(action, PlaceCoordsOf(pitch, villager), role);
		registry.Get<Footballer>(villager).chooseTask = true;
	};
	const auto goalkeeperSave = [&]() {
		if (!CanKick(villager, ball))
		{
			GoToBall(pitch, villager, role);
			return;
		}
		auto target = WorldOf(CoordsOf(pitch));
		for (int i = 0; i < 3; ++i)
		{
			target = Perturb(target, villager);
		}
		Kick(pitch, ball, target, FloatRand(1.0f) + 9.0f, true);
		finishKick();
	};
	switch (role)
	{
	case VillagerStates::FootballAttacker:
		switch (static_cast<rules::AttackerTask>(task))
		{
		case rules::AttackerTask::Shoot:
			kickAtGoal(0.0f, true, 12.0f, 4.0f);
			break;
		case rules::AttackerTask::Pass:
			Pass(pitch, villager, role);
			break;
		case rules::AttackerTask::LobNearGoal:
			kickAtGoal(4.0f, true, 3.0f, 1.0f);
			break;
		case rules::AttackerTask::Dribble:
			dribble();
			break;
		case rules::AttackerTask::RunForPass:
			RunForPass(pitch, villager);
			break;
		case rules::AttackerTask::GoToBall:
			GoToBall(pitch, villager, role);
			break;
		case rules::AttackerTask::GoToPlace:
			goToPlace();
			break;
		case rules::AttackerTask::Idle:
			IdleOutfield(pitch, villager);
			break;
		}
		break;
	case VillagerStates::FootballDefender:
		switch (static_cast<rules::DefenderTask>(task))
		{
		case rules::DefenderTask::Save:
		case rules::DefenderTask::Clear:
			kickAtGoal(10.0f, false, 8.0f, 3.0f);
			break;
		case rules::DefenderTask::Dribble:
			dribble();
			break;
		case rules::DefenderTask::Pass:
			Pass(pitch, villager, role);
			break;
		case rules::DefenderTask::Mark:
		{
			// A home defender marks the away team's attacker, and an away defender the home team's attacker
			const auto target = football.attackerMarks.at(side == rules::Side::Home ? 1 : 0);
			if (target != entt::null && IsThere(target))
			{
				ecs::villager_move_to_object::SetupMoveToObject(action, target, VillagerStates::FootballDefender);
			}
			registry.Get<Footballer>(villager).chooseTask = true;
			break;
		}
		case rules::DefenderTask::GoToBall:
			GoToBall(pitch, villager, role);
			break;
		case rules::DefenderTask::GoToPlace:
			goToPlace();
			break;
		case rules::DefenderTask::Idle:
			IdleOutfield(pitch, villager);
			break;
		}
		break;
	case VillagerStates::FootballGoalie:
		switch (static_cast<rules::GoalkeeperTask>(task))
		{
		case rules::GoalkeeperTask::Save:
		case rules::GoalkeeperTask::Clear:
			goalkeeperSave();
			break;
		case rules::GoalkeeperTask::Look:
		case rules::GoalkeeperTask::Idle:
			GoalkeeperLook(pitch, villager);
			break;
		case rules::GoalkeeperTask::Pass:
			// The goalkeeper passes only near its goal
			if (Metres(PositionOf(villager), PlaceOf(pitch, villager)) > rules::k_GoalkeeperPassReach)
			{
				registry.Get<Footballer>(villager).chooseTask = true;
				break;
			}
			Pass(pitch, villager, role);
			break;
		case rules::GoalkeeperTask::GoToBall:
			GoToBall(pitch, villager, role);
			break;
		case rules::GoalkeeperTask::GoToPlace:
			ecs::villager_home::SetupMoveTo(action, GoalCoords(pitch, rules::KeptGoal(side)), role);
			registry.Get<Footballer>(villager).chooseTask = true;
			break;
		}
		break;
	default:
		break;
	}
}

void FootballSystem::GoToBall(entt::entity pitch, entt::entity villager, VillagerStates role)
{
	auto& registry = Entities();
	const auto ball = BallOf(pitch);
	auto& action = ActionOf(villager);
	if (CanKick(villager, ball))
	{
		registry.Get<Footballer>(villager).chooseTask = true;
		return;
	}
	if (registry.AllOf<InPhysics>(ball))
	{
		// The player runs to where the ball will come down, but not each time
		const bool run = role == VillagerStates::FootballGoalie ? Rand(10) >= 8 : Rand(3) != 0;
		if (run)
		{
			const auto& kicked = registry.Get<const Ball>(ball);
			const auto destination = kicked.destination.value_or(PositionOf(ball));
			ecs::villager_home::SetupMoveTo(action, CoordsOf(destination), role);
			Living().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::FootballMoveToBall, true);
			return;
		}
	}
	ecs::villager_move_to_object::SetupMoveToObject(action, ball, role);
}

void FootballSystem::Pass(entt::entity pitch, entt::entity villager, VillagerStates role)
{
	auto& registry = Entities();
	const auto ball = BallOf(pitch);
	if (!CanKick(villager, ball))
	{
		GoToBall(pitch, villager, role);
		return;
	}
	const auto& football = registry.Get<const FootballPitch>(pitch);
	const auto index = GoalIndexOf(football, villager) - 1;
	if (index >= 0)
	{
		const auto& ranks = football.ranks.at(static_cast<size_t>(SideOf(football, villager)));
		const auto mate = ranks.at(static_cast<size_t>(index));
		// The pass goes to where the team mate walks to, or to where it stands
		auto target = PositionOf(mate);
		if (IsMoving(mate))
		{
			if (const auto* walk = registry.TryGet<const WallHug>(mate))
			{
				target = WorldOf(map_coords::MapCoords {walk->goalPlace.x, walk->goalPlace.y, 0.0f});
			}
		}
		if (Metres(PositionOf(ball), target) > rules::k_ShortestPass)
		{
			Kick(pitch, ball, target, FloatRand(1.0f) + 6.0f, true);
			ActionOf(villager).turnsUntilStateChange = 0;
		}
	}
	registry.Get<Footballer>(villager).chooseTask = true;
}

void FootballSystem::RunForPass(entt::entity pitch, entt::entity villager)
{
	auto& action = ActionOf(villager);
	action.turnsUntilStateChange = 0;
	const auto& football = Entities().Get<const FootballPitch>(pitch);
	const auto side = SideOf(football, villager);
	const auto kept = glm::xz(GoalPosition(pitch, rules::KeptGoal(side)));
	const auto attacked = glm::xz(GoalPosition(pitch, rules::AttackedGoal(side)));
	const float ahead = x87::Add(FloatRand(0.2f), 0.6);
	const float across = FloatRand(18.0f) - 9.0f;
	const auto point = rules::PassPlace(kept, attacked, ahead, across);
	ecs::villager_home::SetupMoveTo(action, point, VillagerStates::FootballAttacker);
	Entities().Get<Footballer>(villager).chooseTask = true;
}

void FootballSystem::IdleOutfield(entt::entity pitch, entt::entity villager)
{
	const auto ball = BallOf(pitch);
	ecs::systems::villager_reactions::LookAt(villager, PositionOf(ball));
	const auto& football = Entities().Get<const FootballPitch>(pitch);
	if (ActionOf(villager).turnsUntilStateChange >= rules::k_IdleTurns || BallIndexOf(football, villager) < 2)
	{
		Entities().Get<Footballer>(villager).chooseTask = true;
	}
}

void FootballSystem::GoalkeeperLook(entt::entity pitch, entt::entity villager)
{
	const auto ball = BallOf(pitch);
	ecs::systems::villager_reactions::LookAt(villager, PositionOf(ball));
	auto& player = Entities().Get<Footballer>(villager);
	++player.lookTurns;
	if (player.lookTurns > rules::k_GoalkeeperLookTurns)
	{
		player.chooseTask = true;
	}
	if (Metres(ball, villager) < rules::k_GoalkeeperLookReach)
	{
		player.chooseTask = true;
	}
}
