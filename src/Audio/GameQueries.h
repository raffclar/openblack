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
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

// What the audio reads from the rest of the game. The audio does not include the ECS: the game registers these
// functions (Game.cpp) and every query left unset gives the neutral value written next to it, which is the value of a
// game without that system (marked (inferred) where the original has no such state).
// The music part and the filters of the sample channels.

namespace openblack::audio
{

/// A script object as the audio sees it: the script object id (the entity's value in openblack)
using ThingId = uint32_t;

/// The camera as the audio reads it
struct CameraState
{
	/// The render camera's position, used for the 3D distances of ProcessThingMusic; the camera's MapCoords of
	/// ProcessAlignmentMusic are the same point
	glm::vec3 position {0.0f};
	/// The height above the land: y - the altitude byte of the camera's cell x 0.67 (not interpolated; y alone off the
	/// map), read by the alignment music
	float heightAboveGround {0.0f};
};

/// A town as the alignment music sees it
struct MusicTown
{
	uint32_t id {0}; ///< what the audio keeps (the town itself in the original)
	int tribe {0};   ///< The town's tribe, the index of the tribe music table
	/// GetDistanceInMetres between the camera's MapCoords and the town's: the x/z distance (hypotenuse(dx, dz)) times
	/// 10 / 65536
	float distance {0.0f};
};

/// A town as Guidance's town desire remarks read it
struct DesireTown
{
	ThingId id {0};            ///< the town (the key of Guidance's last things)
	glm::vec3 position {0.0f}; ///< where its desire is said
	/// The storage pit: the distance to the camera is measured from it; nullopt: no storage pit
	std::optional<glm::vec3> storagePit;
	uint32_t population {0}; ///< the sum of the two population counts the original tests
	struct Desire
	{
		float value {0.0f};
		uint32_t type {0}; ///< TOWN_DESIRE_INFO
		float raw {0.0f};  ///< the raw desire of that type
	};
	std::array<Desire, 17> desires {};
};

/// The local player's citadel as Guidance's worship site desire remarks read it
struct WorshipDesire
{
	struct Site
	{
		ThingId id {0};
		glm::vec3 position {0.0f};
		bool worshippers {false}; ///< the site has worshippers
		float foodDesire {0.0f};  ///< the site's desire for food
	};
	std::array<std::optional<Site>, 6> sites {}; ///< the citadel's worship sites
	glm::vec3 citadelPosition {0.0f};            ///< the citadel's position, where it is said
	float need {0.0f};                           ///< the citadel's need (capped at 1 by the caller)
};

/// What Guidance's UpdateHeartBeat reads of the local player
struct HeartBeatInput
{
	float protectionDesire {0.0f}; ///< the sum of the raw desire 3 (protection) over the player's towns
	float believers {0.0f};        ///< the proportion of the world population who believe in the player
	/// The sum of the active players' (and the neutral one's) influence power over the player's own (sum / own; 0 when
	/// the own is 0)
	float beliefShare {0.0f};
	/// For each other player's creature (whose interface is not the local one) whose nearest town is the local
	/// player's: that distance
	std::vector<uint32_t> enemyCreatureDistances;
	/// The local citadel's position when its heart is there and has life
	std::optional<glm::vec3> citadelHeart;
};

/// The worship site whose dance ProcessChantMusic hears: the nearest citadel within 150 of the camera's MapCoords, then
/// its nearest site with dancers within 100
struct ChantSite
{
	int tribe {0};        ///< the site's tribe, the index of the chant music table
	uint32_t dancers {0}; ///< the number of dancers of the site's dance
	/// The dance centre as a world point: x, z x 10 / 65536, y = land altitude + its altitude
	glm::vec3 position {0.0f};
	float ground {0.0f};       ///< the land altitude of that MapCoords
	float cameraGround {0.0f}; ///< the land altitude of the camera's MapCoords
};

/// What the sound events of an animation clip (audio::AnimationSounds::Fire) read of the animated thing
struct AnimatedThing
{
	glm::vec3 position {0.0f}; ///< the thing's 3D sound position (its position)
	/// Turns since the last state change (for P_THROWN / P_THROWN_VORTEX); 0 for a thing without one
	uint16_t turnsSinceStateChange {0};
	/// The villager's part (IsVillager): nullopt for an animal or any other thing
	struct Villager
	{
		bool alive {true};   ///< life > 0
		bool child {false};  ///< the voice 3 of the clip's group 1 (a child)
		bool female {false}; ///< the voice 2 (else 1, a man)
		/// The villager's abode (for the banter at the house): nullopt for none or a gone one
		std::optional<entt::entity> abode;
	};
	std::optional<Villager> villager;
};

/// A street lantern as the lanterns' SoundTags read it
struct StreetLantern
{
	entt::entity thing {};
	glm::vec3 position {0.0f};
	float height {0.0f}; ///< the lantern's height (the tag's offset (0, height, 0))
};

/// The weather at the camera, refreshed every camera update with the smoothed weather
struct CameraWeatherInfo
{
	int8_t temperature {0};
	int8_t rain {0};
	int8_t snow {0};
	int8_t overcast {0};
	int8_t windX {0};
	int8_t windZ {0};
};

struct GameQueries
{
	/// A full screen video is playing (ProcessMusic and the audio's game turn stop for it); only the intro and fall
	/// films set it, not the tips or the pre-intro. Filled with video::IsPlaying (ECS/AudioQueries.cpp). Unset: false.
	std::function<bool()> videoPlaying;
	/// The land number (SET_LAND_NUMBER); ProcessMusic plays nothing on land 6. Unset: 0.
	std::function<int()> landNumber;
	/// The game turn (ProcessAlignmentMusic needs > 20; the audio's game turn runs only after turn 5). Unset: 0.
	std::function<uint32_t()> turn;
	/// The game camera, nullopt when there is none. Unset: nullopt.
	std::function<std::optional<CameraState>()> camera;
	/// The script's wide screen is on. Unset: false (openblack has no help system yet).
	std::function<bool()> scriptWideScreen;
	/// The wide screen bars are moving. Unset: false.
	std::function<bool()> wideScreenChanging;
	/// The alignment (-1..1) of the player with the most influence where the camera is, written once a turn
	/// (x = clamp((alignment + 1) / 2, 0, 1), value = 2 - 2 (1 - x) - 1). Read by ProcessAtmosBanks (group 1 above -0.6,
	/// else 2) and the alignment music. Game: ecs::audio_queries (ecs::effects::alignment::GetInterfaceAlignment).
	/// Unset: 0, neutral (the audio's reset sets 0).
	std::function<float()> cameraAlignment;
	/// The nearest town of every player and the neutral one strictly closer than maxDistance (GetDistanceInMetres, the
	/// same as MusicTown::distance) that has a centre: its centre set, or a town centre among its abodes or planned
	/// buildings. Game: ecs::audio_queries (ecs::map_cells::GetNearestTownWithCentre from the camera's MapCoords, the
	/// town's Tribe). Unset: nullopt (the generic music of the alignment).
	std::function<std::optional<MusicTown>(float maxDistance)> nearestTown;
	/// The town the audio keeps, again, with its distance to the camera now: nullopt when it is no longer available.
	/// Game: ecs::audio_queries. Unset: nullopt.
	std::function<std::optional<MusicTown>(uint32_t id)> town;
	/// The world position of a script object, nullopt when it is gone. The original builds it from the thing's
	/// MapCoords: (x, land altitude + altitude above the land, z). Unset: nullopt.
	std::function<std::optional<glm::vec3>(ThingId thing)> thingPosition;
	/// The land altitude at a world x / z (a SoundTag made at a MapCoords sits on the land under it). Unset: 0.
	std::function<float(float x, float z)> landAltitude;
	/// The surface type at a world point (the "surface" attribute of the anim effect keys: the clips' events, the
	/// particle sounds with USESURFACE, the sound map's dump): ecs::sea_cells::GetSurfaceType, the one reading of the
	/// map's cells. Unset: 6 (off the map).
	std::function<int32_t(glm::vec3 point)> surfaceType;
	/// The smoothed weather (recalculated) at a point; every camera update stores it at the camera (the sound map's
	/// weather; audio::CameraWeather asks it at the camera). Game: ecs::audio_queries (weather::atmos::GetWeatherSmooth,
	/// the storms and climates of src/ECS/Weather). Unset: all 0 (no rain, snow nor wind).
	std::function<CameraWeatherInfo(glm::vec3 point)> weatherSmooth;
	/// The animated thing of a clip's sound events, nullopt when it is gone or has no position (nothing plays). Unset:
	/// nullopt.
	std::function<std::optional<AnimatedThing>(entt::entity thing)> animatedThing;
	/// The name of the animation clip ANM_ `index` (Data\SmallSounds.SAS is matched to the clips by it), nullopt when
	/// there is no such clip. Unset: nullopt (no clip has sounds).
	std::function<std::optional<std::string>(int32_t index)> animationClipName;
	/// Every street lantern, in the registry's order. Unset: none.
	std::function<std::vector<StreetLantern>()> streetLanterns;

