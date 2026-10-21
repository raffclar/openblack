/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// LHAudioDLL's music player against a backend that plays a chunk per Advance()

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Audio/AudioManagerNoOp.h>
#include <Audio/GameMusic.h>
#include <Audio/MusicPlayer.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Locator.h"

using namespace openblack::audio;

namespace
{
class FakeMusicBackend final: public MusicBackend
{
public:
	struct StreamState
	{
		std::vector<uint32_t> queued;
		uint32_t played {0};
		uint32_t position {0};
		uint64_t framesPlayed {0};
		uint32_t firstSkip {0};
		bool playing {false};
		float volume {0.0f};
		std::optional<MusicPlacement> placement;
	};

	Stream CreateStream([[maybe_unused]] int32_t pitchPercent) override
	{
		streams[next] = {};
		return next++;
	}
	bool QueueChunk(Stream stream, [[maybe_unused]] const MusicBank& bank, uint32_t chunk, uint32_t skipFrames) override
	{
		auto& state = streams.at(stream);
		if (state.queued.empty() && state.played == 0)
		{
			state.firstSkip = skipFrames;
		}
		state.queued.push_back(chunk);
		heard.push_back(chunk);
		return true;
	}
	uint32_t ReleasePlayedChunks(Stream stream) override
	{
		auto& state = streams.at(stream);
		const auto played = state.played;
		state.queued.erase(state.queued.begin(), state.queued.begin() + played);
		state.played = 0;
		if (state.queued.empty())
		{
			state.playing = false;
		}
		return played;
	}
	[[nodiscard]] uint32_t GetPlayPosition(Stream stream) const override { return streams.at(stream).position; }
	[[nodiscard]] uint64_t GetFramesPlayed(Stream stream) const override { return streams.at(stream).framesPlayed; }
	void Play(Stream stream) override { streams.at(stream).playing = true; }
	[[nodiscard]] bool IsPlaying(Stream stream) const override
	{
		return streams.contains(stream) && streams.at(stream).playing;
	}
	void SetVolume(Stream stream, float volume) override { streams.at(stream).volume = volume; }
	void SetPlacement(Stream stream, const std::optional<MusicPlacement>& placement) override
	{
		streams.at(stream).placement = placement;
	}
	void Destroy(Stream stream) override { streams.erase(stream); }

	/// Every stream finishes the chunk it is playing
	void Advance()
	{
		for (auto& [stream, state] : streams)
		{
			if (state.playing && state.played < state.queued.size())
			{
				++state.played;
			}
		}
	}

	std::map<Stream, StreamState> streams;
	std::vector<uint32_t> heard;
	Stream next {1};
};

std::shared_ptr<MusicBank> MakeBank(uint32_t chunks, int32_t group)
{
	auto bank = std::make_shared<MusicBank>();
	bank->groupId = group;
	bank->chunkCount = chunks;
	bank->readChunk = [](uint32_t) { return std::vector<uint8_t>(1); };
	return bank;
}

constexpr auto k_Frame = std::chrono::microseconds(16'000);
} // namespace

TEST(MusicPlayer, PlaysEveryChunkInOrderThenTheLoopsThenFinishes)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	bool finished = false;
	music.Play({.bank = MakeBank(6, 1), .loops = 1, .onFinished = [&finished]() { finished = true; }});
	for (int i = 0; i < 40 && !finished; ++i)
	{
		backend.Advance();
		music.Update(k_Frame);
	}
	EXPECT_TRUE(finished);
	EXPECT_EQ(backend.heard, (std::vector<uint32_t> {0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5}));
	EXPECT_FALSE(music.IsActive());
}

TEST(MusicPlayer, MusicWithoutLoopsFinishesOnlyWhenItsLastChunkHasBeenHeard)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	bool finished = false;
	music.Play({.bank = MakeBank(6, 0), .onFinished = [&finished]() { finished = true; }});
	// The player queues all six chunks after two chunks play. The music ends only after the sixth chunk plays
	for (int i = 0; i < 5; ++i)
	{
		backend.Advance();
		music.Update(k_Frame);
	}
	EXPECT_EQ(backend.heard, (std::vector<uint32_t> {0, 1, 2, 3, 4, 5}));
	EXPECT_FALSE(finished);
	EXPECT_TRUE(music.IsActive());
	backend.Advance();
	music.Update(k_Frame);
	EXPECT_TRUE(finished);
	EXPECT_EQ(backend.heard.size(), 6U);
}

