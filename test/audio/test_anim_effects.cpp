/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/Engine/AnimEffectBank.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"

// The anim effects in the audio core: the tables of a bank read once as it is registered, the effect number drawn with
// the audio library's own random, and PlayAnimationEffect (the caller's distance against 800 and the sample's max
// distance, the owner's point, stop / release of the row's samples). Real editor.sad and
// VillagersBanter.sad (OPENBLACK_TEST_BW_ROOT; without it the tests skip). A fake output stands for QMixer.
// The AnimEffectsSyntheticTest tests run the same logic on small banks built in memory and need no game data.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// The tables of a bank from its path, as the miracles' AnimEffectBank read them (only these tests do): a bank the game has
/// registered already has its tables in the core, which are copied, not read again; its last three parts name it
/// ("Sfx/Game/spells.sad"). A bank not registered is read from its file (nothing when it is missing or has none)
void LoadFromPath(AnimEffectTable& table, const std::filesystem::path& path)
{
	const auto tail = path.parent_path().parent_path().filename() / path.parent_path().filename() / path.filename();
	if (const auto* registered = anim_effects::Tables(FindBank(tail.generic_string())); registered != nullptr)
	{
		table.rows = registered->rows;
		table.waves = registered->waves;
		table.samples = registered->samples;
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	pack::PackFile file;
	if (file.ReadFile(*fileSystem.GetData(path)) != pack::PackResult::Success)
	{
		return;
	}
	table.Load(file);
}

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

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

constexpr auto k_Villager = static_cast<entt::entity>(7);
constexpr auto k_Abode = static_cast<entt::entity>(8);

/// The audio core over the fake output, shared by the installation fixture and the synthetic one
class AnimEffectsBase: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	static inline std::map<uint32_t, glm::vec3> s_Things;
	static inline int s_Rand {16383};
	BankId editor {k_NoBank};
	BankId banter {k_NoBank};

	void StartAudio()
	{
		s_Things.clear();
		s_Rand = 16383;
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		audio::Init(std::move(queries));
	}
	void StartPlayback()
	{
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return s_Rand; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void StopAudio()
	{
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (QMixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// Game's bank loop: RegisterBank, the anim effect tables, the samples' records
	BankId Load(const std::filesystem::path& root, const std::string& relative, const std::string& group)
	{
		pack::PackFile pack;
		EXPECT_EQ(pack.Open(root / relative), pack::PackResult::Success) << relative;
		return Register(pack, relative, group);
	}
	/// The same loop over a pack already read
	BankId Register(const pack::PackFile& pack, const std::string& relative, const std::string& group)
	{
		const auto bank = RegisterBank(relative, group);
		anim_effects::RegisterTables(bank, pack);
		const auto& headers = pack.GetAudioSampleHeaders();
		for (const auto& header : headers)
		{
			const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {}, header, {});
			sound->bank = bank;
			sounds.emplace(SampleId(bank, header.id), *sound);
		}
		return bank;
	}
	int SampleOn(size_t channel) const
	{
		const auto infos = sample_play::Channels();
		return infos[channel].sample;
	}
};

/// The real editor.sad and VillagersBanter.sad
class AnimEffectsTest: public AnimEffectsBase
{
protected:
	void SetUp() override
	{
		const auto root = GameRoot();
		if (!root)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
		}
		StartAudio();
		editor = Load(*root, "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = Load(*root, "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		StartPlayback();
	}
	void TearDown() override
	{
		if (!GameRoot())
		{
			return;
		}
		StopAudio();
	}
};

// ---- small sound banks built in memory ------------------------------------------------------------------------------

struct SyntheticSample
{
	int32_t id;
	float maxDistance;
};

void AppendBlock(std::vector<uint8_t>& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.insert(out.end(), blockName.begin(), blockName.end());
	const auto size = static_cast<uint32_t>(data.size());
	const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
	out.insert(out.end(), sizeBytes, sizeBytes + sizeof(size));
	out.insert(out.end(), data.begin(), data.end());
}

template <typename T>
void AppendValue(std::vector<uint8_t>& out, const T& value)
{
	const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
	out.insert(out.end(), bytes, bytes + sizeof(T));
}

/// A .sad in memory: the bank info, one 4-byte wave per sample, the sample table and the two anim effect tables
std::vector<uint8_t> MakeSoundBank(const std::vector<SyntheticSample>& samples, const std::vector<std::array<int32_t, 6>>& rows,
                                   const std::vector<int32_t>& waves)
{
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	AppendBlock(file, "LHFileSegmentBankInfo", std::vector<uint8_t>(12, 0));
	AppendBlock(file, "LHAudioWaveData", std::vector<uint8_t>(samples.size() * 4, 0));
	std::vector<uint8_t> table;
	AppendValue(table, static_cast<uint16_t>(samples.size()));
	AppendValue(table, static_cast<uint16_t>(0));
	for (size_t i = 0; i < samples.size(); ++i)
	{
		pack::AudioBankSampleHeader header;
		std::memset(&header, 0, sizeof(header)); // the padding too: the loader reads one word across it
		std::strncpy(header.name.data(), "synthetic.wav", header.name.size() - 1);
		header.id = samples[i].id;
		header.size = 4;
		header.offset = static_cast<uint32_t>(i * 4);
		header.sampleRate = 22050;
		header.maxDist = samples[i].maxDistance;
		AppendValue(table, header);
	}
	AppendBlock(file, "LHAudioBankSampleTable", table);
	std::vector<uint8_t> anim;
	AppendValue(anim, static_cast<int32_t>(rows.size()));
	AppendValue(anim, static_cast<int32_t>(6));
	for (const auto& row : rows)
	{
		AppendValue(anim, row);
	}
	AppendBlock(file, "LHAudioAnimArrayTable", anim);
	std::vector<uint8_t> lists;
	for (const auto value : waves)
	{
		AppendValue(lists, value);
	}
	AppendBlock(file, "LHAudioWaveNumTable", lists);
	return file;
}

constexpr int32_t k_Wild = AnimEffectTable::k_Wildcard;
constexpr float k_StepMaxDistance = 20.0f;

/// The effects bank: a footstep row with a list of ten samples (100..109), a saw row with one sample (300), and a row
/// with a wildcard column (400)
std::vector<uint8_t> MakeEffectsBank()
{
	std::vector<SyntheticSample> samples;
	for (int32_t id = 100; id < 110; ++id)
	{
		samples.push_back({id, k_StepMaxDistance});
	}
	samples.push_back({300, 50.0f});
	samples.push_back({400, k_StepMaxDistance});
	const std::vector<std::array<int32_t, 6>> rows = {
	    {1, 2, 1, 2, 4, 0},
	    {1, 2, 1, 2, 31, 11},
	    {k_Wild, 2, 1, 2, 50, 13},
	};
	const std::vector<int32_t> waves = {10, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 1, 300, 1, 400};
	return MakeSoundBank(samples, rows, waves);
}

/// The banter bank: one row with the samples 1..3
std::vector<uint8_t> MakeBanterBank()
{
	return MakeSoundBank({{1, 50.0f}, {2, 50.0f}, {3, 50.0f}}, {{1, 2, 1, 2, 0x92, 0}}, {3, 1, 2, 3});
}

/// The same paths and groups as the installation fixture, with the banks above
class AnimEffectsSyntheticTest: public AnimEffectsBase
{
protected:
	void SetUp() override
	{
		StartAudio();
		editor = RegisterBuffer(MakeEffectsBank(), "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = RegisterBuffer(MakeBanterBank(), "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		StartPlayback();
	}
	void TearDown() override { StopAudio(); }

	BankId RegisterBuffer(const std::vector<uint8_t>& bytes, const std::string& relative, const std::string& group)
	{
		pack::PackFile pack;
		EXPECT_EQ(pack.Open(bytes), pack::PackResult::Success) << relative;
		return Register(pack, relative, group);
	}
};
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, TablesReadOnceByBank)
{
	const auto* tables = anim_effects::Tables(editor);
	ASSERT_NE(tables, nullptr);
	EXPECT_EQ(tables->rows.size(), 201u); // the count the old AnimationSounds logged ("201 + 3 effect rows")
	ASSERT_NE(anim_effects::Tables(banter), nullptr);
	EXPECT_EQ(anim_effects::Tables(banter)->rows.size(), 3u);
	EXPECT_EQ(tables->name, "editor.sad");
	// the miracles' alias is the same type
	const AnimEffectBank* alias = tables;
	EXPECT_EQ(alias->FindList({1, 2, 1, 2, 4}), tables->FindList({1, 2, 1, 2, 4}));
	// a second registration of the same bank reads nothing more
	const auto* before = anim_effects::Tables(editor);
	pack::PackFile pack;
	anim_effects::RegisterTables(editor, pack);
	EXPECT_EQ(anim_effects::Tables(editor), before);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, NumberUsesTheAudioRandom)
{
	// a man's footstep on gravel: {1, 2, 1, 2, 4} -> H_Footstep_Gravel 259..268
	const AnimKey key = {1, 2, 1, 2, 4};
	const auto list = anim_effects::Tables(editor)->FindList(key);
	ASSERT_EQ(list.size(), 10u);
	EXPECT_EQ(list.front(), 259);
	// Random(10) = draw * 10 / 32767, the draw = rand() / 2 plus 0x3FFF on every other call (the flag starts set):
	// the first draw is in the upper half
	s_Rand = 16383;
	EXPECT_EQ(anim_effects::Number(key, editor), list[7]); // (8191 + 16383) * 10 / 32767
	s_Rand = 0;
	EXPECT_EQ(anim_effects::Number(key, editor), list[0]); // 0 * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[9]); // (16383 + 16383) * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[4]);          // 16383 * 10 / 32767
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 9999}, editor), 0); // no row
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, PlayGatesOnTheCallersDistance)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(3.0f, 0.0f, 4.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	const float maxDistance = sounds.at(SampleId(editor, 263)).maxDistance; // the footsteps' 20
	ASSERT_GT(maxDistance, 0.0f);
	// beyond the sample's max distance: nothing
	EXPECT_EQ(
	    PlayAnimationEffect(Owner::Thing(k_Villager), maxDistance + 1.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f),
	    k_NoChannel);
	EXPECT_EQ(output.plays, 0);
	// within: one 3D channel at the owner's point, tracked
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.starts[0].is3D);
	EXPECT_EQ(output.starts[0].position, glm::vec3(3.0f, 0.0f, 4.0f));
	EXPECT_EQ(sample_play::Channels()[0].owner, Owner::Thing(k_Villager));
	EXPECT_TRUE(sample_play::Channels()[0].track);
	// a gone owner has no position: nothing plays
	EXPECT_EQ(PlayAnimationEffect(Owner::Thing(static_cast<entt::entity>(99)), 5.0f, key, AnimAction::Play, editor, false, 0.0f,
	                              0.0f),
	          k_NoChannel);
	// no owner: at the camera
	output = FakeOutput {};
	audio::ClearMap();
	EXPECT_NE(PlayAnimationEffect(Owner::None(), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f), k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(0.0f));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, GlobalMaxDistanceGatesEveryAction)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	// the saw's back stroke {2, 2, 1, *, 31} (M_P_Saw_Wood 760:31) plays, then a stop farther than 800 does nothing
	const AnimKey saw = {1, 2, 1, 2, 31};
	ASSERT_FALSE(anim_effects::Tables(editor)->FindList(saw).empty());
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	PlayAnimationEffect(Owner::Thing(k_Villager), 801.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]); // the stop is gated too
	// another owner's stop leaves it alone; the owner's own stop stops it (the row's samples are stopped)
	PlayAnimationEffect(Owner::Thing(k_Abode), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_FALSE(output.playing[0]);
	// any other action releases the loop
	ASSERT_NE(PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f),
	          k_NoChannel);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Release, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.released[0]);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, BanterAtTheAbodeWithTheVillagersDistance)
{
	// 0x92 (M_P_Yawn 700:146): VillagersBanter.sad, the owner the abode, the distance the villager's
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(2.0f, 0.0f, 0.0f);
	s_Things[static_cast<uint32_t>(k_Abode)] = glm::vec3(30.0f, 0.0f, 0.0f);
	const AnimKey key = {1, 2, 1, 2, 0x92};
	ASSERT_FALSE(anim_effects::Tables(banter)->FindList(key).empty());
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Abode), 2.0f, key, AnimAction::Play, banter, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(30.0f, 0.0f, 0.0f));
	const int sample = SampleOn(0);
	EXPECT_GE(sample, 1);
	EXPECT_LE(sample, 23); // HELP_TEXT_VILLAGER_BANTER_*
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, MinAndMaxOverrideOnlyWhenPositive)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 0.0f, 0.0f);
	const auto withSad = output.starts[0];
	audio::ClearMap();
	output = FakeOutput {};
	// min / max with their caller bits 0x80 / 0x100 when > 0
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 2.5f, 12.0f);
	EXPECT_FLOAT_EQ(output.starts[0].minDistance, 2.5f);
	EXPECT_FLOAT_EQ(output.starts[0].maxDistance, 12.0f);
	EXPECT_FLOAT_EQ(withSad.scale, output.starts[0].scale);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, MiraclesBankCopiesTheCoresTables)
{
	// SpellSounds' AnimEffectBank::Load(path) (LoadFromPath here) of a registered bank takes the tables the core read at
	// registration instead of reading the file again; the same rows, lists and samples as a read of the file
	const auto root = GameRoot();
	const auto spells = Load(*root, "Audio/Sfx/Game/spells.sad", "spells.sad");
	const auto* core = anim_effects::Tables(spells);
	ASSERT_NE(core, nullptr);
	AnimEffectBank copied {"spells.sad", {}, {}, {}};
	LoadFromPath(copied, *root / "Audio" / "Sfx" / "Game" / "spells.sad");
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*root / "Audio/Sfx/Game/spells.sad"), pack::PackResult::Success);
	AnimEffectBank read {"spells.sad", {}, {}, {}};
	read.Load(pack);
	ASSERT_FALSE(read.rows.empty());
	EXPECT_EQ(copied.rows, read.rows);
	EXPECT_EQ(copied.waves, read.waves);
	EXPECT_EQ(copied.samples.size(), read.samples.size());
	// every row's own key finds the same list in both (the attribute columns with the wildcard kept)
	for (const auto& row : read.rows)
	{
		const std::array<int32_t, 5> key = {row[0], row[1], row[2], row[3], row[4]};
		EXPECT_EQ(core->FindList(key), read.FindList(key));
	}
	EXPECT_EQ(copied.SoundId(40), SampleId(spells, 40));
}

