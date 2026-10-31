/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "BenchmarkRecorder.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "TestbedGameStageInterface.h"
#include "TestbedScenarioRegistry.h"

namespace openblack::testbed_scenarios
{

/// How far spawning a scenario's crowd has got
struct CrowdProgress
{
	size_t spawned {0};
	size_t total {0};
	/// The time spent spawning, apart from the rest of the frames it took
	double spawnMs {0.0};
	uint32_t spawnFrames {0};

	[[nodiscard]] bool Done() const { return spawned >= total; }
};

/// How a crowd's frames are measured once it has all spawned
struct BenchmarkSettings
{
	/// Frames left to settle first, as the crowd's minds and routes start
	uint32_t warmUpFrames {120};
	/// The last so many frames are measured
	uint32_t frames {600};
	/// Where the results are written, with .json and .csv after it, once that many frames are measured, after which the
	/// game quits; for running a benchmark from the command line
	std::optional<std::filesystem::path> autoSave;
};

/// Plays a scenario on the testbed: loads the testbed afresh, which clears away everything on it, sets the time of
/// day, the weather and the creatures' body time, puts down the objects and creatures as the scenario has them, frames
/// the camera, then gives the commands in turn as their time comes. Everything goes through the game's systems.
class Runner
{
public:
	/// The stage is the game on the player's computer. The dedicated server has no stage, and the commands that stage the
	/// game fail there.
	explicit Runner(GameStageInterface* stage = nullptr) noexcept
	    : _stage(stage)
	{
	}

	/// Loads the testbed and starts the scenario on it, in place of any other
	void Start(const Scenario& scenario);
	/// Stops giving commands and puts back the settings the scenario changed: the body time, fainting, the footprints'
	/// smileys, the clock and whether anger starts fights. What is on the land stays, to look at or carry on with by hand.
	void Stop();
	/// Once a frame
	void Update(float seconds);
	/// Once a frame of a network game: only the camera follows the scenario. The commands come before the turns.
	void UpdateView() { UpdateCamera(); }
	/// Before each turn of a network game: the scenario steps on by the time of one turn and gives the commands that are
	/// due. The first turn after the layout gives no commands (see TurnStepOf).
	void BeforeTurn();
	/// Tools: the turn and the place in the scenario of each command, as the runner gives it
	using GiveObserver = std::function<void(uint32_t turn, size_t command)>;
	void ObserveGives(GiveObserver observer) { _giveObserver = std::move(observer); }
	/// Tools: the runner gives the commands of the scenario before the turns at which the computer of a game gave them,
	/// and not as its own timeline has them. A server that plays the commands of a game thus gives each command of the
	/// scenario with the game, also a command that waits for what only the computer of the game knows.
	void FollowGives(std::map<uint32_t, std::vector<size_t>> gives) { _followedGives = std::move(gives); }
	/// The turn count at which the last scenario was laid out, or none before the first scenario
	[[nodiscard]] std::optional<uint32_t> GetLayoutTurn() const { return _layoutTurn; }

	[[nodiscard]] bool IsRunning() const { return _running; }
	/// The scenario running, or last run
	[[nodiscard]] const Scenario* GetScenario() const { return _scenario; }
	[[nodiscard]] float GetSeconds() const { return _seconds; }
	[[nodiscard]] const Timeline& GetTimeline() const { return _timeline; }
	/// The scenario's creatures by their place in it, null where one is gone
	[[nodiscard]] std::span<const entt::entity> GetCreatures() const { return _creatures; }
	/// What became of the last commands, the newest last
	[[nodiscard]] const std::deque<std::string>& GetLog() const { return _log; }

