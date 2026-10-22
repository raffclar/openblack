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
	/// The hand's own work of the turn: it can send commands, which a later turn does
	HandInterface,
	TurnCheering,
	Clock,
	/// The count of the turns that the reactions keep
	ReactionClock,
	TurnStartPlaces,
	InfluenceTurnStart,
	MapCells,
	Sharks,
	Storms,
	Snow,
	Players,
	ComputerPlayers,
	Influence,
	PlayerStatistics,
	Heartbeats,
	TempleLooks,
	WorshipSiteLooks,
	PlayerAlignment,
	VillageTotems,
	/// The flags over the towns' stores, and the apple barrel at the player's store. The game draws them and keeps no
	/// object for them: openblack makes their entities in the turn, so that no frame makes an entity of the world.
	TownFlags,
	AppleBarrel,
	ChimneySmoke,
	Dances,
	Fields,
	FishFarms,
	WalkPaths,
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
	/// The motes and the smoke of a creature's alignment
	CreatureMotes,
	/// The blood of a creature's wounds runs down its skin
	CreatureBlood,
	/// The body of a creature sways from what hit it
	CreatureSway,
	/// A fighter's moves and blows in the duel
	CreatureFightMoves,
	/// A fighter gets back stamina
	CreatureFightStamina,
	CreaturePoses,
	/// The turn of the living: each creature, villager and animal in turn, the newest first (see living_turn::Walk)
	Living,
	MapCellsAfterLiving,
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
	HeldThing,
	Rewards,
	Guidance,
	CameraChecks,
	BuildingPieces,
	MusicMoods,
	CreatureAlignment,
	SoundTags,
	Ambience,
	Music,
	Autosave,
	GameOver,
	GlobalStatistics,
	WalkerLookahead,
	InfluenceTurnEnd,
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
    // The frames draw the animals between their last two turns. Before anything else, the turn puts them back at the
    // places of the last turn, so that the turn sees the world as the game keeps it, at any frame rate.
    {.step = Step::DrawnPlaces, .stage = std::nullopt},
    // The commands that the players sent come first in the turn, in the order that they went, before the hand does its
    // own work of the turn
    {.step = Step::PlayerCommands, .stage = Stage::Input},
    // The hand's count of the turns that it stays still is part of the player's input
    {.step = Step::HandStillness, .stage = Stage::Input},
    // Then the hand does the work that it also does each frame: it takes a thing that it waited for, or gives what it
    // holds after a long press. The commands that it sends go to a later turn.
    {.step = Step::HandInterface, .stage = Stage::Input},
    // The speed of the hand, which blows the chimneys' smoke, comes from the same part of the player's input
    {.step = Step::ChimneySmoke, .stage = Stage::Input},
    {.step = Step::Clock, .stage = Stage::StartTurn},
    // The living's reactions count their times by the same turn count as the game
    {.step = Step::ReactionClock, .stage = Stage::StartTurn},
    // The places of the turn's start, which the frames draw from, and the map's cells of what moved
    {.step = Step::TurnStartPlaces, .stage = Stage::StartTurn},
    {.step = Step::InfluenceTurnStart, .stage = Stage::StartTurn},
    {.step = Step::MapCells, .stage = Stage::StartTurn},
    {.step = Step::Sharks, .stage = Stage::StartTurn},
    {.step = Step::Storms, .stage = Stage::Atmosphere},
    {.step = Step::Snow, .stage = Stage::Atmosphere},
    {.step = Step::Players, .stage = Stage::Players},
    // A town's totem takes its turn with the buildings of its town
    {.step = Step::VillageTotems, .stage = Stage::Players},
    {.step = Step::TownFlags, .stage = std::nullopt},
    {.step = Step::AppleBarrel, .stage = std::nullopt},
    {.step = Step::ComputerPlayers, .stage = Stage::Players},
    // The influence of the towns and the temples. The borders that it draws and the influence that the hand keeps are
    // also in this step.
    {.step = Step::Influence, .stage = Stage::Players},
    {.step = Step::PlayerStatistics, .stage = Stage::Players},
    {.step = Step::Heartbeats, .stage = Stage::Players},
    // The temples' looks follow their players' alignments as they were at the end of the last turn
    {.step = Step::TempleLooks, .stage = Stage::Players},
    {.step = Step::WorshipSiteLooks, .stage = Stage::Players},
    // At the end of the players' turns: their alignments, then the alignment at the camera's eye
    {.step = Step::PlayerAlignment, .stage = Stage::Players},
    {.step = Step::Dances, .stage = Stage::Dances},
    {.step = Step::Fields, .stage = Stage::Lists},
    {.step = Step::FishFarms, .stage = Stage::Lists},
    {.step = Step::WalkPaths, .stage = Stage::Lists},
    {.step = Step::Footpaths, .stage = Stage::Lists},
    {.step = Step::ForestGrowth, .stage = Stage::Forests},
    // Each living thing takes its own turn in the order of the game's list of the living. Each creature does the steps
    // of k_CreaturesTurn in its own turn. Each villager takes the step of its walk in its own turn.
    {.step = Step::CreaturesTurnStart, .stage = Stage::Living},
    {.step = Step::Living, .stage = Stage::Living},
    {.step = Step::MapCellsAfterLiving, .stage = Stage::Living},
    {.step = Step::Fires, .stage = Stage::Fires},
    {.step = Step::Reactions, .stage = Stage::Reactions},
    {.step = Step::Teleports, .stage = std::nullopt},
    {.step = Step::WorshipChants, .stage = Stage::Spells},
    {.step = Step::Miracles, .stage = Stage::Spells},
    {.step = Step::Shields, .stage = Stage::Spells},
    {.step = Step::MiracleForests, .stage = Stage::Spells},
    {.step = Step::Particles, .stage = Stage::Particles},
    {.step = Step::Tornadoes, .stage = Stage::Particles},
    {.step = Step::Fireflies, .stage = Stage::Fireflies},
    {.step = Step::Physics, .stage = Stage::Physics},
    {.step = Step::Vortices, .stage = Stage::ParticleEnd},
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
    {.step = Step::HeldThing, .stage = Stage::Hand},
    {.step = Step::Rewards, .stage = Stage::Rewards},
    {.step = Step::Guidance, .stage = Stage::Guidance},
    // The advisors cheer the player's turning of the camera
    {.step = Step::TurnCheering, .stage = Stage::Confirmations},
    {.step = Step::CameraChecks, .stage = Stage::CameraChecks},
    {.step = Step::BuildingPieces, .stage = Stage::Fragments},
    {.step = Step::MusicMoods, .stage = Stage::MusicMoods},
    {.step = Step::SoundTags, .stage = Stage::EndTurn},
    {.step = Step::Ambience, .stage = Stage::EndTurn},
    {.step = Step::Music, .stage = Stage::EndTurn},
    // The autosave comes before the end of the game, so a save that is due in the last turn is written
    {.step = Step::Autosave, .stage = Stage::EndTurn},
    {.step = Step::GameOver, .stage = Stage::EndTurn},
    {.step = Step::GlobalStatistics, .stage = Stage::EndTurn},
    {.step = Step::WalkerLookahead, .stage = Stage::AfterTurn},
    {.step = Step::InfluenceTurnEnd, .stage = Stage::AfterTurn},
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
    // Then its body lives the turn: the marks heal, the motes show its alignment, the blood runs and the body sways.
    // A fighter moves and strikes, and the body takes its pose.
    Step::CreatureSkin,
    Step::CreatureMotes,
    Step::CreatureBlood,
    Step::CreatureSway,
    Step::CreatureFightMoves,
    Step::CreaturePoses,
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

/// Does each step of the turn, in the order of the turn
template <typename Do>
void PlayTurn(Do&& step)
{
	for (const auto& place : k_Turn)
	{
		step(place.step);
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

} // namespace openblack::game_turn
