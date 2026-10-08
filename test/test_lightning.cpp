/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning bolt's PSys classes: UR_Lightning / UR_LightningStrike (Particles/Rules/Lightning.cpp), the chain
// ribbon's UV layout (Particles/Creators/Chain.cpp) and the light map creator (Particles/Creators/LightMap.cpp).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "Common/Zip.h"
#include "GameClock.h"
#include "Locator.h"
#include "Particles/Creators/Chain.h"
#include "Particles/Creators/LightMap.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/SpellLink.h"
#include "support/WorldSystems.h"

using namespace openblack;

namespace
{
/// A bolt cast from the hand with one target (a ground point: no map here) and five joints per fork; group 0 holds the
/// rule, the forks are group 1
constexpr const char* k_Bolt = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS UR_Lightning UR_Lightning0
BEGINPROPERTIES
PROPERTY CastingFromHand BOOL 1
PROPERTY CommonGlowGroup INTEGER 2
PROPERTY ForkGroup INTEGER 1
PROPERTY ForkScale FLOAT 2
PROPERTY Group INTEGER 0
PROPERTY LightMapGroup INTEGER -1
PROPERTY MaxJointsPerFork INTEGER 5
PROPERTY MaxLightningObjects INTEGER 1
PROPERTY MaxLightningObjectsAtOnce INTEGER 1
PROPERTY MinLightningObjects INTEGER 1
PROPERTY DefaultSearchRadius FLOAT 20
PROPERTY RandomFrac FLOAT 0.05
PROPERTY SplitAngle FLOAT 0.2
PROPERTY RenewSearchEvery FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticleChainCreator0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleChainCreator ParticleChainCreator0
BEGINPROPERTIES
PROPERTY NumTexturesForWholeChain INTEGER 4
PROPERTY TextureFileName STRING .\Data\Textures\S_Lightning.raw
PROPERTY UseAdditiveAlpha BOOL 1
ENDPROPERTIES
ENDCLASS
)";

/// Keeps the events the rules send and answers 1
class RecordingSink final: public psys::SpellSink
{
public:
	int SpellEvent(const psys::SpellEventInfo& event) override
	{
		events.push_back(event);
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	std::vector<psys::SpellEventInfo> events;
};

/// Every cell at one altitude (height units), for the fork's cut against the land (LandIslandInterface::RayCast)
class LevelIsland final: public LandIslandInterface
{
public:
	explicit LevelIsland(uint8_t altitude) { _cell.altitude = altitude; }
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { return _cell; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	lnd::LNDCell _cell {};
	std::vector<lnd::LNDCountry> _countries;
};

std::shared_ptr<const psys::File> ParseBolt()
{
	auto file = psys::File::Parse(k_Bolt, "SF_LightningBoltTest");
	return file.has_value() ? std::make_shared<const psys::File>(*file) : nullptr;
}

/// One step of a bolt cast from `hand` towards +x
void StepBolt(psys::Effect& effect, const glm::vec3& hand)
{
	psys::ProcessInfo info;
	info.handPos = hand;
	info.cameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
	info.enabled = true;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
}

/// The class of that name in the file, built through the registry (nullptr when nobody registered it)
std::unique_ptr<psys::Creator> MakeCreator(const psys::File& file, const std::string& name)
{
	const auto* object = file.Find(name);
	if (object == nullptr)
	{
		return nullptr;
	}
	const auto factory = psys::FindCreatorFactory(object->className);
	return factory != nullptr ? factory(*object) : nullptr;
}

const psys::Object* Find(const psys::File& file, const std::string& className)
{
	for (const auto& object : file.objects)
	{
		if (object.className == className)
		{
			return &object;
		}
	}
	return nullptr;
}

/// Data\Spells\ZSpellFiles\<name>_txt.zzz: the length then a deflate stream (PSysFile.cpp does the same)
std::optional<psys::File> LoadSpellFile(const std::filesystem::path& root, const std::string& name)
{
	std::ifstream stream(root / "Data" / "Spells" / "ZSpellFiles" / (name + "_txt.zzz"), std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (bytes.size() <= 4)
	{
		return std::nullopt;
	}
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), sizeof(size));
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	return psys::File::Parse(std::string(inflated.begin(), inflated.end()), name);
}

/// The bolts' tests set the turn: it goes back to 0 after each one, also when an ASSERT stops it
class Lightning: public ::testing::Test
{
protected:
	void TearDown() override { game_clock::SetTurn(0); }
};
} // namespace

