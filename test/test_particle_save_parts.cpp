/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The save keeps the particles in two parts: the synced effects, and the effects of this computer. Each part loads
// alone, and a save from before the split still loads.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
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

#include "3D/WaterRings.h"
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

/// The two parts of a save of the particles
struct Parts
{
	std::vector<uint8_t> synced;
	std::vector<uint8_t> local;
	std::vector<std::pair<size_t, size_t>> syncedPresentation;
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
	}

	void TearDown() override
	{
		Locator::gameRandom::reset();
		Locator::resources::reset();
		Locator::filesystem::reset();
	}

	static Parts Save(ParticleRuleSystem& rules)
	{
		Parts parts;
		reflection::BinaryWriter synced(Context());
		rules.SaveState(synced);
		parts.syncedPresentation = synced.PresentationRanges();
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
	// The synced part has no presentation value. All of the other part is a presentation value.
	EXPECT_TRUE(withLocalParts.syncedPresentation.empty());
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

	// The synced part starts with its form and the values of the rules. The tables, the effects and the world follow,
	// as in the one part of an old save.
	reflection::BinaryWriter head(Context());
	reflection::Save(head, uint32_t {1});
	reflection::Save(head, false);
	reflection::Save(head, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(head, ParticleRuleSystemInterface::k_NoEffect);
	reflection::Save(head, std::vector<glm::vec3> {});
	reflection::Save(head, ParticleRuleSystemInterface::k_NoEffect);
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
