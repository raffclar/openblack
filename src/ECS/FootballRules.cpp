/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootballRules.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <numeric>

#include "3D/MapCoords.h"
#include "Common/X87.h"

namespace rules = openblack::ecs::football_rules;

namespace
{

/// A value held between a low and a high limit, in the order of the game's two tests
[[nodiscard]] float Hold(float value, float low, float high)
{
	if (low <= value)
	{
		return high < value ? high : value;
	}
	return low;
}

constexpr std::array<rules::TaskTests, 8> k_AttackerTasks {{
    {.first = rules::Test::NearestToBall},
    {.first = rules::Test::NearestToBall},
    {.first = rules::Test::NearestToBall},
    {.first = rules::Test::NearestToBall},
    {.first = rules::Test::NotNearestToBall},
    {.first = rules::Test::DoesNotTouchBall},
    {.first = rules::Test::DoesNotTouchBall, .second = rules::Test::NotAtPlace},
    {.first = rules::Test::NotQuiteNearBall},
}};

constexpr std::array<rules::TaskTests, 8> k_DefenderTasks {{
    {.first = rules::Test::QuiteNearBall},
    {.first = rules::Test::QuiteNearBall},
    {.first = rules::Test::QuiteNearBall},
    {.first = rules::Test::NearestToBall},
    {.first = rules::Test::DoesNotTouchBall},
    {.first = rules::Test::DoesNotTouchBall},
    {.first = rules::Test::NotNearestToBall, .second = rules::Test::NotAtPlace},
    {.first = rules::Test::NotQuiteNearBall},
}};

constexpr std::array<rules::TaskTests, 7> k_GoalkeeperTasks {{
    {.first = rules::Test::QuiteNearBall, .second = rules::Test::AtPlace},
    {.first = rules::Test::QuiteNearBall, .second = rules::Test::AtPlace},
    {.first = rules::Test::AtPlace},
    {.first = rules::Test::QuiteNearBall},
    {.first = rules::Test::DoesNotTouchBall},
    {.first = rules::Test::NotAtPlace},
    {.first = rules::Test::NotQuiteNearBall, .second = rules::Test::AtPlace},
}};

/// The priority that the game starts from: a task with no priority above it is never taken
constexpr float k_NoPriority = -0.0001f;

} // namespace

bool rules::IsFootballOn(bool addOnSwitch, bool multiplayer)
{
	return addOnSwitch && !multiplayer;
}

rules::Side rules::Other(Side side)
{
	return side == Side::Home ? Side::Away : Side::Home;
}

uint32_t rules::AttackedGoal(Side side)
{
	return side == Side::Home ? 1u : 0u;
}

uint32_t rules::KeptGoal(Side side)
{
	return side == Side::Home ? 0u : 1u;
}

rules::Joins rules::JoinAs(bool hasReferee, uint32_t homePlayers, uint32_t awayPlayers)
{
	if (!hasReferee)
	{
		return Joins::Referee;
	}
	if (homePlayers < k_TeamSize)
	{
		return Joins::Home;
	}
	if (awayPlayers < k_TeamSize)
	{
		return Joins::Away;
	}
	return Joins::Crowd;
}

std::optional<rules::Side> rules::ScriptTeam(bool homeTeam, uint32_t homePlayers, uint32_t awayPlayers)
{
	if (homeTeam && homePlayers < k_TeamSize)
	{
		return Side::Home;
	}
	if (awayPlayers < k_TeamSize)
	{
		return Side::Away;
	}
	return std::nullopt;
}

float rules::CrowdPlaceAngle(uint32_t filledPlaces)
{
	return x87::Mul(static_cast<double>(filledPlaces), static_cast<double>(k_CrowdPlaceAngle));
}

int32_t rules::Scatter(int32_t fixed, float metres)
{
	float value = x87::Mul(static_cast<double>(fixed), 10.0);
	value = value * 1.52587890625e-05f;
	value = value + metres;
	value = value * 65536.0f;
	value = value / 10.0f;
	return map_coords::FtoL(value);
}

std::optional<uint32_t> rules::StepWave(Wave& wave)
{
	if (wave.rest != 0)
	{
		--wave.rest;
		return std::nullopt;
	}
	const uint32_t place = wave.next;
	++wave.next;
	if (wave.next >= k_WavePlaces)
	{
		wave.rest = k_WaveRestTurns;
		wave.next = 0;
	}
	return place;
}

rules::WaitingTurn rules::StepWaiting(Match& match, uint32_t homePlayers, uint32_t awayPlayers,
                                      const std::function<uint32_t()>& walkingPlayers)
{
	WaitingTurn out;
	++match.turns;
	if (homePlayers < k_FewestPlayersToStart || awayPlayers < k_FewestPlayersToStart)
	{
		out.sendPlayersHome = match.turns > k_SendHomeTurns;
		return out;
	}
	if (match.turns > k_KickOffWaitTurns)
	{
		out.placeBall = true;
		match.kickOffReady = true;
		match.turns = 0;
	}
	if (match.kickOffReady && walkingPlayers() < k_WalkersThatStopTheKickOff)
	{
		match.state = PlayState::Playing;
		match.roleTurns = k_ReRoleTurns;
		out.kickOff = true;
	}
	return out;
}

