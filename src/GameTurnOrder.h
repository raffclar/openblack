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

#include <algorithm>
#include <array>
#include <optional>
#include <span>

/// The order of the steps of one game turn. The game does its turn in a fixed order of stages, and many systems depend
/// on that order: what one stage changes, the next stages see in the same turn. Each step of openblack's turn has the
/// stage of the game that it does. A step whose stage is not known keeps its place between its neighbours. The order
/// keeps no game state, so that the game and the tests use it both.
namespace openblack::game_turn
{

/// The stages of the game's turn, in the game's order
enum class Stage : uint8_t
{
	/// The player's input of the turn, before the turn count moves on
	Input,
	/// The turn count moves on
	StartTurn,
	/// The storms move on, and the snow on the land lies and melts
	Atmosphere,
	/// Each player in turn: its temple, its towns, its computer mind, its influence and its alignment
	Players,
	Dances,
	/// The fields, the fish farms, the things that walk along tracks and the footpaths that are found
	Lists,
	/// The forests' trees grow and spread
	Forests,
	/// The living: creatures, villagers and animals
	Living,
	Fires,
	Reactions,
	/// The miracles: the spell icons charge, the upkeep is paid, and each miracle takes its turn
	Spells,
	/// The particle effects that are not part of a miracle
	Particles,
	Fireflies,
	/// The bodies that fly, collide and come to rest
	Physics,
	/// The end of the turn of the particle effects, which also moves the vortices on
	ParticleEnd,
	/// The scripts, then the script fade
	Scripts,
	/// The advisors, the tooltips and the banter of the help
	Help,
	/// The help's count of what the player did
	HelpProfile,
	/// The time of day
	TimeOfDay,
	/// The weather things follow their storms
	WeatherThings,
	/// The scrolls and signs of the scripts
	Highlights,
	/// The climates
	Climates,
	/// What the hand holds
	Hand,
	/// The reward chests
	Rewards,
	/// The guidance of the advisors
	Guidance,
	/// The advisors confirm what the player does, as when they cheer the player's turning of the camera
	Confirmations,
	/// The camera's stacked views let go of things that are not there now
	CameraChecks,
	/// The pieces broken off buildings
	Fragments,
	/// The moods of the custom music
	MusicMoods,
	/// The end of the turn: the sounds, the ambience, the autosave, then the end of the game
	EndTurn,
	/// After the turn: a walker that met an obstacle tries its walk both ways round
	AfterTurn,
};

/// The steps of openblack's turn
enum class Step : uint8_t
{
	/// The things that the frames draw between their last two turns stand at the place of the last turn
	DrawnPlaces,
	PlayerCommands,
	HandStillness,
	/// The influence that each god's hand keeps past the border of the god's influence
	VirtualInfluence,
	/// The hand's own work of the turn: it can send commands, which a later turn does
	HandInterface,
	TurnCheering,
	Clock,
	/// The count of the turns that the reactions keep
	ReactionClock,
	TurnStartPlaces,
	InfluenceTurnStart,
	Sharks,
	Storms,
	Snow,
	Players,
	ComputerPlayers,
	/// The power that each tribe gives each player
	TribalPowers,
	Influence,
	PlayerStatistics,
	Heartbeats,
	TempleLooks,
	WorshipSiteLooks,
	PlayerAlignment,
	/// The end of a skirmish or a network game
	GameEnd,
	/// The gods of a network game get the winning conditions of the game
	GameConditions,
	VillageTotems,
	/// The flags over the towns' stores. The game draws them and keeps no object for them: openblack makes their entities
	/// in the turn, so that no frame makes an entity of the world.
	TownFlags,
	ChimneySmoke,
	Dances,
	Fields,
	FishFarms,
	WalkPaths,
	/// The mists that a script fades take a step of their fades
	Mists,
	PuzzleGames,
	Footpaths,
	ForestGrowth,
	/// The parts of the creatures' turns that are not for one creature, before the turn of the living
	CreaturesTurnStart,
	CreaturePhysiology,
	CreaturePens,
	/// The breath of a creature goes back to the resting period of its size
	CreatureBreath,
	CreatureBodies,
	CreatureSkin,
	Leashes,
	/// The first part of a creature's mind: it looks about and sees to its emergencies
	CreatureMinds,
	/// The second part of a creature's mind: its desires change, and it decides when it has no plan
	CreatureDesires,
	CreaturePlans,
	/// The last part of a creature's mind: it does the step of its agenda
	CreatureAgendas,
	CreatureLearning,
	CreatureLocomotion,
	CreatureObjectActions,
	CreatureFights,
	CreatureFizz,
	CreatureHelp,
	/// The drawn shape of a creature's body follows what it became
	CreatureMorph,
	/// The motes and the smoke of a creature's alignment
	CreatureMotes,
	/// The blood of a creature's wounds runs down its skin
	CreatureBlood,
	/// The body of a creature sways from what hit it
	CreatureSway,
	/// The animations of what a creature does with things go on by the time of the turn
	CreatureBodyActions,
	/// A fighter's moves and blows in the duel
	CreatureFightMoves,
	/// A fighter gets back stamina
	CreatureFightStamina,
	CreaturePoses,
	/// After the pose: the body's spheres knock things about, and the hands take hold of and let go of things
	CreatureHandMoments,
	/// The turn of the living: each creature, villager and animal in turn, the newest first (see living_turn::Walk)
	Living,
	CreatureWebPage,
	CreatureCarryOver,
	Fires,
	Reactions,
	Teleports,
	WorshipChants,
	Miracles,
	Shields,
	MiracleForests,
	Particles,
	Tornadoes,
	Fireflies,
	Physics,
	Vortices,
	/// The tutorial's hand demonstration plays on for the scripts by the time of the turn
	HandDemo,
	/// The opening's light and the clip of the hand that lifts the boy move on for the scripts by the time of the turn
	IntroLift,
	/// The scripts' clock of their spoken lines and texts, and the player's clicks on the dialogue
	ScriptVoices,
	Scripts,
	ScriptObjects,
	ScriptFade,
	Advisors,
	HandToolTips,
	Banter,
	HelpProfile,
	TipBubble,
	TimeOfDay,
	WeatherThings,
	/// The camera's bookmarks on things, and the flight to the one that the camera follows
	CameraBookmarks,
	ScriptHighlights,
	Climates,
	/// Every tenth turn the hand tells the player at this computer the belief that the towns gained in the player
	BeliefGained,
	/// Every tenth turn the advisors warn when a town of the player at this computer loses its belief in the player
	LosingBelief,
	HeldThing,
	/// The sparkles at the cameras of the other players of a network game
	PlayerSparkles,
	Rewards,
	Guidance,
	CameraChecks,
	BuildingPieces,
	MusicMoods,
	CreatureAlignment,
	SoundTags,
	Ambience,
	/// The clock of the music that the scripts start: its cues and its end, which the scripts wait on
	ScriptMusic,
	Music,
	Autosave,
	GameOver,
	GlobalStatistics,
	RealWeather,
	WalkerLookahead,
	/// A new creature, or one with a new body, gets the skeleton of its shape and its first pose
	CreatureShapes,
	/// The things that the turn made go into the map's cells
	MapCellsAtTurnEnd,
	InfluenceTurnEnd,
	/// The checksum of the synced world, as the turn left it, for a network game
	SyncChecksum,
	/// The run record of a tool sees the world as the turn left it, before the steps of the frame
	RunRecord,

