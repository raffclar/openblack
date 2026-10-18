/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The running state of particle effects in a saved game: an effect read back from a save carries on as the effect that
// was saved does

#include <cmath>
#include <cstdint>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <ParticleFile.h>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Particles/ParticleArchive.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"
#include "Particles/ParticleMiracleMaths.h"

using namespace openblack;
using namespace openblack::particles;
namespace reflection = openblack::ecs::reflection;

namespace
{
constexpr float k_Step = 0.1f;

const entt::meta_ctx& Context()
{
	static const auto context = []() {
		auto made = std::make_unique<entt::meta_ctx>();
		reflection::RegisterComponents(*made);
		return made;
	}();
	return *context;
}

/// Flat land, shields that the world finds, things to strike
class FakeWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return 0.0f; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF0000u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] std::optional<TargetInfo> Target(entt::entity target, bool centre) const override
	{
		for (const auto& candidate : candidates)
		{
			if (candidate.object == target)
			{
				return TargetInfo {.position =
				                       candidate.position + glm::vec3(0.0f, centre ? candidate.height * 0.5f : 0.0f, 0.0f),
				                   .radius = 1.0f,
				                   .height = candidate.height};
			}
		}
		return std::nullopt;
	}
	[[nodiscard]] std::vector<StrikeCandidate> StrikeCandidates(glm::vec3 /*centre*/, size_t /*cells*/) const override
	{
		return candidates;
	}
	void AddShield(const std::shared_ptr<ShieldSphere>& shield) override { shields.push_back(shield); }
	[[nodiscard]] std::shared_ptr<ShieldSphere> FindShield(glm::vec3 point, float margin) const override
	{
		for (const auto& weak : shields)
		{
			if (auto shield = weak.lock(); shield && maths::InsideSphere(point, shield->centre, shield->radius, margin))
			{
				return shield;
			}
		}
		return nullptr;
	}
	[[nodiscard]] std::shared_ptr<ShieldSphere> FindShieldCrossing(glm::vec3 before, glm::vec3 point,
	                                                               float margin) const override
	{
		const auto shield = FindShield(point, margin);
		return shield && !maths::InsideSphere(before, shield->centre, shield->radius, margin) ? shield : nullptr;
	}

	std::vector<StrikeCandidate> candidates;
	std::vector<std::weak_ptr<ShieldSphere>> shields;
};

class FakeRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t n) override { return n == 0 ? 0 : game_random::LHRand(n, _seeds.synced); }
	float GameFloatRand(float x) override { return game_random::FloatRand(x, _seeds.synced); }
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

/// A miracle that records its events
class FakeSpell final: public SpellSink
{
public:
	bool SpellEvent(const SpellEventInfo& event) override
	{
		events.push_back(event);
		return event.type != SpellEventInfo::Type::HitSpell;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	[[nodiscard]] entt::entity Spell() const override { return self; }

	std::vector<SpellEventInfo> events;
	entt::entity self {entt::null};
};

std::string Header(std::string_view initially = "1 0 0")
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\nPROPERTY Hierarchies ARRAY SIZE 3 0 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 3 " +
	       std::string(initially) + "\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n";
}

std::string Object(std::string_view className, std::string_view name, std::string_view properties)
{
	return "BEGINCLASS " + std::string(className) + " " + std::string(name) + "\nBEGINPROPERTIES\n" + std::string(properties) +
	       "ENDPROPERTIES\nENDCLASS\n";
}

constexpr std::string_view k_Point = "PROPERTY ColorA INTEGER 255\n";
constexpr std::string_view k_Sprite = "PROPERTY ColorA INTEGER 200\nPROPERTY InitialScale FLOAT 2\n"
                                      "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet1.raw\n";
constexpr std::string_view k_Chain = "PROPERTY ColorA INTEGER 255\nPROPERTY InitialScale FLOAT 1\n"
                                     "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_Lightning.raw\n";

/// The bytes of one save of effects: the effects, then the tables of what they share
struct Saved
{
	std::vector<uint8_t> effects;
	std::vector<uint8_t> tables;
};