	/// The scenario's crowd as it spawns, if it has one
	[[nodiscard]] std::optional<CrowdProgress> GetCrowdProgress() const;
	void SetBenchmarkSettings(BenchmarkSettings settings) { _benchmark = std::move(settings); }
	[[nodiscard]] const BenchmarkSettings& GetBenchmarkSettings() const { return _benchmark; }
	/// The frames still to settle before the crowd's frames are measured, and how many have been
	[[nodiscard]] uint32_t GetWarmUpLeft() const;
	[[nodiscard]] size_t GetMeasuredFrames() const { return _recorder ? _recorder->Count() : 0; }
	/// The frames measured so far, summed up every so often as they are
	[[nodiscard]] const benchmark::Results& GetLiveResults() const { return _liveResults; }
	[[nodiscard]] std::span<const benchmark::StageInfo> GetStages() const;
	/// How many entities there are of each kind
	[[nodiscard]] std::vector<std::pair<std::string, size_t>> EntityCounts() const;
	/// Writes the results of the frames measured so far to the path with .json and .csv after it, or to the benchmarks
	/// folder under the scenario's id and the time when none is given; the JSON file's path, or none if they couldn't be
	/// written
	std::optional<std::filesystem::path> SaveResults(std::optional<std::filesystem::path> base = std::nullopt);