// ---- the same logic on the banks built in memory (no game data) ------------------------------------------------------

TEST_F(AnimEffectsSyntheticTest, TablesReadOnceByBankSynthetic)
{
	const auto* tables = anim_effects::Tables(editor);
	ASSERT_NE(tables, nullptr);
	ASSERT_EQ(tables->rows.size(), 3u); // the three rows of MakeEffectsBank
	EXPECT_EQ(tables->rows[0], (std::array<int32_t, 6> {1, 2, 1, 2, 4, 0}));
	EXPECT_EQ(tables->waves.size(), 15u);
	ASSERT_NE(anim_effects::Tables(banter), nullptr);
	EXPECT_EQ(anim_effects::Tables(banter)->rows.size(), 1u);
	EXPECT_EQ(tables->name, "editor.sad");
	// the miracles' alias is the same type
	const AnimEffectBank* alias = tables;
	EXPECT_EQ(alias->FindList({1, 2, 1, 2, 4}), tables->FindList({1, 2, 1, 2, 4}));
	// a second registration of the same bank reads nothing more
	const auto* before = anim_effects::Tables(editor);
	pack::PackFile empty;
	anim_effects::RegisterTables(editor, empty);
	EXPECT_EQ(anim_effects::Tables(editor), before);
	// a bank whose pack has no anim effect tables gets none
	const auto plain = RegisterBank("Audio/Sfx/Game/plain.sad", "plain.sad");
	anim_effects::RegisterTables(plain, empty);
	EXPECT_EQ(anim_effects::Tables(plain), nullptr);
}

