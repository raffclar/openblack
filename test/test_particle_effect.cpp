/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <ParticleFile.h>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/Zip.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleEffect.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
constexpr float k_Step = 0.1f;
constexpr float k_Epsilon = 1e-4f;

/// Flat land at a height, red players and a camera looking north
class FakeWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return landHeight; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF0000u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	void StartSound(const Effect& /*effect*/, const Atom& /*atom*/, const ParticleSound& sound) override
	{
		sounds.push_back(sound.action.sound);
	}

	float landHeight {0.0f};
	std::vector<std::string> sounds;
};

/// The game's generator on seeds of its own
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

/// Records the events a miracle is sent
class FakeSpell final: public SpellSink
{
public:
	bool SpellEvent(const SpellEventInfo& event) override
	{
		events.push_back(event);
		return true;
	}
	[[nodiscard]] int PowerUpLevel() const override { return 0; }

	std::vector<SpellEventInfo> events;
};

std::string Header(std::string_view hierarchies = "0 0", float maxAge = -1.0f, bool deleteOnCloseDown = false)
{
	return "BEGINPROPERTIES\n"
	       "PROPERTY DeleteOnCloseDown BOOL " +
	       std::string(deleteOnCloseDown ? "1" : "0") + "\nPROPERTY Hierarchies ARRAY SIZE 2 " + std::string(hierarchies) +
	       "\nPROPERTY InitiallyCreated ARRAY SIZE 2 1 0\nPROPERTY MaxSpellAge FLOAT " + std::to_string(maxAge) +
	       "\nENDPROPERTIES\n";
}

std::string Object(std::string_view className, std::string_view name, std::string_view properties)
{
	return "BEGINCLASS " + std::string(className) + " " + std::string(name) + "\nBEGINPROPERTIES\n" + std::string(properties) +
	       "ENDPROPERTIES\nENDCLASS\n";
}

constexpr std::string_view k_Sprite = "PROPERTY ColorA INTEGER 200\nPROPERTY InitialScale FLOAT 2\n"
                                      "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet1.raw\n";

class ParticleEffectTest: public ::testing::Test
{
protected:
	std::unique_ptr<Effect> Make(const std::string& text, glm::vec3 origin = glm::vec3(0.0f), float magnitude = 1.0f)
	{
		auto file = psys::ParticleFile::Parse(text);
		EXPECT_TRUE(file.has_value()) << text;
		return std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                                EffectServices {classes, world, random, noise}, origin, magnitude, false);
	}

	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	FakeWorld world;
	FakeRandom random;
	maths::ValueNoise noise;
};
} // namespace

TEST_F(ParticleEffectTest, AnEmitterFillsItsCollectionUpToItsLimit)
{
	auto effect = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                       Object("DiskEmitter", "Emitter0",
	                              "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"
	                              "PROPERTY EmissionFreq FLOAT 10\nPROPERTY Randomise BOOL 0\nPROPERTY MaxAtoms INTEGER 4\n"
	                              "PROPERTY Radius FLOAT 3\nPROPERTY Height FLOAT 1\n"),
	                   {100.0f, 0.0f, 200.0f});
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 1u);
	for (int i = 0; i < 20; ++i)
	{
		effect->Step(k_Step);
	}
	// One a step while no more than four are alive
	EXPECT_EQ(effect->AtomCount(), 5u);
	std::vector<Effect::DrawAtom> atoms;
	effect->Collect(1.0f, atoms);
	ASSERT_EQ(atoms.size(), 5u);
	for (const auto& atom : atoms)
	{
		// On the disk round the origin, a unit up, with the creator's look
		EXPECT_LE(glm::length(glm::vec2(atom.position.x - 100.0f, atom.position.z - 200.0f)), 3.0f + k_Epsilon);
		EXPECT_NEAR(atom.position.y, 1.0f, k_Epsilon);
		EXPECT_NEAR(atom.scale, 2.0f, k_Epsilon);
		EXPECT_NEAR(atom.alpha, 200.0f, k_Epsilon);
	}
	EXPECT_FALSE(effect->Finished());
}