TEST(MusicPlayer, TheBanksOwnLoopCountWinsOverTheCallers)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto looping = MakeBank(3, 0);
	looping->loopOverride = -1;
	bool finished = false;
	music.Play({.bank = looping, .onFinished = [&finished]() { finished = true; }});
	for (int i = 0; i < 20; ++i)
	{
		backend.Advance();
		music.Update(k_Frame);
	}
	EXPECT_FALSE(finished);
	EXPECT_TRUE(music.IsActive());
	EXPECT_GT(backend.heard.size(), 12U);
	EXPECT_EQ(std::vector<uint32_t>(backend.heard.begin(), backend.heard.begin() + 6),
	          (std::vector<uint32_t> {0, 1, 2, 0, 1, 2}));
}

TEST(MusicPlayer, KeepsFourChunksQueued)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	music.Play({.bank = MakeBank(20, 1)});
	EXPECT_EQ(backend.streams.begin()->second.queued.size(), MusicPlayer::k_QueuedChunks);
	backend.Advance();
	music.Update(k_Frame);
	EXPECT_EQ(backend.streams.begin()->second.queued.size(), MusicPlayer::k_QueuedChunks);
	EXPECT_EQ(music.GetChannels()[0].playingChunk, 2u);
}

TEST(MusicPlayer, NewMusicFadesInWhileTheOldFadesOut)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	const auto first = MakeBank(500, 1);
	const auto second = MakeBank(500, 2);
	music.Play({.bank = first, .volume = 80});
	EXPECT_EQ(music.GetChannels()[0].volume, 80);

	music.Play({.bank = second, .volume = 80, .fadeIn = true});
	EXPECT_EQ(music.GetCurrentChannel(), 1);
	EXPECT_EQ(music.GetChannels()[1].volume, 0);

	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(music.GetChannels()[0].volume, 80 - MusicPlayer::k_FadeOutStep);
	EXPECT_EQ(music.GetChannels()[1].volume, MusicPlayer::k_FadeInStep);

	// The old music is released once silent, 80 / 3 ticks on
	for (int i = 0; i < 30; ++i)
	{
		music.Update(MusicPlayer::k_Tick);
	}
	EXPECT_FALSE(music.GetChannels()[0].active);
	EXPECT_EQ(music.GetChannels()[1].volume, 80);
}

TEST(MusicPlayer, SyncedMusicStartsWhereItsGroupHasGotTo)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	music.Play({.bank = MakeBank(100, 1)});
	for (int i = 0; i < 7; ++i)
	{
		backend.Advance();
		music.Update(k_Frame);
	}
	backend.streams.begin()->second.position = 1234;
	music.Play({.bank = MakeBank(100, 1), .startChunk = 50, .sync = true, .fadeIn = true});
	EXPECT_EQ(music.GetChannels()[1].playingChunk, music.GetChannels()[0].playingChunk);
	EXPECT_EQ(backend.streams.rbegin()->second.firstSkip, 1234u);
}

TEST(MusicPlayer, PlayingTheSameBankAgainRetargetsIt)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	const auto bank = MakeBank(100, 4);
	music.Play({.bank = bank, .volume = 127});
	music.Stop(true);
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(music.GetChannels()[0].volume, 127 - MusicPlayer::k_FadeOutStep);
	music.Play({.bank = bank, .volume = 127, .fadeIn = true});
	music.Update(MusicPlayer::k_Tick);
	// Back up to its target, which it does not go past
	EXPECT_EQ(music.GetChannels()[0].volume, 127);
	EXPECT_FALSE(music.GetChannels()[1].active);
}

TEST(GameMusic, AlignmentPicksEvilNeutralOrGood)
{
	EXPECT_EQ(GameMusic::GetAlignmentIndex(-1.0f), 0);
	EXPECT_EQ(GameMusic::GetAlignmentIndex(0.0f), 1);
	EXPECT_EQ(GameMusic::GetAlignmentIndex(1.0f), 2);
	// Seven steps of the alignment: the lowest two are evil, the middle three neutral and the top two good
	EXPECT_EQ(GameMusic::GetAlignmentIndex(-0.43f), 0);
	EXPECT_EQ(GameMusic::GetAlignmentIndex(-0.42f), 1);
	EXPECT_EQ(GameMusic::GetAlignmentIndex(0.42f), 1);
	EXPECT_EQ(GameMusic::GetAlignmentIndex(0.43f), 2);
}