	/// The branches of ProcessMusic that need systems openblack does not have yet. Each one is "the original branch
	/// returned non-zero" (it took the music); unset = false, so ProcessMusic goes on to the next one.
	/// The local creature fighting
	std::function<bool()> creatureFightMusic;
	/// The worship site of the chant music (GameMusic::ProcessChantMusic plays it). Game:
	/// ecs::audio_queries (map_cells::GetNearestCitadel, worship::citadel::FindNearestWorshipSite). Unset: nullopt (no
	/// chant)
	std::function<std::optional<ChantSite>()> chantSite;
	/// A creature leading a dance
	std::function<bool()> creatureDanceMusic;

	/// Inside the citadel (set on entering, cleared on leaving): PlaySoundEffect plays only the samples of user
	/// parameter 2 (also through PlayAnimationEffect), measures the 3D cull from the render camera and ProcessMusic
	/// plays the citadel's music (ProcessCitadelMusic). Game: openblack's temple interior (Locator::temple,
	/// TempleInteriorInterface::Active: ENTER_EXIT_CITADEL and the debug window). Unset: false.
	std::function<bool()> insideCitadel;
	/// The local player's alignment (read by ProcessCitadelMusic), -1..1. Game: ecs::audio_queries
	/// (ecs::effects::alignment::Get of PLAYER_ONE). Unset: 0 (neutral, a new game's without a profile).
	std::function<float()> localPlayerAlignment;
	/// The local interface's state: in the states 0x10, 0x16 and 0x17 the samples of user parameter 4 do not play.
	/// Unset: 0, none of them (openblack has no interface states).
	std::function<int()> interfaceState;

