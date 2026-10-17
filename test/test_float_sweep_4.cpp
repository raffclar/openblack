/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Bit tests of the game's float maths for magic, particles, gestures, shadows, the menu backdrop, the boat and the
// learning marker. Each expected value comes from a model of the game's instructions with each operation rounded to a
// float. Each input is one where the old code, which kept parts of the sums in double, gave another bit.

#include <cstdint>

#include <array>
#include <bit>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/BoatLaunchRules.h"
#include "ECS/LearningMarker.h"
#include "Gestures/GestureTrailBuilder.h"
#include "Graphics/GroundShadow.h"
#include "Gui/MenuBackdrop.h"
#include "Magic/VortexCrossingRules.h"
#include "Particles/BeamMaths.h"
#include "Particles/GestureTrail.h"
#include "Particles/ParticleFlocking.h"
#include "Particles/RisingSparks.h"

using namespace openblack;

namespace
{
float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t B(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

TEST(FloatSweep4, RisingSparkSizeRoundsAtEachStep)
{
	particles::rising_sparks::Sparks sparks {};
	sparks.sparks.at(0).ageMilliseconds = 397;
	const auto look = particles::rising_sparks::LookOf(sparks, 0, {0.0f, 37.3f, 0.0f}, 255);
	ASSERT_TRUE(look.has_value());
	EXPECT_EQ(B(look->halfSize), 0x3FFFBF72u);
}

TEST(FloatSweep4, PlasmaLengthRoundsTheLastAdd)
{
	const glm::vec3 start {F(0xC38499AFu), F(0x408EC41Eu), F(0xC38AC055u)};
	const auto [first, second] = particles::maths::PlasmaTangents(
	    start, glm::vec3(0.0f), {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, glm::vec3(0.0f), glm::vec3(0.0f), 0.0f, 1.0f);
	EXPECT_EQ(B(first.x), 0x43BFEFD3u);
	EXPECT_EQ(B(second.x), 0x43BFEFD3u);
}

TEST(FloatSweep4, VortexPlaceTextRoundsTheTensToAFloat)
{
	const map_coords::MapCoords offset {.x = 1021956985, .z = 0, .altitude = 0.0f};
	EXPECT_EQ(vortex::crossing::PlaceText(offset), "\"155938.27,0.00\"");
}

TEST(FloatSweep4, VortexThrowOutPlaceRoundsTheSineTimesTheDistance)
{
	const map_coords::MapCoords vortex {.x = 30653 << 16, .z = 0, .altitude = 0.0f};
	const auto place = vortex::crossing::ThrowOutPlace(vortex, F(0x3F2C8788u));
	EXPECT_EQ(place.x >> 16, 30654);
}

TEST(FloatSweep4, VortexAttractKeepsTheCosineWideOnlyForItsMultiply)
{
	const vortex::crossing::AttractSettings settings {};
	const vortex::crossing::AttractAtom atom {.position = {F(0xC15F3076u), 10.0f, F(0xC2B5723Cu)},
	                                          .takenVelocity = {F(0x408FC024u), 0.0f, 0.0f},
	                                          .startHeight = 0.0f,
	                                          .age = F(0x3F60A9B6u),
	                                          .indestructible = false};
	const glm::vec3 origin {F(0xC21A09DCu), F(0xC3B834F7u), F(0xC21640BFu)};
	const auto step = vortex::crossing::Attract(settings, atom, origin, F(0x3DCCCCCDu), [](glm::vec2) { return 0.0f; });
	ASSERT_TRUE(step.has_value());
	EXPECT_EQ(B(step->position.x), 0xC15DE734u);
}

TEST(FloatSweep4, ForestPathKeepsTheCosineOfPhiWide)
{
	const auto point =
	    particles::forest_path::PathPoint(F(0x4020E3E8u), F(0x40142990u), F(0x3FEEEB6Du), F(0x3F315498u), F(0x408AA660u),
	                                      F(0x3D03CA37u), {F(0x402FBA49u), F(0x3F33B8ACu), F(0x3FF15DD6u)});
	EXPECT_EQ(B(point.x), 0x3EA4AA42u);
}

TEST(FloatSweep4, GestureTrailEasesAndWigglesInFloat)
{
	EXPECT_EQ(B(particles::gesture_trail::Transition(F(0x3F34174Au), F(0x3EE76F49u))), 0x3F3DEA0Du);
	EXPECT_EQ(B(particles::gesture_trail::WiggleAmount(F(0x3FD4D701u), F(0x3FECCB2Au), 100.0f, 1.0f)), 0x3A8C6C8Au);
}

TEST(FloatSweep4, GestureTrailLiftAddsTheSquaresZXY)
{
	const auto lifted = particles::gesture_trail::Lift({F(0x429B9900u), F(0x410DEE91u), F(0x40965EE7u)},
	                                                   {F(0x40ED97ECu), F(0x439B59EFu), F(0xC2ADC4B2u)}, F(0x403C23BBu));
	EXPECT_EQ(B(lifted.x), 0x429A3A13u);
	EXPECT_EQ(B(lifted.y), 0x413CF780u);
	EXPECT_EQ(B(lifted.z), 0x4073A7BDu);
}

TEST(FloatSweep4, TrailSymbolSquareRoundsTheAdd)
{
	const std::array<glm::vec3, 1> points {glm::vec3(F(0xC0ECC0BFu), 0.0f, 0.0f)};
	const auto symbol = gesture::MakeTrailSymbol(points);
	EXPECT_EQ(B(symbol.minX), 0x3EED0F4Cu);
}

TEST(FloatSweep4, ShadowAlphaRoundsBeforeTheStep)
{
	EXPECT_EQ(graphics::GroundShadow::ScaledAlpha(F(0x4327FFFFu)), 168);
}

TEST(FloatSweep4, ShadowOnTheLevelInFloat)
{
	const graphics::GroundShadow::Point point {.position = {F(0xC237A446u), F(0xC1419192u), F(0x41A2C8C9u)},
	                                           .keptAboveLevel = false};
	const graphics::GroundShadow::Cast cast {
	    .light = {F(0xC3D197BAu), F(0x448D5A7Au), F(0x42E94CD2u)}, .level = F(0xC0998876u), .ground = 0.0f};
	EXPECT_EQ(B(graphics::GroundShadow::OnLevel(point, cast).x), 0xC2413800u);
}

TEST(FloatSweep4, LearningMarkerTextRoundsTheHundredthsToAFloat)
{
	// The product is exactly a half, which the C library rounds up
	EXPECT_EQ(ecs::learning_marker::Text(F(0x3F10A3D7u)), "57%");
}

TEST(FloatSweep4, BoatTurnRoundsEachProduct)
{
	glm::mat4 root(1.0f);
	root[0][0] = F(0x3F00BCD3u);
	root[2][0] = F(0xB3BA9A9Bu);
	const auto m = ecs::boat_launch::LaunchMatrix(root, glm::vec3(0.0f), 0.0f);
	// The model is mirrored after the turn, so its first axis changes sign
	EXPECT_EQ(B(m[0][0]), 0x33E9CF24u);
	EXPECT_EQ(B(m[2][0]), 0xBF00BCD3u);
}

TEST(FloatSweep4, MenuBackdropSwirlsKeepTheirSinesWideOnlyForTheAdd)
{
	constexpr std::array<std::array<uint32_t, 3>, 8> k_Angles {{
	    {0xC01E0EBDu, 0xBFA5B898u, 0x3F8A4611u},
	    {0x3FB31EDDu, 0xBEF86132u, 0xC0239882u},
	    {0xBFB91EC8u, 0xBFE63B2Fu, 0xBFADA6EDu},
	    {0x3FF5A0B0u, 0xBFEE7D78u, 0x401952CBu},
	    {0x40168908u, 0xC030A8DAu, 0xBF4057B5u},
	    {0xBD527B6Bu, 0xC03D14F6u, 0xBEEEF3ACu},
	    {0x40214384u, 0xC019F0A6u, 0x3F19B6A2u},
	    {0xC0164B83u, 0x3EF9D384u, 0x401CDB3Eu},
	}};
	gui::menu_backdrop::Swirls swirls {};
	for (size_t k = 0; k < k_Angles.size(); ++k)
	{
		swirls.at(k).angles = {F(k_Angles.at(k).at(0)), F(k_Angles.at(k).at(1)), F(k_Angles.at(k).at(2))};
	}
	gui::menu_backdrop::Layer layer {};
	gui::menu_backdrop::PlaceLayer(swirls, 0, 0.0f, layer);
	EXPECT_EQ(B(layer.at(0).x), 0x3F3CDC1Cu);
	EXPECT_EQ(B(layer.at(0).y), 0x3FAE35A0u);
}