TEST(GameMusic, TheLandsMusicFollowsTheAlignmentOfThePlace)
{
	GameMusic music;
	GameMusic::TurnInputs inputs {
	    .turn = 100,
	    .camera = {1000.0f, 100.0f, 1000.0f},
	    .groundHeight = 0.0f,
	    .inCitadel = false,
	    .alignment = -0.8f,
	    // The player's own alignment is not what the land's music follows
	    .playerAlignment = 1.0f,
	    .cinema = false,
	    .towns = {{.position = {1100.0f, 0.0f, 1000.0f}, .tribe = 6, .id = 1}},
	    .thingPosition = {},
	};
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GreekTownEvil);
	inputs.alignment = 0.9f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GreekTownGood);
	inputs.camera.x = 3000.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GenericGood);
	inputs.alignment = -1.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GenericEvil);
}

TEST(GameMusic, TheTemplesMusicFollowsThePlayersOwnAlignment)
{
	GameMusic::TurnInputs inputs {
	    .turn = 100,
	    .camera = {},
	    .groundHeight = 0.0f,
	    .inCitadel = true,
	    // Where the camera is does not matter inside the temple
	    .alignment = 1.0f,
	    .playerAlignment = -0.9f,
	    .cinema = false,
	    .towns = {},
	    .thingPosition = {},
	};
	EXPECT_EQ(GameMusic::SelectCitadelType(inputs), MusicType::CitadelEvil);
	inputs.playerAlignment = 0.0f;
	EXPECT_EQ(GameMusic::SelectCitadelType(inputs), MusicType::CitadelNeutral);
	inputs.playerAlignment = 0.9f;
	EXPECT_EQ(GameMusic::SelectCitadelType(inputs), MusicType::CitadelGood);
}

TEST(GameMusic, TheLandsMusicWaitsForTheFirstTurnsAndTheCinemaBars)
{
	const GameMusic music;
	GameMusic::TurnInputs inputs {
	    .turn = 21,
	    .camera = {},
	    .groundHeight = 0.0f,
	    .inCitadel = false,
	    .alignment = 0.0f,
	    .playerAlignment = 0.0f,
	    .cinema = false,
	    .towns = {},
	    .thingPosition = {},
	};
	EXPECT_TRUE(music.LandMusicAllowed(inputs));
	inputs.cinema = true;
	EXPECT_FALSE(music.LandMusicAllowed(inputs));
	inputs.cinema = false;
	inputs.turn = 20;
	EXPECT_FALSE(music.LandMusicAllowed(inputs));
}

TEST(MusicPlayer, AnotherVersionCrossfadesInTime)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto evil = MakeBank(8, 3);
	auto good = MakeBank(8, 3);
	// The land's music plays at 80: the version coming in rises by 4 a tick, the one going falls by 3
	music.Play({.bank = evil, .volume = 80, .loops = -1, .sync = true, .fadeIn = true});
	for (int i = 0; i < 40; ++i)
	{
		music.Update(MusicPlayer::k_Tick);
	}
	ASSERT_EQ(music.GetChannels()[0].volume, 80);
	music.Play({.bank = good, .volume = 80, .loops = -1, .sync = true, .fadeIn = true});
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(music.GetChannels()[0].volume, 77);
	EXPECT_EQ(music.GetChannels()[1].volume, 4);
	// In time with the version it replaces
	EXPECT_EQ(music.GetChannels()[1].playingChunk, music.GetChannels()[0].playingChunk);
	// 20 ticks, 2.4 seconds, to be in, and 27, 3.24 seconds, for the other to be gone
	for (int i = 1; i < 20; ++i)
	{
		music.Update(MusicPlayer::k_Tick);
	}
	EXPECT_EQ(music.GetChannels()[1].volume, 80);
	EXPECT_EQ(music.GetChannels()[0].volume, 20);
	for (int i = 20; i < 27; ++i)
	{
		music.Update(MusicPlayer::k_Tick);
	}
	EXPECT_FALSE(music.GetChannels()[0].active);
}