TEST_F(ParticleEffectTest, AtomsDieOfOldAgeAndTheEffectEndsAfterClosingDown)
{
	auto effect = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                   Object("EmitterRuleSimple", "Emitter0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"
	                          "PROPERTY EmissionFreq FLOAT 10\nPROPERTY Randomise BOOL 0\nPROPERTY Speed FLOAT 0\n"
	                          "PROPERTY RemoveOnCloseDown BOOL 1\n") +
	                   Object("RemoveRuleOldAgeOnly", "Remove0", "PROPERTY Group INTEGER 0\nPROPERTY DieAge FLOAT 0.45\n"));
	for (int i = 0; i < 20; ++i)
	{
		effect->Step(k_Step);
	}
	// Each lives five steps
	EXPECT_EQ(effect->AtomCount(), 5u);
	effect->CloseDown();
	EXPECT_TRUE(effect->Closing());
	EXPECT_FALSE(effect->DeleteOnCloseDown());
	for (int i = 0; i < 4; ++i)
	{
		effect->Step(k_Step);
		EXPECT_FALSE(effect->Finished());
	}
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_TRUE(effect->Finished());
}

TEST_F(ParticleEffectTest, ItEndsAtItsFilesAge)
{
	auto effect =
	    Make(Header("0 0", 0.25f) + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	         Object("CreateRuleAnAtom", "Create0", "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"));
	effect->Step(k_Step);
	EXPECT_FALSE(effect->Finished());
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_TRUE(effect->Finished());
}

TEST_F(ParticleEffectTest, RulesFadeScaleAndMoveAtoms)
{
	auto effect = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                   Object("CreateRuleAnAtom", "Create0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\nPROPERTY OffsetY FLOAT 10\n") +
	                   Object("AR_FadeAlpha", "Fade0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY StartTime FLOAT 0\nPROPERTY StopTime FLOAT 1\n"
	                          "PROPERTY StartAlpha INTEGER 200\nPROPERTY StopAlpha INTEGER 0\n") +
	                   Object("UR_ChangeScale", "Scale0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY StartTime FLOAT 0\nPROPERTY StopTime FLOAT 1\n"
	                          "PROPERTY StartScale FLOAT 1\nPROPERTY StopScale FLOAT 3\n") +
	                   Object("UpdateRuleGravity", "Gravity0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY Gravity FLOAT 10\nPROPERTY MaxSpeed FLOAT 100\n"));
	effect->Step(k_Step); // made
	for (int i = 0; i < 5; ++i)
	{
		effect->Step(k_Step);
	}
	std::vector<Effect::DrawAtom> atoms;
	effect->Collect(1.0f, atoms);
	ASSERT_EQ(atoms.size(), 1u);
	// Half a second old by the last step's rules: half faded, scaled half way, fallen
	EXPECT_NEAR(atoms[0].alpha, 100.0f, 1.0f);
	EXPECT_NEAR(atoms[0].scale, 2.0f * 2.0f, 1e-3f);
	EXPECT_LT(atoms[0].position.y, 10.0f);
	// Drawn half way between the last two steps
	std::vector<Effect::DrawAtom> between;
	effect->Collect(0.5f, between);
	ASSERT_EQ(between.size(), 1u);
	EXPECT_GT(between[0].position.y, atoms[0].position.y);
}

