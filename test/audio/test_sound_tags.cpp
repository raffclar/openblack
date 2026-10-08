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

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/SoundTags.h"

// Sound tags on the 16 channels. A tag of a thing replays through PlaySoundEffect
// every turn (mode 2: only when silent); a thing that goes releases a playing loop and the tag waits for its end; a
// point tag plays once and goes when it stops; a delayed one waits for the sound at 347 a second (CheckDelay); the
// street lantern's tag sounds at the lantern's top within the sample's max distance. A fake output stands for QMixer.

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
	/// the index of the channel that plays, -1 for none (one at a time in these tests)
	[[nodiscard]] int Single() const
	{
		int found = -1;
		for (size_t i = 0; i < playing.size(); ++i)
		{
			if (playing[i])
			{
				found = found == -1 ? static_cast<int>(i) : -2;
			}
		}
		return found;
	}
};

constexpr auto k_Thing = static_cast<entt::entity>(42);

class SoundTagTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	/// the things' positions (GameQueries::thingPosition); a missing one is gone
	static inline std::map<uint32_t, glm::vec3> s_Things;
	static inline glm::vec3 s_Camera {0.0f};
	BankId inGame {k_NoBank};

	void SetUp() override
	{
		s_Things.clear();
		s_Camera = glm::vec3(0.0f);
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> {
			CameraState camera;
			camera.position = s_Camera;
			return camera;
		};
		audio::Init(std::move(queries));
		inGame = RegisterBank("audio/sfx/game/InGame.sad", "InGame.sad");
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return s_Camera; };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void TearDown() override
	{
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (QMixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// An InGame.sad sample with the defaults of the options (no .sad override) and a max distance
	void Add(int number, float maxDistance)
	{
		Sound sound;
		sound.name = fmt::format("InGame {}", number);
		sound.id = number;
		sound.bank = inGame;
		sound.priority = 100;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.maxDistance = maxDistance;
		sounds.emplace(SampleId(inGame, number), std::move(sound));
	}
};
} // namespace

TEST_F(SoundTagTest, ThingTagMode2ReplaysOnlyWhenSilent)
{
	Add(13, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(10.0f, 0.0f, 0.0f);
	// the windmill's creation: (windmill, 13 G_windmill, 0, 2, -1, 0, 1, InGame, 0)
	const auto tag = tags::Create(k_Thing, 13, false, 2, -1, false, true, SfxBank::InGame, 0);
	EXPECT_EQ(output.plays, 0); // a tag of a thing plays from the next ProcessSoundTags
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_TRUE(output.starts[static_cast<size_t>(channel)].is3D);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].loops, -1);
	tags::ProcessSoundTags();
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1); // mode 2: nothing while it plays
	output.playing[static_cast<size_t>(channel)] = false;
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 2); // silent again: it replays
	EXPECT_TRUE(tags::Exists(tag));
}

TEST_F(SoundTagTest, ThingTagMode3RestartsEveryTurn)
{
	Add(10, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(1.0f);
	tags::Create(k_Thing, 10, false, 3, 0, false, true, SfxBank::InGame, 0);
	tags::ProcessSoundTags();
	tags::ProcessSoundTags();
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 3);
	EXPECT_EQ(output.Single(), 0); // the same channel, restarted (mode 3: same bank, owner and sample)
}

TEST_F(SoundTagTest, OutOfRangeAndInactive)
{
	Add(13, 50.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(60.0f, 0.0f, 0.0f);
	const auto tag = tags::Create(k_Thing, 13, false, 2, -1, false, true, SfxBank::InGame, 0);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 0); // farther than the sample's max distance from the camera
	s_Camera = glm::vec3(20.0f, 0.0f, 0.0f);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
	// an active tag turned off stops at once and does not replay
	tags::SetActive(tag, false);
	EXPECT_EQ(output.Single(), -1);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
	tags::SetActive(tag, true);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 2);
}

