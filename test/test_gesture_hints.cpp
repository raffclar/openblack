/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Gestures/GestureHints.h"

using namespace openblack;
using namespace openblack::gesture_hints;

namespace
{
constexpr float k_Tolerance = 1e-4f;

gesture::Request Pick(GestureType gesture, LeashType leash)
{
	return {.gesture = gesture, .purpose = gesture::Purpose::PickLeash, .leash = leash};
}

// A circle 4 long and every other gesture 6 long, each a straight line across its square
std::optional<Shape> FakeShapes(GestureType gesture)
{
	const float length = gesture == GestureType::Circle ? 4.0f : 6.0f;
	return Shape {.points = {{0.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.5f}}, .length = length};
}
} // namespace

TEST(GestureHints, OnlyTheLeashesThePickerOffersAreHinted)
{
	const std::vector<gesture::Request> requests {
	    {.gesture = GestureType::Scribble, .purpose = gesture::Purpose::ClosePicker},
	    Pick(GestureType::VerticalScribble, LeashType::Evil),
	    Pick(GestureType::Heart, LeashType::Good),
	};
	const auto hints = HintsFor(requests, FakeShapes);
	ASSERT_EQ(hints.size(), 2u);
	// The higher leash takes the square nearest the right edge
	EXPECT_EQ(hints[0].leash, LeashType::Good);
	EXPECT_EQ(hints[0].order, 6);
	EXPECT_EQ(hints[1].leash, LeashType::Evil);
	EXPECT_EQ(hints[1].order, 4);
}

TEST(GestureHints, TheLeashGestureIsHintedWithTheCollarOfTheLeashItIsAbout)
{
	const std::vector<gesture::Request> requests {
	    {.gesture = GestureType::SquareSpirial, .purpose = gesture::Purpose::LeashGesture, .leash = LeashType::Rope},
	    Pick(GestureType::Heart, LeashType::Good),
	};
	const auto hints = HintsFor(requests, FakeShapes);
	ASSERT_EQ(hints.size(), 2u);
	// The picker's leashes come nearer the right edge than the leash gesture
	EXPECT_EQ(hints[0].leash, LeashType::Good);
	EXPECT_EQ(hints[1].gesture, GestureType::SquareSpirial);
	EXPECT_EQ(hints[1].leash, LeashType::Rope);
	EXPECT_EQ(hints[1].order, k_LeashGestureOrder);
}

TEST(GestureHints, InAFightTheSpecialMoveAndTheFightMiraclesAreHinted)
{
	const std::vector<gesture::Request> requests {
	    {.gesture = GestureType::Star, .purpose = gesture::Purpose::FightSpecialMove},
	    {.gesture = GestureType::VerticalScribble,
	     .purpose = gesture::Purpose::FightMiracle,
	     .magic = MagicType::Fireball,
	     .seed = SpellSeedType::Fire},
	    {.gesture = GestureType::Heart,
	     .purpose = gesture::Purpose::FightMiracle,
	     .magic = MagicType::Heal,
	     .seed = SpellSeedType::Heal},
	};
	const auto hints = HintsFor(requests, FakeShapes);
	ASSERT_EQ(hints.size(), 3u);
	// The fight miracles take the squares nearest the right edge, the later gesture number first; the special move is
	// last
	EXPECT_EQ(hints[0].gesture, GestureType::Heart);
	EXPECT_EQ(hints[0].seed, SpellSeedType::Heal);
	EXPECT_EQ(hints[0].order, k_FightMiracleOrder);
	EXPECT_EQ(hints[1].gesture, GestureType::VerticalScribble);
	EXPECT_EQ(hints[1].seed, SpellSeedType::Fire);
	EXPECT_EQ(hints[2].gesture, GestureType::Star);
	EXPECT_EQ(hints[2].order, k_SpecialMoveOrder);
	EXPECT_EQ(hints[2].seed, SpellSeedType::None);
	EXPECT_EQ(hints[2].leash, LeashType::None);
}

TEST(GestureHints, TheDotRunsAlongAGestureInTimeWithItsLengthAgainstTheCircles)
{
	const std::vector<gesture::Request> requests {Pick(GestureType::Heart, LeashType::Good)};
	const auto hints = HintsFor(requests, FakeShapes);
	ASSERT_EQ(hints.size(), 1u);
	EXPECT_NEAR(hints[0].period, k_CirclePeriod * 6.0f / 4.0f, k_Tolerance);
}