	/// Puts the camera on a shot of one of the scenario's creatures, or of all of them; following and close ups keep
	/// up with the creature until the camera is let go
	void Frame(Shot shot, size_t creature, float distance = 1.0f);
	/// The camera is the player's again
	void ReleaseCamera() { _shot.reset(); }
	[[nodiscard]] std::optional<Shot> GetShot() const { return _shot; }

private:
	void SetUpEnvironment(const Environment& environment);
	/// The machine's clock gives the scenario's date, or the date it gave before a scenario set one
	void SetUpDate(const std::optional<RealDate>& date);
	void PlaceHighlights(const Scenario& scenario, glm::vec2 middle);
	void PlaceObjects(const Scenario& scenario, glm::vec2 middle);
	/// Lays the scenario's footpaths and links them to the objects they lead from
	void PlaceFootpaths(const Scenario& scenario, glm::vec2 middle);
	/// The scenario's temples and its flocks of the land's birds
	void PlaceBirds(const Scenario& scenario, glm::vec2 middle);
	/// The scenario's town's worship site is put up built and its totem raised to the share
	static void SetUpWorship(float share);
	/// The testbed tool forces each action of the computer gods that the scenario gives, at its turn
	void UpdateComputerActions();
	/// The thing that a forced action of a computer god gets, or none when the scenario has no such thing
	[[nodiscard]] entt::entity ComputerActionThingOf(const ComputerActionThing& thing, PlayerNames player) const;
	/// Marks the temple of the player as hit at the turn, as a thrown thing that harms the heart does
	void MarkTempleHit(PlayerNames player, uint32_t turn) const;
	/// The first player's statistics and their creature's tallies, as the scenario gives them
	static void SetUpStatistics(const Scenario& scenario);
	void PlaceCreatures(const Scenario& scenario, glm::vec2 middle);
	/// Spawns the next batch of the crowd
	void SpawnCrowd();
	void SpawnCrowdMember(size_t index);
	/// Takes the last frame's times from the profiler, once the crowd has spawned and settled
	void Measure();
	/// Starts the scenario's particle effect of that index
	[[nodiscard]] uint32_t StartParticle(size_t index) const;
	/// The scenario's dispensers and lone bubbles, after the testbed's grid stays or goes as it asks
	void PlaceDispensers(const Scenario& scenario);
	/// A dispenser made as a challenge's reward makes one, at a point on the land
	void PlaceRewardDispenser(const DispenserSetup& setup, glm::vec3 position);
	/// Casts the scenario's miracles as their times come, and lets go of held ones when theirs are up
	void UpdateMiracles(float seconds);
	/// The scenario's villagers set walking, and dropped on teleport stones, as their times come
	void UpdateVillagerWalks();
	/// The scenario's objects thrown when their throws are due
	void UpdateThrows();
	/// Asks again for the turns that did not start while their creatures walked to a stop
	void UpdateTurnsAsked();
	/// Casts the scenario's miracle of that index; the running miracle, or none
	entt::entity CastMiracle(size_t index);
	void UpdateParticles(float seconds);
	/// The scenario's vortices open when it is time and are told to fade out when it is time
	void UpdateVortices();
	/// The crossing file a vortex out of the last land would have left, written for a vortex into this one to read
	void WriteCrossingFile(std::span<const std::string_view> lines);
	void UpdateRewards();
	/// The needs and desires go on once the body and mind have started, and every frame for those that hold them
	void ApplyStates();
	void Give(const Command& command);
	/// The commands on things, of the player's hand and of the leashes; each returns what came of it
	std::string GiveObjectCommand(entt::entity creature, const Command& command);
	std::string GiveHandCommand(entt::entity creature, const Command& command);
	std::string GiveLeashCommand(entt::entity creature, const Command& command);
	/// The commands of the player's hand alone: a seed put in it, a gesture drawn with it
	std::string GivePlayerCommand(const Command& command);
	/// Tells one of the advisors what to do, as the scripts tell it
	[[nodiscard]] std::string GiveAdvisorCommand(const Command& command) const;
	/// Records a challenge as its script would, its picture of a view the camera is put at first
	[[nodiscard]] std::string RecordChallenge(const Command& command) const;
	/// The explorers' boat film played on the testbed, the dry dock at the command's point
	[[nodiscard]] std::string PlayBoatFilm(const Command& command) const;
	/// Starts a land's script as another script's call starts it; what came of that
	[[nodiscard]] std::string RunScript(const Command& command);
	/// Runs a script native with the command's arguments, and tells what it gave back
	[[nodiscard]] std::string CallNative(const Command& command);
	/// The value that a script gets for an argument: a number, or an object that the scripts' table knows
	[[nodiscard]] std::optional<lhvm::VMValue> ScriptValueOf(const ScriptArgument& argument);
	/// The script flock of the scenario's villagers and animals. The first call makes it. None without an object.
	[[nodiscard]] std::optional<entt::entity> ScriptFlock();
	[[nodiscard]] std::string StopScript(const Command& command);
	/// The hand goes over a fireball in flight to take hold of it; what came of that
	std::string HandTakeFireBall();
	std::string HandTapObject(size_t index);
	std::string ChargeIconWithSeedHeld(const Command& command);
	/// Once the hand is over the fireball, it taps it, or presses the action button with a seed in it
	void FinishTakingFireBall();
	/// Draws a gesture through the gesture recogniser, across the middle of the screen
	std::string DrawGesture(GestureType gesture);
	/// Once a drawn gesture is finished, logs what was recognised
	void WatchGesture();
	/// The commands of fights, and of being knocked out and brought round
	std::string GiveFightCommand(entt::entity creature, const Command& command);
	/// Creature Mode's and the Creature Cave's commands, as the player's keys and clicks give them
	std::string GiveCreatureModeCommand(entt::entity creature, const Command& command);
	/// A script opening or closing the scenery, and the player's hand laying a gate stone in the plinth
	void GiveSceneryCommand(const Command& command);
	/// The player's mouse: presses, moves and the wheel, made as the mouse's own events
	std::string GivePointerCommand(const Command& command);
	/// Where the middle of the body of the opponent of a creature (by its place in the scenario) is on a screen of a size
	[[nodiscard]] std::optional<glm::ivec2> OpponentOnScreen(size_t creature, glm::vec2 size) const;
	/// The middle of the body of a creature on the screen
	[[nodiscard]] static std::optional<glm::ivec2> BodyOnScreen(entt::entity creature, glm::vec2 size);
	[[nodiscard]] std::optional<glm::ivec2> RopeOnScreen(size_t creature, glm::vec2 size) const;
	/// Where the middle of the foot of the scenario's town's worship site altar is on the screen
	[[nodiscard]] std::optional<glm::ivec2> AltarOnScreen(glm::vec2 size) const;
	[[nodiscard]] std::optional<glm::ivec2> ObjectOnScreen(size_t object, float height, glm::vec2 size) const;
	/// A pixel over the body of the creature in the Creature Cave, as the cave finds it under the pointer, on a screen
	/// of a size
	[[nodiscard]] static std::optional<glm::ivec2> CaveCreatureOnScreen(glm::vec2 size);
	/// Moves the mouse on along a sweep, and lets go of it once the scenario's commands are done
	void UpdatePointer(float seconds);
	/// Moves the hand that the scenario holds still to the land under the pointer, at the same height above the land
	void MoveHeldHand(glm::ivec2 pointer);
	/// Where the cursor and the hand are on the screen, for the log
	[[nodiscard]] std::string HandOnScreen() const;
	/// Sets a desire or the stage of growing up, or rewards what the creature last did by the kind of thing it was to
	std::string TeachMind(entt::entity entity, const Command& command);
	/// Loads a mind file named as a scenario names it into a creature
	void LoadMindFile(entt::entity entity, std::string_view name);
	void UpdateCamera();
	[[nodiscard]] bool IsFree(size_t creature) const;
	[[nodiscard]] std::optional<entt::entity> CreatureAt(size_t index) const;
	/// Puts the pointer on the middle of a creature's body, where the interface picks it
	void PointAtBody(entt::entity creature) const;
	/// The scenario's objects by their place in it, while they are still about
	[[nodiscard]] std::optional<entt::entity> ObjectAt(size_t index) const;
	[[nodiscard]] std::optional<entt::entity> GlobeAt(size_t index) const;
	/// The totem of a scenario's object, a town centre
	[[nodiscard]] std::optional<entt::entity> TotemAt(size_t index) const;
	/// Builds the worship site of the town of a town centre at once
	void BuildWorshipSiteOf(entt::entity centre) const;
	void Log(std::string line);
	/// Every miracle's position and the land's height under it, for a scenario that logs them
	void LogMiracles();

