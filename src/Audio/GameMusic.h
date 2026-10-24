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
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Systems/SavedStateInterface.h"
#include "MusicType.h"
#include "ScriptMusic.h"

namespace openblack::audio
{

/// The game's music: once a game turn, picks what music plays and hands it to the audio library's music player.
///
/// In order, the first that wants to play wins: the citadel's music while inside the citadel, music a script has
/// started, the music of a creature's dance with villagers near the camera, music a script has attached to an object
/// while the camera is near it, and the music of the land under the
/// camera. Over land the music follows the player's alignment, and near a town that of its tribe too. Each music group
/// remembers where it got to, so the land's music carries on in time when it changes and picks up where it left off when
/// it comes back.
// TODO(raffclar): the game also plays creature fight and chant music before the creature dance music
class GameMusic final: public ecs::systems::SavedStateInterface
{
public:
	/// Music a script has attached to an object: heard from the object while the camera is near it
	struct AttachedMusic
	{
		/// The object's entity
		uint32_t thing;
		MusicType type;
	};

	/// How an attached piece stands this turn, for choosing the one heard
	struct AttachedMusicCandidate
	{
		/// Its bank is in the game's data
		bool hasBank;
		/// From the camera to its object
		float distance;
		/// How near the camera must be to hear it
		float range;
	};

	/// How near the camera must be to an object to hear the music attached to it: as far as its bank says it carries,
	/// or 100 for a bank that says nothing
	[[nodiscard]] static float AttachedMusicRange(float bankMaxDistance);
	/// The attached piece heard, if any: the first, from the most recently attached, whose bank is there and whose
	/// object is in range
	[[nodiscard]] static std::optional<size_t> SelectAttachedMusic(std::span<const AttachedMusicCandidate> candidates);

	struct Town
	{
		glm::vec3 position;
		/// Tribe, from 0 (celtic) to 8 (tibetan)
		int32_t tribe;
		uint32_t id;
	};

	struct TurnInputs
	{
		uint32_t turn;
		glm::vec3 camera;
		/// Height of the land under the camera
		float groundHeight;
		/// Inside the citadel
		bool inCitadel;
		/// Alignment of the place the camera is in, -1 (evil) to 1 (good): that of the player of most influence there,
		/// neutral where none has any. The land's music follows it.
		float alignment;
		/// The local player's own alignment, -1 (evil) to 1 (good), which the temple's music follows
		float playerAlignment;
		/// A script holds the cinema bars, or they are sliding in or out: the land's music waits
		bool cinema;
		std::vector<Town> towns;
		/// The info table's town distances: a town's music starts within the first of the camera and carries on to the
		/// second, and there is no town music while the camera is the second or more above the land. The defaults are the
		/// shipped table's values.
		float townTriggerDistance {300.0f};
		float townTriggerOffDistance {400.0f};
		/// Where an object with attached music is, none once it has gone
		std::function<std::optional<glm::vec3>(uint32_t thing)> thingPosition;
		/// The music of the creature's dance with villagers that the camera hears, and where the creature is
		struct CreatureDance
		{
			MusicType type;
			glm::vec3 position;
		};
		std::optional<CreatureDance> creatureDance;
		/// The music of the local player's creature's fight, heard while the player directs the fight or while the camera
		/// is near the arena of the fight (see SelectFightType)
		std::optional<MusicType> creatureFight;
		/// A full-screen film plays. The music is not changed, and the script's music plays again after the film.
		bool film {false};
		/// The music that the scripts start, as the world sees it
		struct ScriptPiece
		{
			MusicType type {MusicType::None};
			/// The world plays the piece. A piece that played before a load plays no sound.
			bool started {false};
			/// How many times the world played the scripts' music. A change plays the piece again.
			uint32_t playCount {0};
			/// The place that the world's channel of the piece has reached, none when no channel plays it
			std::optional<MusicPiecePosition> place;
		};
		ScriptPiece script;
	};

	/// With no banks loaded (the -NOLOADMUSIC switch), no music of the game plays
	explicit GameMusic(bool banksLoaded = true)
	    : _banksLoaded(banksLoaded)
	{
	}
	/// Stops the music, which would otherwise call back into it when it ends
	~GameMusic() override;
	GameMusic(const GameMusic&) = delete;
	GameMusic& operator=(const GameMusic&) = delete;

