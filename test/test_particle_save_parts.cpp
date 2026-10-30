/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The save keeps the particles in two parts: the synced effects, and the effects of this computer. Each part loads
// alone, and a save from before the split still loads. The synced part keeps no value that comes from the listener.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/meta/context.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/WaterRings.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameSoundRequests.h"
#include "Common/GameRandom.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Systems/Implementations/ParticleRuleSystem.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs::systems;
namespace reflection = openblack::ecs::reflection;

namespace
{

const entt::meta_ctx& Context()
{
	static const auto context = []() {
		auto made = std::make_unique<entt::meta_ctx>();
		reflection::RegisterComponents(*made);
		return made;
	}();
	return *context;
}

/// The files of the effects, in memory
class FakeFiles final: public filesystem::FileSystemInterface
{
public:
	[[nodiscard]] std::filesystem::path FindPath(const std::filesystem::path& path) const override
	{
		if (!files.contains(path))
		{
			throw std::runtime_error("not found");
		}
		return path;
	}
	std::unique_ptr<std::istream> GetData(const std::filesystem::path& /*unused*/) override { return nullptr; }
	[[nodiscard]] bool IsPathValid(const std::filesystem::path& path) override { return files.contains(path); }
	std::unique_ptr<filesystem::Stream> Open(const std::filesystem::path& /*unused*/,
	                                         filesystem::Stream::Mode /*unused*/) override
	{
		return nullptr;
	}
	[[nodiscard]] bool Exists(const std::filesystem::path& path) const override { return files.contains(path); }
	void SetGamePath(const std::filesystem::path& path) override { gamePath = path; }
	[[nodiscard]] const std::filesystem::path& GetGamePath() const override { return gamePath; }
	void AddAdditionalPath(const std::filesystem::path& /*unused*/) override {}
	std::vector<uint8_t> ReadAll(const std::filesystem::path& path) override { return files.at(path); }
	void Iterate(const std::filesystem::path& /*unused*/, bool /*unused*/,
	             const std::function<void(const std::filesystem::path&)>& /*unused*/) const override
	{
	}

	std::map<std::filesystem::path, std::vector<uint8_t>> files;
	std::filesystem::path gamePath;
};

class FakeRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t n, DrawSite /*site*/) override { return n == 0 ? 0 : game_random::LHRand(n, _seeds.synced); }
	float SyncedFloatRand(float x, DrawSite /*site*/) override { return game_random::FloatRand(x, _seeds.synced); }
	uint32_t LocalRand(int32_t n) override { return n == 0 ? 0 : game_random::LHRand(static_cast<uint32_t>(n), _seeds.local); }
	float LocalFloatRand(float x) override { return game_random::FloatRand(x, _seeds.local); }
	int32_t CrtRand() override { return game_random::CrtRand(_crt); }
	void CrtSrand(uint32_t seed) override { _crt = seed; }
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return _seeds; }
	void SetSeeds(GameRandomSeeds seeds) override { _seeds = seeds; }
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return _stream; }
	void SetParticleStream(ParticleRandomStream stream) override { _stream = stream; }

private:
	GameRandomSeeds _seeds;
	ParticleRandomStream _stream {ParticleRandomStream::None};
	uint32_t _crt {1};
};

/// An effect of sprites that a disk sends out
std::string EffectText(int maxAtoms)
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\nPROPERTY Hierarchies ARRAY SIZE 3 0 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 3 1 0 0\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	       "BEGINCLASS ParticleSpriteCreator Sprite0\nBEGINPROPERTIES\nPROPERTY ColorA INTEGER 200\n"
	       "PROPERTY InitialScale FLOAT 2\nPROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet1.raw\n"
	       "ENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS DiskEmitter Emitter0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	       "PROPERTY PCreator PERSIS_PNTR Sprite0\nPROPERTY EmissionFreq FLOAT 10\nPROPERTY MaxAtoms INTEGER " +
	       std::to_string(maxAtoms) +
	       "\nPROPERTY Radius FLOAT 3\nPROPERTY Height FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS RemoveRuleOldAgeOnly Remove0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	       "PROPERTY DieAge FLOAT 0.45\nENDPROPERTIES\nENDCLASS\n";
}