TEST_F(Lightning, classesAreRegistered)
{
	// the registry is filled on the first lookup (PSysRegistry.cpp)
	EXPECT_NE(psys::FindModifierFactory("UR_Lightning"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_LightningStrike"), nullptr);
	EXPECT_NE(psys::FindCreatorFactory("ParticleChainCreator"), nullptr);
	EXPECT_NE(psys::FindCreatorFactory("ParticleLightMapCreator"), nullptr);
}

TEST_F(Lightning, chainCreatorProperties)
{
	const auto file = psys::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n"
	                                    "BEGINCLASS ParticleChainCreator Chain0\nBEGINPROPERTIES\n"
	                                    "PROPERTY NumTexturesForWholeChain INTEGER 4\n"
	                                    "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_Lightning.raw\n"
	                                    "PROPERTY UseAdditiveAlpha BOOL 1\n"
	                                    "PROPERTY MaterialUpdateZBuffer BOOL 0\n"
	                                    "PROPERTY InitialScale FLOAT 1\n"
	                                    "ENDPROPERTIES\nENDCLASS\n",
	                                    "test");
	ASSERT_TRUE(file.has_value());
	const auto creator = MakeCreator(*file, "Chain0");
	ASSERT_NE(creator, nullptr);
	const auto* chain = dynamic_cast<const psys::ChainCreator*>(creator.get());
	ASSERT_NE(chain, nullptr);
	// a chain is its own draw kind: the collection becomes one ribbon, not one sprite per joint
	EXPECT_EQ(chain->kind, psys::Creator::Kind::Chain);
	EXPECT_EQ(chain->numTexturesForWholeChain, 4);
	EXPECT_TRUE(chain->additive);
	EXPECT_FALSE(chain->writeDepth);
	// without the game's Data\Textures the name keeps the spell file's spelling
	EXPECT_TRUE(chain->texture == "S_Lightning" || chain->texture == "S_lightning");
}

TEST_F(Lightning, chainSegmentUv)
{
	// SegmentUv with the creator's default 32 x 64 frames: V runs along the chain, U over one frame column
	psys::ChainCreator chain;
	EXPECT_EQ(chain.frameHeight, 64);
	EXPECT_EQ(chain.frameWidth, 32);
	EXPECT_EQ(chain.numTexturesForWholeChain, -1);
	chain.numTexturesForWholeChain = 4;
	// 4 textures over 8 segments: two segments each, the first stretch with FrameOfTail, the last with FrameOfHead
	const auto first = chain.SegmentUv(0, 8, 0.0f);
	EXPECT_FLOAT_EQ(first[0].x, 0.0f);
	EXPECT_FLOAT_EQ(first[0].y, 0.0f);
	EXPECT_FLOAT_EQ(first[1].x, 0.125f);
	EXPECT_FLOAT_EQ(first[2].y, 0.125f);
	chain.frameOfHead = 2;
	chain.fileOffset = 5;
	const auto last = chain.SegmentUv(7, 8, 0.0f);
	EXPECT_FLOAT_EQ(last[0].x, 0.875f); // (5 + 2) x 32 / 256
	EXPECT_FLOAT_EQ(last[1].x, 1.0f);
	EXPECT_FLOAT_EQ(last[0].y, 0.125f); // the second of its two segments
	EXPECT_FLOAT_EQ(last[3].y, 0.25f);
	// -1: one texture per segment, the inner ones with frame 0 (+ FileOffset); the scroll on every v
	chain.numTexturesForWholeChain = -1;
	const auto inner = chain.SegmentUv(3, 8, 0.1f);
	EXPECT_FLOAT_EQ(inner[0].x, 0.625f);
	EXPECT_FLOAT_EQ(inner[0].y, 0.1f);
	EXPECT_FLOAT_EQ(inner[2].y, 0.25f + 0.1f);
}

// ChainCreator::SegmentUv with the creator's defaults (64 high, 32 wide) and
// SF_LightningBolt's 4 repeats on a 10 joint fork: U across the ribbon is frame 0 (texels 0..32), V along it is 64
// texels per repeat. uv[0] = (u0, v0), uv[1] = (u1, v0), uv[2] = (u0, v1), uv[3] = (u1, v1)
TEST_F(Lightning, chainSegmentUvRepeats)
{
	psys::ChainCreator chain;
	chain.numTexturesForWholeChain = 4;
	auto uv = chain.SegmentUv(0, 9, 0.0f); // repeat 0 holds segments 0..1
	EXPECT_FLOAT_EQ(uv[0].x, 0.0f);
	EXPECT_FLOAT_EQ(uv[1].x, 0.125f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.0f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.125f);
	uv = chain.SegmentUv(1, 9, 0.0f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
	uv = chain.SegmentUv(2, 9, 0.0f); // repeat 1 starts again at the frame's top
	EXPECT_FLOAT_EQ(uv[0].y, 0.0f);
	uv = chain.SegmentUv(8, 9, 0.0f); // repeat 3 holds segments 6..8
	EXPECT_FLOAT_EQ(uv[0].y, 64.0f * 2.0f / 3.0f / 256.0f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
	// SF_GestureChain: FrameOfTail 1 in the first repeat, FrameOfHead 2 in the last, 0 between, all + FileOffset 5
	chain.numTexturesForWholeChain = 5;
	chain.frameOfHead = 2;
	chain.frameOfTail = 1;
	chain.fileOffset = 5;
	EXPECT_FLOAT_EQ(chain.SegmentUv(0, 9, 0.0f)[0].x, 6.0f * 32.0f / 256.0f);
	EXPECT_FLOAT_EQ(chain.SegmentUv(4, 9, 0.0f)[0].x, 5.0f * 32.0f / 256.0f);
	EXPECT_FLOAT_EQ(chain.SegmentUv(8, 9, 0.0f)[0].x, 7.0f * 32.0f / 256.0f);
	EXPECT_FLOAT_EQ(chain.SegmentUv(8, 9, 0.0f)[1].x, 1.0f);
	// -1: one repeat per segment
	chain.numTexturesForWholeChain = -1;
	uv = chain.SegmentUv(3, 8, 0.0f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.0f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
}

TEST_F(Lightning, lightMapCreatorProperties)
{
	const auto file = psys::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n"
	                                    "BEGINCLASS ParticleLightMapCreator LM0\nBEGINPROPERTIES\n"
	                                    "PROPERTY Pitch INTEGER 5\n"
	                                    "PROPERTY NumFramesInFile INTEGER 16\n"
	                                    "PROPERTY NumFramesInUse INTEGER 16\n"
	                                    "PROPERTY FrameRate FLOAT 26\n"
	                                    "PROPERTY PlayAnim BOOL 1\n"
	                                    "PROPERTY LoopAnim BOOL 0\n"
	                                    "PROPERTY RandJitter FLOAT 0\n"
	                                    "PROPERTY UseRandJitter BOOL 1\n"
	                                    "PROPERTY ShiftX FLOAT 9.97788\n"
	                                    "PROPERTY TextureFileName STRING .\\Data\\SPELLS\\LightMaps\\S_lm.raw\n"
	                                    "ENDPROPERTIES\nENDCLASS\n",
	                                    "test");
	ASSERT_TRUE(file.has_value());
	const auto creator = MakeCreator(*file, "LM0");
	ASSERT_NE(creator, nullptr);
	const auto* lightMap = dynamic_cast<const psys::LightMapCreator*>(creator.get());
	ASSERT_NE(lightMap, nullptr);
	EXPECT_EQ(lightMap->pitch, 5);
	EXPECT_EQ(lightMap->numFramesInFile, 16);
	EXPECT_EQ(lightMap->numFramesInUse, 16);
	EXPECT_TRUE(lightMap->useRandJitter);
	EXPECT_FLOAT_EQ(lightMap->shiftX, 9.97788f);
	// not a sprite: stamped into the land's cells (light_map_atoms::SubmitFrame -> land_light::AddStamp)
	EXPECT_EQ(lightMap->kind, psys::Creator::Kind::Other);
	EXPECT_EQ(lightMap->numFrames, 16);
	EXPECT_EQ(lightMap->bitmap, nullptr); // no such file here
	EXPECT_TRUE(lightMap->playAnim);
	EXPECT_FALSE(lightMap->loopAnim);
	EXPECT_FLOAT_EQ(lightMap->frameRate, 26.0f);
}

/// With OPENBLACK_GAME_PATH set to the install: the real bolt files and every class they use
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(Lightning, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::filesystem::path root(game);
	const auto bolt = LoadSpellFile(root, "SF_LightningBolt");
	ASSERT_TRUE(bolt.has_value());
	// SF_LightningBolt: groups 4 (light maps) and 5 (root sprite) exist from the start, the forks are group 1
	const auto created = bolt->header.Array("InitiallyCreated");
	ASSERT_EQ(created.size(), 25u);
	EXPECT_EQ(created[4], 1);
	EXPECT_EQ(created[5], 1);
	EXPECT_EQ(created[0], 0);
	const auto* rule = Find(*bolt, "UR_Lightning");
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(rule->Int("ForkGroup", -1), 1);
	EXPECT_EQ(rule->Int("LightMapGroup", -1), 4);
	EXPECT_EQ(rule->Int("CommonGlowGroup", -1), 6);
	EXPECT_EQ(rule->Int("MaxLightningObjects", 0), 6);
	EXPECT_EQ(rule->Int("MinLightningObjects", 0), 3);
	EXPECT_EQ(rule->Int("MaxLightningObjectsAtOnce", 0), 4);
	EXPECT_EQ(rule->Int("MaxJointsPerFork", 0), 10);
	EXPECT_FLOAT_EQ(rule->Float("SplitAngle", 0.0f), 0.2f);
	EXPECT_FLOAT_EQ(rule->Float("ForkScale", 0.0f), 2.0f);
	EXPECT_FLOAT_EQ(rule->Float("AverageLightmapLife", 0.0f), 1.5f);
	EXPECT_TRUE(rule->Bool("CastingFromHand", false));
	// every class of the file is either registered or deliberately unsupported: none may be missing a factory for the
	// bolt's own classes
	for (const auto& name : {"UR_Lightning", "ParticleChainCreator", "ParticleLightMapCreator"})
	{
		const auto* object = Find(*bolt, name);
		ASSERT_NE(object, nullptr) << name;
	}
	// the power-ups only change the fork counts and scale
	const auto two = LoadSpellFile(root, "SF_LightningBoltPUTwo");
	ASSERT_TRUE(two.has_value());
	const auto* ruleTwo = Find(*two, "UR_Lightning");
	ASSERT_NE(ruleTwo, nullptr);
	EXPECT_EQ(ruleTwo->Int("MinLightningObjects", 0), 15);
	EXPECT_EQ(ruleTwo->Int("MaxLightningObjects", 0), 28);
	EXPECT_EQ(ruleTwo->Int("MaxLightningObjectsAtOnce", 0), 20);
	EXPECT_FLOAT_EQ(ruleTwo->Float("ForkScale", 0.0f), 8.0f);
	// the script / climate strike: UR_LightningStrike makes the atom whose group holds the UR_Lightning
	const auto strike = LoadSpellFile(root, "SF_LightningStrike");
	ASSERT_TRUE(strike.has_value());
	const auto* strikeRule = Find(*strike, "UR_LightningStrike");
	ASSERT_NE(strikeRule, nullptr);
	EXPECT_EQ(strikeRule->Array("NextGroups"), std::vector<int>({1}));
	const auto* strikeLightning = Find(*strike, "UR_Lightning");
	ASSERT_NE(strikeLightning, nullptr);
	EXPECT_FALSE(strikeLightning->Bool("CastingFromHand", true));
	EXPECT_EQ(strikeLightning->Int("ForkGroup", -1), 2);
	// the light map of the bolt: 16 frames of 5 x 5 RGB stacked (1200 bytes)
	const auto* lightMap = Find(*bolt, "ParticleLightMapCreator");
	ASSERT_NE(lightMap, nullptr);
	EXPECT_EQ(lightMap->Int("Pitch", 0), 5);
	EXPECT_EQ(lightMap->Int("NumFramesInFile", 0), 16);
	std::ifstream raw(root / "Data" / "Spells" / "LightMaps" / "S_lightning_lightmap_with_border.raw", std::ios::binary);
	ASSERT_TRUE(raw.is_open());
	const std::vector<uint8_t> pixels((std::istreambuf_iterator<char>(raw)), std::istreambuf_iterator<char>());
	EXPECT_EQ(pixels.size(), 5u * 5u * 16u * 3u);
}

/// The same reads as realData on the test bolt above and a small strike file: the header's group array, the rule's
/// properties by class, the fallbacks of absent ones and the strike's NextGroups
TEST_F(Lightning, realDataSynthetic)
{
	const auto bolt = ParseBolt();
	ASSERT_NE(bolt, nullptr);
	// the test bolt creates group 0 only
	const auto created = bolt->header.Array("InitiallyCreated");
	ASSERT_EQ(created.size(), 25u);
	EXPECT_EQ(created[0], 1);
	EXPECT_EQ(created[4], 0);
	EXPECT_EQ(created[5], 0);
	const auto* rule = Find(*bolt, "UR_Lightning");
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(rule->Int("ForkGroup", 99), 1);
	EXPECT_EQ(rule->Int("LightMapGroup", 99), -1);
	EXPECT_EQ(rule->Int("CommonGlowGroup", 99), 2);
	EXPECT_EQ(rule->Int("MaxLightningObjects", 0), 1);
	EXPECT_EQ(rule->Int("MinLightningObjects", 0), 1);
	EXPECT_EQ(rule->Int("MaxLightningObjectsAtOnce", 0), 1);
	EXPECT_EQ(rule->Int("MaxJointsPerFork", 0), 5);
	EXPECT_FLOAT_EQ(rule->Float("SplitAngle", 0.0f), 0.2f);
	EXPECT_FLOAT_EQ(rule->Float("ForkScale", 0.0f), 2.0f);
	EXPECT_TRUE(rule->Bool("CastingFromHand", false));
	// not in the test bolt: the fallback
	EXPECT_FLOAT_EQ(rule->Float("AverageLightmapLife", 7.5f), 7.5f);
	EXPECT_NE(Find(*bolt, "ParticleChainCreator"), nullptr);
	EXPECT_EQ(Find(*bolt, "ParticleLightMapCreator"), nullptr);

	const auto strike = psys::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\n"
	                                      "BEGINCLASS UR_LightningStrike Strike0\nBEGINPROPERTIES\n"
	                                      "PROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 1 1\n"
	                                      "ENDPROPERTIES\nENDCLASS\n"
	                                      "BEGINCLASS UR_Lightning Bolt0\nBEGINPROPERTIES\n"
	                                      "PROPERTY Group INTEGER 1\nPROPERTY CastingFromHand BOOL 0\n"
	                                      "PROPERTY ForkGroup INTEGER 2\nENDPROPERTIES\nENDCLASS\n",
	                                      "SF_LightningStrikeTest");
	ASSERT_TRUE(strike.has_value());
	const auto* strikeRule = Find(*strike, "UR_LightningStrike");
	ASSERT_NE(strikeRule, nullptr);
	EXPECT_EQ(strikeRule->Array("NextGroups"), std::vector<int>({1}));
	const auto* strikeLightning = Find(*strike, "UR_Lightning");
	ASSERT_NE(strikeLightning, nullptr);
	EXPECT_FALSE(strikeLightning->Bool("CastingFromHand", true));
	EXPECT_EQ(strikeLightning->Int("ForkGroup", -1), 2);
	// an empty header: no group array
	EXPECT_TRUE(strike->header.Array("InitiallyCreated").empty());
}

TEST_F(Lightning, handDrawOffset)
{
	// HandDrawOffset::SetReference clamps the weight to 0..1 (a NaN gives 0); GetOffset is
	// (hand - reference) x weight
	psys::Atom::HandDrawOffset offset;
	offset.SetReference(glm::vec3(1.0f, 2.0f, 3.0f), 0.5f);
	const auto moved = offset.GetOffset(glm::vec3(3.0f, 2.0f, -1.0f));
	EXPECT_FLOAT_EQ(moved.x, 1.0f);
	EXPECT_FLOAT_EQ(moved.y, 0.0f);
	EXPECT_FLOAT_EQ(moved.z, -2.0f);
	offset.SetReference(glm::vec3(0.0f), 1.5f);
	EXPECT_FLOAT_EQ(offset.weight, 1.0f);
	offset.SetReference(glm::vec3(0.0f), -0.5f);
	EXPECT_FLOAT_EQ(offset.weight, 0.0f);
	offset.SetReference(glm::vec3(0.0f), std::numeric_limits<float>::quiet_NaN());
	EXPECT_FLOAT_EQ(offset.weight, 0.0f);
}

TEST_F(Lightning, trunkJointsFollowTheHand)
{
	// It reaches the map cells and the dead list
	const openblack::test::ScopedWorldSystems worldSystems;
	// cast from the hand with no spell (the local player is casting): every joint has a
	// HandDrawOffset, and the trunk's carry the step's origin with the weight 1 - i / (n - 1)
	const auto file = ParseBolt();
	ASSERT_NE(file, nullptr);
	game_clock::SetTurn(3);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	const glm::vec3 hand(100.0f, 30.0f, 200.0f);
	std::vector<psys::Effect::DrawChain> chains;
	for (int i = 0; i < 3 && chains.empty(); ++i)
	{
		StepBolt(effect, hand);
		effect.CollectChains(1.0f, chains);
	}
	ASSERT_FALSE(chains.empty());
	const auto& trunk = chains.front().joints;
	ASSERT_EQ(trunk.size(), 5u);
	for (size_t i = 0; i < trunk.size(); ++i)
	{
		ASSERT_NE(trunk[i].atom, nullptr);
		ASSERT_TRUE(trunk[i].atom->drawOffset.has_value());
		EXPECT_FLOAT_EQ(trunk[i].atom->drawOffset->reference.x, hand.x);
		EXPECT_FLOAT_EQ(trunk[i].atom->drawOffset->reference.z, hand.z);
		EXPECT_FLOAT_EQ(trunk[i].atom->drawOffset->weight, 1.0f - static_cast<float>(i) / 4.0f);
	}
	// the trunk starts at the hand and is not interpolated
	EXPECT_FLOAT_EQ(trunk.front().position.x, hand.x);
	EXPECT_FLOAT_EQ(trunk.front().position.y, hand.y);
	game_clock::SetTurn(0);
}

TEST_F(Lightning, chainTexturesOverride)
{
	// UR_Lightning's NumTexturesToTile overrides the chain's repeats: it replaces the creator's ones
	psys::ChainCreator chain;
	chain.numTexturesForWholeChain = 4;
	const auto own = chain.SegmentUv(1, 8, 0.0f);
	const auto tiled = chain.SegmentUv(1, 8, 0.0f, 8); // one repeat per segment
	EXPECT_FLOAT_EQ(own[0].y, 0.125f);
	EXPECT_FLOAT_EQ(tiled[0].y, 0.0f);
	EXPECT_FLOAT_EQ(tiled[2].y, 0.25f);
}

TEST_F(Lightning, twoBoltsClash)
{
	// It reaches the map cells and the dead list
	const openblack::test::ScopedWorldSystems worldSystems;
	// a newer bolt cast from the hand next to an older one aiming the same way links to it. The newer one's
	// trunk ends at the clash point and strikes nothing; the older one's trunk ends there too and a fork three
	// times as thick (opaque) goes on to its target, whose event has strength 2 and also goes to
	// the newer bolt's spell
	const auto file = ParseBolt();
	ASSERT_NE(file, nullptr);
	RecordingSink olderSink;
	RecordingSink newerSink;
	game_clock::SetTurn(20);
	psys::Effect older(file, glm::vec3(0.0f), 1.0f);
	older.SetSink(&olderSink);
	StepBolt(older, glm::vec3(0.0f, 2.0f, 0.0f));
	game_clock::SetTurn(21);
	psys::Effect newer(file, glm::vec3(0.0f), 1.0f);
	newer.SetSink(&newerSink);
	olderSink.events.clear();
	newerSink.events.clear();
	StepBolt(newer, glm::vec3(-4.0f, 2.0f, 0.0f));
	StepBolt(older, glm::vec3(0.0f, 2.0f, 0.0f));
	std::vector<psys::Effect::DrawChain> olderChains;
	std::vector<psys::Effect::DrawChain> newerChains;
	older.CollectChains(1.0f, olderChains);
	newer.CollectChains(1.0f, newerChains);
	ASSERT_EQ(newerChains.size(), 1u); // the trunk only
	ASSERT_EQ(olderChains.size(), 2u); // the trunk and the fork after the clash point
	const auto clash = newerChains.front().joints.back().position;
	EXPECT_NEAR(olderChains[0].joints.back().position.x, clash.x, 1e-4f);
	EXPECT_NEAR(olderChains[0].joints.back().position.z, clash.z, 1e-4f);
	EXPECT_NEAR(olderChains[1].joints.front().position.x, clash.x, 1e-4f);
	for (const auto& joint : olderChains[1].joints)
	{
		EXPECT_FLOAT_EQ(joint.alpha, 255.0f);
	}
	// the thick fork: depth 1 from S x 3 / 2 to S x 3 / 3, S = 2 (ForkScale)
	EXPECT_NEAR(olderChains[1].joints.front().scale, 3.0f, 1e-4f);
	const auto landed = [](const RecordingSink& sink) {
		int count = 0;
		for (const auto& event : sink.events)
		{
			if (event.type == psys::SpellEventInfo::Type::Landed)
			{
				EXPECT_FLOAT_EQ(event.strength, 2.0f);
				++count;
			}
		}
		return count;
	};
	EXPECT_EQ(landed(olderSink), 1);
	EXPECT_EQ(landed(newerSink), 1);
	game_clock::SetTurn(0);
}

TEST_F(Lightning, landCutsTheFork)
{
	// It reaches the map cells and the dead list
	const openblack::test::ScopedWorldSystems worldSystems;
	// LandIslandInterface::RayCast from the fork's origin through its split point; land met
	// closer (in x z) than the split point ends the fork there: no strike. The target is a ground point 2 m over
	// GetHeightAt (0 here) and the hand 30 m up: over land at altitude 0 the ray meets it past the target, over land at
	// altitude 30 (20.1 m) a third of the way down
	const auto file = ParseBolt();
	ASSERT_NE(file, nullptr);
	const auto strikes = [&file](uint8_t altitude) {
		Locator::terrainSystem::emplace<LevelIsland>(altitude);
		RecordingSink sink;
		game_clock::SetTurn(40);
		psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
		effect.SetSink(&sink);
		for (int i = 0; i < 3; ++i)
		{
			StepBolt(effect, glm::vec3(1000.0f, 30.0f, 1000.0f));
		}
		int count = 0;
		for (const auto& event : sink.events)
		{
			count += event.type == psys::SpellEventInfo::Type::Landed ? 1 : 0;
		}
		Locator::terrainSystem::reset();
		game_clock::SetTurn(0);
		return count;
	};
	EXPECT_GT(strikes(0), 0);
	EXPECT_EQ(strikes(30), 0);
}
