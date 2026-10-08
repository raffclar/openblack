/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <map>
#include <optional>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/ScriptSound.h"
#include "Audio/Services/SoundTags.h"

// The script's sound effects: PLAY_SOUND_EFFECT owns its channel by the sample number; GAME_SOUND_PLAYING asks that
// channel; STOP_SOUND_EFFECT (not isSay) stops it; ATTACH_SOUND_TAG makes a mode 2 tag of the thing that replays
// while silent, DETACH_SOUND_TAG removes it. A fake output stands for the mixer.

using namespace openblack;
using namespace openblack::audio;

namespace
{
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<bool, 16> released {};
	std::array<Start, 16> starts {};
	int plays {0};

	bool Play(size_t channel, Sound&, const Start& start) override
	{
		playing[channel] = true;
		released[channel] = false;
		starts[channel] = start;
		++plays;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t channel) override { released[channel] = true; }
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override { playing.fill(false); }
};

constexpr auto k_Thing = static_cast<entt::entity>(77);
constexpr int k_ScriptSfx = 5; // AUDIO_SFX_BANK_TYPE of scriptsfx.sad

class ScriptSoundTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	static inline std::map<uint32_t, glm::vec3> s_Things;
	BankId scriptSfx {k_NoBank};

	void SetUp() override
	{
		s_Things.clear();
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		audio::Init(std::move(queries));
		scriptSfx = RegisterBank("audio/sfx/script/scriptsfx.sad", "scriptsfx.sad");
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void TearDown() override
	{
		audio::SetGameSound(true);
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (the mixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// A scriptsfx.sad sample with the options' defaults, a max distance and, for a loop, the .sad's flag 0x40
	void Add(int number, float maxDistance, int loops)
	{
		Sound sound;
		sound.name = fmt::format("ScriptSfx {}", number);
		sound.id = number;
		sound.bank = scriptSfx;
		sound.priority = 100;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.maxDistance = maxDistance;
		if (loops != 0)
		{
			sound.overrides = 0x40;
			sound.loops = loops;
		}
		sounds.emplace(SampleId(scriptSfx, number), std::move(sound));
	}

	/// The channel info of the first channel in use, nullopt for none
	[[nodiscard]] static std::optional<sample_play::ChannelInfo> Playing()
	{
		for (const auto& info : sample_play::Channels())
		{
			if (info.playing)
			{
				return info;
			}
		}
		return std::nullopt;
	}
};
} // namespace

TEST_F(ScriptSoundTest, PlayOwnsItsChannelByTheSampleNumber)
{
	// land4meteorites: PLAY_SOUND_EFFECT(93 SimonBell, scriptsfx, pos, 1)
	Add(93, 100.0f, 0);
	const auto channel = script_sound::PlaySoundEffect(93, k_ScriptSfx, glm::vec3(3.0f, 1.0f, 4.0f), true);
	ASSERT_NE(channel, k_NoChannel);
	const auto info = Playing();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->owner, Owner::Key(93)); // owned by the sample number
	EXPECT_TRUE(info->is3D);
	EXPECT_FALSE(info->track);
	EXPECT_EQ(output.starts.at(0).position, glm::vec3(3.0f, 1.0f, 4.0f));
}

TEST_F(ScriptSoundTest, GameSoundPlayingUntilStopped)
{
	Add(93, 100.0f, 0);
	EXPECT_FALSE(script_sound::GameSoundPlaying(93, k_ScriptSfx));
	script_sound::PlaySoundEffect(93, k_ScriptSfx, glm::vec3(0.0f), true);
	EXPECT_TRUE(script_sound::GameSoundPlaying(93, k_ScriptSfx));
	// another sample number is another owner: not playing
	EXPECT_FALSE(script_sound::GameSoundPlaying(94, k_ScriptSfx));
	script_sound::StopSoundEffect(false, 93, k_ScriptSfx);
	EXPECT_FALSE(script_sound::GameSoundPlaying(93, k_ScriptSfx));
	EXPECT_FALSE(Playing().has_value());
}