/// The bolts of the storms as the game's file has them: a point under the cloud, whose strike thunders from the land
std::string StormBoltsText()
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\nPROPERTY Hierarchies ARRAY SIZE 3 0 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 3 1 0 0\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	       "BEGINCLASS ParticleSpriteCreator Root\nBEGINPROPERTIES\nPROPERTY ColorA INTEGER 255\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS UR_LightningStrike Strike\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	       "PROPERTY NextGroups ARRAY SIZE 1 1\nPROPERTY PCreator PERSIS_PNTR Root\n"
	       "PROPERTY SoundLightning SOUND_ACTION SOUND_SPELL_LIGHTNING LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0\n"
	       "ENDPROPERTIES\nENDCLASS\n";
}

/// Where the storm strikes, under its cloud
constexpr glm::vec3 k_CloudPoint {300.0f, 160.0f, 0.0f};
/// A listener at the place where the thunder starts, which hears it at once
constexpr glm::vec3 k_NearListener {300.0f, 0.0f, 0.0f};
/// A listener 3 km from the strike, which hears the thunder after more than 8 seconds
constexpr glm::vec3 k_FarListener {-2700.0f, 0.0f, 0.0f};
/// The turns of each run of the storm
constexpr int k_StormTurns = 40;

/// The two parts of a save of the particles
struct Parts
{
	std::vector<uint8_t> synced;
	std::vector<uint8_t> local;
	std::vector<std::pair<size_t, size_t>> localPresentation;
};

/// The number, the file and the place of each effect, newest first
using EffectList = std::vector<std::tuple<ParticleRuleSystemInterface::EffectId, std::string, float>>;

class ParticleSaveParts: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& files = static_cast<FakeFiles&>(Locator::filesystem::emplace<FakeFiles>());
		const auto folder = files.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles";
		for (const auto& [name, atoms] : {std::pair {"Synced", 6}, std::pair {"Local", 4}, std::pair {"Other", 5}})
		{
			const auto text = EffectText(atoms);
			files.files[folder / (std::string(name) + ".txt")] = std::vector<uint8_t>(text.begin(), text.end());
		}
		Locator::resources::emplace<resources::Resources>();
		Locator::gameRandom::emplace<FakeRandom>();
		// The rules write their warnings to the log of the game
		if (spdlog::get("game") == nullptr)
		{
			_log = spdlog::null_logger_st("game");
		}
	}

	void TearDown() override
	{
		if (_log != nullptr)
		{
			spdlog::drop("game");
			_log.reset();
		}
		Locator::sounds::reset();
		Locator::audio::reset();
		Locator::gameRandom::reset();
		Locator::resources::reset();
		Locator::filesystem::reset();
	}

	static Parts Save(ParticleRuleSystem& rules)
	{
		Parts parts;
		reflection::BinaryWriter synced(Context());
		rules.SaveState(synced);
		parts.synced = std::move(synced.Bytes());
		reflection::BinaryWriter local(Context());
		rules.LocalEffectsState()->SaveState(local);
		parts.localPresentation = local.PresentationRanges();
		parts.local = std::move(local.Bytes());
		return parts;
	}

	static bool Read(SavedStateInterface& state, std::span<const uint8_t> bytes)
	{
		const reflection::EntityRemap identity;
		reflection::BinaryReader in(Context(), bytes, identity);
		return state.LoadState(in) && !in.Failed() && in.Remaining() == 0;
	}

	/// The files of the two effects that each turn keeps: the bolts of the storms, and the pieces of what blasts break
	static void AddStormFiles()
	{
		auto& files = static_cast<FakeFiles&>(Locator::filesystem::value());
		const auto folder = files.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles";
		const auto bolts = StormBoltsText();
		files.files[folder / "SF_LightningStrike.txt"] = std::vector<uint8_t>(bolts.begin(), bolts.end());
		const auto pieces = EffectText(1);
		files.files[folder / "SF_ExplodeObject.txt"] = std::vector<uint8_t>(pieces.begin(), pieces.end());
	}

	/// Puts the listener of the audio at a place. With no place, the computer has no audio, so no sound waits.
	static void PlaceListener(std::optional<glm::vec3> listener)
	{
		Locator::sounds::emplace<audio::GameSoundRequests>();
		Locator::audio::reset();
		if (listener.has_value())
		{
			Locator::audio::emplace<audio::AudioManagerNoOp>().SetListenerOverride(audio::Listener {.origin = *listener});
		}
	}

	/// The storm strikes once in the first turn. Gives the two parts of the save and the count of the sounds after
	/// each turn.
	static std::vector<std::pair<Parts, size_t>> RunStorm(ParticleRuleSystem& rules, std::optional<glm::vec3> listener)
	{
		PlaceListener(listener);
		Locator::gameRandom::emplace<FakeRandom>();
		rules.AddStormBolt(k_CloudPoint);
		std::vector<std::pair<Parts, size_t>> turns;
		for (int turn = 0; turn < k_StormTurns; ++turn)
		{
			rules.ProcessTurn();
			turns.emplace_back(Save(rules), rules.GetSoundCount());
		}
		return turns;
	}

	/// The turns from now until the particles have no sound
	static int TurnsUntilSilent(ParticleRuleSystem& rules)
	{
		int turns = 0;
		while (rules.GetSoundCount() != 0 && turns < 1000)
		{
			rules.ProcessTurn();
			++turns;
		}
		return turns;
	}

	std::shared_ptr<spdlog::logger> _log;

	static EffectList Effects(const ParticleRuleSystem& rules)
	{
		EffectList list;
		for (const auto& effect : rules.GetEffects())
		{
			list.emplace_back(effect.id, effect.file, effect.origin.x);
		}
		return list;
	}
};

} // namespace