bool rules::PlaysOn(uint32_t homePlayers, uint32_t awayPlayers)
{
	return homePlayers != 0 && awayPlayers != 0 && (homePlayers != 1 || awayPlayers != 1);
}

bool rules::TownMatchStops(Match& match, bool townWantsPlay)
{
	++match.turns;
	return !townWantsPlay && match.turns > k_TownMatchShortestTurns;
}

bool rules::RoleTimeIsOver(Match& match)
{
	if (match.roleTurns < k_ReRoleTurns)
	{
		++match.roleTurns;
		return false;
	}
	match.roleTurns = 0;
	return true;
}

bool rules::StepGoalPause(Match& match)
{
	++match.goalTurns;
	if (match.goalTurns > k_GoalPauseTurns)
	{
		match.state = PlayState::Playing;
		match.roleTurns = k_ReRoleTurns;
		return true;
	}
	return false;
}

bool rules::ScoreGoal(Match& match, uint32_t goal)
{
	// The team that keeps the goal mourns. The tally counts the goal for that team.
	const bool homeMourns = goal == KeptGoal(Side::Home);
	match.state = PlayState::Scored;
	match.goalTurns = 0;
	match.homeMournedLastGoal = homeMourns;
	auto& tally = match.score.at(homeMourns ? 0 : 1);
	tally = static_cast<uint8_t>(tally + 1);
	return homeMourns;
}

bool rules::CrossesGoal(glm::vec3 from, glm::vec3 to, const GoalMouth& goal)
{
	const float dx = to.x - from.x;
	if (from.x < goal.line)
	{
		if (goal.line > to.x)
		{
			return false;
		}
	}
	else
	{
		if (goal.line <= to.x)
		{
			return false;
		}
		// A ball that starts on the line counts at once
		if (from.x == goal.line)
		{
			return true;
		}
	}
	const float part = (goal.line - from.x) / dx;
	if (goal.height < (to.y - from.y) * part + from.y)
	{
		return false;
	}
	const float across = (to.z - from.z) * part + from.z;
	return !(goal.halfWidth < across) && !(across < -goal.halfWidth);
}

bool rules::IsOnPitch(float alongPitch, float halfLength)
{
	return alongPitch <= halfLength && -halfLength <= alongPitch;
}

rules::Side rules::DeadBallFetcher(int32_t homeFirstDistance, int32_t awayFirstDistance)
{
	return static_cast<float>(homeFirstDistance) <= static_cast<float>(awayFirstDistance) ? Side::Away : Side::Home;
}

rules::RoleAndNumber rules::RoleForRank(uint32_t rank, uint32_t teamSize)
{
	if (rank == teamSize - 1)
	{
		return {.role = Role::Goalkeeper, .number = 0};
	}
	const uint32_t half = teamSize >> 1u;
	if (rank < half)
	{
		return {.role = Role::Attacker, .number = static_cast<uint8_t>(rank)};
	}
	return {.role = Role::Defender, .number = static_cast<uint8_t>(rank - half)};
}

