/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The audio service (Locator::audio): the functions of Audio.h hand each call, with its arguments, to whatever the slot
// holds, and fail clearly when it holds nothing. A recording fake is injected through the slot.

#include <cstdint>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameQueries.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// Records what reaches it; the rest answers as the no-op base does
class RecordingAudio final: public AudioManagerNoOp
{
public:
	Channel PlaySoundEffect(const PlayOptions& options) override
	{
		lastOptions = options;
		calls.push_back("options " + std::to_string(options.sample.bank) + " " + std::to_string(options.sample.number));
		return 7;
	}
	Channel PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, SfxBank bank) override
	{
		calls.push_back("sfx bank " + std::to_string(static_cast<int>(bank)) + " sample " + std::to_string(sample) + " owner " +
		                std::to_string(owner.id) + " " + std::to_string(mode) + " " + std::to_string(loops) + " " +
		                std::to_string(extra3DFlag) + std::to_string(is3D));
		return 8;
	}
	Channel PlaySoundEffect(Owner /*owner*/, int sample, int /*mode*/, int /*loops*/, bool /*extra3DFlag*/, bool /*is3D*/,
	                        BankId bank) override
	{
		calls.push_back("bank id " + std::to_string(bank) + " sample " + std::to_string(sample));
		return 9;
	}
	Channel PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                          BankId bank) override
	{
		positional = PositionalCall {
		    .owner = owner,
		    .position = position,
		    .sample = sample,
		    .mode = mode,
		    .loops = loops,
		    .extra3DFlag = extra3DFlag,
		    .is3D = is3D,
		    .bank = bank,
		};
		return 10;
	}
	[[nodiscard]] bool IsPlaying(Channel channel) override { return channel == 7; }
	[[nodiscard]] uint32_t NewObjectId() override { return 99; }
	void RegisterObject(uint32_t id, ObjectPositionFn position) override
	{
		registered = id;
		registeredPosition = position();
	}
	[[nodiscard]] std::optional<Sample> FindSample(BankId bank, std::string_view wavName) override
	{
		if (wavName == "G_PickUpFood.wav")
		{
			return Sample {bank, 12};
		}
		return std::nullopt;
	}
	void Init(GameQueries queries) override
	{
		calls.push_back(std::string("init ") + (queries.videoPlaying && queries.videoPlaying() ? "video" : "no video"));
	}
	void SetSampleMainVolume(int volume) override { mainVolume = volume; }
	[[nodiscard]] int SampleMainVolume() override { return mainVolume; }
	void StopAllSoundEffects() override { calls.emplace_back("stop all"); }
	void ProcessTurn() override { calls.emplace_back("turn"); }
	void ProcessCitadelTurn() override { calls.emplace_back("citadel turn"); }
	void Paused() override { calls.emplace_back("paused"); }

	/// The arguments of the last PlaySoundEffectAt with a bank
	struct PositionalCall
	{
		Owner owner;
		glm::vec3 position;
		int sample;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		BankId bank;
	};

	std::vector<std::string> calls;
	std::optional<PlayOptions> lastOptions;
	std::optional<PositionalCall> positional;
	uint32_t registered {0};
	std::optional<glm::vec3> registeredPosition;
	int mainVolume {0};
};
} // namespace

TEST(AudioManager, TheFreeFunctionsReachTheService)
{
	test::RestoreService<Locator::audio> keep;
	auto& fake = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());

	PlayOptions options;
	options.sample = {3, 41};
	EXPECT_EQ(audio::PlaySoundEffect(options), 7u);
	EXPECT_EQ(audio::PlaySoundEffect(Owner::Tag(5), 2, 1, 3, true, false, SfxBank::Spells), 8u);
	EXPECT_EQ(audio::PlaySoundEffect(Owner::None(), 4, 0, 0, false, true, BankId {6}), 9u);
	EXPECT_TRUE(audio::IsPlaying(Channel {7}));
	EXPECT_FALSE(audio::IsPlaying(Channel {8}));
	EXPECT_EQ(audio::NewObjectId(), 99u);
	audio::StopAllSoundEffects();
	ASSERT_EQ(fake.calls.size(), 4u);
	EXPECT_EQ(fake.calls[0], "options 3 41");
	EXPECT_EQ(fake.calls[1], "sfx bank 3 sample 2 owner 5 1 3 10");
	EXPECT_EQ(fake.calls[2], "bank id 6 sample 4");
	EXPECT_EQ(fake.calls[3], "stop all");
}