TEST_F(ScriptSoundTest, PlayWithoutPositionIs2D)
{
	Add(1, 0.0f, 0);
	// a 2D one is not culled by the camera distance (only a 3D one is)
	ASSERT_NE(script_sound::PlaySoundEffect(1, k_ScriptSfx, glm::vec3(5000.0f, 0.0f, 0.0f), false), k_NoChannel);
	const auto info = Playing();
	ASSERT_TRUE(info.has_value());
	EXPECT_FALSE(info->is3D);
}

TEST_F(ScriptSoundTest, Play3DFartherThanItsMaxDistanceIsCulled)
{
	Add(46, 50.0f, 0);
	EXPECT_EQ(script_sound::PlaySoundEffect(46, k_ScriptSfx, glm::vec3(60.0f, 0.0f, 0.0f), true), k_NoChannel);
	EXPECT_EQ(output.plays, 0);
}

TEST_F(ScriptSoundTest, BankOutsideTheTablePlaysNothing)
{
	Add(93, 100.0f, 0);
	EXPECT_EQ(script_sound::BankType(11), SfxBank::None);
	EXPECT_EQ(script_sound::BankType(0), SfxBank::None);
	EXPECT_EQ(script_sound::PlaySoundEffect(93, 11, glm::vec3(0.0f), false), k_NoChannel);
	EXPECT_EQ(output.plays, 0);
}

TEST_F(ScriptSoundTest, SetGameSoundOffLeavesOnlyTheDialogueBanks)
{
	Add(93, 100.0f, 0);
	// SET_GAME_SOUND false: the game sound is off and only HelpSprites / Villagers get through
	audio::SetGameSound(false);
	EXPECT_EQ(script_sound::PlaySoundEffect(93, k_ScriptSfx, glm::vec3(0.0f), false), k_NoChannel);
	audio::SetGameSound(true);
	EXPECT_NE(script_sound::PlaySoundEffect(93, k_ScriptSfx, glm::vec3(0.0f), false), k_NoChannel);
}

TEST_F(ScriptSoundTest, AttachSoundTagReplaysInMode2AndDetachReleasesTheLoop)
{
	// LandControlAll: ATTACH_SOUND_TAG(1, 126 G_PhoneBoxRing, scriptsfx, box), a loop by the .sad's flag 0x40
	Add(126, 100.0f, -1);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(2.0f, 0.0f, 0.0f);
	const auto tag = script_sound::AttachSoundTag(true, 126, k_ScriptSfx, k_Thing);
	ASSERT_NE(tag, tags::k_NoTag);
	tags::ProcessSoundTags();
	auto info = Playing();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->owner, Owner::Tag(tag)); // the tag owns the channel
	EXPECT_TRUE(info->is3D);
	EXPECT_TRUE(info->track); // track = threeD != 0
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1); // mode 2: nothing while it plays
	// DETACH_SOUND_TAG: the tag is marked to be deleted, the playing loop finishes its pass
	script_sound::DetachSoundTag(126, k_ScriptSfx, k_Thing);
	EXPECT_TRUE(output.released.at(0));
	output.playing.at(0) = false;
	tags::ProcessSoundTags();
	EXPECT_FALSE(tags::Exists(tag));
	EXPECT_EQ(output.plays, 1);
}

TEST_F(ScriptSoundTest, AttachSoundTag2DDoesNotTrack)
{
	Add(126, 100.0f, -1);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(2.0f, 0.0f, 0.0f);
	const auto tag = script_sound::AttachSoundTag(false, 126, k_ScriptSfx, k_Thing);
	tags::ProcessSoundTags();
	const auto info = Playing();
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->owner, Owner::Tag(tag));
	EXPECT_FALSE(info->is3D);
	EXPECT_FALSE(info->track);
}

TEST_F(ScriptSoundTest, NoThingNoTag)
{
	Add(126, 100.0f, -1);
	EXPECT_EQ(script_sound::AttachSoundTag(true, 126, k_ScriptSfx, entt::null), tags::k_NoTag);
	script_sound::DetachSoundTag(126, k_ScriptSfx, entt::null);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 0);
}

TEST_F(ScriptSoundTest, SoundExistsNeedsTheDevice)
{
	// no OpenAL device in the tests (device::Open is never called): no sound is installed
	EXPECT_FALSE(audio::SoundExists());
}