TEST_F(ParticleSaveParts, TheTwoPartsGiveBackTheEffectsInTheirOrder)
{
	ParticleRuleSystem rules;
	ASSERT_NE(rules.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true), ParticleRuleSystemInterface::k_NoEffect);
	ASSERT_NE(rules.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, false), ParticleRuleSystemInterface::k_NoEffect);
	ASSERT_NE(rules.Start("Other", {3.0f, 0.0f, 0.0f}, 1.0f, true), ParticleRuleSystemInterface::k_NoEffect);
	const auto parts = Save(rules);

	ParticleRuleSystem loaded;
	ASSERT_TRUE(Read(loaded, parts.synced));
	ASSERT_TRUE(Read(*loaded.LocalEffectsState(), parts.local));
	// The effect of this computer goes back between the synced effects
	EXPECT_EQ(Effects(loaded), Effects(rules));
	ASSERT_EQ(Effects(loaded).size(), 3u);
	// A second save of what was read gives the same bytes
	const auto again = Save(loaded);
	EXPECT_EQ(again.synced, parts.synced);
	EXPECT_EQ(again.local, parts.local);
}

TEST_F(ParticleSaveParts, TheSyncedPartDoesNotChangeWithTheEffectsOfThisComputer)
{
	ParticleRuleSystem alone;
	alone.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	ParticleRuleSystem withLocal;
	withLocal.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	withLocal.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, false);
	const auto aloneParts = Save(alone);
	const auto withLocalParts = Save(withLocal);
	EXPECT_EQ(aloneParts.synced, withLocalParts.synced);
	// All of the part of this computer is a presentation value
	ASSERT_EQ(withLocalParts.localPresentation.size(), 1u);
	EXPECT_EQ(withLocalParts.localPresentation.front(), std::make_pair(size_t {0}, withLocalParts.local.size()));
	EXPECT_NE(aloneParts.local, withLocalParts.local);
}