class ParticleSaveTest: public ::testing::Test
{
protected:
	std::unique_ptr<Effect> Make(const std::string& text, glm::vec3 origin = glm::vec3(0.0f))
	{
		auto file = psys::ParticleFile::Parse(text);
		EXPECT_TRUE(file.has_value()) << text;
		auto effect = std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                                       EffectServices {classes, world, random, noise}, origin, 1.0f, false);
		effect->SetPlayer(0);
		return effect;
	}

	static Saved Save(const Effect& effect)
	{
		SharedTables tables;
		reflection::BinaryWriter effectBytes(Context());
		EffectWriter effects(effectBytes, tables);
		effect.Save(effects);
		reflection::BinaryWriter tableBytes(Context());
		EffectWriter tableWriter(tableBytes, tables);
		SaveSharedTables(tableWriter);
		EXPECT_TRUE(effectBytes.Unsaved().empty());
		return {.effects = std::move(effectBytes.Bytes()), .tables = std::move(tableBytes.Bytes())};
	}

	/// Reads a save into an effect just made from the same file; the tables it read
	SharedTables Load(const Saved& saved, Effect& effect)
	{
		SharedTables tables;
		const reflection::EntityRemap identity;
		reflection::BinaryReader tableBytes(Context(), saved.tables, identity);
		EffectReader tableReader(tableBytes, tables);
		LoadSharedTables(tableReader);
		EXPECT_EQ(tableBytes.Remaining(), 0u);
		reflection::BinaryReader effectBytes(Context(), saved.effects, identity);
		EffectReader effects(effectBytes, tables);
		EXPECT_TRUE(effect.Load(effects));
		EXPECT_EQ(effectBytes.Remaining(), 0u);
		return tables;
	}

	/// What an effect draws, as a list of numbers to compare
	static std::vector<float> Drawn(const Effect& effect)
	{
		Effect::DrawWalk walk;
		effect.Walk(1.0f, walk);
		std::vector<float> numbers;
		for (const auto* list : {&walk.atoms, &walk.joints})
		{
			for (const auto& atom : *list)
			{
				numbers.insert(numbers.end(), {atom.position.x, atom.position.y, atom.position.z, atom.scale, atom.alpha});
			}
		}
		numbers.push_back(static_cast<float>(effect.AtomCount()));
		return numbers;
	}

	/// Steps the saved effect and the loaded one from the same random numbers, and checks that they draw the same
	void ExpectSameSteps(Effect& saved, Effect& loaded, int steps)
	{
		const auto seeds = random.GetSeeds();
		const auto stream = random.GetParticleStream();
		std::vector<std::vector<float>> before;
		for (int i = 0; i < steps; ++i)
		{
			saved.Step(k_Step);
			before.push_back(Drawn(saved));
		}
		random.SetSeeds(seeds);
		random.SetParticleStream(stream);
		for (int i = 0; i < steps; ++i)
		{
			loaded.Step(k_Step);
			ASSERT_EQ(Drawn(loaded), before.at(static_cast<size_t>(i))) << "step " << i;
		}
	}

	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	FakeWorld world;
	FakeRandom random;
	maths::ValueNoise noise;
	FakeSpell spell;
};
} // namespace

TEST_F(ParticleSaveTest, AnEffectComesBackWithItsAtomsAndCarriesOnAsBefore)
{
	const auto text = Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                  Object("DiskEmitter", "Emitter0",
	                         "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"
	                         "PROPERTY EmissionFreq FLOAT 10\nPROPERTY MaxAtoms INTEGER 6\n"
	                         "PROPERTY Radius FLOAT 3\nPROPERTY Height FLOAT 1\n") +
	                  Object("RemoveRuleOldAgeOnly", "Remove0", "PROPERTY Group INTEGER 0\nPROPERTY DieAge FLOAT 0.45\n");
	auto saved = Make(text, {10.0f, 0.0f, 20.0f});
	for (int i = 0; i < 7; ++i)
	{
		saved->Step(k_Step);
	}
	const auto bytes = Save(*saved);
	auto loaded = Make(text);
	Load(bytes, *loaded);
	EXPECT_EQ(loaded->AtomCount(), saved->AtomCount());
	EXPECT_EQ(loaded->GetAge(), saved->GetAge());
	EXPECT_EQ(loaded->GetOrigin(), saved->GetOrigin());
	EXPECT_EQ(Drawn(*loaded), Drawn(*saved));
	// A second save of what was read gives the same bytes
	const auto again = Save(*loaded);
	EXPECT_EQ(again.effects, bytes.effects);
	EXPECT_EQ(again.tables, bytes.tables);
	ExpectSameSteps(*saved, *loaded, 10);
}