TEST_F(AnimEffectsSyntheticTest, NumberUsesTheAudioRandomSynthetic)
{
	const AnimKey key = {1, 2, 1, 2, 4};
	const auto list = anim_effects::Tables(editor)->FindList(key);
	ASSERT_EQ(list.size(), 10u);
	EXPECT_EQ(list.front(), 100);
	EXPECT_EQ(list.back(), 109);
	// Random(10) = draw * 10 / 32767, the draw = rand / 2 plus 16383 on every other call, the first call included
	s_Rand = 16383;
	EXPECT_EQ(anim_effects::Number(key, editor), list[7]); // (8191 + 16383) * 10 / 32767
	s_Rand = 0;
	EXPECT_EQ(anim_effects::Number(key, editor), list[0]); // 0 * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[9]); // (16383 + 16383) * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[4]);          // 16383 * 10 / 32767
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 9999}, editor), 0); // no row
	// a list of one sample gives that sample
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 31}, editor), 300);
	// a wildcard column matches any value of the key
	EXPECT_EQ(anim_effects::Number({77, 2, 1, 2, 50}, editor), 400);
}

TEST_F(AnimEffectsSyntheticTest, PlayGatesOnTheCallersDistanceSynthetic)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(3.0f, 0.0f, 4.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	// every sample of the footstep list has the same max distance, so the draw does not matter
	const float maxDistance = sounds.at(SampleId(editor, 103)).maxDistance;
	ASSERT_FLOAT_EQ(maxDistance, k_StepMaxDistance);
	// beyond the sample's max distance: nothing
	EXPECT_EQ(
	    PlayAnimationEffect(Owner::Thing(k_Villager), maxDistance + 1.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f),
	    k_NoChannel);
	EXPECT_EQ(output.plays, 0);
	// within: one 3D channel at the owner's point, tracked
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.starts[0].is3D);
	EXPECT_EQ(output.starts[0].position, glm::vec3(3.0f, 0.0f, 4.0f));
	EXPECT_EQ(sample_play::Channels()[0].owner, Owner::Thing(k_Villager));
	EXPECT_TRUE(sample_play::Channels()[0].track);
	// an owner that is gone has no position: nothing plays
	EXPECT_EQ(PlayAnimationEffect(Owner::Thing(static_cast<entt::entity>(99)), 5.0f, key, AnimAction::Play, editor, false, 0.0f,
	                              0.0f),
	          k_NoChannel);
	// no owner: at the camera
	output = FakeOutput {};
	audio::ClearMap();
	EXPECT_NE(PlayAnimationEffect(Owner::None(), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f), k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(0.0f));
}