TEST_F(SoundTagTest, GoneThingReleasesItsLoop)
{
	Add(74, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(5.0f, 0.0f, 0.0f);
	// the workshop's update: (workshop, 74 G_Workshop, 0, 2, -1, 0, 1, InGame, 0)
	const auto tag = tags::Create(k_Thing, 74, false, 2, -1, false, true, SfxBank::InGame, 0);
	tags::ProcessSoundTags();
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	s_Things.clear();
	// not functional -> ToBeDeleted -> a dead object's tag: looping and playing -> released
	tags::ProcessSoundTags();
	EXPECT_TRUE(output.released[static_cast<size_t>(channel)]);
	EXPECT_TRUE(output.playing[static_cast<size_t>(channel)]);
	EXPECT_TRUE(tags::Exists(tag));
	tags::ProcessSoundTags();
	EXPECT_TRUE(tags::Exists(tag)); // still playing
	EXPECT_EQ(output.plays, 1);     // and not replayed
	output.playing[static_cast<size_t>(channel)] = false;
	tags::ProcessSoundTags();
	EXPECT_FALSE(tags::Exists(tag));
}

TEST_F(SoundTagTest, GoneThingOneShotGoesAtOnce)
{
	Add(11, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(5.0f, 0.0f, 0.0f);
	// the totem statue's worship percentage: (totem, 11 G_TotumMove, 0, 2, 0, 0, 1, InGame, 0)
	const auto tag = tags::Create(k_Thing, 11, false, 2, 0, false, true, SfxBank::InGame, 0);
	tags::ProcessSoundTags();
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	s_Things.clear();
	tags::ProcessSoundTags();
	EXPECT_FALSE(tags::Exists(tag));
	EXPECT_FALSE(output.released[static_cast<size_t>(channel)]);
	EXPECT_TRUE(output.playing[static_cast<size_t>(channel)]); // its sample plays on
}

TEST_F(SoundTagTest, PointTagPlaysOnceAndGoes)
{
	Add(31, 100.0f);
	// a falling tree's physics turn: a point tag (coords, 31 G_TreeFall_01, 0, 3, 0, 0, 1, InGame, 0)
	const auto tag = tags::Create(glm::vec3(3.0f, 0.0f, 4.0f), 31, false, 3, 0, false, true, SfxBank::InGame, 0);
	EXPECT_EQ(output.plays, 1); // at once
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(3.0f, 0.0f, 4.0f));
	tags::ProcessSoundTags();
	EXPECT_TRUE(tags::Exists(tag));
	EXPECT_EQ(output.plays, 1); // a point tag never replays
	output.playing[static_cast<size_t>(channel)] = false;
	tags::ProcessSoundTags();
	EXPECT_FALSE(tags::Exists(tag));
}

TEST_F(SoundTagTest, MapCoordsTagIsTheMapPoint)
{
	Add(10, 100.0f);
	// a tag at map coords: x and z x 10 / 65536, y = the land's altitude (0 without a land)
	// + the altitude above the land
	const map_coords::MapCoords coords {3 * 0x10000 + 0x8000, 0x10000 + 0x4000, 2.0f};
	tags::CreateAtMapCoords(coords, 10, false, 3, 0, false, true, SfxBank::InGame, 0);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(35.0f, 2.0f, 12.5f));
	// the same map point already in metres (magic::ToMap's): not quantised again
	output = FakeOutput {};
	tags::CreateAtMapCoords(map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z), 2.0f, 10, false, 3, 0, false, true,
	                        SfxBank::InGame, 0);
	const int second = output.Single();
	ASSERT_GE(second, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(second)].position, glm::vec3(35.0f, 2.0f, 12.5f));
}

TEST_F(SoundTagTest, DelayedPointTagWaitsForTheSound)
{
	Add(20, 500.0f);
	// 100 away: 347 * turns * 0.1 >= 100 from the 3rd turn (34.7, 69.4, 104.1)
	const auto tag = tags::Create(glm::vec3(100.0f, 0.0f, 0.0f), 20, false, 2, 0, false, true, SfxBank::InGame, 1);
	EXPECT_EQ(output.plays, 0);
	tags::ProcessSoundTags();
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 0);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(tags::Exists(tag));
	// a 2D tag keeps no delay: at once
	tags::Create(glm::vec3(100.0f, 0.0f, 0.0f), 20, false, 1, 0, false, false, SfxBank::InGame, 1);
	EXPECT_EQ(output.plays, 2);
}

TEST_F(SoundTagTest, RemoveStopsOrReleases)
{
	Add(166, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(1.0f);
	// the creature's creed power: (creature, 166 G_Creed_01, 0, 2, -1, ...), then Remove
	auto tag = tags::Create(k_Thing, 166, false, 2, -1, false, true, SfxBank::InGame, 0);
	tags::ProcessSoundTags();
	int channel = output.Single();
	ASSERT_GE(channel, 0);
	tags::Remove(k_Thing, 166, SfxBank::Editor); // another bank type: not that tag
	EXPECT_TRUE(tags::Exists(tag));
	tags::Remove(k_Thing, 166, SfxBank::InGame);
	EXPECT_TRUE(output.released[static_cast<size_t>(channel)]); // ToBeDeleted, the loop released
	EXPECT_TRUE(tags::Exists(tag));                             // as a dead object's tag until it ends
	output.playing.fill(false);
	tags::ProcessSoundTags();
	EXPECT_FALSE(tags::Exists(tag));

	tag = tags::Create(k_Thing, 166, false, 2, -1, false, true, SfxBank::InGame, 0);
	tags::ProcessSoundTags();
	channel = output.Single();
	ASSERT_GE(channel, 0);
	tags::Remove(k_Thing, 166, SfxBank::InGame, true); // with stop: stopped, then gone
	EXPECT_FALSE(output.playing[static_cast<size_t>(channel)]);
	EXPECT_FALSE(tags::Exists(tag));
}

TEST_F(SoundTagTest, StreetLanternAtItsTop)
{
	// InGame 147 G_Lantern_01: max distance 5
	Add(147, 5.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(100.0f, 10.0f, 100.0f);
	// the street lantern's creation: a tag at (0, h, 0) of sample 0x93 (0, 2, -1, 0, 1, InGame, 0), then SetActive, off
	// by day
	const auto tag = tags::Create(k_Thing, glm::vec3(0.0f, 3.0f, 0.0f), 147, false, 2, -1, false, true, SfxBank::InGame, 0);
	tags::SetActive(tag, false);
	s_Camera = glm::vec3(100.0f, 17.0f, 100.0f); // 4 above the top
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 0); // daylight: inactive
	tags::SetActive(tag, true);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(100.0f, 13.0f, 100.0f));
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].loops, -1);
	// the camera going away does not stop it (an untracked channel), and mode 2 leaves it alone
	s_Camera = glm::vec3(0.0f);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.playing[static_cast<size_t>(channel)]);
	// nor does it start again while the camera stays farther than 5 from the top
	output.playing.fill(false);
	s_Camera = glm::vec3(100.0f, 19.0f, 100.0f);
	tags::ProcessSoundTags();
	EXPECT_EQ(output.plays, 1);
}

TEST_F(SoundTagTest, RandomSample)
{
	// LocalRand(0) is 0
	EXPECT_EQ(tags::RandomSample(180, 0), 180);
}