TEST_F(ParticleSaveTest, AFireballInFlightComesBackWithItsTrailAndFliesOn)
{
	const auto text =
	    Header("1 0 0") + Object("ParticleSpriteCreator", "Ball", k_Point) +
	    Object("CreateWithInitialDirection", "Throw",
	           "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Ball\nPROPERTY NextGroups ARRAY SIZE 1 1\n") +
	    Object("UpdateRuleGravityWithFloor", "Flight",
	           "PROPERTY Group INTEGER 0\nPROPERTY Gravity FLOAT 30\nPROPERTY UseWind BOOL 0\n"
	           "PROPERTY DampingVerticalBounce FLOAT 0.5\n") +
	    Object("UR_Trail", "Trail",
	           "PROPERTY Group INTEGER 1\nPROPERTY PCreator PERSIS_PNTR Ball\nPROPERTY NumAtoms INTEGER 4\n") +
	    Object("EventAlways", "Event", "PROPERTY Group INTEGER 0\n");
	auto saved = Make(text);
	saved->SetSink(&spell);
	saved->SetDirection({0.0f, 0.0f, 30.0f});
	saved->SetProcessInfo({.handPosition = {0.0f, 5.0f, 0.0f}});
	for (int i = 0; i < 6; ++i)
	{
		saved->Step(k_Step);
	}
	ASSERT_GT(saved->AtomCount(), 1u);
	const auto bytes = Save(*saved);
	auto loaded = Make(text);
	Load(bytes, *loaded);
	FakeSpell loadedSpell;
	loaded->RestoreSink(&loadedSpell);
	// The miracle is told nothing new when the load puts it back
	EXPECT_TRUE(loadedSpell.events.empty());
	EXPECT_EQ(loaded->GetDirection(), saved->GetDirection());
	EXPECT_EQ(loaded->GetProcessInfo().handPosition, saved->GetProcessInfo().handPosition);
	EXPECT_EQ(Save(*loaded).effects, bytes.effects);
	spell.events.clear();
	ExpectSameSteps(*saved, *loaded, 20);
	// Each says where the ball is on every step, as before the load
	ASSERT_EQ(loadedSpell.events.size(), spell.events.size());
	for (size_t i = 0; i < spell.events.size(); ++i)
	{
		EXPECT_EQ(loadedSpell.events[i].position, spell.events[i].position);
	}
}

TEST_F(ParticleSaveTest, AShieldComesBackAsTheShieldOfTheLoadedEffectWithWhereItWasStruck)
{
	const auto text =
	    Header() +
	    Object("UR_AddDefensiveSphere", "Sphere", "PROPERTY Group INTEGER 0\nPROPERTY SphereRadius PERSIS_PNTR Radius\n") +
	    Object("ConstFloatProvider", "Radius", "PROPERTY ConstValue FLOAT 10\n");
	spell.self = static_cast<entt::entity>(5);
	auto saved = Make(text, {0.0f, 0.0f, 30.0f});
	saved->SetSink(&spell);
	saved->Step(k_Step);
	ASSERT_EQ(world.shields.size(), 1u);
	const auto shield = world.shields.front().lock();
	shield->impacts.emplace_back(0.0f, 0.0f, 20.0f);
	const auto bytes = Save(*saved);
	auto loaded = Make(text);
	const auto tables = Load(bytes, *loaded);
	ASSERT_EQ(tables.shields.items.size(), 1u);
	const auto& back = *tables.shields.items.front();
	EXPECT_EQ(back.owner, loaded.get());
	EXPECT_EQ(back.centre, shield->centre);
	EXPECT_EQ(back.radius, 10.0f);
	EXPECT_EQ(back.spell, spell.self);
	EXPECT_EQ(back.impacts, shield->impacts);
	// The effect holds the same shield as the table: a second save gives the same bytes
	const auto again = Save(*loaded);
	EXPECT_EQ(again.effects, bytes.effects);
	EXPECT_EQ(again.tables, bytes.tables);
}