TEST_F(AnimEffectsSyntheticTest, GlobalMaxDistanceGatesEveryActionSynthetic)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	// the saw row has one sample (300, max distance 50)
	const AnimKey saw = {1, 2, 1, 2, 31};
	ASSERT_EQ(anim_effects::Tables(editor)->FindList(saw), (std::vector<int32_t> {300}));
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	// a stop farther than the global maximum does nothing
	PlayAnimationEffect(Owner::Thing(k_Villager), anim_effects::k_MaxDistance + 1.0f, saw, AnimAction::Stop, editor, true, 0.0f,
	                    0.0f);
	EXPECT_TRUE(output.playing[0]);
	// another owner's stop leaves it alone; the owner's own stop stops it
	PlayAnimationEffect(Owner::Thing(k_Abode), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_FALSE(output.playing[0]);
	// any other action releases the loop
	ASSERT_NE(PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f),
	          k_NoChannel);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Release, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.released[0]);
}

TEST_F(AnimEffectsSyntheticTest, BanterAtTheAbodeWithTheVillagersDistanceSynthetic)
{
	// the banter bank, the owner the abode, the distance the villager's
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(2.0f, 0.0f, 0.0f);
	s_Things[static_cast<uint32_t>(k_Abode)] = glm::vec3(30.0f, 0.0f, 0.0f);
	const AnimKey key = {1, 2, 1, 2, 0x92};
	ASSERT_EQ(anim_effects::Tables(banter)->FindList(key), (std::vector<int32_t> {1, 2, 3}));
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Abode), 2.0f, key, AnimAction::Play, banter, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(30.0f, 0.0f, 0.0f));
	const int sample = SampleOn(0);
	EXPECT_GE(sample, 1);
	EXPECT_LE(sample, 3); // the row's list
}