TEST(GestureHints, AGestureWithoutAShapeTakesTheCircles)
{
	const auto onlyCircle = [](GestureType gesture) -> std::optional<Shape> {
		return gesture == GestureType::Circle ? FakeShapes(gesture) : std::nullopt;
	};
	const std::vector<gesture::Request> requests {Pick(GestureType::EShape, LeashType::Rope)};
	const auto hints = HintsFor(requests, onlyCircle);
	ASSERT_EQ(hints.size(), 1u);
	EXPECT_EQ(hints[0].path.size(), 2u);
	EXPECT_NEAR(hints[0].period, k_CirclePeriod, k_Tolerance);
}

TEST(GestureHints, TheRowIsReplacedOnlyWhenItsHintsChange)
{
	const auto a = HintsFor(std::vector {Pick(GestureType::Heart, LeashType::Good)}, nullptr);
	auto shown = HintsFor(std::vector {Pick(GestureType::Heart, LeashType::Good)}, FakeShapes);
	shown[0].dot = 0.5f;
	EXPECT_FALSE(Differs(shown, a));
	const auto b = HintsFor(std::vector {Pick(GestureType::EShape, LeashType::Rope)}, nullptr);
	EXPECT_TRUE(Differs(shown, b));
	EXPECT_TRUE(Differs(shown, {}));
}

TEST(GestureHints, TheSquaresRunLeftFromTheBottomRightCornerByShareOfTheWidth)
{
	const glm::vec2 screen {800.0f, 600.0f};
	const auto first = Square(0, screen);
	EXPECT_NEAR(first.max.x, 800.0f - 12.0f, k_Tolerance);
	EXPECT_NEAR(first.min.x, 800.0f - 12.0f - 28.0f, k_Tolerance);
	EXPECT_NEAR(first.min.y, 600.0f - 28.0f, k_Tolerance);
	EXPECT_NEAR(first.max.y, 600.0f, k_Tolerance);
	const auto second = Square(1, screen);
	EXPECT_NEAR(first.min.x - second.max.x, 4.0f, k_Tolerance);
	const auto picture = PictureSquare(first);
	EXPECT_EQ(picture.max.y, first.min.y);
	EXPECT_NEAR(picture.min.y, first.min.y - 28.0f, k_Tolerance);
}

TEST(GestureHints, OnAWideScreenTheRowIsSizedAsOnA4By3ScreenAsTall)
{
	const auto wide = Square(0, {2560.0f, 1080.0f});
	const auto square = Square(0, {1440.0f, 1080.0f});
	EXPECT_NEAR(wide.max.x - wide.min.x, square.max.x - square.min.x, 1e-3f);
	EXPECT_NEAR(2560.0f - wide.max.x, 1440.0f - square.max.x, 1e-3f);
}

TEST(GestureHints, ThePicturesAreSizedByTheCamerasFieldOfView)
{
	// A 70 degree field of view across a 4:3 screen 800 wide
	const float fov = glm::radians(70.0f);
	EXPECT_NEAR(SpriteSide(k_PictureHalfSize, 800.0f, fov), 0.02f / std::tan(fov / 2.0f) * 800.0f, k_Tolerance);
	EXPECT_NEAR(SpriteSide(k_DotHalfSize, 800.0f, fov), SpriteSide(k_PictureHalfSize, 800.0f, fov) / 2.0f, k_Tolerance);
}

TEST(GestureHints, TheRowFadesOutWhileTheAdvisorsSpeakAndBackAfter)
{
	EXPECT_NEAR(Fade(1.0f, 0.25f, true), 0.5f, k_Tolerance);
	EXPECT_EQ(Fade(0.2f, 0.25f, true), 0.0f);
	EXPECT_NEAR(Fade(0.0f, 0.25f, false), 0.5f, k_Tolerance);
	EXPECT_EQ(Fade(0.9f, 0.25f, false), 1.0f);
	EXPECT_EQ(Fade(1.0f, 0.25f, false), 1.0f);
}

TEST(GestureHints, TheDotWrapsBackOverlappingTheStart)
{
	EXPECT_NEAR(StepDot(0.0f, 0.5f, 2.0f), 0.25f, k_Tolerance);
	EXPECT_NEAR(StepDot(0.95f, 0.2f, 2.0f), 0.15f, k_Tolerance);
	EXPECT_EQ(StepDot(0.3f, 1.0f, 0.0f), 0.3f);
	EXPECT_EQ(DotsAt(0.5f).size(), 1u);
	const auto ends = DotsAt(0.95f);
	ASSERT_EQ(ends.size(), 2u);
	EXPECT_NEAR(ends[1], 0.05f, k_Tolerance);
}