TEST_F(ParticleSaveTest, ABoltComesBackWithItsTargetsAndStrikesOnAsBefore)
{
	world.candidates = {{.object = static_cast<entt::entity>(1), .position = {0.0f, 0.0f, 20.0f}, .height = 10.0f}};
	const auto text = Header("1 0 0") + Object("ParticleChainCreator", "Joint", k_Chain) +
	                  Object("UR_Lightning", "Bolt",
	                         "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Joint\nPROPERTY ForkGroup INTEGER 1\n"
	                         "PROPERTY MinLightningObjects INTEGER 1\nPROPERTY MaxJointsPerFork INTEGER 6\n"
	                         "PROPERTY DefaultSearchRadius FLOAT 50\nPROPERTY SplitAngle FLOAT 0.785\n");
	const particles::ProcessInfo info {
	    .handPosition = {0.0f, 15.0f, 0.0f}, .cameraForward = {0.0f, 0.0f, 1.0f}, .enabled = true};
	auto saved = Make(text);
	saved->SetSink(&spell);
	saved->SetProcessInfo(info);
	for (int i = 0; i < 5; ++i)
	{
		saved->Step(k_Step);
	}
	ASSERT_GT(saved->AtomCount(), 0u);
	const auto bytes = Save(*saved);
	auto loaded = Make(text);
	Load(bytes, *loaded);
	FakeSpell loadedSpell;
	loaded->RestoreSink(&loadedSpell);
	EXPECT_EQ(Drawn(*loaded), Drawn(*saved));
	EXPECT_EQ(Save(*loaded).effects, bytes.effects);
	ExpectSameSteps(*saved, *loaded, 15);
}

TEST_F(ParticleSaveTest, AGatheringOfCloudsComesBackAndGathersOnAsBefore)
{
	const auto text = Header() + Object("ParticleSpriteCreator", "Cloud", k_Sprite) +
	                  Object("UR_CloudGather", "Gather",
	                         "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Cloud\nPROPERTY NumAtoms INTEGER 6\n"
	                         "PROPERTY TimeToForm FLOAT 2\nPROPERTY RadiusFloatProvider PERSIS_PNTR Size\n"
	                         "PROPERTY ScaleFloatProvider PERSIS_PNTR Size\nPROPERTY CloudHeight PERSIS_PNTR Size\n") +
	                  Object("ConstFloatProvider", "Size", "PROPERTY ConstValue FLOAT 30\n");
	auto saved = Make(text, {50.0f, 0.0f, 50.0f});
	saved->SetSink(&spell);
	saved->SetProcessInfo({.cameraForward = {0.0f, 0.0f, 1.0f}, .power = 1.0f});
	for (int i = 0; i < 8; ++i)
	{
		saved->Step(k_Step);
	}
	ASSERT_GT(saved->AtomCount(), 0u);
	const auto bytes = Save(*saved);
	auto loaded = Make(text);
	Load(bytes, *loaded);
	loaded->RestoreSink(&spell);
	EXPECT_EQ(Save(*loaded).effects, bytes.effects);
	ExpectSameSteps(*saved, *loaded, 15);
}

TEST_F(ParticleSaveTest, ARuleStateOfAnotherFileIsSkippedAndTheLoadFailsCleanly)
{
	const auto text =
	    Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	    Object("DiskEmitter", "Emitter0",
	           "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\nPROPERTY EmissionFreq FLOAT 10\n");
	auto saved = Make(text);
	saved->Step(k_Step);
	const auto bytes = Save(*saved);
	// A file with no rules cannot take the saved rules' state
	auto other = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite));
	SharedTables tables;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(Context(), bytes.effects, identity);
	EffectReader reader(in, tables);
	EXPECT_FALSE(other->Load(reader));
}
