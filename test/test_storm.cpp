/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The storm miracle: the storm spell's clamp and upkeep, UR_CloudGather's storm descriptor and cloud ramps,
// UR_Tornado's funnel formulas, the lightning flash and how it reaches the camera, the storm puffs' colour, the
// interface alignment of the sky, and, with the install, the real SF_LightningStormPush / SF_StormCast.

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Common/Zip.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Weather/LightningFlash.h"
#include "ECS/Weather/StormClouds.h"
#include "ECS/Weather/Storms.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/Storm.h"

using namespace openblack;

namespace
{
std::shared_ptr<const psys::File> Parse(std::string_view text, const char* name)
{
	auto file = psys::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const psys::File>(*file) : nullptr;
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

/// One cloud core (CreateRuleSphere of 1 atom, NextGroups 2) and its UR_CloudGather of 10 clouds, with the providers
/// of SF_LightningStormPush (Radius x 1.2, CloudScale x 0.0075, CloudHeight x 1.5) and a point creator for the clouds
constexpr std::string_view k_Gather = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider FP_Radius
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1e+006
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.2
ENDPROPERTIES
ENDCLASS
BEGINCLASS MagnitudeFloatProvider FP_Scale
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1e+006
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 0.0075
ENDPROPERTIES
ENDCLASS
BEGINCLASS MagnitudeFloatProvider FP_Height
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1e+006
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.5
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator Point
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleSphere Cores
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 2
PROPERTY NumAtoms INTEGER 1
PROPERTY PCreator PERSIS_PNTR Point
PROPERTY Radius FLOAT 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_CloudGather Gather
BEGINPROPERTIES
PROPERTY Group INTEGER 2
PROPERTY NumAtoms INTEGER 10
PROPERTY TimeToForm FLOAT 8
PROPERTY FracToMaxSize FLOAT 0.413717
PROPERTY PCreator PERSIS_PNTR Point
PROPERTY RadiusFloatProvider PERSIS_PNTR FP_Radius
PROPERTY ScaleFloatProvider PERSIS_PNTR FP_Scale
PROPERTY CloudHeight PERSIS_PNTR FP_Height
PROPERTY MaxAlpha INTEGER 180
PROPERTY MinAlpha INTEGER 0
PROPERTY MaxColor INTEGER 120
PROPERTY MinColor INTEGER 90
PROPERTY MaxCloudRatio FLOAT 10
PROPERTY MinCloudRatio FLOAT 1
PROPERTY CloudRatioMaxCollection FLOAT 1
PROPERTY CollectionRadiusInitialScale FLOAT 2
PROPERTY LightningGroup INTEGER 4
ENDPROPERTIES
ENDCLASS
)";
} // namespace

TEST(Storm, radiusClampAndUpkeep)
{
	// InitWithPos: min(r, max) then max(r, min); STORM rows: 20..1000
	EXPECT_FLOAT_EQ(magic::ClampStormRadius(20.0f, 1000.0f, 5.0f), 20.0f);
	EXPECT_FLOAT_EQ(magic::ClampStormRadius(20.0f, 1000.0f, 20.0f), 20.0f);
	EXPECT_FLOAT_EQ(magic::ClampStormRadius(20.0f, 1000.0f, 60.0f), 60.0f);
	EXPECT_FLOAT_EQ(magic::ClampStormRadius(20.0f, 1000.0f, 5000.0f), 1000.0f);
	// CalculateCostToMaintain: base x (r / radiusForNormalCost 40)^2
	EXPECT_FLOAT_EQ(magic::StormCostToMaintain(20.0f, 40.0f, 40.0f), 20.0f);
	EXPECT_FLOAT_EQ(magic::StormCostToMaintain(20.0f, 80.0f, 40.0f), 80.0f);
	EXPECT_FLOAT_EQ(magic::StormCostToMaintain(30.0f, 60.0f, 40.0f), 30.0f * 2.25f);
}