TEST(GestureHints, TheDotIsBrightestMidGesture)
{
	EXPECT_NEAR(DotBrightness(0.5f), 1.0f, k_Tolerance);
	EXPECT_NEAR(DotBrightness(0.0f), 0.0f, k_Tolerance);
	EXPECT_NEAR(DotBrightness(0.25f), 0.75f, k_Tolerance);
	EXPECT_EQ(DotShade(0.5f, 1.0f), 254);
	EXPECT_EQ(DotShade(0.0f, 1.0f), 0);
}

TEST(GestureHints, ThePictureTakesThePlayersColourDimmedWithTheRow)
{
	const auto full = PictureColour(0xFF4646u, 1.0f);
	EXPECT_EQ(full[0], 254);
	EXPECT_EQ(full[1], 69);
	EXPECT_EQ(full[3], 254);
	const auto half = PictureColour(0xFF4646u, 0.5f);
	EXPECT_EQ(half[0], (255 * 127) >> 8);
	EXPECT_EQ(half[3], (255 * 127) >> 8);
}

TEST(GestureHints, PointsAlongAPathAreBetweenItsPoints)
{
	const std::vector<glm::vec3> path {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	EXPECT_NEAR(PointAlong(path, 0.25f).x, 0.5f, k_Tolerance);
	EXPECT_NEAR(PointAlong(path, 0.75f).z, 0.5f, k_Tolerance);
	EXPECT_EQ(PointAlong(path, -1.0f), path.front());
	EXPECT_EQ(PointAlong(path, 1.0f), path.back());
	EXPECT_EQ(PixelOf({.min = {10.0f, 20.0f}, .max = {38.0f, 48.0f}}, {0.5f, 0.5f}), glm::vec2(24.0f, 34.0f));
}

TEST(GestureHints, CollarsTumbleKeepingTheirAxesSquare)
{
	Axes axes;
	for (int i = 0; i < 100; ++i)
	{
		axes = Tumble(axes, LeashType::Good, 0.05f);
	}
	EXPECT_NEAR(glm::length(axes.x), 1.0f, 1e-3f);
	EXPECT_NEAR(glm::dot(axes.x, axes.y), 0.0f, 1e-3f);
	EXPECT_NEAR(glm::dot(axes.y, axes.z), 0.0f, 1e-3f);
	EXPECT_NE(axes, Axes {});
	// No time, no turn
	EXPECT_EQ(Tumble(Axes {}, LeashType::Evil, 0.0f), Axes {});
	// Each leash turns at its own pace
	EXPECT_NE(Tumble(Axes {}, LeashType::Evil, 0.5f), Tumble(Axes {}, LeashType::Good, 0.5f));
}

TEST(GestureHints, TheRowStopsWhileFadedOut)
{
	Row row;
	row.hints = HintsFor(std::vector {Pick(GestureType::Heart, LeashType::Good)}, FakeShapes);
	Step(row, 0.1f, false);
	EXPECT_GT(row.hints[0].dot, 0.0f);
	EXPECT_NE(row.collars.at(3), Axes {});
	row.alpha = 0.0f;
	const auto dot = row.hints[0].dot;
	Step(row, 0.1f, true);
	EXPECT_EQ(row.hints[0].dot, dot);
}

TEST(GestureHints, TheCollarsCameraLooksAtItInTheMiddleOfItsSquare)
{
	const PixelRect rect {.x = 700, .y = 500, .width = 28, .height = 28};
	const glm::vec2 centre {714.0f, 514.0f};
	const auto clip = CollarProjection(rect, centre, 571.0f, 1.0f) * CollarView() * glm::vec4(k_CollarAt, 1.0f);
	EXPECT_NEAR(clip.x / clip.w, 0.0f, k_Tolerance);
	EXPECT_NEAR(clip.y / clip.w, 0.0f, k_Tolerance);
	// A point a pixel's worth to the right at the collar's distance lands a pixel to the right
	const float distance = glm::length(k_CollarAt - k_CollarEye);
	const auto right = glm::vec4(k_CollarAt + glm::vec3(distance / 571.0f, 0.0f, 0.0f), 1.0f);
	const auto moved = CollarProjection(rect, centre, 571.0f, 1.0f) * CollarView() * right;
	EXPECT_NEAR(moved.x / moved.w * 14.0f, 1.0f, 1e-2f);
	EXPECT_EQ(PixelsOf({.min = {700.4f, 499.6f}, .max = {728.4f, 527.6f}}), rect);
}

namespace
{
// Each seed casts the miracle of its own number plus its power-up level times a hundred
MagicType FakeMagic(SpellSeedType seed, int powerUp)
{
	return static_cast<MagicType>(static_cast<int>(seed) + (100 * (powerUp + 1)));
}

const Hint* HintOf(const std::vector<Hint>& hints, GestureType gesture)
{
	for (const auto& hint : hints)
	{
		if (hint.gesture == gesture)
		{
			return &hint;
		}
	}
	return nullptr;
}
} // namespace

TEST(GestureHints, TheSelectionGesturesShowAGlobeAndTheCreaturesSeed)
{
	const std::vector<gesture::Request> requests {
	    {.gesture = GestureType::Spiral, .purpose = gesture::Purpose::OpenSelection},
	    {.gesture = GestureType::InverseSpiral, .purpose = gesture::Purpose::OpenSelection, .forCreature = true},
	    {.gesture = GestureType::RShape, .purpose = gesture::Purpose::RepeatMiracle, .seed = SpellSeedType::Water},
	};
	const auto hints = HintsFor(requests, FakeShapes, FakeMagic);
	ASSERT_EQ(hints.size(), 3u);
	// The R is nearest the right edge, then the creature's spiral, then the player's
	EXPECT_EQ(hints[0].gesture, GestureType::RShape);
	EXPECT_EQ(hints[0].order, k_RepeatOrder);
	EXPECT_EQ(hints[0].seed, SpellSeedType::Water);
	EXPECT_EQ(hints[0].magic, FakeMagic(SpellSeedType::Water, -1));
	EXPECT_FALSE(hints[0].globe);
	EXPECT_EQ(hints[1].order, k_CreatureSelectionOrder);
	EXPECT_EQ(hints[1].seed, k_CreatureSelectionSeed);
	EXPECT_FALSE(hints[1].globe);
	EXPECT_EQ(hints[2].order, k_OwnSelectionOrder);
	EXPECT_EQ(hints[2].seed, SpellSeedType::None);
	EXPECT_TRUE(hints[2].globe);
}

TEST(GestureHints, TheSelectionStepsShowNoSeed)
{
	const std::vector<gesture::Request> requests {
	    {.gesture = GestureType::Scribble, .purpose = gesture::Purpose::CloseSelection},
	    {.gesture = GestureType::WShape, .purpose = gesture::Purpose::SelectMiracle, .seed = SpellSeedType::Water},
	};
	const auto hints = HintsFor(requests, FakeShapes, FakeMagic);
	ASSERT_EQ(hints.size(), 1u);
	EXPECT_EQ(hints[0].order, k_SelectionStepOrder);
	EXPECT_EQ(hints[0].seed, SpellSeedType::None);
	EXPECT_FALSE(hints[0].globe);
}

TEST(GestureHints, ThePowerUpsShowTheSeedWithItsLevelAndTheCancelOnlyOnceAsked)
{
	const std::vector<gesture::Request> unasked {
	    {.gesture = GestureType::Circle, .purpose = gesture::Purpose::SizeCircle},
	    {.gesture = GestureType::Scribble, .purpose = gesture::Purpose::DropSeed, .seed = SpellSeedType::Storm},
	    {.gesture = GestureType::InverseSpiral,
	     .purpose = gesture::Purpose::PowerUp,
	     .powerUpLevel = 0,
	     .seed = SpellSeedType::Storm},
	    {.gesture = GestureType::Spiral, .purpose = gesture::Purpose::PowerUp, .powerUpLevel = 1, .seed = SpellSeedType::Storm},
	};
	auto hints = HintsFor(unasked, FakeShapes, FakeMagic);
	ASSERT_EQ(hints.size(), 3u);
	EXPECT_EQ(HintOf(hints, GestureType::Scribble), nullptr);
	ASSERT_NE(HintOf(hints, GestureType::Circle), nullptr);
	EXPECT_EQ(HintOf(hints, GestureType::Circle)->order, k_SizeCircleOrder);
	EXPECT_EQ(HintOf(hints, GestureType::Circle)->seed, SpellSeedType::None);
	const auto* first = HintOf(hints, GestureType::InverseSpiral);
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(first->order, k_PowerUpOrderBase);
	EXPECT_EQ(first->seed, SpellSeedType::Storm);
	EXPECT_EQ(first->powerUp, 0);
	EXPECT_EQ(first->magic, FakeMagic(SpellSeedType::Storm, 0));
	EXPECT_EQ(HintOf(hints, GestureType::Spiral)->order, k_PowerUpOrderBase + 1);

	const std::vector<gesture::Request> asked {
	    {.gesture = GestureType::Scribble,
	     .purpose = gesture::Purpose::DropSeed,
	     .powerUpLevel = 0,
	     .seed = SpellSeedType::Storm},
	    {.gesture = GestureType::Spiral, .purpose = gesture::Purpose::PowerUp, .powerUpLevel = 1, .seed = SpellSeedType::Storm},
	};
	hints = HintsFor(asked, FakeShapes, FakeMagic);
	const auto* cancel = HintOf(hints, GestureType::Scribble);
	ASSERT_NE(cancel, nullptr);
	EXPECT_EQ(cancel->order, k_CancelPowerUpOrder);
	EXPECT_EQ(cancel->seed, SpellSeedType::Storm);
	// The scribble's seed has no rings
	EXPECT_EQ(cancel->powerUp, -1);
}

TEST(GestureHints, ASeedPictureSpinsAndWrapsItsTurns)
{
	SeedPicture picture {.seed = SpellSeedType::Water};
	StepPicture(picture, 0.5f);
	EXPECT_NEAR(picture.spin, 1.0f, k_Tolerance);
	// Without a power-up, the rings do not spin
	EXPECT_EQ(picture.ringSpin, 0.0f);
	StepPicture(picture, 3.0f);
	EXPECT_NEAR(picture.spin, 7.0f - glm::two_pi<float>(), k_Tolerance);
	SeedPicture ringed {.seed = SpellSeedType::Storm, .powerUp = 1};
	StepPicture(ringed, 0.1f);
	EXPECT_NEAR(ringed.ringSpin, 1.03f, k_Tolerance);
}

TEST(GestureHints, TwoHintsOfOneMiracleStepItsPictureTwice)
{
	Row row;
	const auto path = std::vector<glm::vec3> {{0.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.5f}};
	row.hints = {
	    Hint {.gesture = GestureType::Spiral, .seed = SpellSeedType::Storm, .magic = MagicType::Fireball, .path = path},
	    Hint {.gesture = GestureType::Scribble, .seed = SpellSeedType::Storm, .magic = MagicType::Fireball, .path = path},
	    Hint {.gesture = GestureType::Circle, .seed = SpellSeedType::Water, .magic = MagicType::Water, .path = path}};
	row.seeds.emplace(MagicType::Fireball, SeedPicture {.seed = SpellSeedType::Storm});
	Step(row, 0.25f, false);
	EXPECT_NEAR(row.seeds.at(MagicType::Fireball).spin, 1.0f, k_Tolerance);
	// A miracle without a picture gets none from the step
	EXPECT_FALSE(row.seeds.contains(MagicType::Water));
	// Faded out, nothing turns
	row.alpha = 0.0f;
	Step(row, 0.25f, true);
	EXPECT_NEAR(row.seeds.at(MagicType::Fireball).spin, 1.0f, k_Tolerance);
}

TEST(GestureHints, TheSeedAndTheGlobeSitWhereTheCollarsCameraLooks)
{
	EXPECT_EQ(SeedMiddle(), k_CollarAt);
	EXPECT_NEAR(HolderPoint(2.0f).y, 0.24f, k_Tolerance);
	EXPECT_EQ(HolderPoint(2.0f).z, k_CollarAt.z);
	const auto globe = GlobeModel();
	EXPECT_EQ(globe[3], glm::vec4(k_CollarAt, 1.0f));
	EXPECT_FLOAT_EQ(globe[0].x, k_GlobeScale);
	// The globe's up axis points to the camera, its depth axis up
	EXPECT_NEAR(globe[1].z, -k_GlobeScale, 1e-7f);
	EXPECT_NEAR(globe[1].y, 0.0f, 1e-7f);
	EXPECT_NEAR(globe[2].y, k_GlobeScale, 1e-7f);
	EXPECT_NEAR(globe[2].z, 0.0f, 1e-7f);
}

TEST(GestureHints, ThePicturesFadeWithTheRowAndTheRingsAreFainter)
{
	EXPECT_EQ(PictureAlpha(1.0f), 255);
	EXPECT_EQ(PictureAlpha(0.5f), 127);
	EXPECT_EQ(PictureAlpha(0.0f), 0);
	EXPECT_EQ(SeedRingAlpha(255), 119);
	EXPECT_EQ(SeedRingAlpha(0), 0);
}