TEST(GameMusic, TownsNearTheCameraPlayTheirTribesMusic)
{
	GameMusic music;
	GameMusic::TurnInputs inputs {
	    .turn = 100,
	    .camera = {1000.0f, 100.0f, 1000.0f},
	    .groundHeight = 0.0f,
	    .inCitadel = false,
	    .alignment = 0.0f,
	    .playerAlignment = 0.0f,
	    .cinema = false,
	    .towns = {{.position = {1200.0f, 0.0f, 1000.0f}, .tribe = 5, .id = 1},
	              {.position = {560.0f, 0.0f, 1000.0f}, .tribe = 2, .id = 2}},
	    .thingPosition = {},
	};
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::EgyptianTownNeutral);
	// Another town is nearer but not within 300: the town heard last carries on while within 400
	inputs.camera.x = 870.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::EgyptianTownNeutral);
	// Within 300 of the other town, its music takes over
	inputs.camera.x = 820.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::AztecTownNeutral);
	// Away from the towns, and high above the land, the player's alignment music plays
	inputs.camera.x = 2000.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GenericNeutral);
	inputs.camera = {1000.0f, 500.0f, 1000.0f};
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GenericNeutral);
}

TEST(GameMusic, TheTownDistancesComeFromTheInfoTable)
{
	GameMusic music;
	GameMusic::TurnInputs inputs {
	    .turn = 100,
	    .camera = {1000.0f, 100.0f, 1000.0f},
	    .groundHeight = 0.0f,
	    .inCitadel = false,
	    .alignment = 0.0f,
	    .playerAlignment = 0.0f,
	    .cinema = false,
	    .towns = {{.position = {1200.0f, 0.0f, 1000.0f}, .tribe = 5, .id = 1}},
	    .townTriggerDistance = 150.0f,
	    .townTriggerOffDistance = 250.0f,
	    .thingPosition = {},
	};
	// The town is 200 away: not within the table's 150, so no town music starts
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GenericNeutral);
	inputs.camera.x = 1100.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::EgyptianTownNeutral);
	// The camera is 250 above the land, the table's off height: no town music
	inputs.camera.y = 250.0f;
	EXPECT_EQ(music.SelectLandType(inputs), MusicType::GenericNeutral);
}

TEST(GameMusic, AttachedMusicIsHeardAsFarAsItsBankCarries)
{
	// The celtic chant's bank carries 120
	EXPECT_FLOAT_EQ(GameMusic::AttachedMusicRange(120.0f), 120.0f);
	// A bank without samples says nothing: 100
	EXPECT_FLOAT_EQ(GameMusic::AttachedMusicRange(-1.0f), 100.0f);
	// A bank that carries nowhere is never heard
	EXPECT_FLOAT_EQ(GameMusic::AttachedMusicRange(0.0f), 0.0f);
}

TEST(GameMusic, TheMostRecentlyAttachedMusicInRangeIsHeard)
{
	using Candidate = GameMusic::AttachedMusicCandidate;
	const std::array<Candidate, 4> candidates {{
	    // Out of range: right at its range is too far
	    {.hasBank = true, .distance = 120.0f, .range = 120.0f},
	    // In range, but its bank is not in the game's data
	    {.hasBank = false, .distance = 10.0f, .range = 120.0f},
	    {.hasBank = true, .distance = 119.0f, .range = 120.0f},
	    {.hasBank = true, .distance = 5.0f, .range = 100.0f},
	}};
	EXPECT_EQ(GameMusic::SelectAttachedMusic(candidates), std::optional<size_t>(2));
	EXPECT_EQ(GameMusic::SelectAttachedMusic(std::span(candidates).first(2)), std::nullopt);
	EXPECT_EQ(GameMusic::SelectAttachedMusic({}), std::nullopt);
}