TEST(Storm, gatherDescriptorRadii)
{
	// inner = max(R, 60), outer = max(2.5 R, inner + 20, 80), R = 1.2 x magnitude
	psys::storm::GatherStormInput input;
	input.radius = 72.0f; // magnitude 60
	input.magnitude = 60.0f;
	auto d = psys::storm::GatherStormDescriptor(input);
	EXPECT_FLOAT_EQ(d.innerRadius, 72.0f);
	EXPECT_FLOAT_EQ(d.outerRadius, 180.0f);
	input.radius = 24.0f; // the smallest storm, magnitude 20
	input.magnitude = 20.0f;
	d = psys::storm::GatherStormDescriptor(input);
	EXPECT_FLOAT_EQ(d.innerRadius, 60.0f);
	EXPECT_FLOAT_EQ(d.outerRadius, 80.0f);
	input.radius = 45.0f;
	d = psys::storm::GatherStormDescriptor(input);
	EXPECT_FLOAT_EQ(d.innerRadius, 60.0f);
	EXPECT_FLOAT_EQ(d.outerRadius, 112.5f);
	input.radius = 1200.0f;
	d = psys::storm::GatherStormDescriptor(input);
	EXPECT_FLOAT_EQ(d.innerRadius, 1200.0f);
	EXPECT_FLOAT_EQ(d.outerRadius, 3000.0f);
	// the rest of the descriptor
	EXPECT_FLOAT_EQ(d.fadeInTime, 5.0f); // 0.5 x TimeToForm (10, the default)
	EXPECT_FLOAT_EQ(d.lifeTime, 1e9f);
	EXPECT_FLOAT_EQ(d.strength, 1.0f);
	EXPECT_EQ(d.numClouds, 0);
	EXPECT_EQ(d.weather.temperature, 20);
	EXPECT_EQ(d.weather.overcast, 80);
	EXPECT_EQ(d.weather.snow, 0);
	EXPECT_EQ(d.position, glm::vec3(0.0f)); // moved to the core after the registration
}

TEST(Storm, gatherDescriptorWindAndRain)
{
	psys::storm::GatherStormInput input;
	input.radius = 72.0f;
	input.magnitude = 60.0f; // f = (60 - 20) / (100 - 20) = 0.5 -> 40 + 0.5 x 60 = 70
	input.heading = glm::vec3(1.0f, 0.0f, 0.0f);
	auto d = psys::storm::GatherStormDescriptor(input);
	EXPECT_EQ(d.weather.windX, 70);
	EXPECT_EQ(d.weather.windZ, 0);
	input.heading = glm::vec3(-0.5f, 0.0f, 0.5f);
	d = psys::storm::GatherStormDescriptor(input);
	EXPECT_EQ(d.weather.windX, -35);
	EXPECT_EQ(d.weather.windZ, 35);
	// clamped to +-128 and rounded into a byte: 128 wraps to -128
	input.heading = glm::vec3(3.0f, 0.0f, -3.0f);
	d = psys::storm::GatherStormDescriptor(input);
	EXPECT_EQ(d.weather.windX, -128);
	EXPECT_EQ(d.weather.windZ, -128);
	// the strength multiplies the speed
	input.heading = glm::vec3(1.0f, 0.0f, 0.0f);
	input.power = 0.5f;
	d = psys::storm::GatherStormDescriptor(input);
	EXPECT_EQ(d.weather.windX, 35);
	// rain: min(trunc(power x rainAmount) as a byte, 100); 100 without a storm spell; 0 when off
	input.power = 1.0f;
	input.rainAmount = 50.0f;
	EXPECT_EQ(psys::storm::GatherStormDescriptor(input).weather.rain, 50);
	input.rainAmount = 100.0f;
	EXPECT_EQ(psys::storm::GatherStormDescriptor(input).weather.rain, 100);
	input.rainAmount = 0.0f; // the tornado row
	EXPECT_EQ(psys::storm::GatherStormDescriptor(input).weather.rain, 0);
	input.power = 3.0f; // 300 & 0xFF = 44 (the byte is compared with 100)
	input.rainAmount = 100.0f;
	EXPECT_EQ(psys::storm::GatherStormDescriptor(input).weather.rain, 44);
	input.power = 1.0f;
	input.rainAmount = -1.0f;
	EXPECT_EQ(psys::storm::GatherStormDescriptor(input).weather.rain, 100);
	input.rainOn = false;
	EXPECT_EQ(psys::storm::GatherStormDescriptor(input).weather.rain, 0);
}