TEST_F(ParticleEffectTest, HierarchiesCarryTheirChildren)
{
	// Group 0's atom is a hierarchy: group 1's atoms live in its frame, two up and scaled by its scale of 3
	auto effect = Make(
	    Header("1 0") + Object("ParticlePointCreator", "Point0", "PROPERTY InitialScale FLOAT 3\n") +
	        Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	        Object("CreateRuleAnAtom", "Create0",
	               "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Point0\nPROPERTY NextGroups ARRAY SIZE 1 1\n") +
	        Object("CreateRuleAnAtom", "Create1",
	               "PROPERTY Group INTEGER 1\nPROPERTY PCreator PERSIS_PNTR Sprite0\nPROPERTY OffsetY FLOAT 2\n"),
	    {50.0f, 0.0f, 0.0f});
	effect->Step(k_Step);
	effect->Step(k_Step);
	std::vector<Effect::DrawAtom> atoms;
	effect->Collect(1.0f, atoms);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_NEAR(atoms[0].position.x, 50.0f, k_Epsilon);
	EXPECT_NEAR(atoms[0].position.y, 6.0f, k_Epsilon);
	// Drawn at its scale times its parent's
	EXPECT_NEAR(atoms[0].scale, 6.0f, k_Epsilon);
	EXPECT_EQ(effect->CollectionCount(), 2u);
}

TEST_F(ParticleEffectTest, FloatProvidersFollowTheMagnitude)
{
	auto effect = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                       Object("MagnitudeFloatProvider", "Magnitude0",
	                              "PROPERTY ScaleBy FLOAT 0.5\nPROPERTY Minimum FLOAT 0\nPROPERTY Maximum FLOAT 10\n") +
	                       Object("CreateRuleAnAtom", "Create0",
	                              "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"
	                              "PROPERTY InitScaleFP PERSIS_PNTR Magnitude0\n"),
	                   glm::vec3(0.0f), 4.0f);
	effect->Step(k_Step);
	effect->Step(k_Step);
	std::vector<Effect::DrawAtom> atoms;
	effect->Collect(1.0f, atoms);
	ASSERT_EQ(atoms.size(), 1u);
	// The creator's 2 times 4 * 0.5
	EXPECT_NEAR(atoms[0].scale, 4.0f, k_Epsilon);
}

TEST_F(ParticleEffectTest, ConditionsGateRules)
{
	// The emitter only runs once the effect closes down
	auto effect = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                   Object("EventConditionTrueOnCloseDown", "OnClose", "") +
	                   Object("EmitterRuleSimple", "Emitter0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"
	                          "PROPERTY Condition PERSIS_PNTR OnClose\nPROPERTY EmissionFreq FLOAT 10\n"
	                          "PROPERTY Randomise BOOL 0\n"));
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
	effect->CloseDown();
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 1u);
}

TEST_F(ParticleEffectTest, LandingTellsTheMiracle)
{
	world.landHeight = 5.0f;
	auto effect = Make(Header() + Object("ParticleSpriteCreator", "Sprite0", k_Sprite) +
	                   Object("CreateRuleAnAtom", "Create0",
	                          "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\nPROPERTY OffsetY FLOAT 8\n") +
	                   Object("UpdateRuleGravity", "Gravity0", "PROPERTY Group INTEGER 0\nPROPERTY Gravity FLOAT 50\n") +
	                   Object("LandscapeCollide", "Land0", "PROPERTY Group INTEGER 0\nPROPERTY SendEvent BOOL 1\n"));
	FakeSpell spell;
	effect->SetSink(&spell);
	ASSERT_EQ(spell.events.size(), 1u);
	EXPECT_EQ(spell.events[0].type, SpellEventInfo::Type::Started);
	for (int i = 0; i < 30 && effect->AtomCount() <= 1; ++i)
	{
		effect->Step(k_Step);
		if (spell.events.size() > 1)
		{
			break;
		}
	}
	ASSERT_EQ(spell.events.size(), 2u);
	EXPECT_EQ(spell.events[1].type, SpellEventInfo::Type::Landed);
	EXPECT_LT(spell.events[1].position.y, 5.0f);
	EXPECT_EQ(effect->AtomCount(), 0u);
}