TEST(GameMusic, ScriptsAttachChangeMoveAndDetachMusic)
{
	GameMusic music;
	music.AttachMusic(7, MusicType::CelticChantVox);
	music.AttachMusic(9, MusicType::ScriptPiperTune);
	// The most recently attached comes first
	ASSERT_EQ(music.GetAttachedMusic().size(), 2u);
	EXPECT_EQ(music.GetAttachedMusic()[0].thing, 9u);
	EXPECT_EQ(music.GetAttachedMusic()[1].thing, 7u);
	// Attaching to an object that has music changes its music in its place
	music.AttachMusic(7, MusicType::NorseChant);
	ASSERT_EQ(music.GetAttachedMusic().size(), 2u);
	EXPECT_EQ(music.GetAttachedMusic()[1].type, MusicType::NorseChant);
	music.MoveMusic(7, 11);
	EXPECT_EQ(music.GetAttachedMusic()[1].thing, 11u);
	music.DetachMusic(9);
	ASSERT_EQ(music.GetAttachedMusic().size(), 1u);
	EXPECT_EQ(music.GetAttachedMusic()[0].thing, 11u);
	music.DetachMusic(11);
	EXPECT_TRUE(music.GetAttachedMusic().empty());
}

TEST(MusicPlayer, PositionalMusicTakesItsBanksDistancesAndFollowsItsObject)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto chant = MakeBank(8, 7);
	// The chant banks' headers give 30, 120 and a scale of 2
	chant->minDistance = 30.0f;
	chant->maxDistance = 120.0f;
	chant->distanceScale = 2.0f;
	music.Play({.bank = chant, .volume = 127, .sync = true, .fadeIn = true, .position = glm::vec3(1.0f, 2.0f, 3.0f)});
	const auto& placed = backend.streams.at(music.GetChannels()[0].stream).placement;
	ASSERT_TRUE(placed.has_value());
	EXPECT_EQ(placed->position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(placed->minDistance, 30.0f);
	EXPECT_FLOAT_EQ(placed->maxDistance, 120.0f);
	EXPECT_FLOAT_EQ(placed->distanceScale, 2.0f);
	// Played again from where the object has moved to, the same channel follows it
	music.Play({.bank = chant, .volume = 127, .sync = true, .fadeIn = true, .position = glm::vec3(4.0f, 5.0f, 6.0f)});
	EXPECT_FALSE(music.GetChannels()[1].active);
	EXPECT_EQ(backend.streams.at(music.GetChannels()[0].stream).placement->position, glm::vec3(4.0f, 5.0f, 6.0f));

	// A bank that overrides nothing keeps the player's distances, 10 and 100 unscaled
	auto plain = MakeBank(8, 0);
	music.Play({.bank = plain, .volume = 127, .position = glm::vec3(0.0f)});
	const auto& defaults = backend.streams.at(music.GetChannels()[1].stream).placement;
	ASSERT_TRUE(defaults.has_value());
	EXPECT_FLOAT_EQ(defaults->minDistance, 10.0f);
	EXPECT_FLOAT_EQ(defaults->maxDistance, 100.0f);
	EXPECT_FLOAT_EQ(defaults->distanceScale, 1.0f);

	// Music that is not positional is centred on the listener
	auto land = MakeBank(8, 3);
	music.Play({.bank = land, .volume = 80});
	EXPECT_FALSE(backend.streams.at(music.GetChannels()[2].stream).placement.has_value());
}

TEST(MusicPlayer, StoppingOneBank)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto first = MakeBank(8, 1);
	auto second = MakeBank(8, 2);
	music.Play({.bank = first, .volume = 127});
	music.Play({.bank = second, .volume = 127});
	// A bank that is not the one heard is left be when asked to fade, and stops at once otherwise
	music.StopBank(first, true);
	EXPECT_TRUE(music.GetChannels()[0].active);
	music.StopBank(first, false);
	EXPECT_FALSE(music.GetChannels()[0].active);
	EXPECT_TRUE(music.GetChannels()[1].active);
	// Stopping the bank heard stops all the music
	music.Play({.bank = first, .volume = 127});
	music.StopBank(first, false);
	EXPECT_FALSE(music.IsActive());
}

TEST(MusicCues, TheClockCountsWholeChunksFromTheStartChunk)
{
	EXPECT_EQ(CuePositionAfter(1, 0), (MusicCuePosition {.chunk = 1, .frame = 0}));
	EXPECT_EQ(CuePositionAfter(1, k_CueChunkFrames - 1), (MusicCuePosition {.chunk = 1, .frame = k_CueChunkFrames - 1}));
	EXPECT_EQ(CuePositionAfter(1, k_CueChunkFrames), (MusicCuePosition {.chunk = 2, .frame = 0}));
	EXPECT_EQ(CuePositionAfter(5, 2 * k_CueChunkFrames + 7), (MusicCuePosition {.chunk = 7, .frame = 7}));
}