	/// Picks the music for this game turn
	void ProcessTurn(const TurnInputs& inputs);
	/// Forgets what was playing, when a land is loaded
	void Reset();
	/// A save keeps the music that scripts attached to objects, the town whose music played and whether the land's
	/// music is on. It does not keep the music that plays, nor where each music group got to. The world keeps the
	/// music that the scripts start.
	void SaveState(ecs::reflection::BinaryWriter& out) const override;
	/// A load gives back what SaveState keeps, after Reset. No music plays until the next game turn picks it.
	bool LoadState(ecs::reflection::BinaryReader& in) override;
	/// Registers every music bank with the audio once, as the game does when its audio starts, so that starting a
	/// piece later reads none of them
	static void RegisterBanks();

	/// Forgets the music that plays, so that the next turn starts the land's music again. A film does this when it
	/// starts.
	void ForgetPlaying();
	/// The script command that turns the alignment music on or off
	void SetAlignmentMusicEnabled(bool enabled) { _alignmentMusicEnabled = enabled; }
	/// A script attaches music to an object, or changes the music already attached to it. Music attached afresh stops
	/// its bank at once if it is playing.
	void AttachMusic(uint32_t thing, MusicType type);
	/// A script takes the music off an object
	void DetachMusic(uint32_t thing);
	/// A script moves an object's music to another object
	void MoveMusic(uint32_t from, uint32_t to);
	[[nodiscard]] const std::vector<AttachedMusic>& GetAttachedMusic() const { return _attached; }
	/// Whether the music attached to an object has finished: an object without music counts as finished, and attached
	/// music never finishes while it stays attached
	[[nodiscard]] bool IsAttachedMusicFinished(uint32_t thing) const;
	/// A script starts an object's music again from the top: its bank stops at once if it is playing, and the music
	/// comes back the next time it is chosen
	void RestartAttachedMusic(uint32_t thing);

	// Debug introspection
	[[nodiscard]] MusicType GetPlaying() const { return _playing; }
	[[nodiscard]] MusicType GetLandType() const { return _landType; }
	[[nodiscard]] MusicType GetBlockedType() const { return _blockedType; }
	[[nodiscard]] uint32_t GetBlockedTurns() const { return _blockedTurns; }
	[[nodiscard]] bool IsAlignmentMusicEnabled() const { return _alignmentMusicEnabled; }
	[[nodiscard]] const std::map<int32_t, uint32_t>& GetResumeChunks() const { return _resumeChunks; }

	/// The alignment in seven steps, grouped into the evil, neutral or good index of the music tables
	[[nodiscard]] static int32_t GetAlignmentIndex(float alignment);
	/// The land's music at the camera
	[[nodiscard]] MusicType SelectLandType(const TurnInputs& inputs);
	/// The music of a fight of the player's creature: the creature fight music on the first three lands of the story,
	/// the big fight music on later lands and in a skirmish
	[[nodiscard]] static MusicType SelectFightType(int32_t landNumber, bool skirmish);
	/// The temple's music, in the version of the local player's own alignment
	[[nodiscard]] static MusicType SelectCitadelType(const TurnInputs& inputs);
	/// Whether the land's music may play this turn: it is turned on, the game is past its first turns, and no script
	/// holds the cinema bars nor are they sliding
	[[nodiscard]] bool LandMusicAllowed(const TurnInputs& inputs) const;

	/// The bank of a type of music, none when the banks are not loaded
	[[nodiscard]] std::string_view BankPath(MusicType type) const;

private:
	void ProcessMusic(const TurnInputs& inputs);
	bool ProcessCitadel(const TurnInputs& inputs);
	bool ProcessScript(const TurnInputs::ScriptPiece& script);
	bool ProcessCreatureFight(const TurnInputs& inputs);
	bool ProcessCreatureDance(const TurnInputs& inputs);
	bool ProcessAttached(const TurnInputs& inputs);
	bool ProcessLand(const TurnInputs& inputs);
	/// Remembers where each playing music group has got to
	void SaveResumeChunks();
	[[nodiscard]] uint32_t GetResumeChunk(int32_t groupId) const;

	/// The music type heard
	MusicType _playing {MusicType::None};
	/// The land's music playing, None when other music has taken over
	MusicType _landType {MusicType::None};
	/// The land's music that played to its end: it is not played again for a while
	MusicType _blockedType {MusicType::None};
	uint32_t _blockedTurns {0};
	/// The scripts' piece plays in the music heard, and the play count of the world that it follows
	bool _scriptStarted {false};
	uint32_t _scriptPlayCount {0};
	bool _alignmentMusicEnabled {true};
	bool _banksLoaded {true};
	/// Whether the land's music could play last turn, for logging when that changes
	std::optional<bool> _landMusicWasAllowed;
	std::optional<uint32_t> _rememberedTown;
	/// 1-based chunk each music group carries on from
	std::map<int32_t, uint32_t> _resumeChunks;
	/// Music attached to objects, the most recently attached first
	std::vector<AttachedMusic> _attached;
};

} // namespace openblack::audio