TEST_F(ParticleEffectTest, UnportedClassesAreNamedAndKeepAMiraclesEffectAlive)
{
	auto effect = Make(Header() + Object("UR_SomethingNew", "New0", "PROPERTY Group INTEGER 0\n"));
	ASSERT_EQ(effect->UnportedClasses().size(), 1u);
	EXPECT_EQ(effect->UnportedClasses()[0], "UR_SomethingNew");
	effect->Step(k_Step);
	// Without a miracle nothing keeps it
	EXPECT_TRUE(effect->Finished());
	FakeSpell spell;
	effect->SetSink(&spell);
	EXPECT_FALSE(effect->Finished());
	effect->CloseDown();
	EXPECT_TRUE(effect->Finished());
}

TEST_F(ParticleEffectTest, SpritesTakeThePlayersColour)
{
	auto effect =
	    Make(Header() + Object("ParticleSpriteCreator", "Sprite0", std::string(k_Sprite) + "PROPERTY UsePlayerColor BOOL 1\n") +
	         Object("CreateRuleAnAtom", "Create0",
	                "PROPERTY Group INTEGER 0\nPROPERTY PCreator PERSIS_PNTR Sprite0\n"
	                "PROPERTY SoundOfCreate SOUND_ACTION SOUND_POP LOOPING 0 ONLYONE 0 SOFTRELEASE 0 "
	                "USESURFACE 0\n"));
	effect->SetPlayer(0);
	effect->Step(k_Step);
	effect->Step(k_Step);
	std::vector<Effect::DrawAtom> atoms;
	effect->Collect(1.0f, atoms);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_EQ(atoms[0].rgb, (std::array<uint8_t, 3> {254, 0, 0}));
	// And its creation's sound was asked for
	EXPECT_EQ(world.sounds, (std::vector<std::string> {"SOUND_POP"}));
}

TEST_F(ParticleEffectTest, RandomNumbersAreDrawnOnlyInASteps)
{
	auto effect = Make(Header());
	EXPECT_FLOAT_EQ(effect->Random(10.0f), 0.0f);
	EXPECT_EQ(random.GetParticleStream(), ParticleRandomStream::None);
}

TEST_F(ParticleEffectTest, RunsEveryFileOfTheGame)
{
	const char* gamePath = std::getenv("OPENBLACK_GAME_PATH");
	if (gamePath == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH is not set";
	}
	const auto directory = std::filesystem::path(gamePath) / "Data" / "Spells" / "ZSpellFiles";
	if (!std::filesystem::is_directory(directory))
	{
		GTEST_SKIP() << "No particle files in the game folder";
	}
	size_t count = 0;
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		const auto name = entry.path().filename().string();
		if (!name.ends_with("_txt.zzz"))
		{
			continue;
		}
		std::ifstream stream(entry.path(), std::ios::binary);
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		const auto compressed = psys::SplitCompressed(bytes);
		ASSERT_TRUE(compressed.has_value()) << name;
		const auto text =
		    zip::Inflate(std::vector<uint8_t>(compressed->deflated.begin(), compressed->deflated.end()), compressed->textSize);
		auto effect = Make(std::string(text.begin(), text.end()), glm::vec3(0.0f), 2.0f);
		// Ten seconds of turns, then closing down for another ten
		size_t most = 0;
		size_t sprites = 0;
		for (int step = 0; step < 200; ++step)
		{
			if (step == 100)
			{
				effect->CloseDown();
			}
			effect->Step(k_Step);
			most = std::max(most, effect->AtomCount());
			std::vector<Effect::DrawAtom> atoms;
			effect->Collect(1.0f, atoms);
			sprites = std::max(sprites, atoms.size());
			if (effect->Finished())
			{
				break;
			}
		}
		// The spot visuals' smoke, steam and bonfire run entirely on the classes ported so far
		if (name == "SF_Smoke_txt.zzz" || name == "SF_Steam_txt.zzz" || name == "SF_Bonfire_txt.zzz")
		{
			EXPECT_TRUE(effect->UnportedClasses().empty()) << name;
			EXPECT_GT(sprites, 10u) << name;
		}
		++count;
	}
	EXPECT_GT(count, 100u);
}