std::vector<size_t> rules::OrderByDistance(std::span<const int32_t> distances)
{
	std::vector<size_t> order(distances.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::stable_sort(order, [&distances](size_t a, size_t b) { return distances[a] < distances[b]; });
	return order;
}

rules::Marks rules::FindMarks(std::span<const Role> roles, std::span<const uint32_t> distances)
{
	Marks marks;
	// The defender must beat the nearest attacker: the game does not start its search again
	uint32_t best = std::numeric_limits<uint32_t>::max();
	for (size_t i = 0; i < roles.size() && i < distances.size(); ++i)
	{
		if (roles[i] == Role::Attacker && distances[i] < best)
		{
			marks.attacker = i;
			best = distances[i];
		}
	}
	for (size_t i = 0; i < roles.size() && i < distances.size(); ++i)
	{
		if (roles[i] == Role::Defender && distances[i] < best)
		{
			marks.defender = i;
			best = distances[i];
		}
	}
	return marks;
}

float rules::LineOffset(uint32_t place, int32_t players, float width)
{
	if (players == 1)
	{
		return 0.0f;
	}
	if (players == 2)
	{
		return (static_cast<float>(place) - 0.5f) * width;
	}
	return width / static_cast<float>(players - 1) * static_cast<float>(static_cast<int32_t>(place)) - width * 0.5f;
}

rules::PitchPlace rules::AttackerPlace(uint32_t number, uint32_t teamSize)
{
	const uint32_t row = number / 5;
	const uint32_t place = number % 5;
	return {.side = LineOffset(place, static_cast<int32_t>(teamSize >> 1u), 0.8f),
	        .ahead = 0.33f - static_cast<float>(row) * 0.1f};
}

rules::PitchPlace rules::DefenderPlace(uint32_t number, uint32_t teamSize)
{
	const uint32_t row = number / 3;
	const uint32_t place = number % 3;
	return {.side = LineOffset(place, static_cast<int32_t>((teamSize - 1u) >> 1u), 0.5f),
	        .ahead = -0.76f - static_cast<float>(row) * 0.1f};
}

glm::vec2 rules::PlacePoint(glm::vec2 centre, glm::vec2 goal, const PitchPlace& place)
{
	const float dx = goal.x - centre.x;
	const float dz = goal.y - centre.y;
	const float sideX = -dz * place.side;
	const float sideZ = dx * place.side;
	const float z = dz * place.ahead + centre.y;
	return {dx * place.ahead + centre.x + sideX, z + sideZ};
}

glm::vec2 rules::PassPlace(glm::vec2 keptGoal, glm::vec2 attackedGoal, float ahead, float side)
{
	const float dx = attackedGoal.x - keptGoal.x;
	const float dz = attackedGoal.y - keptGoal.y;
	float normalX = -dz;
	float normalZ = dx;
	if (normalX != 0.0f || normalZ != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(normalX * normalX + dx * dx + 0.0f);
		normalX = inverse * normalX;
		normalZ = inverse * dx;
	}
	const float x = dx * ahead + keptGoal.x;
	const float z = ahead * dz + keptGoal.y;
	return {side * normalX + x, side * normalZ + z};
}

float rules::ShootPriority(float distanceToGoal)
{
	const float near = Hold(1.0f - Hold(distanceToGoal, 0.0f, 40.0f) * 0.025f, 0.1f, 1.0f);
	// The game also weighs the angle to the goal, but its sum always gives a factor of 1
	return 1.0f * near;
}

float rules::DribblePriority(float distanceToGoal)
{
	return Hold(Hold(distanceToGoal, 0.0f, 40.0f) * 0.025f, 0.1f, 1.0f);
}

float rules::RunForPassPriority(float distanceToBall)
{
	return Hold(Hold(distanceToBall, 0.0f, 20.0f) * 0.05f, 0.0f, 0.2f);
}

float rules::GoToPlacePriority(float distanceToPlace)
{
	return Hold(Hold(distanceToPlace, 0.0f, 20.0f) * 0.05f, 0.1f, 1.0f);
}

float rules::IdlePriority(float distanceToBall, float distanceToPlace)
{
	const float far = (Hold(distanceToBall, 10.0f, 40.0f) - 10.0f) * 0.025f;
	const float near = Hold(distanceToPlace, 0.0f, 10.0f);
	return Hold((1.0f - x87::Mul(near, 0.1)) * far, 0.0f, 1.0f);
}

std::span<const rules::TaskTests> rules::TasksOf(Role role)
{
	switch (role)
	{
	case Role::Attacker:
		return k_AttackerTasks;
	case Role::Defender:
		return k_DefenderTasks;
	case Role::Goalkeeper:
		return k_GoalkeeperTasks;
	}
	return {};
}

std::optional<uint8_t> rules::ChooseTask(Role role, const std::function<bool(Test)>& passes,
                                         const std::function<float(uint8_t)>& priority)
{
	std::optional<uint8_t> chosen;
	float best = k_NoPriority;
	const auto tasks = TasksOf(role);
	for (size_t i = 0; i < tasks.size(); ++i)
	{
		// The game makes both tests, also when the first test fails
		const bool first = passes(tasks[i].first);
		const bool second = !tasks[i].second.has_value() || passes(*tasks[i].second);
		if (first && second)
		{
			const float value = priority(static_cast<uint8_t>(i));
			if (best < value)
			{
				best = value;
				chosen = static_cast<uint8_t>(i);
			}
		}
	}
	return chosen;
}

float rules::MatchBelief(float beliefForMatch, uint32_t matchMeasure, float matchDuration)
{
	const auto turnsPerSecond = static_cast<float>(k_TurnsPerSecond);
	const int32_t lengthInSeconds = map_coords::FtoL(turnsPerSecond * matchDuration);
	const int32_t lengthInTurns = map_coords::FtoL(static_cast<float>(lengthInSeconds) * turnsPerSecond);
	const float ratio = static_cast<float>(matchMeasure) / static_cast<float>(lengthInTurns);
	const float measure = ratio < 2.0f ? ratio : 2.0f;
	return measure * beliefForMatch;
}

bool rules::IsMatchState(VillagerStates state)
{
	return state >= VillagerStates::FootballWalkToPosition && state <= VillagerStates::FootballMexicanWave;
}