	_Count
};

/// A step of the turn and the stage of the game that it does. No stage: the place of the step in the game's turn is not
/// known, and the step keeps its place between its neighbours.
struct Place
{
	Step step;
	std::optional<Stage> stage;
};

/// The turn, step by step
inline constexpr std::array k_Turn = std::to_array<Place>({
    // The frames draw the animals and the creatures that walk between their last two turns. Before anything else, the
    // turn puts them back at the places of the last turn, so that the turn sees the world as the game keeps it, at any
    // frame rate.
    {.step = Step::DrawnPlaces, .stage = std::nullopt},
    // The commands that the players sent come first in the turn, in the order that they went, before the hand does its
    // own work of the turn
    {.step = Step::PlayerCommands, .stage = Stage::Input},
    // The hand's count of the turns that it stays still is part of the player's input
    {.step = Step::HandStillness, .stage = Stage::Input},
    // After the stillness of each hand, the influence that the hand keeps past the border, before the hand's own work
    // and before the players and the computer minds act
    {.step = Step::VirtualInfluence, .stage = Stage::Input},
    // Then the hand does the work that it also does each frame: it takes a thing that it waited for, or gives what it
    // holds after a long press. The commands that it sends go to a later turn.
    {.step = Step::HandInterface, .stage = Stage::Input},
    // The speed of the hand, which blows the chimneys' smoke, comes from the same part of the player's input
    {.step = Step::ChimneySmoke, .stage = Stage::Input},
    {.step = Step::Clock, .stage = Stage::StartTurn},
    // The humans of a network game join with the commands of its first turn. Then the gods are known, and each god gets
    // the winning conditions of the game before a thing in the world can count for them.
    {.step = Step::GameConditions, .stage = Stage::StartTurn},
    // The living's reactions count their times by the same turn count as the game
    {.step = Step::ReactionClock, .stage = Stage::StartTurn},
    // The places of the turn's start, which the frames draw from. Each thing goes into its new map cells as it moves,
    // so the turn has no step for them.
    {.step = Step::TurnStartPlaces, .stage = Stage::StartTurn},
    {.step = Step::InfluenceTurnStart, .stage = Stage::StartTurn},
    {.step = Step::Sharks, .stage = Stage::StartTurn},
    {.step = Step::Storms, .stage = Stage::Atmosphere},
    {.step = Step::Snow, .stage = Stage::Atmosphere},
    {.step = Step::Players, .stage = Stage::Players},
    // A town's totem takes its turn with the buildings of its town
    {.step = Step::VillageTotems, .stage = Stage::Players},
    // Each player's stones drop their lost travellers in the turn of the player. Here, the stones of a miracle that is
    // not there go, as a safety net. The stones of a miracle go in the miracle's own turn, when it closes down.
    {.step = Step::Teleports, .stage = Stage::Players},
    {.step = Step::TownFlags, .stage = std::nullopt},
    {.step = Step::ComputerPlayers, .stage = Stage::Players},
    // After its computer mind, the power that each tribe gives a player becomes the most power that the tribe gave it
    {.step = Step::TribalPowers, .stage = Stage::Players},
    // The influence of the towns and the temples, and the borders that it draws
    {.step = Step::Influence, .stage = Stage::Players},
    {.step = Step::PlayerStatistics, .stage = Stage::Players},
    {.step = Step::Heartbeats, .stage = Stage::Players},
    // The temples' looks follow their players' alignments as they were at the end of the last turn
    {.step = Step::TempleLooks, .stage = Stage::Players},
    {.step = Step::WorshipSiteLooks, .stage = Stage::Players},
    // After the players' turns, a god with no temple is out, and the game is decided when one god is left
    {.step = Step::GameEnd, .stage = Stage::Players},
    // At the end of the players' turns: their alignments, then the alignment at the camera's eye
    {.step = Step::PlayerAlignment, .stage = Stage::Players},
    {.step = Step::Dances, .stage = Stage::Dances},
    {.step = Step::Fields, .stage = Stage::Lists},
    {.step = Step::FishFarms, .stage = Stage::Lists},
    {.step = Step::WalkPaths, .stage = Stage::Lists},
    // The game steps the fades of the mists in its lists of things, after the things that walk along tracks
    {.step = Step::Mists, .stage = Stage::Lists},
    // The puzzle games put back their pieces that moved, newest puzzle first
    {.step = Step::PuzzleGames, .stage = Stage::Lists},
    {.step = Step::Footpaths, .stage = Stage::Lists},
    {.step = Step::ForestGrowth, .stage = Stage::Forests},
    // Each living thing takes its own turn in the order of the game's list of the living. Each creature does the steps
    // of k_CreaturesTurn in its own turn. Each villager takes the step of its walk in its own turn.
    {.step = Step::CreaturesTurnStart, .stage = Stage::Living},
    {.step = Step::Living, .stage = Stage::Living},
    {.step = Step::Fires, .stage = Stage::Fires},
    {.step = Step::Reactions, .stage = Stage::Reactions},
    {.step = Step::WorshipChants, .stage = Stage::Spells},
    {.step = Step::Miracles, .stage = Stage::Spells},
    {.step = Step::Shields, .stage = Stage::Spells},
    {.step = Step::MiracleForests, .stage = Stage::Spells},
    {.step = Step::Particles, .stage = Stage::Particles},
    {.step = Step::Tornadoes, .stage = Stage::Particles},
    {.step = Step::Fireflies, .stage = Stage::Fireflies},
    {.step = Step::Physics, .stage = Stage::Physics},
    {.step = Step::Vortices, .stage = Stage::ParticleEnd},
    // The scripts see the hand demonstration play by the time of the turn alone. The game plays it in its frames, so
    // the step has no stage of the game's turn.
    {.step = Step::HandDemo, .stage = std::nullopt},
    // The same for the light of the opening and the hand that lifts the boy, which the game also moves in its frames
    {.step = Step::IntroLift, .stage = std::nullopt},
    // The scripts' clock moves on, and the dialogue takes the player's clicks, just before the scripts look at the world
    {.step = Step::ScriptVoices, .stage = Stage::Scripts},
    {.step = Step::Scripts, .stage = Stage::Scripts},
    {.step = Step::ScriptObjects, .stage = Stage::Scripts},
    {.step = Step::ScriptFade, .stage = Stage::Scripts},
    {.step = Step::Advisors, .stage = Stage::Help},
    {.step = Step::HandToolTips, .stage = Stage::Help},
    // The tip bubble closes when its sign is gone, after the hand's tooltip and before the banter
    {.step = Step::TipBubble, .stage = Stage::Help},
    {.step = Step::Banter, .stage = Stage::Help},
    {.step = Step::HelpProfile, .stage = Stage::HelpProfile},
    {.step = Step::TimeOfDay, .stage = Stage::TimeOfDay},
    {.step = Step::WeatherThings, .stage = Stage::WeatherThings},
    {.step = Step::CameraBookmarks, .stage = Stage::WeatherThings},
    {.step = Step::ScriptHighlights, .stage = Stage::Highlights},
    {.step = Step::Climates, .stage = Stage::Climates},
    {.step = Step::BeliefGained, .stage = Stage::Climates},
    {.step = Step::LosingBelief, .stage = Stage::Climates},
    {.step = Step::HeldThing, .stage = Stage::Hand},
    {.step = Step::PlayerSparkles, .stage = Stage::Hand},
    {.step = Step::Rewards, .stage = Stage::Rewards},
    {.step = Step::Guidance, .stage = Stage::Guidance},
    // The advisors cheer the player's turning of the camera
    {.step = Step::TurnCheering, .stage = Stage::Confirmations},
    {.step = Step::CameraChecks, .stage = Stage::CameraChecks},
    {.step = Step::BuildingPieces, .stage = Stage::Fragments},
    {.step = Step::MusicMoods, .stage = Stage::MusicMoods},
    {.step = Step::SoundTags, .stage = Stage::EndTurn},
    {.step = Step::Ambience, .stage = Stage::EndTurn},
    // The world times the scripts' music before the music heard follows it
    {.step = Step::ScriptMusic, .stage = Stage::EndTurn},
    {.step = Step::Music, .stage = Stage::EndTurn},
    // The autosave comes before the end of the game, so a save that is due in the last turn is written
    {.step = Step::Autosave, .stage = Stage::EndTurn},
    {.step = Step::GameOver, .stage = Stage::EndTurn},
    {.step = Step::GlobalStatistics, .stage = Stage::EndTurn},
    // The real-world weather follows the turn of the game, also once the game is over
    {.step = Step::RealWeather, .stage = Stage::AfterTurn},
    {.step = Step::WalkerLookahead, .stage = Stage::AfterTurn},
    // The frames only draw, so a creature that the turn made gets its skeleton and its first pose at the end of the turn
    {.step = Step::CreatureShapes, .stage = Stage::AfterTurn},
    // The frames only read the map's cells, so the things that the turn made go into them at the end of the turn
    {.step = Step::MapCellsAtTurnEnd, .stage = Stage::AfterTurn},
    {.step = Step::InfluenceTurnEnd, .stage = Stage::AfterTurn},
    // The checksum of the world that the turn left, which a network game sends after each turn
    {.step = Step::SyncChecksum, .stage = Stage::AfterTurn},
    {.step = Step::RunRecord, .stage = Stage::AfterTurn},
});

/// The steps of a creature's own turn, in the game's order. The turn of the living does them for each creature at the
/// place of the creature in its list.
inline constexpr std::array k_CreaturesTurn = std::to_array<Step>({
    // Its home, the size that it shows there, and the breath of that size
    Step::CreaturePens,
    Step::CreatureBreath,
    // The leash pulls it and keeps it near, before its body lives the turn
    Step::Leashes,
    Step::CreaturePhysiology,
    Step::CreatureMinds,
    // The advisor tells of it before its desires change
    Step::CreatureHelp,
    Step::CreatureDesires,
    // It decides before it does the step of its agenda
    Step::CreaturePlans,
    Step::CreatureAgendas,
    Step::CreatureLearning,
    Step::CreatureLocomotion,
    Step::CreatureObjectActions,
    Step::CreatureFights,
    // After its action: its body follows its fatness, and it fizzes on out of sight or back in
    Step::CreatureBodies,
    Step::CreatureFizz,
    // Then its body lives the turn: the marks heal, the shape follows what it became, the motes show its alignment, the blood
    // runs and the body sways.
    // A fighter moves and strikes, and the body takes its pose.
    Step::CreatureSkin,
    Step::CreatureMorph,
    Step::CreatureMotes,
    Step::CreatureBlood,
    Step::CreatureSway,
    // What it does with things plays on, by the time of the turn
    Step::CreatureBodyActions,
    Step::CreatureFightMoves,
    Step::CreaturePoses,
    // With the body in its new pose: what its body hits, and the things that its hands take hold of and let go of
    Step::CreatureHandMoments,
    // The rest of its turn: what a land keeps of it, its picture for its web page, the stamina of a fighter, and the
    // change of its alignment at the end
    Step::CreatureCarryOver,
    Step::CreatureWebPage,
    Step::CreatureFightStamina,
    Step::CreatureAlignment,
});

/// True when no step of the turn comes before a step of an earlier stage. The steps with no stage are not checked.
[[nodiscard]] constexpr bool InGameOrder(std::span<const Place> turn) noexcept
{
	std::optional<Stage> last;
	for (const auto& place : turn)
	{
		if (!place.stage.has_value())
		{
			continue;
		}
		if (last.has_value() && *place.stage < *last)
		{
			return false;
		}
		last = place.stage;
	}
	return true;
}

/// True when the step of the turn at the index plays after the game is decided. Then the game does only the input of the
/// players and the steps after the turn. The turn count of the game does not move on. In openblack the turn count moves
/// on, because the turns of a network game come by that count. A step with no stage follows the stage of the last step
/// before it that has a stage.
[[nodiscard]] constexpr bool PlaysOnceDecided(std::span<const Place> turn, size_t index) noexcept
{
	if (turn[index].step == Step::Clock)
	{
		return true;
	}
	std::optional<Stage> stage;
	for (size_t i = 0; i <= index; ++i)
	{
		if (turn[i].stage.has_value())
		{
			stage = turn[i].stage;
		}
	}
	return !stage.has_value() || *stage == Stage::Input || *stage == Stage::AfterTurn;
}

/// Does each step of the turn, in the order of the turn. After the game is decided, the turn does only the steps that
/// PlaysOnceDecided gives.
template <typename Do>
void PlayTurn(Do&& step, bool decided = false)
{
	for (size_t index = 0; index < k_Turn.size(); ++index)
	{
		if (!decided || PlaysOnceDecided(k_Turn, index))
		{
			step(k_Turn[index].step);
		}
	}
}

/// Does each step of the creatures' turns, in their order
template <typename Do>
void PlayCreaturesTurn(Do&& step)
{
	for (const auto creatureStep : k_CreaturesTurn)
	{
		step(creatureStep);
	}
}

static_assert(k_Turn.size() + k_CreaturesTurn.size() == static_cast<size_t>(Step::_Count),
              "Each step has one place in the turn or in the creatures' turn");
static_assert(InGameOrder(k_Turn), "The steps of the turn follow the game's stages");

/// The step that starts the part of the turn that measures the influence from where the hands were at the turn. The
/// game measures so for its full turn, the players' commands too. Then all computers get the same result.
inline constexpr Step k_TurnHandsInfluenceStart = Step::DrawnPlaces;
/// The step that ends that part. From this step on, the influence comes from where this computer shows its hand.
inline constexpr Step k_TurnHandsInfluenceEnd = Step::InfluenceTurnEnd;

/// True when the step measures the influence from where the hands were at the turn. The steps of a creature's own
/// turn are in the turn of the living, so they measure so too.
[[nodiscard]] constexpr bool MeasuresInfluenceFromTurnHands(Step step) noexcept
{
	bool measures = false;
	for (const auto& place : k_Turn)
	{
		if (place.step == k_TurnHandsInfluenceStart)
		{
			measures = true;
		}
		if (place.step == k_TurnHandsInfluenceEnd)
		{
			measures = false;
		}
		if (place.step == step)
		{
			return measures;
		}
	}
	return std::ranges::find(k_CreaturesTurn, step) != k_CreaturesTurn.end();
}

static_assert(k_Turn.front().step == k_TurnHandsInfluenceStart, "The influence comes from the turn hands for the full turn");

} // namespace openblack::game_turn