TEST_F(ParticleSaveParts, ALoadOfTheSyncedPartAloneHasNoEffectOfThisComputer)
{
	ParticleRuleSystem rules;
	const auto synced = rules.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	const auto local = rules.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, false);
	const auto parts = Save(rules);

	ParticleRuleSystem loaded;
	ASSERT_TRUE(Read(loaded, parts.synced));
	EXPECT_TRUE(loaded.IsRunning(synced));
	EXPECT_FALSE(loaded.IsRunning(local));
	ASSERT_EQ(Effects(loaded).size(), 1u);
	// A new effect takes a number that no loaded effect has
	const auto next = loaded.Start("Other", {3.0f, 0.0f, 0.0f}, 1.0f, false);
	EXPECT_NE(next, synced);
	EXPECT_NE(next, ParticleRuleSystemInterface::k_NoEffect);
	// The synced part of this computer is the same as the synced part of the save
	EXPECT_EQ(Save(loaded).synced, parts.synced);
}

TEST_F(ParticleSaveParts, ASaveFromBeforeTheSplitLoads)
{
	ParticleRuleSystem rules;
	const auto synced = rules.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	const auto parts = Save(rules);

	// The synced part starts with its form, the next synced number and the values of the rules. The tables, the effects
	// and the world follow, as in the one part of an old save.
	reflection::BinaryWriter head(Context());
	reflection::Save(head, uint32_t {3});
	reflection::Save(head, ParticleRuleSystemInterface::EffectId {synced + 1});
	reflection::Save(head, false);
	reflection::Save(head, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(head, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(head, std::vector<glm::vec3> {});
	reflection::Save(head, false);
	const auto& headBytes = head.Bytes();
	ASSERT_GE(parts.synced.size(), headBytes.size());
	ASSERT_TRUE(std::equal(headBytes.begin(), headBytes.end(), parts.synced.begin()));

	reflection::BinaryWriter old(Context());
	reflection::Save(old, ParticleRuleSystemInterface::EffectId {synced + 1});
	reflection::Save(old, false);
	reflection::Save(old, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(old, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(old, std::vector<glm::vec3> {});
	reflection::Save(old, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(old, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(old, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(old, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(old, false);
	reflection::Save(old, water_rings::Pace {});
	std::vector<uint8_t> oldBytes = old.Bytes();
	oldBytes.insert(oldBytes.end(), parts.synced.begin() + static_cast<std::ptrdiff_t>(headBytes.size()), parts.synced.end());

	ParticleRuleSystem loaded;
	ASSERT_TRUE(Read(*loaded.OldSaveState(), oldBytes));
	EXPECT_EQ(Effects(loaded), Effects(rules));
	const auto again = Save(loaded);
	EXPECT_EQ(again.synced, parts.synced);
	EXPECT_EQ(again.local, parts.local);
}

TEST_F(ParticleSaveParts, TheSyncedPartIsTheSameWhereverTheListenerIs)
{
	AddStormFiles();
	// A server with no audio, a client whose listener is at the strike, and a client far from the strike
	ParticleRuleSystem server;
	const auto serverTurns = RunStorm(server, std::nullopt);
	ParticleRuleSystem near;
	const auto nearTurns = RunStorm(near, k_NearListener);
	ParticleRuleSystem far;
	const auto farTurns = RunStorm(far, k_FarListener);
	ASSERT_EQ(serverTurns.size(), static_cast<size_t>(k_StormTurns));
	bool thunderWaited = false;
	bool localPartsDiffer = false;
	for (size_t turn = 0; turn < serverTurns.size(); ++turn)
	{
		EXPECT_EQ(nearTurns[turn].first.synced, serverTurns[turn].first.synced) << "turn " << turn;
		EXPECT_EQ(farTurns[turn].first.synced, serverTurns[turn].first.synced) << "turn " << turn;
		// The far client keeps the thunder while it travels, after the strike has gone
		thunderWaited = thunderWaited || farTurns[turn].second > serverTurns[turn].second;
		localPartsDiffer = localPartsDiffer || farTurns[turn].first.local != nearTurns[turn].first.local;
	}
	EXPECT_GT(serverTurns.front().second, 0u);
	EXPECT_TRUE(thunderWaited);
	// The part of this computer keeps the wait, and all of it is a presentation value
	EXPECT_TRUE(localPartsDiffer);
	const auto& last = farTurns.back().first;
	ASSERT_EQ(last.localPresentation.size(), 1u);
	EXPECT_EQ(last.localPresentation.front(), std::make_pair(size_t {0}, last.local.size()));
}

TEST_F(ParticleSaveParts, AThunderThatTravelsComesBackWithTheSaveAndArrivesAtTheSameTurn)
{
	AddStormFiles();
	ParticleRuleSystem far;
	const auto turns = RunStorm(far, k_FarListener);
	// The strike has gone, and only the thunder on its way to the listener is left
	ASSERT_EQ(turns.back().second, 1u);
	const auto& parts = turns.back().first;

	ParticleRuleSystem loaded;
	ASSERT_TRUE(Read(loaded, parts.synced));
	EXPECT_EQ(loaded.GetSoundCount(), 0u);
	ASSERT_TRUE(Read(*loaded.LocalEffectsState(), parts.local));
	EXPECT_EQ(loaded.GetSoundCount(), 1u);
	const auto again = Save(loaded);
	EXPECT_EQ(again.synced, parts.synced);
	EXPECT_EQ(again.local, parts.local);

	const auto original = TurnsUntilSilent(far);
	EXPECT_GT(original, 0);
	EXPECT_EQ(TurnsUntilSilent(loaded), original);
}

TEST_F(ParticleSaveParts, AThunderThatTravelsWhileItsStrikeLivesKeepsItsWaitInThePartOfThisComputer)
{
	AddStormFiles();
	ParticleRuleSystem far;
	PlaceListener(k_FarListener);
	Locator::gameRandom::emplace<FakeRandom>();
	far.AddStormBolt(k_CloudPoint);
	far.ProcessTurn();
	// The strike lives, so the synced part keeps its sound
	ASSERT_EQ(far.GetSoundCount(), 1u);
	const auto parts = Save(far);

	ParticleRuleSystem loaded;
	ASSERT_TRUE(Read(loaded, parts.synced));
	EXPECT_EQ(loaded.GetSoundCount(), 1u);
	ASSERT_TRUE(Read(*loaded.LocalEffectsState(), parts.local));
	EXPECT_EQ(loaded.GetSoundCount(), 1u);
	const auto again = Save(loaded);
	EXPECT_EQ(again.synced, parts.synced);
	EXPECT_EQ(again.local, parts.local);
	// The wait came back: the sound stays after its strike has gone, until it arrives
	const auto original = TurnsUntilSilent(far);
	EXPECT_GT(original, 20);
	EXPECT_EQ(TurnsUntilSilent(loaded), original);
}

TEST_F(ParticleSaveParts, TwoPartsOfTheFormBeforeTheWaitsMovedLoad)
{
	ParticleRuleSystem rules;
	rules.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	rules.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, false);
	const auto parts = Save(rules);

	// Each part starts with its form. The older form has the same bytes after it when no sound waits, but:
	// - the synced part has no next synced number after its form
	// - the synced part keeps the effect of the gesture trails after the waiting storm bolts
	// - the part of this computer keeps the one number of all effects after its form
	// - the part of this computer has no effect of the gesture trails
	// - the part of this computer has no list of waits. With no sound, that list is one of the counts of 0 at its end.
	reflection::BinaryWriter now(Context());
	reflection::Save(now, uint32_t {3});
	reflection::BinaryWriter before(Context());
	reflection::Save(before, uint32_t {1});
	const auto& nowBytes = now.Bytes();
	const auto& beforeBytes = before.Bytes();
	ASSERT_EQ(nowBytes.size(), beforeBytes.size());
	const auto older = [&](std::vector<uint8_t> bytes) {
		EXPECT_TRUE(std::equal(nowBytes.begin(), nowBytes.end(), bytes.begin()));
		std::copy(beforeBytes.begin(), beforeBytes.end(), bytes.begin());
		return bytes;
	};
	auto synced = older(parts.synced);
	const auto afterForm = static_cast<std::ptrdiff_t>(nowBytes.size());
	synced.erase(synced.begin() + afterForm, synced.begin() + afterForm + static_cast<std::ptrdiff_t>(sizeof(uint32_t)));
	// The pause, the two effects and the count of the waiting storm bolts come before the effect of the gesture trails
	constexpr std::ptrdiff_t k_TrailsAt = 1 + 4 + 4 + 4;
	const std::array<uint8_t, sizeof(uint32_t)> noEffect {};
	synced.insert(synced.begin() + afterForm + k_TrailsAt, noEffect.begin(), noEffect.end());
	auto local = older(parts.local);
	local.erase(local.begin() + afterForm + static_cast<std::ptrdiff_t>(sizeof(uint32_t)),
	            local.begin() + afterForm + static_cast<std::ptrdiff_t>(2 * sizeof(uint32_t)));
	// The one number of all effects was after the synced effect
	constexpr uint32_t k_OldNext = 2;
	std::memcpy(local.data() + afterForm, &k_OldNext, sizeof(k_OldNext));
	constexpr size_t k_ZeroCounts = 4;
	ASSERT_GE(local.size(), k_ZeroCounts * sizeof(uint32_t));
	ASSERT_TRUE(std::all_of(local.end() - static_cast<std::ptrdiff_t>(k_ZeroCounts * sizeof(uint32_t)), local.end(),
	                        [](uint8_t byte) { return byte == 0; }));
	local.resize(local.size() - sizeof(uint32_t));

	ParticleRuleSystem loaded;
	ASSERT_TRUE(Read(loaded, synced));
	ASSERT_TRUE(Read(*loaded.LocalEffectsState(), local));
	EXPECT_EQ(Effects(loaded), Effects(rules));
	// A save of what was read has the form of now
	const auto again = Save(loaded);
	EXPECT_EQ(again.synced, parts.synced);
	EXPECT_EQ(again.local, parts.local);
}

TEST_F(ParticleSaveParts, TheEffectsOfThisComputerTakeNoNumberOfASyncedEffect)
{
	// A game server starts the synced effects only. A client also starts effects of its own between them.
	ParticleRuleSystem server;
	const auto serverFirst = server.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	const auto serverSecond = server.Start("Other", {3.0f, 0.0f, 0.0f}, 1.0f, true);
	ParticleRuleSystem client;
	const auto clientFirst = client.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	const auto local = client.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, false);
	const auto clientSecond = client.Start("Other", {3.0f, 0.0f, 0.0f}, 1.0f, true);
	EXPECT_EQ(clientFirst, serverFirst);
	EXPECT_EQ(clientSecond, serverSecond);
	EXPECT_GE(local, ParticleRuleSystem::k_FirstLocalEffect);
	EXPECT_LT(local, k_FirstEffectHandle);
	EXPECT_EQ(Save(client).synced, Save(server).synced);
}

TEST_F(ParticleSaveParts, AJoinerGivesTheNextSyncedEffectTheNumberOfTheServer)
{
	// The newest synced effect ends before the save, so no loaded effect has the highest number
	ParticleRuleSystem server;
	server.Start("Synced", {1.0f, 0.0f, 0.0f}, 1.0f, true);
	const auto ended = server.Start("Other", {3.0f, 0.0f, 0.0f}, 1.0f, true);
	server.Delete(ended);
	const auto parts = Save(server);

	ParticleRuleSystem joiner;
	ASSERT_TRUE(Read(joiner, parts.synced));
	EXPECT_EQ(joiner.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, true), server.Start("Local", {2.0f, 0.0f, 0.0f}, 1.0f, true));
}