TEST(AudioManager, APositionalSoundReachesTheServiceWithItsArguments)
{
	// The temple plays its sounds at a point of its own, with a bank and a sample, and through play options naming the
	// sound itself
	test::RestoreService<Locator::audio> keep;
	auto& fake = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());

	EXPECT_EQ(audio::PlaySoundEffectAt(Owner::None(), {1.0f, 2.0f, 3.0f}, 60, 0, 0, false, true, BankId {2}), 10u);
	ASSERT_TRUE(fake.positional.has_value());
	EXPECT_EQ(fake.positional->owner, Owner::None());
	EXPECT_EQ(fake.positional->position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(fake.positional->sample, 60);
	EXPECT_EQ(fake.positional->mode, 0);
	EXPECT_EQ(fake.positional->loops, 0);
	EXPECT_FALSE(fake.positional->extra3DFlag);
	EXPECT_TRUE(fake.positional->is3D);
	EXPECT_EQ(fake.positional->bank, BankId {2});

	constexpr entt::id_type k_Sound = 0x1234ABCDu;
	const glm::vec3 point {4.0f, 5.0f, 6.0f};
	PlayOptions options;
	options.sound = k_Sound;
	options.is3D = true;
	options.track = false;
	options.position = point;
	EXPECT_EQ(audio::PlaySoundEffect(options), 7u);
	ASSERT_TRUE(fake.lastOptions.has_value());
	EXPECT_EQ(fake.lastOptions->sound, k_Sound);
	EXPECT_TRUE(fake.lastOptions->is3D);
	EXPECT_FALSE(fake.lastOptions->track);
	EXPECT_EQ(fake.lastOptions->position, point);
}

TEST(AudioManager, EachTurnReachesTheServiceAsItsOwnCall)
{
	// the world's turn, the temple's turn inside the citadel and the paused end of turn are three calls, not one
	test::RestoreService<Locator::audio> keep;
	auto& fake = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	audio::ProcessTurn();
	audio::ProcessCitadelTurn();
	audio::Paused();
	ASSERT_EQ(fake.calls.size(), 3u);
	EXPECT_EQ(fake.calls[0], "turn");
	EXPECT_EQ(fake.calls[1], "citadel turn");
	EXPECT_EQ(fake.calls[2], "paused");
}

TEST(AudioManager, ArgumentsAndAnswersPassThrough)
{
	test::RestoreService<Locator::audio> keep;
	auto& fake = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());

	// A function object is handed over whole
	audio::RegisterObject(21, []() -> std::optional<glm::vec3> { return glm::vec3(1.0f, 2.0f, 3.0f); });
	EXPECT_EQ(fake.registered, 21u);
	ASSERT_TRUE(fake.registeredPosition.has_value());
	EXPECT_EQ(*fake.registeredPosition, glm::vec3(1.0f, 2.0f, 3.0f));

	// An optional answer, both ways
	const auto found = audio::FindSample(BankId {4}, "G_PickUpFood.wav");
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->bank, 4);
	EXPECT_EQ(found->number, 12);
	EXPECT_FALSE(audio::FindSample(BankId {4}, "nothing.wav").has_value());

	// A set value read back
	audio::SetSampleMainVolume(64);
	EXPECT_EQ(audio::SampleMainVolume(), 64);

	// The game's queries go through by value
	GameQueries queries;
	queries.videoPlaying = [] { return true; };
	audio::Init(queries);
	ASSERT_EQ(fake.calls.size(), 1u);
	EXPECT_EQ(fake.calls[0], "init video");
}

TEST(AudioManager, TheNoOpBaseAnswersAsAnEmptyEngine)
{
	test::RestoreService<Locator::audio> keep;
	Locator::audio::emplace<AudioManagerNoOp>();
	EXPECT_EQ(audio::PlaySoundEffect(PlayOptions {}), k_NoChannel);
	EXPECT_EQ(audio::CreatureBank("Ape"), k_NoBank);
	EXPECT_FALSE(audio::FindSample(BankId {1}, "x.wav").has_value());
	EXPECT_FALSE(audio::IsPlaying(Channel {1}));
	EXPECT_EQ(audio::MaxDistance(Sample {1, 1}), 0.0f);
	EXPECT_FALSE(audio::SoundExists());
}

TEST(AudioManager, AnEmptySlotIsAClearError)
{
	test::RestoreService<Locator::audio> keep;
	Locator::audio::reset();
	try
	{
		audio::StopAllSoundEffects();
		FAIL() << "no error";
	}
	catch (const std::logic_error& error)
	{
		EXPECT_NE(std::string(error.what()).find("Locator::audio"), std::string::npos) << error.what();
	}
}