	const Scenario* _scenario {nullptr};
	bool _running {false};
	float _seconds {0.0f};
	/// The first value that the last native call gave back, for a later call that uses it
	std::optional<lhvm::VMValue> _lastNativeResult;
	/// The turn count at which the scenario was laid out
	std::optional<uint32_t> _layoutTurn;
	/// When the made-up ping of the ping line started, in the scenario's seconds; none while it does not show
	std::optional<float> _demoPingSince;
	/// The game that the commands stage, or null on the dedicated server
	GameStageInterface* _stage {nullptr};
	/// Gives the made-up ping to the game while it shows
	void UpdateDemoPing() const;
	/// The debug option's bonus creatures as they were before the scenario took some as unlocked
	std::optional<creature_swap::Unlocked> _unlockedBefore;
	std::optional<int32_t> _landNumberBefore;
	/// The date that the machine's clock was pinned to (none for the wall clock's) before a scenario set the date
	std::optional<std::optional<int64_t>> _pinnedDateBefore;
	/// Whether the game was a skirmish before a scenario made it one
	std::optional<bool> _skirmishBefore;
	/// Whether the world played as a network game before a scenario made it one
	std::optional<bool> _networkGameBefore;
	/// The players that the scenario put in the game, which go when it stops
	std::vector<entt::entity> _addedPlayers;
	/// Frames since the hand went over a fireball to take hold of it, none while it isn't
	std::optional<int> _fireBallTakeFrames;
	/// The fireball the hand goes over to tap
	entt::entity _fireBallTaken {entt::null};
	/// When the miracles' positions are next logged, for a scenario that logs them
	float _nextMiracleLog {0.0f};
	Timeline _timeline;
	GiveObserver _giveObserver;
	std::optional<std::map<uint32_t, std::vector<size_t>>> _followedGives;
	glm::vec2 _middle {0.0f};
	std::vector<entt::entity> _creatures;
	/// The creatures that the scenario's filled slots got, in the order of their ids
	std::vector<entt::entity> _slotCreatures;
	/// The game turn at which the scenario started, and which of its computer actions the tool forced
	uint32_t _startTurn {0};
	std::vector<bool> _computerActionsDone;
	std::vector<entt::entity> _objects;
	/// The script flock of the scenario's villagers and animals, after a command gave it
	entt::entity _scriptFlock {entt::null};
	/// The objects and creatures that the scenario keeps as script variables keep them
	std::vector<entt::entity> _keptByScript;
	/// The objects that the native calls of the scenario gave back, in order
	std::vector<uint32_t> _madeByNatives;
	/// The globes that the scenario puts down on their own, in the order of its list
	std::vector<entt::entity> _globes;
	/// The scenario's temples, in its order
	std::vector<entt::entity> _temples;
	/// When each of the scenario's throws is next due, in seconds; none once done
	std::vector<std::optional<float>> _throwsDue;
	/// The turns to face a point that wait until their creatures stand
	std::vector<std::pair<entt::entity, glm::vec2>> _turnsAsked;
	/// For each object, when it next walks, whether it next walks out or back, and whether it has been dropped yet; and
	/// the walk it is on: where to and the game turn it set off, until it gets there
	struct ObjectWalk
	{
		std::optional<float> nextAt;
		bool outwards {true};
		bool dropped {false};
		glm::vec2 goal {0.0f};
		std::optional<uint32_t> setOffTurn;
	};
	std::vector<ObjectWalk> _walks;
	/// The scenario's particle effects, and the seconds since each was last started
	struct RunningParticle
	{
		uint32_t effect;
		float seconds;
	};
	std::vector<RunningParticle> _particles;
	/// The scenario's vortices: each once made, whether it has been told to fade out, and the crowd of its newcomers
	struct RunningVortex
	{
		entt::entity vortex {entt::null};
		bool fading {false};
		entt::entity crowd {entt::null};
		bool crowdBrokenUp {false};
	};
	std::vector<RunningVortex> _vortices;
	/// The scenario's reward chests, each once given
	std::vector<entt::entity> _rewards;
	/// The scenario's miracles: when each is next cast, and the one held in its hand until when
	struct RunningMiracle
	{
		std::optional<float> nextAt;
		std::optional<float> letGoAt;
		entt::entity spell {entt::null};
		/// A shield is broken then
		std::optional<float> breakAt;
		/// Cast by hand: when its button is pressed and let go, and where the hand is
		std::optional<float> pressAt;
		std::optional<float> releaseAt;
		/// After letting go, the hand sweeps on until then
		std::optional<float> sweepUntil;
		glm::vec3 hand {0.0f};
	};
	/// A miracle cast through the hand: its seed in the hand and the hand over its point, the button pressed later
	void StartHandCast(size_t index);
	/// The hand over a miracle's point or creature, as the scenario puts it
	[[nodiscard]] std::optional<ecs::systems::MagicSystemInterface::HandFrame> HandFrameFor(size_t index, glm::vec3 hand) const;
	std::vector<RunningMiracle> _miracles;
	/// Whether each creature's needs and desires have been set as it started
	std::vector<bool> _started;