TEST_F(AnimEffectsSyntheticTest, MinAndMaxOverrideOnlyWhenPositiveSynthetic)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 0.0f, 0.0f);
	const auto withSad = output.starts[0];
	audio::ClearMap();
	output = FakeOutput {};
	// a positive min / max of the caller replaces the sample's
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 2.5f, 12.0f);
	EXPECT_FLOAT_EQ(output.starts[0].minDistance, 2.5f);
	EXPECT_FLOAT_EQ(output.starts[0].maxDistance, 12.0f);
	EXPECT_FLOAT_EQ(withSad.scale, output.starts[0].scale);
}

TEST_F(AnimEffectsSyntheticTest, MiraclesBankCopiesTheCoresTablesSynthetic)
{
	// a registered bank's tables are copied from the core, not read from its file
	const auto bytes = MakeEffectsBank();
	const auto spells = RegisterBuffer(bytes, "Audio/Sfx/Game/spells.sad", "spells.sad");
	const auto* core = anim_effects::Tables(spells);
	ASSERT_NE(core, nullptr);
	AnimEffectBank copied {"spells.sad", {}, {}, {}};
	LoadFromPath(copied, std::filesystem::path("Audio") / "Sfx" / "Game" / "spells.sad");
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(bytes), pack::PackResult::Success);
	AnimEffectBank read {"spells.sad", {}, {}, {}};
	read.Load(pack);
	ASSERT_EQ(read.rows.size(), 3u);
	EXPECT_EQ(copied.rows, read.rows);
	EXPECT_EQ(copied.waves, read.waves);
	EXPECT_EQ(copied.samples.size(), read.samples.size());
	EXPECT_EQ(read.samples.size(), 12u); // 100..109, 300 and 400
	// every row's own key finds the same list in both (the wildcard column kept)
	for (const auto& row : read.rows)
	{
		const std::array<int32_t, 5> key = {row[0], row[1], row[2], row[3], row[4]};
		EXPECT_EQ(core->FindList(key), read.FindList(key));
	}
	EXPECT_EQ(copied.SoundId(40), SampleId(spells, 40));
}

