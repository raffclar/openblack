/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// The rules of the football that the free Soccer Add-On switches on: the switch, the teams, the match on the pitch,
/// the crowd and its wave, and how each footballer picks what it does next. Pure rules, free of the game's state, so
/// that tests can check them with made-up values.
namespace openblack::ecs::football_rules
{

/// The turns in one second of the game
inline constexpr uint32_t k_TurnsPerSecond = 10;
/// The ball goes when it is farther than this from its pitch, in metres
inline constexpr float k_BallLostDistance = 100.0f;
/// The ball goes when a god holds it farther than this from its pitch, in metres
inline constexpr float k_BallInHandLostDistance = 80.0f;
/// With too few players, the players go home after this many turns of waiting (180 seconds)
inline constexpr uint32_t k_SendHomeTurns = 180 * k_TurnsPerSecond;
/// With enough players, the ball goes on the centre spot after this many turns (30 seconds)
inline constexpr uint32_t k_KickOffWaitTurns = 30 * k_TurnsPerSecond;
/// The turns between two times that the teams get their roles again (15 seconds)
inline constexpr uint32_t k_ReRoleTurns = 15 * k_TurnsPerSecond;
/// After a goal, play starts again when this many turns are over
inline constexpr uint32_t k_GoalPauseTurns = 70;
/// A match of a town stops only when it has run longer than this many turns
inline constexpr uint32_t k_TownMatchShortestTurns = 1800;
/// The places of the crowd that do the wave
inline constexpr uint32_t k_WavePlaces = 40;
/// After the wave goes all the way round, it stops for this many turns
inline constexpr uint32_t k_WaveRestTurns = 256;
/// The most players on one team
inline constexpr uint32_t k_TeamSize = 5;
/// A match starts only with at least this many players on each team
inline constexpr uint32_t k_FewestPlayersToStart = 3;
/// The kick-off comes when fewer than this many players still walk
inline constexpr uint32_t k_WalkersThatStopTheKickOff = 5;
/// The angle between two places of the crowd: a twentieth of a half circle (9 degrees), as a float
inline constexpr float k_CrowdPlaceAngle = 0.15707963705062866f;
/// How far outside the edge of the pitch the crowd stands, in metres
inline constexpr float k_CrowdDistance = 2.0f;
/// The size of the random square round a place of the crowd, in metres
inline constexpr float k_CrowdScatter = 4.0f;
/// The size of the random square round the centre where a player waits for the kick-off, in metres
inline constexpr float k_KickOffScatter = 8.0f;
/// The speed of the kick that sends a dead ball back to the centre spot, in metres a second
inline constexpr float k_DeadBallKickSpeed = 10.0f;
/// A footballer that is nearer than this to its place on the pitch is at its place, in metres
inline constexpr float k_AtPlaceDistance = 2.0f;
/// A footballer that is this near to the ball or nearer is quite near it, in metres
inline constexpr float k_QuiteNearBallDistance = 5.0f;
/// A pass goes only to a point farther than this from the ball, in metres
inline constexpr float k_ShortestPass = 1.0f;
/// The goalkeeper passes only when it is this near to its goal or nearer, in metres
inline constexpr float k_GoalkeeperPassReach = 10.0f;
/// The goalkeeper looks for a new task when the ball comes nearer than this, in metres
inline constexpr float k_GoalkeeperLookReach = 6.0f;
/// The goalkeeper looks for a new task after it looks at the ball for more turns than this
inline constexpr uint8_t k_GoalkeeperLookTurns = 20;
/// An idle outfield player looks for a new task after this many turns
inline constexpr uint16_t k_IdleTurns = 600;
/// The turns that a footballer counts before its first task in a new role
inline constexpr uint16_t k_NewRoleTurns = 20;

/// Whether the game plays football: the add-on switch must be on, and the game must not be a multiplayer game. A
/// skirmish counts as a game for one player. The game reads this one time when a game starts.
[[nodiscard]] bool IsFootballOn(bool addOnSwitch, bool multiplayer);

/// What happens on a pitch
enum class PlayState : uint8_t
{
	WaitingForPlayers,
	Playing,
	Paused,
	Scored,
	Ended,
};

/// The two teams. The home team attacks the second goal of the goal model and keeps the first goal.
enum class Side : uint8_t
{
	Home,
	Away,
};

/// The other team
[[nodiscard]] Side Other(Side side);
/// The goal of the goal model that a team attacks
[[nodiscard]] uint32_t AttackedGoal(Side side);
/// The goal of the goal model that a team keeps
[[nodiscard]] uint32_t KeptGoal(Side side);

/// What a villager that comes to play becomes: the referee when there is none, then a home player, then an away
/// player, then one of the crowd
enum class Joins : uint8_t
{
	Referee,
	Home,
	Away,
	Crowd,
};
[[nodiscard]] Joins JoinAs(bool hasReferee, uint32_t homePlayers, uint32_t awayPlayers);
/// The team that a script adds a villager to: the home team when the script asks for it and that team has room, else the
/// away team when that team has room. None when no team has room.
[[nodiscard]] std::optional<Side> ScriptTeam(bool homeTeam, uint32_t homePlayers, uint32_t awayPlayers);

/// The angle round the pitch of a new place in the crowd, from the number of filled places that the new place makes
[[nodiscard]] float CrowdPlaceAngle(uint32_t filledPlaces);

/// A distance in whole map units moved by a random part of a metre, as the game moves a place on the pitch. The result
/// keeps the game's fixed point steps.
[[nodiscard]] int32_t Scatter(int32_t fixed, float metres);

/// The wave of the crowd: the next place that can start it, and the turns that it still rests
struct Wave
{
	uint32_t rest {0};
	uint32_t next {0};
};

/// One turn of the wave. While it rests, it counts down. Otherwise it gives the place that starts the wave this turn,
/// if a villager stands still there. After the last place, the wave rests.
[[nodiscard]] std::optional<uint32_t> StepWave(Wave& wave);

/// The match of a pitch, as the pitch keeps it from turn to turn
struct Match
{
	PlayState state {PlayState::WaitingForPlayers};
	/// The turns that the match waits for players, or plays
	uint32_t turns {0};
	/// The ball is on the centre spot, and the kick-off can come
	bool kickOffReady {false};
	/// The turns since the last goal
	uint32_t goalTurns {0};
	/// The turns since the teams got their roles
	uint32_t roleTurns {0};
	/// Each team's tally of goals, home first
	std::array<uint8_t, 2> score {};
	/// The last goal made the home team mourn
	bool homeMournedLastGoal {false};
	/// A match of the town, which stops when the town wants other things. A script can keep the pitch for itself.
	bool townMatch {true};
};

/// What one turn of a match that waits for players tells the pitch to do
struct WaitingTurn
{
	/// Each player goes home
	bool sendPlayersHome {false};
	/// The ball goes on the centre spot, unless it flies
	bool placeBall {false};
	/// The match starts: the teams get their places and roles
	bool kickOff {false};
};

/// One turn of a match that waits for players. With too few players on a team, the match counts to the time that
/// sends them home. With enough players, it counts to the time that puts the ball on the centre spot, then the kick-off
/// comes when few enough players still walk.
[[nodiscard]] WaitingTurn StepWaiting(Match& match, uint32_t homePlayers, uint32_t awayPlayers,
                                      const std::function<uint32_t()>& walkingPlayers);

/// A match plays on only while each team has a player and the two teams are not one player each
[[nodiscard]] bool PlaysOn(uint32_t homePlayers, uint32_t awayPlayers);

/// A match of a town counts its turns, and stops when it ran long enough and the town wants most of all something that
/// is not relaxation or play
[[nodiscard]] bool TownMatchStops(Match& match, bool townWantsPlay);

/// The teams get their roles again when the time between two times is over; else the count goes on
[[nodiscard]] bool RoleTimeIsOver(Match& match);

/// One turn of the pause after a goal: the match plays again when the pause is over
[[nodiscard]] bool StepGoalPause(Match& match);

/// A goal goes in one of the two goals. The team that keeps that goal mourns, and the other team celebrates. The game
/// adds the goal to the tally of the team that mourns. Whether the home team mourns.
bool ScoreGoal(Match& match, uint32_t goal);

/// A goal of the goal model, in the pitch's own space
struct GoalMouth
{
	/// The line of the goal, along the length of the pitch
	float line {0.0f};
	/// The top of the goal
	float height {0.0f};
	/// Half the width of the goal
	float halfWidth {0.0f};
};

/// Whether the ball went in a goal this turn: its way from where it was to where it is crosses the line of the goal
/// under the top and between the posts. The ball counts only after a kick that can score sends it.
[[nodiscard]] bool CrossesGoal(glm::vec3 from, glm::vec3 to, const GoalMouth& goal);

/// Whether the ball is in play: between the two ends of the pitch along its length. The sides of the pitch do not
/// count.
[[nodiscard]] bool IsOnPitch(float alongPitch, float halfLength);

/// The team whose first player fetches a dead ball: the one whose first player is farther from the ball. A tie sends
/// the away player.
[[nodiscard]] Side DeadBallFetcher(int32_t homeFirstDistance, int32_t awayFirstDistance);

/// The role of a footballer
enum class Role : uint8_t
{
	Attacker,
	Goalkeeper,
	Defender,
};

/// A footballer's role from its place when the team stands in order of distance to the goal that it attacks: the
/// nearer half attack, the last keeps goal, the others defend. The number gives the place among the attackers or the
/// defenders.
struct RoleAndNumber
{
	Role role {Role::Attacker};
	uint8_t number {0};
};
[[nodiscard]] RoleAndNumber RoleForRank(uint32_t rank, uint32_t teamSize);

/// The order of a team by distance, nearest first. Players at the same distance keep the order of the team.
[[nodiscard]] std::vector<size_t> OrderByDistance(std::span<const int32_t> distances);

/// The players that mark: the attacker nearest to the ball, then a defender only when it is nearer to the ball than
/// that attacker
struct Marks
{
	std::optional<size_t> attacker;
	std::optional<size_t> defender;
};
[[nodiscard]] Marks FindMarks(std::span<const Role> roles, std::span<const uint32_t> distances);

/// The side offset of a place in a line of players: none for one player, then evenly across the width
[[nodiscard]] float LineOffset(uint32_t place, int32_t players, float width);

/// A footballer's own place on the pitch, as a part of the way from the centre spot to the goal that it attacks (ahead)
/// and a part of that way to the side
struct PitchPlace
{
	float side {0.0f};
	float ahead {0.0f};
};
/// The place of an attacker: rows of five across most of the width, a third of the way to the goal
[[nodiscard]] PitchPlace AttackerPlace(uint32_t number, uint32_t teamSize);
/// The place of a defender: rows of three across half the width, behind the centre spot
[[nodiscard]] PitchPlace DefenderPlace(uint32_t number, uint32_t teamSize);
/// The point of a place on the ground, from the centre spot and the goal
[[nodiscard]] glm::vec2 PlacePoint(glm::vec2 centre, glm::vec2 goal, const PitchPlace& place);

/// Where an attacker runs to take a pass: most of the way from the goal that it keeps to the goal that it attacks, and
/// some metres to the side
[[nodiscard]] glm::vec2 PassPlace(glm::vec2 keptGoal, glm::vec2 attackedGoal, float ahead, float side);

/// How much an attacker wants to shoot: more the nearer it is to the goal
[[nodiscard]] float ShootPriority(float distanceToGoal);
/// How much a player wants to dribble: more the farther it is from the goal
[[nodiscard]] float DribblePriority(float distanceToGoal);
/// How much an attacker wants to run to take a pass: more the farther it is from the ball
[[nodiscard]] float RunForPassPriority(float distanceToBall);
/// How much a player wants to go back to its place: more the farther it is from it
[[nodiscard]] float GoToPlacePriority(float distanceToPlace);
/// How much an attacker wants to stay idle: more the farther it is from the ball and the nearer to its place
[[nodiscard]] float IdlePriority(float distanceToBall, float distanceToPlace);

/// The tests of a task. A player must pass the tests of a task to take it. After each turn of the task, the game makes
/// the first test again, and the player looks for a new task when the test fails.
enum class Test : uint8_t
{
	TouchesBall,
	DoesNotTouchBall,
	NearestToBall,
	NotNearestToBall,
	AtPlace,
	NotAtPlace,
	QuiteNearBall,
	NotQuiteNearBall,
};

/// The tasks of each role, in the game's order
enum class AttackerTask : uint8_t
{
	Shoot,
	Pass,
	LobNearGoal,
	Dribble,
	RunForPass,
	GoToBall,
	GoToPlace,
	Idle,
};
enum class DefenderTask : uint8_t
{
	Save,
	Clear,
	Dribble,
	Pass,
	Mark,
	GoToBall,
	GoToPlace,
	Idle,
};
enum class GoalkeeperTask : uint8_t
{
	Save,
	Clear,
	Look,
	Pass,
	GoToBall,
	GoToPlace,
	Idle,
};

/// The tests of each task of a role
struct TaskTests
{
	Test first {Test::TouchesBall};
	std::optional<Test> second;
};
[[nodiscard]] std::span<const TaskTests> TasksOf(Role role);

/// The task with the highest priority among the tasks whose tests pass. A tie keeps the first. None when no task is
/// open. The priorities come only for the open tasks, in order, because they can use random numbers.
[[nodiscard]] std::optional<uint8_t> ChooseTask(Role role, const std::function<bool(Test)>& passes,
                                                const std::function<float(uint8_t)>& priority);

/// The belief that a town gets when its match stops: the belief for a match, times a measure of how long the match ran
/// against the match length of the town's info, at most two times
[[nodiscard]] float MatchBelief(float beliefForMatch, uint32_t matchMeasure, float matchDuration);

/// Whether a state of a villager is a state of the match, which a player, the referee or one of the crowd has. Each of
/// these states makes the villager leave its team when the villager goes into a state outside the match.
[[nodiscard]] bool IsMatchState(VillagerStates state);

} // namespace openblack::ecs::football_rules