TEST(MusicCues, CuesPassedAreAfterTheLastCheckAndUpToNow)
{
	const std::vector<MusicCue> cues {
	    {.chunk = 1, .frame = 0, .text = "L1"},
	    {.chunk = 3, .frame = 18815, .text = "L2"},
	    {.chunk = 3, .frame = 2303, .text = "p"},
	};
	// Nothing checked yet: a cue at the very start is passed at once
	EXPECT_EQ(CuesPassed(cues, {}, {.chunk = 1, .frame = 0}), (std::vector<std::string_view> {"L1"}));
	EXPECT_TRUE(CuesPassed(cues, {.chunk = 1, .frame = 0}, {.chunk = 3, .frame = 2302}).empty());
	EXPECT_EQ(CuesPassed(cues, {.chunk = 3, .frame = 2302}, {.chunk = 3, .frame = 2303}),
	          (std::vector<std::string_view> {"p"}));
	// Several in one step come in the bank's cue order
	EXPECT_EQ(CuesPassed(cues, {.chunk = 2, .frame = 0}, {.chunk = 4, .frame = 0}),
	          (std::vector<std::string_view> {"L2", "p"}));
	// Starting later than a cue's chunk, the first check passes it
	EXPECT_EQ(CuesPassed(cues, {}, {.chunk = 3, .frame = 0}), (std::vector<std::string_view> {"L1"}));
}

TEST(MusicCues, TheChannelHeardPassesItsCuesOnTheTicksAsItsStreamPlays)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto bank = MakeBank(8, 1);
	bank->cues = {
	    {.chunk = 1, .frame = 0, .text = "L1"},
	    {.chunk = 1, .frame = 9000, .text = "p"},
	    {.chunk = 2, .frame = 100, .text = "L2"},
	};
	std::vector<std::string> heard;
	music.Play({.bank = bank, .onCue = [&heard](std::string_view cue) { heard.emplace_back(cue); }});
	auto& stream = backend.streams.at(music.GetChannels()[0].stream);

	// Cues wait for the next tick of the music thread
	music.Update(MusicPlayer::k_Tick / 2);
	EXPECT_TRUE(heard.empty());
	music.Update(MusicPlayer::k_Tick / 2);
	EXPECT_EQ(heard, (std::vector<std::string> {"L1"}));

	// The stream's frames heard time them, however long the ticks have taken
	stream.framesPlayed = 8999;
	music.Update(MusicPlayer::k_Tick * 5);
	EXPECT_EQ(heard, (std::vector<std::string> {"L1"}));
	stream.framesPlayed = k_CueChunkFrames + 100;
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(heard, (std::vector<std::string> {"L1", "p", "L2"}));
	// Each cue is passed once
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(heard.size(), 3u);
}

TEST(MusicCues, OnlyTheChannelHeardPassesCuesAndAnotherCatchesUpWhenHeardAgain)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto verse = MakeBank(8, 1);
	verse->cues = {{.chunk = 1, .frame = 0, .text = "L1"}, {.chunk = 1, .frame = 5000, .text = "L2"}};
	auto other = MakeBank(8, 2);
	std::vector<std::string> heard;
	music.Play({.bank = verse, .onCue = [&heard](std::string_view cue) { heard.emplace_back(cue); }});
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(heard, (std::vector<std::string> {"L1"}));

	// Other music takes over while the verse fades out: the verse's cues wait
	music.Play({.bank = other, .volume = 127});
	backend.streams.at(music.GetChannels()[0].stream).framesPlayed = 6000;
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(heard, (std::vector<std::string> {"L1"}));

	// Heard again, it passes what it missed
	music.Play({.bank = verse, .volume = 127});
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(heard, (std::vector<std::string> {"L1", "L2"}));
}

TEST(MusicCues, MusicWithoutAHandlerPassesNone)
{
	FakeMusicBackend backend;
	MusicPlayer music(backend);
	auto bank = MakeBank(8, 1);
	bank->cues = {{.chunk = 1, .frame = 0, .text = "L1"}};
	music.Play({.bank = bank});
	music.Update(MusicPlayer::k_Tick);
	EXPECT_EQ(music.GetChannels()[0].cueChecked, MusicCuePosition {});
}