TEST(Storm, cloudRamps)
{
	// UR_CloudGather with SF_LightningStormPush's values
	psys::storm::CloudLookParams p;
	p.fracToMaxSize = 0.413717f;
	p.minCloudRatio = 1.0f;
	p.maxCloudRatio = 10.0f;
	p.minColor = 90;
	p.maxColor = 120;
	p.minAlpha = 0;
	p.maxAlpha = 180;
	p.minScaleFactor = 0.0f;
	p.maxScaleFactor = 1.0f;
	p.scaleProvider = 0.45f;
	auto look = psys::storm::CloudLookAt(0.0f, p);
	EXPECT_FLOAT_EQ(look.grow, 0.0f);
	EXPECT_FLOAT_EQ(look.ratio, 10.0f);
	EXPECT_EQ(look.colour, 120);
	EXPECT_EQ(look.alpha, 0);
	EXPECT_FLOAT_EQ(look.scale, 0.0f);
	look = psys::storm::CloudLookAt(0.413717f, p);
	EXPECT_NEAR(look.grow, 1.0f, 1e-5f);
	EXPECT_EQ(look.alpha, 180);
	EXPECT_NEAR(look.scale, 0.45f, 1e-5f);
	look = psys::storm::CloudLookAt(1.0f, p);
	EXPECT_NEAR(look.grow, 0.0f, 1e-5f);
	EXPECT_FLOAT_EQ(look.ratio, 1.0f);
	EXPECT_EQ(look.colour, 90);
	// the collection's ramps over its first 10 s
	EXPECT_FLOAT_EQ(psys::storm::CollectionScale(0.0f, 2.0f), 2.0f);
	EXPECT_FLOAT_EQ(psys::storm::CollectionScale(5.0f, 2.0f), 1.5f);
	EXPECT_FLOAT_EQ(psys::storm::CollectionScale(10.0f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(psys::storm::CollectionScale(30.0f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(psys::storm::CollectionScale(4.0f, 1.0f), 1.0f);
}

TEST(Storm, tornadoFunnel)
{
	// the funnel radius with SF_LightningStormPush's radii and magnitude 60 (TornadoScale 0.013 x 60)
	const float s = 0.78f;
	EXPECT_NEAR(psys::storm::FunnelRadius(0.0f, 4.38053f, 36.7624f, s), 4.38053f * s, 1e-5f);
	EXPECT_NEAR(psys::storm::FunnelRadius(1.0f, 4.38053f, 36.7624f, s), 36.7624f * s, 1e-4f);
	EXPECT_NEAR(psys::storm::FunnelRadius(0.5f, 4.38053f, 36.7624f, s), (4.38053f + 32.38187f * 0.25f) * s, 1e-4f);
	EXPECT_NEAR(psys::storm::FunnelRadius(2.0f, 4.38053f, 36.7624f, s), 36.7624f * s, 1e-4f); // clamped
	// Schlick's bias and gain (using 1 / ln 0.5): b = 0.5 is the identity
	EXPECT_NEAR(psys::storm::Bias(0.5f, 0.3f), 0.3f, 1e-5f);
	EXPECT_NEAR(psys::storm::Bias(0.632743f, 0.5f), 0.632743f, 1e-5f);
	EXPECT_NEAR(psys::storm::Gain(0.5f, 0.25f), 0.25f, 1e-5f);
	EXPECT_NEAR(psys::storm::Gain(0.668142f, 0.5f), 0.5f, 1e-5f);
	EXPECT_LT(psys::storm::Gain(0.668142f, 0.25f), 0.25f);
	// the radial rate: rho = 20 against r = 10, dt 0.1: k1 = -5, k2 = -0.5 (-0.5 + 10) = -4.75
	EXPECT_NEAR(psys::storm::RadialRate(10.0f, 20.0f, 0.1f), -4.875f, 1e-5f);
	EXPECT_FLOAT_EQ(psys::storm::RadialRate(10.0f, 10.0f, 0.1f), 0.0f);
}

TEST(Storm, gatherRegistersOneStorm)
{
	ASSERT_NE(psys::FindModifierFactory("UR_CloudGather"), nullptr); // fills the registry
	weather::storms::Clear();
	const auto file = Parse(k_Gather, "SF_GatherTest");
	ASSERT_NE(file, nullptr);
	{
		psys::Effect effect(file, glm::vec3(500.0f, 0.0f, 700.0f), 60.0f);
		effect.Step(0.1f);
		// the first core's gather registers the storm at its core (no spell: level -1, no lightning, rain 100)
		int count = 0;
		weather::storms::ForEach([&](const weather::storms::Storm& storm) {
			++count;
			EXPECT_FLOAT_EQ(storm.descriptor.innerRadius, 72.0f);
			EXPECT_FLOAT_EQ(storm.descriptor.outerRadius, 180.0f);
			EXPECT_EQ(storm.descriptor.weather.rain, 100);
			EXPECT_FLOAT_EQ(storm.descriptor.position.x, 500.0f);
			EXPECT_FLOAT_EQ(storm.descriptor.position.z, 700.0f);
			EXPECT_FLOAT_EQ(storm.descriptor.fadeInTime, 4.0f);
			EXPECT_FLOAT_EQ(storm.descriptor.elevation, 90.0f);
		});
		EXPECT_EQ(count, 1);
		// NumAtoms / TimeToForm = 1.25 clouds a second, made while the count is below the amount owed: the first at
		// once (0 < 0.125), the second once 1 is passed (the tenth step)
		EXPECT_EQ(effect.AtomCount(), 1u + 1u); // the core and the first cloud
		for (int i = 0; i < 4; ++i)             // 0.625 owed
		{
			effect.Step(0.1f);
		}
		EXPECT_EQ(effect.AtomCount(), 1u + 1u);
		for (int i = 0; i < 7; ++i) // 1.5 owed
		{
			effect.Step(0.1f);
		}
		EXPECT_EQ(effect.AtomCount(), 1u + 2u);
		for (int i = 0; i < 80; ++i)
		{
			effect.Step(0.1f);
		}
		EXPECT_EQ(effect.AtomCount(), 1u + 10u); // all of them after TimeToForm
		// closing marks the storm
		effect.CloseDown();
		effect.Step(0.1f);
		int alive = 0;
		weather::storms::ForEach([&](const weather::storms::Storm& storm) { alive += storm.deleteCounter == 0 ? 1 : 0; });
		EXPECT_EQ(alive, 0);
	}
	weather::storms::Clear();
}

/// The clouds as ParticleMistCreator atoms: their own mist atlas counter, seeded as trunc(Random(0, 16)) & 15, and the
/// atom's specular (opaque black until a strike) carried by the draw atoms for the mist renderer's colour
TEST(Storm, cloudMistAtoms)
{
	ASSERT_NE(psys::FindCreatorFactory("ParticleMistCreator"), nullptr);
	weather::storms::Clear();
	std::string text(k_Gather);
	text += R"(BEGINCLASS ParticleMistCreator Mist
BEGINPROPERTIES
PROPERTY ColorA INTEGER 144
PROPERTY InitialScale FLOAT 12
PROPERTY InitialScaleMin FLOAT 8
PROPERTY RandomiseScale BOOL 1
PROPERTY TakeRatioFromMatrix BOOL 1
ENDPROPERTIES
ENDCLASS
)";
	const auto at = text.find("PROPERTY PCreator PERSIS_PNTR Point\nPROPERTY RadiusFloatProvider");
	ASSERT_NE(at, std::string::npos);
	text.replace(at, std::strlen("PROPERTY PCreator PERSIS_PNTR Point"), "PROPERTY PCreator PERSIS_PNTR Mist");
	const auto file = Parse(text, "SF_GatherMistTest");
	ASSERT_NE(file, nullptr);
	{
		psys::Effect effect(file, glm::vec3(500.0f, 0.0f, 700.0f), 60.0f);
		for (int i = 0; i < 40; ++i)
		{
			effect.Step(0.1f);
		}
		std::vector<psys::Effect::DrawAtom> atoms;
		effect.Collect(1.0f, atoms, psys::Creator::Kind::Other);
		ASSERT_FALSE(atoms.empty());
		for (const auto& atom : atoms)
		{
			EXPECT_EQ(atom.specular, 0xFF000000u);
			ASSERT_NE(atom.atom, nullptr);
			EXPECT_EQ(atom.atom->specular, 0xFF000000u);
			const int counter = atom.atom->mist.counter;
			EXPECT_GE(counter, 0);
			EXPECT_LE(counter, 15);
		}
	}
	weather::storms::Clear();
}

// A creator's SpecColorR/G/B properties (0 by default), which are packed into the atom's specular as
// (R << 16) | (G << 8) | B, alpha 0
TEST(Storm, creatorSpecColour)
{
	psys::Object object;
	object.className = "ParticleMistCreator";
	psys::Creator plain;
	psys::ReadCreatorProperties(object, plain);
	EXPECT_EQ(plain.specR, 0);
	EXPECT_EQ(plain.specG, 0);
	EXPECT_EQ(plain.specB, 0);
	psys::Object mist = object;
	mist.properties["SpecColorR"].integer = 10;
	mist.properties["SpecColorG"].integer = 20;
	mist.properties["SpecColorB"].integer = 30;
	psys::Creator creator;
	psys::ReadCreatorProperties(mist, creator);
	EXPECT_EQ(creator.specR, 10);
	EXPECT_EQ(creator.specG, 20);
	EXPECT_EQ(creator.specB, 30);
}

TEST(Storm, lightningFlash)
{
	weather::storms::Storm::Flash flash;
	weather::flash::Start(flash, glm::vec3(1.0f, 2.0f, 3.0f), 180.0f, 0.5f);
	EXPECT_TRUE(flash.active);
	EXPECT_FLOAT_EQ(flash.radius, 180.0f);
	// at age 0: f1 = 1, f3 = 1, times the intensity
	EXPECT_FLOAT_EQ(weather::flash::Frame(flash), 0.5f);
	EXPECT_FLOAT_EQ(flash.f1, 0.5f);
	// the dip between 0.2 and 0.5 s: 0.1
	weather::flash::Age(flash, 0.3f);
	weather::flash::Frame(flash);
	EXPECT_FLOAT_EQ(flash.f1, 0.05f);
	EXPECT_FLOAT_EQ(flash.f3, 0.05f);
	// 0.6 s: s = 0.4, f3 = s^3
	weather::flash::Age(flash, 0.3f);
	weather::flash::Frame(flash);
	EXPECT_NEAR(flash.f1, 0.2f, 1e-6f);
	EXPECT_NEAR(flash.f3, 0.032f, 1e-6f);
	// off past 0.8 s
	weather::flash::Age(flash, 0.25f);
	EXPECT_FALSE(flash.active);
	EXPECT_FLOAT_EQ(weather::flash::Frame(flash), 0.0f);
	EXPECT_FLOAT_EQ(flash.f1, 0.0f);
	// the flash bitmap
	EXPECT_EQ(weather::flash::BitmapTexel(0, 0), 255);
	EXPECT_EQ(weather::flash::BitmapTexel(31, 0), 9);
	EXPECT_EQ(weather::flash::BitmapTexel(-32, 0), 0);
	EXPECT_EQ(weather::flash::BitmapTexel(20, 20), static_cast<uint8_t>(static_cast<int>((32.0f - std::sqrt(800.0f)) * 9.0f)));
}

TEST(Storm, lightningFlashAtCamera)
{
	weather::storms::Clear();
	weather::storms::StormDescriptor far;
	far.position = glm::vec3(1000.0f, 0.0f, 0.0f);
	far.innerRadius = 100.0f;
	far.outerRadius = 300.0f;
	weather::storms::StormDescriptor near = far;
	near.position = glm::vec3(0.0f, 0.0f, 250.0f);
	const auto farId = weather::storms::Create(far);   // the older one
	const auto nearId = weather::storms::Create(near); // the newest, first in the list
	weather::flash::Start(weather::storms::Find(nearId)->flash, near.position, 300.0f, 1.0f);
	weather::flash::UpdateFrame();
	// the camera at the origin: the near storm is 250 away, outside ((100 + 300) / 2) = 200
	EXPECT_EQ(weather::LightningFlashAtCamera(glm::vec3(0.0f)), 0);
	// 150 away: inside, the flash f1 = 1 -> 255
	EXPECT_EQ(weather::LightningFlashAtCamera(glm::vec3(0.0f, 0.0f, 100.0f)), 255);
	// at (900, 0, 0) the far one is nearest and inside; not flashing -> 0, then flashing at 0.5 -> trunc(127.5)
	EXPECT_EQ(weather::LightningFlashAtCamera(glm::vec3(900.0f, 0.0f, 0.0f)), 0);
	weather::flash::Start(weather::storms::Find(farId)->flash, far.position, 300.0f, 0.5f);
	weather::flash::UpdateFrame();
	EXPECT_EQ(weather::LightningFlashAtCamera(glm::vec3(900.0f, 0.0f, 0.0f)), 127);
	weather::storms::Clear();
}

TEST(Storm, lightningFlashInsideFlagSticks)
{
	// "inside" is set by any storm that was the nearest so far and never cleared, so a
	// nearer later storm the camera is outside of still gives its flash
	weather::storms::Clear();
	weather::storms::StormDescriptor small;
	small.position = glm::vec3(100.0f, 0.0f, 0.0f);
	small.innerRadius = 10.0f;
	small.outerRadius = 30.0f;
	weather::storms::StormDescriptor big;
	big.position = glm::vec3(0.0f, 0.0f, 450.0f);
	big.innerRadius = 400.0f;
	big.outerRadius = 600.0f;
	const auto smallId = weather::storms::Create(small); // second in the list
	weather::storms::Create(big);                        // first: the camera is inside it
	weather::flash::Start(weather::storms::Find(smallId)->flash, small.position, 30.0f, 0.5f);
	weather::flash::UpdateFrame();
	EXPECT_EQ(weather::LightningFlashAtCamera(glm::vec3(0.0f)), 127);
	weather::storms::Clear();
}

TEST(Storm, puffColour)
{
	// the base colour, each byte x (1 - blackness / 2) when dark, alpha fade x 0.75 x A
	EXPECT_EQ(weather::storm_clouds::PuffColour(0xFFC0A080u, 0.5f, 1.0f), 0xBF907860u);
	EXPECT_EQ(weather::storm_clouds::PuffColour(0xFFC0A080u, 0.0f, 1.0f), 0xBFC0A080u);
	EXPECT_EQ(weather::storm_clouds::PuffColour(0xFFFFFFFFu, 1.0f, 0.5f) >> 24u, 95u);
}

TEST(Storm, interfaceAlignment)
{
	// no player has influence here (no land): the neutral player, x = (a + 1) / 2
	ecs::effects::alignment::SetClamped(PlayerNames::NEUTRAL, 0.0f);
	EXPECT_EQ(ecs::effects::alignment::MostInfluentialPlayer(glm::vec3(0.0f)), PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::InterfaceAlignmentAt(glm::vec3(0.0f)), 0.5f);
	ecs::effects::alignment::SetClamped(PlayerNames::NEUTRAL, -0.6f);
	EXPECT_NEAR(ecs::effects::alignment::InterfaceAlignmentAt(glm::vec3(0.0f)), 0.2f, 1e-6f);
	ecs::effects::alignment::SetClamped(PlayerNames::NEUTRAL, 1.0f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::InterfaceAlignmentAt(glm::vec3(0.0f)), 1.0f);
	ecs::effects::alignment::ResetInterfaceAlignment();
	EXPECT_FLOAT_EQ(ecs::effects::alignment::GetInterfaceAlignment(), 0.5f);
}

TEST(Storm, classesAreRegistered)
{
	for (const auto* name : {"UR_CloudMoverNew", "UR_CloudGather", "UR_Tornado", "UR_StormCast"})
	{
		EXPECT_NE(psys::FindModifierFactory(name), nullptr) << name;
	}
	EXPECT_NE(psys::FindCreatorFactory("ParticleMistCreator"), nullptr);
}

/// With OPENBLACK_GAME_PATH set to the install: the real storm files
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Storm, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::filesystem::path root(game);
	const auto push = LoadSpellFile(root, "SF_LightningStormPush");
	ASSERT_TRUE(push.has_value());
	const auto created = push->header.Array("InitiallyCreated");
	ASSERT_EQ(created.size(), 25u);
	EXPECT_EQ(created[0], 1);
	EXPECT_EQ(created[6], 1);
	EXPECT_EQ(created[12], 1);
	const auto* gather = Find(*push, "UR_CloudGather");
	ASSERT_NE(gather, nullptr);
	EXPECT_EQ(gather->Int("NumAtoms", 0), 10);
	EXPECT_FLOAT_EQ(gather->Float("TimeToForm", 0.0f), 8.0f);
	EXPECT_FLOAT_EQ(gather->Float("LightningDelay", 0.0f), 8.0f);
	EXPECT_FLOAT_EQ(gather->Float("SwitchLife", 0.0f), 3.0f);
	EXPECT_FLOAT_EQ(gather->Float("LightningLife", 0.0f), 0.2f);
	EXPECT_EQ(gather->Int("LightningGroup", -1), 4);
	EXPECT_EQ(gather->String("TornadoGroup"), ""); // not in the file: the default 10
	const auto* tornado = Find(*push, "UR_Tornado");
	ASSERT_NE(tornado, nullptr);
	EXPECT_EQ(tornado->Int("Group", -1), 11);
	EXPECT_EQ(tornado->Int("GroupFlying", -1), 13);
	EXPECT_EQ(tornado->Int("GroupToMoveToOnceDone", -1), 12);
	EXPECT_EQ(tornado->Int("NumAtomsToCreate", -1), 0); // makes nothing
	EXPECT_FLOAT_EQ(tornado->Float("FadeInTime", 0.0f), 8.0f);
	const auto* mover = Find(*push, "UR_CloudMoverNew");
	ASSERT_NE(mover, nullptr);
	EXPECT_FLOAT_EQ(mover->Float("WindMagnification", 0.0f), 30.0f);
	const auto* lightning = Find(*push, "UR_Lightning");
	ASSERT_NE(lightning, nullptr);
	EXPECT_FALSE(lightning->Bool("CastingFromHand", true));
	EXPECT_EQ(lightning->Int("ForkGroup", -1), 5);
	// every class of both files is registered (or a creator the registry makes)
	for (const auto& object : push->objects)
	{
		const auto& c = object.className;
		if (c.ends_with("FloatProvider") || c.starts_with("EventCondition"))
		{
			continue;
		}
		const bool known = psys::FindModifierFactory(c) != nullptr || psys::FindCreatorFactory(c) != nullptr ||
		                   c == "ParticleSpriteCreator" || c == "ParticlePointCreator";
		EXPECT_TRUE(known) << c;
	}
	const auto cast = LoadSpellFile(root, "SF_StormCast");
	ASSERT_TRUE(cast.has_value());
	const auto* swirl = Find(*cast, "UR_StormCast");
	ASSERT_NE(swirl, nullptr);
	EXPECT_EQ(swirl->Int("NumAtoms", 0), 30);
	EXPECT_FLOAT_EQ(swirl->Float("DispersalAge", 0.0f), 2.4f);
	// SF_StormCast stepped alone: 30 atoms under the root, gone after DispersalAge + FadeOutTime (2.4 + 2)
	{
		auto file = std::make_shared<const psys::File>(*cast);
		psys::Effect effect(file, glm::vec3(0.0f), 60.0f);
		effect.Step(0.1f);
		effect.Step(0.1f);
		EXPECT_EQ(effect.AtomCount(), 1u + 30u);
		for (int i = 0; i < 50; ++i)
		{
			effect.Step(0.1f);
		}
		EXPECT_EQ(effect.AtomCount(), 1u);
	}
}

/// The UR_StormCast part of realData on a small hand-written file: one core (as in k_Gather) whose group 2 is a
/// UR_StormCast of 4 atoms that disperse after 1 s and fade out over 0.5 s. The atoms are made at once, stay while
/// they fade, and are all gone once DispersalAge + FadeOutTime (1.5 s) has passed; the core stays
TEST(Storm, realDataSynthetic)
{
	ASSERT_NE(psys::FindModifierFactory("UR_StormCast"), nullptr); // fills the registry
	constexpr std::string_view k_Cast = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator Point
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleSphere Core
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 2
PROPERTY NumAtoms INTEGER 1
PROPERTY PCreator PERSIS_PNTR Point
PROPERTY Radius FLOAT 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_StormCast Swirl
BEGINPROPERTIES
PROPERTY Group INTEGER 2
PROPERTY NumAtoms INTEGER 4
PROPERTY DispersalAge FLOAT 1
PROPERTY FadeOutTime FLOAT 0.5
PROPERTY PCreator PERSIS_PNTR Point
ENDPROPERTIES
ENDCLASS
)";
	constexpr size_t k_Atoms = 4;
	const auto file = Parse(k_Cast, "SF_StormCastTest");
	ASSERT_NE(file, nullptr);
	const auto* swirl = Find(*file, "UR_StormCast");
	ASSERT_NE(swirl, nullptr);
	EXPECT_EQ(swirl->Int("NumAtoms", 0), static_cast<int>(k_Atoms));
	EXPECT_FLOAT_EQ(swirl->Float("DispersalAge", 0.0f), 1.0f);
	psys::Effect effect(file, glm::vec3(0.0f), 60.0f);
	effect.Step(0.1f);
	effect.Step(0.1f);
	EXPECT_EQ(effect.AtomCount(), 1u + k_Atoms); // the core and the swirl
	// about 1.2 s: past DispersalAge, still fading out
	for (int i = 0; i < 10; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 1u + k_Atoms);
	// about 3 s: well past DispersalAge + FadeOutTime, only the core is left
	for (int i = 0; i < 18; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 1u);
}