// The samples' bytes move out of the pack into their sounds unchanged (synthetic bank, no game data)
TEST(SoundBankBytes, MovedIntoTheSoundUnchangedSynthetic)
{
	const std::vector<uint8_t> waves = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	AppendBlock(file, "LHFileSegmentBankInfo", std::vector<uint8_t>(12, 0));
	AppendBlock(file, "LHAudioWaveData", waves);
	std::vector<uint8_t> table;
	AppendValue(table, static_cast<uint16_t>(2));
	AppendValue(table, static_cast<uint16_t>(0));
	for (const auto& [offset, size] : {std::pair<uint32_t, uint32_t> {0, 4}, std::pair<uint32_t, uint32_t> {4, 5}})
	{
		pack::AudioBankSampleHeader header;
		std::memset(&header, 0, sizeof(header));
		std::strncpy(header.name.data(), "synthetic.wav", header.name.size() - 1);
		header.offset = offset;
		header.size = size;
		header.sampleRate = 22050;
		AppendValue(table, header);
	}
	AppendBlock(file, "LHAudioBankSampleTable", table);
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(file), pack::PackResult::Success);
	const auto copy = pack.GetAudioSamplesData();
	ASSERT_EQ(copy.size(), 2u);
	EXPECT_EQ(copy[0], (std::vector<uint8_t> {1, 2, 3, 4}));
	EXPECT_EQ(copy[1], (std::vector<uint8_t> {5, 6, 7, 8, 9}));
	auto taken = pack.TakeAudioSamplesData();
	EXPECT_EQ(taken, copy);
	EXPECT_TRUE(pack.GetAudioSamplesData().empty());
	for (size_t i = 0; i < taken.size(); ++i)
	{
		std::vector<std::vector<uint8_t>> buffer;
		buffer.emplace_back(std::move(taken[i]));
		const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {},
		                                             pack.GetAudioSampleHeader(static_cast<uint32_t>(i)), std::move(buffer));
		ASSERT_EQ(sound->buffer.size(), 1u);
		EXPECT_EQ(sound->buffer[0], copy[i]) << i;
	}
}