	std::deque<std::string> _log;
	/// The gesture being drawn, and the count of gestures recognised before it
	std::optional<GestureType> _drawing;
	uint32_t _recognisedBefore {0};
	/// Whether the Action button went down on the miracles for the gesture being drawn, to come up once it is done
	bool _drawingPressedAction {false};

	/// The mouse moving along a sweep, in pixels a second, and for how much longer
	struct PointerSweep
	{
		glm::vec2 pixelsPerSecond {0.0f};
		float secondsLeft {0.0f};
		/// The part of a pixel left over from the frames before
		glm::vec2 remainder {0.0f};
	};
	std::optional<PointerSweep> _sweep;
	/// The hand that the scenario holds still over the land, and its height above the land
	std::optional<ecs::systems::MagicSystemInterface::HandFrame> _heldHand;
	float _heldHandHeight {0.0f};
	/// After the held hand gives its thing to a creature, a sweep of the pointer moves the held hand too
	bool _sweepMovesHeldHand {false};
	/// The hand's place on the screen is logged every frame for a while after the mouse's buttons change
	float _handWatchSeconds {0.0f};
	/// The testbed took the pointer, and gives it back when it is done; a pointer the debug inspector holds it leaves be
	bool _ownsPointer {false};
	/// The frames since the pointer last moved, for a press, which waits for the game to know what the pointer is over
	uint64_t _framesSincePointerMoved {1000};
	/// A press that came too soon after the pointer moved, made once the game knows what the pointer is over
	std::optional<Command> _pressWaiting;

	/// The crowd laid out, the next of it to spawn, its homes and towns as they have spawned, and how long it took
	std::vector<CrowdCreature> _crowdCreatures;
	VillageLayout _village;
	size_t _crowdNext {0};
	std::vector<entt::entity> _crowdAbodes;
	std::vector<entt::entity> _crowdEntities;
	CrowdProgress _crowdProgress;

	BenchmarkSettings _benchmark;
	uint32_t _settledFrames {0};
	std::unique_ptr<benchmark::Recorder> _recorder;
	benchmark::Results _liveResults;
	uint32_t _framesSinceSummary {0};
	bool _saved {false};
	/// A scenario made another player the god of this computer (see Command::Kind::OwnGod)
	bool _ownGodChanged {false};

	std::optional<Shot> _shot;
	size_t _shotCreature {0};
	float _shotDistance {1.0f};
};

} // namespace openblack::testbed_scenarios