	// ---- Guidance and spooky voices (Guidance.h, SpookyVoices.h) ----

	/// A playground game. Unset: false.
	std::function<bool()> playgroundGame;
	/// A multiplayer game. Unset: false (openblack has no multiplayer).
	std::function<bool()> multiplayerGame;
	/// The help level read by Guidance's PlayNow: 0 with the help switched off (SET_HELP_SYSTEM; on after a reset),
	/// else the profile's HELP_LEVEL (3 when the profile has none). Unset: 3.
	std::function<int()> helpLevel;
	/// The player number of the local interface's player: the owner of Guidance's samples. Unset: 0.
	std::function<uint32_t()> localPlayerNumber;
	/// It is visually night. Unset: false.
	std::function<bool()> visualNight;
	/// The hand's MapCoords (inferred) as a world point. Unset: nullopt (no remark that needs it).
	std::function<std::optional<glm::vec3>()> handPosition;
	/// The point is inside the camera's view. Unset: false.
	std::function<bool(glm::vec3 point)> pointOnScreen;
	/// Every player's towns for the town desire remarks (in player order). Unset: none (openblack's towns have no
	/// desires yet).
	std::function<std::vector<DesireTown>()> desireTowns;
	/// The local player's citadel for the worship site desire remarks. Unset: nullopt (no citadel desires).
	std::function<std::optional<WorshipDesire>()> worshipSites;
	/// The nearest town within maxDistance of a point (for PlayResourceDropRemark): strictly nearer, every player and the
	/// neutral one; the town, nullopt for none. Game: ecs::audio_queries (ecs::map_cells::GetNearestTown). Unset:
	/// nullopt (no town).
	std::function<std::optional<ThingId>(glm::vec3 point, float maxDistance)> nearestTownAt;
	/// That town's three values of a RESOURCE_RAIN_TYPE (the resource drop sound's sample choice: three town desire
	/// arrays of the desires 0, 1 and 10), each summed, indexed food, wood, rain. Unset: nullopt, nothing is said (openblack's
	/// components::TownDesire does not have those three arrays yet: TODO).
	std::function<std::optional<std::array<float, 3>>(ThingId town)> townResourceNeeds;
	/// UpdateHeartBeat's input. Unset: all 0 and no citadel heart (the beat is computed, nothing plays).
	std::function<HeartBeatInput()> heartBeat;
	/// Run a help message script: nothing for first > last; the running scripts are stopped (false while a task that
	/// is not a help one has the dialogue), the turn is stored, the two numbers are pushed as floats and the script
	/// starts (types 0x7F in a single-player game). Unset: nothing.
	std::function<bool(uint32_t first, uint32_t last, std::string_view script)> helpRunMessage;
	/// Trigger a help category: its last turn becomes the current turn. Unset: nothing.
	std::function<void(int category)> helpTriggerCategory;
	/// The first name the spooky voices say: the current profile's.
	/// (inferred) openblack has no profiles: the name of OPENBLACK_PLAYER_NAME, if set. Unset: empty.
	std::function<std::u16string()> profileName;
};

} // namespace openblack::audio