TEST(GameMusic, FightsOnTheFirstLandsHaveTheirOwnMusicAndLaterOnesTheBigFight)
{
	EXPECT_EQ(GameMusic::SelectFightType(0, false), MusicType::CreatureFight);
	EXPECT_EQ(GameMusic::SelectFightType(1, false), MusicType::CreatureFight);
	EXPECT_EQ(GameMusic::SelectFightType(3, false), MusicType::CreatureFight);
	EXPECT_EQ(GameMusic::SelectFightType(4, false), MusicType::CreatureBigFight);
	EXPECT_EQ(GameMusic::SelectFightType(5, false), MusicType::CreatureBigFight);
	// A negative land number counts as a later land
	EXPECT_EQ(GameMusic::SelectFightType(-1, false), MusicType::CreatureBigFight);
	EXPECT_EQ(GameMusic::SelectFightType(1, true), MusicType::CreatureBigFight);
}

TEST(GameMusic, WithNoBanksLoadedNoMusicHasABank)
{
	const GameMusic loaded;
	EXPECT_EQ(loaded.BankPath(MusicType::GenericNeutral), "audio/music/align/neutral.sad");
	// The -NOLOADMUSIC switch: the game loads no music bank, so no music can play
	const GameMusic unloaded(false);
	EXPECT_TRUE(unloaded.BankPath(MusicType::GenericNeutral).empty());
	EXPECT_TRUE(unloaded.BankPath(MusicType::ScriptPiperTune).empty());
}

namespace
{

/// Counts the music that the game asks to play. Each bank has four chunks.
class MusicCountingAudio final: public AudioManagerNoOp
{
public:
	bool MusicPlay(const std::string& bankPath, const MusicPlayOptions& /*options*/) override
	{
		played.push_back(bankPath);
		return true;
	}
	[[nodiscard]] std::optional<MusicBankInfo> GetMusicBankInfo(const std::string& /*bankPath*/) override
	{
		return MusicBankInfo {.groupId = 1, .chunkCount = 4};
	}

	std::vector<std::string> played;
};

/// The game music writes to the audio log, which the tests do not make
void MakeAudioLog()
{
	if (spdlog::get("audio") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_st>("audio");
	}
}

GameMusic::TurnInputs QuietLandTurn()
{
	return {
	    .turn = 100,
	    .camera = {3000.0f, 100.0f, 3000.0f},
	    .groundHeight = 0.0f,
	    .inCitadel = false,
	    .alignment = 0.0f,
	    .playerAlignment = 0.0f,
	    .cinema = false,
	    .towns = {},
	    .thingPosition = {},
	};
}

} // namespace

TEST(GameMusic, AFilmLeavesTheMusicAndTheScriptMusicStartsAgainAfterIt)
{
	MakeAudioLog();
	openblack::Locator::audio::emplace<MusicCountingAudio>();
	auto& audio = static_cast<MusicCountingAudio&>(openblack::Locator::audio::value());
	{
		GameMusic music;
		music.StartScriptMusic(MusicType::ScriptIntro);
		auto inputs = QuietLandTurn();
		music.ProcessTurn(inputs);
		music.ProcessTurn(inputs);
		ASSERT_EQ(audio.played.size(), 1U);
		// The turns of a film play nothing and stop nothing
		inputs.film = true;
		music.ProcessTurn(inputs);
		EXPECT_EQ(audio.played.size(), 1U);
		EXPECT_EQ(music.GetPlaying(), MusicType::None);
		// After the film, the script's music plays again from its start
		inputs.film = false;
		music.ProcessTurn(inputs);
		ASSERT_EQ(audio.played.size(), 2U);
		EXPECT_EQ(audio.played[1], audio.played[0]);
	}
	openblack::Locator::audio::reset();
}

TEST(GameMusic, AFilmThatStartsMakesTheLandsMusicStartAgain)
{
	MakeAudioLog();
	openblack::Locator::audio::emplace<MusicCountingAudio>();
	auto& audio = static_cast<MusicCountingAudio&>(openblack::Locator::audio::value());
	{
		GameMusic music;
		const auto inputs = QuietLandTurn();
		music.ProcessTurn(inputs);
		music.ProcessTurn(inputs);
		ASSERT_EQ(audio.played.size(), 1U);
		music.ForgetPlaying();
		music.ProcessTurn(inputs);
		EXPECT_EQ(audio.played.size(), 2U);
	}
	openblack::Locator::audio::reset();
}
