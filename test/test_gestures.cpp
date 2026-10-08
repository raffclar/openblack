/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "InfoConstants.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Magic/Gestures/GestureTemplates.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "Resources/Loaders.h"

using namespace openblack;
using namespace openblack::magic::gestures;

namespace
{
constexpr float k_Ratio = 4.0f / 3.0f;

/// Mouse samples along a polyline, `step` pixels apart (a sample per mouse-move message)
void FeedPolyline(GestureSystem& system, const std::vector<glm::vec2>& points, float step)
{
	system.Clear();
	glm::vec2 at = points.front();
	system.AddSample(glm::vec3(at.x, 0.0f, at.y), glm::ivec2(glm::round(at)));
	for (size_t k = 1; k < points.size(); ++k)
	{
		const glm::vec2 to = points[k];
		while (glm::distance(at, to) > step)
		{
			at += glm::normalize(to - at) * step;
			system.AddSample(glm::vec3(at.x, 0.0f, at.y), glm::ivec2(glm::round(at)));
		}
	}
	at = points.back();
	system.AddSample(glm::vec3(at.x, 0.0f, at.y), glm::ivec2(glm::round(at)));
}

/// A template drawn back on a screen: its normalised keypoints x 200 px (z divided by the screen ratio, as the
/// template tool's normaliser multiplied it), mirrored left-right if asked
std::vector<glm::vec2> StrokeOf(const GestureData& tpl, bool mirror)
{
	std::vector<glm::vec2> points;
	for (int k = 0; k < tpl.count; ++k)
	{
		const float x = tpl.samples[k].x * 200.0f;
		points.emplace_back(100.0f + (mirror ? 200.0f - x : x), 100.0f + tpl.samples[k].z * 200.0f / k_Ratio);
	}
	return points;
}

float Length(const std::vector<glm::vec2>& points)
{
	float length = 0.0f;
	for (size_t k = 1; k < points.size(); ++k)
	{
		length += glm::distance(points[k - 1], points[k]);
	}
	return length;
}

/// A stroke spread over about 60 messages (the buffer keeps 80)
bool Recognise(const std::vector<GestureData>& list, const std::vector<glm::vec2>& stroke, Gesture gesture, Result& result)
{
	GestureSystem system;
	FeedPolyline(system, stroke, std::max(5.0f, Length(stroke) / 60.0f));
	const auto input = BuildFromSystem(system, k_Ratio);
	return MatchGesture(list, gesture, input, result, k_Ratio);
}

/// A synthetic template from headings: keypoints at the corners of a unit polyline
GestureData TemplateOf(Gesture gesture, const std::vector<glm::vec2>& corners, bool allowReverse)
{
	GestureData tpl;
	tpl.SetToZero();
	tpl.gesture = gesture;
	tpl.positionMode = 2;
	tpl.checkDirection = true;
	tpl.allowReverse = allowReverse;
	float previous = 0.0f;
	for (size_t k = 0; k < corners.size(); ++k)
	{
		KeySample s;
		s.x = corners[k].x;
		s.z = corners[k].y;
		if (k + 1 < corners.size())
		{
			const auto d = corners[k + 1] - corners[k];
			const float heading = Atan2Positive(d.x, d.y);
			s.direction = Octant(heading);
			s.turn = k == 0 ? 0.0f : WrapDifference(previous, heading);
			previous = heading;
		}
		tpl.Append(s);
	}
	return tpl;
}

std::unique_ptr<InfoConstants> LoadInfo(const char* game)
{
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	return info;
}

std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

/// The octants on the screen (y down) and the rounding of an exact .5
TEST(Gestures, octant)
{
	EXPECT_EQ(Octant(Atan2Positive(0.0f, -1.0f)), 0u); // up
	EXPECT_EQ(Octant(Atan2Positive(1.0f, 0.0f)), 2u);  // right
	EXPECT_EQ(Octant(Atan2Positive(0.0f, 1.0f)), 4u);  // down
	EXPECT_EQ(Octant(Atan2Positive(-1.0f, 0.0f)), 6u); // left
	EXPECT_EQ(Octant(Atan2Positive(1.0f, -1.0f)), 1u); // up-right
	EXPECT_EQ(RoundHalfDown(0.5f), 0);
	EXPECT_EQ(RoundHalfDown(1.5f), 1);
	EXPECT_EQ(RoundHalfDown(2.5f), 2);
	EXPECT_EQ(RoundHalfDown(0.50001f), 1);
	EXPECT_EQ(RoundHalfDown(7.49f), 7);
	// the turn wraps: +2pi-ish differences come back into (-pi, pi]
	EXPECT_NEAR(WrapDifference(6.0f, 0.2f), 0.2f + glm::two_pi<float>() - 6.0f, 1e-5f);
	EXPECT_NEAR(WrapDifference(0.0f, glm::pi<float>()), glm::pi<float>(), 1e-6f);
}

/// The online corner extraction: a square drawn clockwise gives the start, three corners and the end, with turns of
/// +pi/2 and the right first direction (74 samples: the buffer keeps the last 80, an older start would be lost)
TEST(Gestures, cornersOfASquare)
{
	GestureSystem system;
	FeedPolyline(system, {{100, 100}, {250, 100}, {250, 250}, {100, 250}, {100, 110}}, 8.0f);
	ASSERT_LE(system.Count(), 79);
	const auto data = BuildFromSystem(system, k_Ratio);
	ASSERT_EQ(data.count, 5);
	EXPECT_EQ(data.samples[0].direction, 2u);
	for (int k = 1; k <= 3; ++k)
	{
		EXPECT_NEAR(data.samples[k].turn, glm::half_pi<float>(), 0.15f) << k;
	}
	EXPECT_EQ(data.samples[1].direction, 4u);
	EXPECT_EQ(data.samples[2].direction, 6u);
	EXPECT_EQ(data.samples[3].direction, 0u);
}

/// 70 samples on the same pixel as the one two messages before wipe the buffer (compared with head - 2), and
/// that sample starts it again: the 72nd still sample wipes it
TEST(Gestures, stationaryWipe)
{
	GestureSystem system;
	for (int k = 0; k < 20; ++k)
	{
		system.AddSample(glm::vec3(0.0f), {10 * k, 0});
	}
	EXPECT_EQ(system.Count(), 20);
	for (int k = 0; k < 71; ++k)
	{
		system.AddSample(glm::vec3(0.0f), {500, 0});
	}
	EXPECT_EQ(system.Count(), 80);
	system.AddSample(glm::vec3(0.0f), {500, 0});
	EXPECT_EQ(system.Count(), 1);
}

/// A synthetic square template recognises its own stroke, not a triangle's; a mirrored square only with allowReverse
TEST(Gestures, syntheticTemplates)
{
	std::vector<GestureData> list;
	list.push_back(TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}, true));
	list.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}, false));
	Result result;
	const std::vector<glm::vec2> square {{100, 100}, {300, 100}, {300, 300}, {100, 300}, {100, 110}};
	EXPECT_TRUE(Recognise(list, square, k_Circle, result));
	EXPECT_FALSE(result.reversed);
	EXPECT_FALSE(Recognise(list, square, k_Spiral, result));
	const std::vector<glm::vec2> triangle {{100, 300}, {200, 100}, {300, 300}};
	EXPECT_TRUE(Recognise(list, triangle, k_Spiral, result));
	EXPECT_FALSE(Recognise(list, triangle, k_Circle, result));
	// the square anticlockwise: left, down, right, up
	const std::vector<glm::vec2> mirrored {{300, 100}, {100, 100}, {100, 300}, {300, 300}, {300, 110}};
	EXPECT_TRUE(Recognise(list, mirrored, k_Circle, result));
	EXPECT_TRUE(result.reversed);
	list[0].allowReverse = false;
	EXPECT_FALSE(Recognise(list, mirrored, k_Circle, result));
}

/// The selection path with a fake worship icon: the spiral opens the selection only while
/// an icon of that category is requestable, the miracle's gesture then asks for its seed
TEST(Gestures, selectionWithFakeIcon)
{
	std::vector<GestureData> list;
	list.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}, false));
	list.push_back(TemplateOf(9 /*FORK_RIGHT*/, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}, false));
	struct FakeIcons final: IconProvider
	{
		bool available = false;
		int requested = -1;
		[[nodiscard]] bool AnyRequestableIconOfCategory(Gesture category) const override
		{
			return available && category == k_Spiral;
		}
		void ForEachRequestableIcon(Gesture category, const std::function<void(int seedType)>& visit) const override
		{
			if (available && category == k_Spiral)
			{
				visit(4); // SHIELD's row: selection SPIRAL, gesture FORK_RIGHT (9)
			}
		}
		[[nodiscard]] bool IconValidForRequest(int /*seedType*/) const override { return available; }
		void RequestSpell(int seedType) override { requested = seedType; }
	} icons;
	SelectionTables tables;
	tables.selectionGesture = [](int seed) { return seed == 4 ? k_Spiral : k_None; };
	tables.gesture = [](int seed) { return seed == 4 ? Gesture(9) : k_None; };
	tables.gestureStage2 = [](int /*seed*/) { return k_None; };

	Selection selection;
	Result result;
	GestureSystem system;
	auto recognise = [&](Gesture g) {
		const auto input = BuildFromSystem(system, k_Ratio);
		return MatchGesture(list, g, input, result, k_Ratio);
	};
	FeedPolyline(system, {{100, 300}, {200, 100}, {300, 300}}, 8.0f);
	EXPECT_FALSE(selection.Open(k_Spiral, icons, tables)) << "no icon, no selection";
	icons.available = true;
	ASSERT_TRUE(recognise(k_Spiral));
	ASSERT_TRUE(selection.Open(k_Spiral, icons, tables));
	EXPECT_TRUE(selection.open);
	EXPECT_EQ(selection.stage, 1u);
	FeedPolyline(system, {{100, 100}, {250, 100}, {250, 250}, {100, 250}, {100, 110}}, 8.0f);
	const auto outcome = selection.Stage(0.1f, 30.0f, recognise, icons);
	EXPECT_EQ(outcome, Selection::Outcome::Requested);
	EXPECT_EQ(icons.requested, 4);
	EXPECT_FALSE(selection.open);
}

/// The real Data\Gestures.jty and info.dat (OPENBLACK_GAME_PATH): 81 templates of 1628 bytes; each player miracle's
/// stroke is its own gesture and none of the other player miracles' gestures; CIRCLE and STAR drawn mirrored match with
/// reversed = 1
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Gestures, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto bytes = ReadFile(std::filesystem::path(game) / "Data" / "Gestures.jty");
	ASSERT_EQ(bytes.size(), 4u + 81u * 1628u);
	std::vector<GestureData> list;
	ASSERT_TRUE(LoadTemplates(bytes, list));
	ASSERT_EQ(list.size(), 81u);
	EXPECT_EQ(list[0].gesture, k_Spiral);
	EXPECT_EQ(list[0].count, 26);
	EXPECT_EQ(list[5].gesture, k_Circle);
	EXPECT_TRUE(list[5].allowReverse);
	for (const auto& tpl : list)
	{
		EXPECT_EQ(tpl.positionMode, 2);
	}
	const auto info = LoadInfo(game);
	ASSERT_NE(info, nullptr);
	// the player miracles' gestures (selection SPIRAL)
	std::set<Gesture> playerGestures;
	for (size_t s = 0; s < magic::k_SpellSeedCount; ++s)
	{
		const auto& seed = info->spellSeed[s];
		if (static_cast<Gesture>(seed.selectionGesture) == k_Spiral && seed.gesture != GestureType::None)
		{
			playerGestures.insert(static_cast<Gesture>(seed.gesture));
		}
	}
	std::string names;
	for (const auto g : playerGestures)
	{
		names += " " + std::to_string(g);
	}
	EXPECT_EQ(playerGestures.size(), 14u) << names;
	Result result;
	for (const auto gesture : playerGestures)
	{
		// the first template of that gesture, drawn
		const auto it = std::find_if(list.begin(), list.end(), [gesture](const auto& t) { return t.gesture == gesture; });
		ASSERT_NE(it, list.end());
		const auto stroke = StrokeOf(*it, false);
		EXPECT_TRUE(Recognise(list, stroke, gesture, result)) << "gesture " << int(gesture);
		for (const auto other : playerGestures)
		{
			if (other != gesture)
			{
				EXPECT_FALSE(Recognise(list, stroke, other, result))
				    << "gesture " << int(gesture) << " also matches " << int(other);
			}
		}
	}
	// mirrored strokes: the templates with allowReverse
	for (const Gesture gesture : {k_Circle, Gesture(8) /*STAR*/})
	{
		const auto it = std::find_if(list.begin(), list.end(), [gesture](const auto& t) { return t.gesture == gesture; });
		ASSERT_NE(it, list.end());
		EXPECT_TRUE(Recognise(list, StrokeOf(*it, true), gesture, result)) << "mirrored " << int(gesture);
		EXPECT_TRUE(result.reversed) << int(gesture);
	}
}

namespace
{
constexpr size_t k_JtySampleBytes = 20;
constexpr size_t k_JtyRecordBytes = 1628; // 80 samples, then six u32 fields and the aspect

/// The bytes of a Gestures.jty file holding `templates`: a u32 count, then one record each
std::vector<uint8_t> EncodeTemplates(const std::vector<GestureData>& templates)
{
	std::vector<uint8_t> bytes(4 + templates.size() * k_JtyRecordBytes, 0);
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(size_t {0}, static_cast<uint32_t>(templates.size()));
	for (size_t i = 0; i < templates.size(); ++i)
	{
		const auto& tpl = templates[i];
		const size_t base = 4 + i * k_JtyRecordBytes;
		for (size_t s = 0; s < k_MaxSamples; ++s)
		{
			const auto& sample = tpl.samples[s];
			const size_t at = base + s * k_JtySampleBytes;
			put(at, sample.x);
			put(at + 4, sample.y);
			put(at + 8, sample.z);
			put(at + 12, sample.turn);
			put(at + 16, sample.direction);
		}
		const size_t tail = base + k_MaxSamples * k_JtySampleBytes;
		put(tail, static_cast<uint32_t>(tpl.count));
		put(tail + 4, static_cast<uint32_t>(tpl.gesture));
		put(tail + 8, static_cast<uint32_t>(tpl.positionMode));
		put(tail + 12, static_cast<uint32_t>(tpl.checkDirection ? 1 : 0));
		put(tail + 16, static_cast<uint32_t>(tpl.allowReverse ? 1 : 0));
		put(tail + 20, static_cast<uint32_t>(tpl.checkAspect ? 1 : 0));
		put(tail + 24, tpl.aspect);
	}
	return bytes;
}
} // namespace

/// The template loader on a small in-memory Gestures.jty: every field survives the round trip, only the low byte of
/// the byte-sized fields is kept, short data is refused, and the loaded templates recognise strokes as the originals do
TEST(Gestures, realDataSynthetic)
{
	std::vector<GestureData> source;
	source.push_back(TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}, true));
	source.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}, false));
	// a third template with the remaining flags set; no stroke below asks for its gesture
	auto shape = TemplateOf(k_RShape, {{0, 1}, {0, 0}, {1, 0.5f}}, false);
	shape.checkDirection = false;
	shape.checkAspect = true;
	shape.aspect = 1.5f;
	source.push_back(shape);

	auto bytes = EncodeTemplates(source);
	ASSERT_EQ(bytes.size(), 4u + 3u * k_JtyRecordBytes);
	// the file stores the gesture as a u32: set a high byte on the third record, which the loader drops
	const size_t gestureAt = 4 + 2 * k_JtyRecordBytes + k_MaxSamples * k_JtySampleBytes + 4;
	const uint32_t gestureWithHighByte = 256u + k_RShape;
	std::memcpy(bytes.data() + gestureAt, &gestureWithHighByte, sizeof(gestureWithHighByte));

	std::vector<GestureData> list;
	ASSERT_TRUE(LoadTemplates(bytes, list));
	ASSERT_EQ(list.size(), source.size());
	for (size_t t = 0; t < source.size(); ++t)
	{
		EXPECT_EQ(list[t].count, source[t].count) << t;
		EXPECT_EQ(list[t].gesture, source[t].gesture) << t;
		EXPECT_EQ(list[t].positionMode, 2) << t;
		EXPECT_EQ(list[t].checkDirection, source[t].checkDirection) << t;
		EXPECT_EQ(list[t].allowReverse, source[t].allowReverse) << t;
		EXPECT_EQ(list[t].checkAspect, source[t].checkAspect) << t;
		EXPECT_FLOAT_EQ(list[t].aspect, source[t].aspect) << t;
		for (size_t s = 0; s < k_MaxSamples; ++s)
		{
			EXPECT_FLOAT_EQ(list[t].samples[s].x, source[t].samples[s].x) << t << " " << s;
			EXPECT_FLOAT_EQ(list[t].samples[s].y, source[t].samples[s].y) << t << " " << s;
			EXPECT_FLOAT_EQ(list[t].samples[s].z, source[t].samples[s].z) << t << " " << s;
			EXPECT_FLOAT_EQ(list[t].samples[s].turn, source[t].samples[s].turn) << t << " " << s;
			EXPECT_EQ(list[t].samples[s].direction, source[t].samples[s].direction) << t << " " << s;
		}
	}
	EXPECT_EQ(list[0].count, 5);
	EXPECT_TRUE(list[0].allowReverse);
	EXPECT_EQ(list[2].gesture, k_RShape);
	EXPECT_TRUE(list[2].checkAspect);

	// the loaded list recognises the same strokes as the templates it was written from
	Result result;
	const std::vector<glm::vec2> square {{100, 100}, {300, 100}, {300, 300}, {100, 300}, {100, 110}};
	EXPECT_TRUE(Recognise(list, square, k_Circle, result));
	EXPECT_FALSE(result.reversed);
	EXPECT_FALSE(Recognise(list, square, k_Spiral, result));
	const std::vector<glm::vec2> triangle {{100, 300}, {200, 100}, {300, 300}};
	EXPECT_TRUE(Recognise(list, triangle, k_Spiral, result));
	EXPECT_FALSE(Recognise(list, triangle, k_Circle, result));
	const std::vector<glm::vec2> mirrored {{300, 100}, {100, 100}, {100, 300}, {300, 300}, {300, 110}};
	EXPECT_TRUE(Recognise(list, mirrored, k_Circle, result));
	EXPECT_TRUE(result.reversed);

	// short data: under the count, and a count that promises more records than there are
	EXPECT_FALSE(LoadTemplates(std::vector<uint8_t>(3, 0), list));
	EXPECT_TRUE(list.empty());
	bytes.resize(bytes.size() - 1);
	EXPECT_FALSE(LoadTemplates(bytes, list));
	EXPECT_TRUE(list.empty());
	// an empty file is a valid empty list
	EXPECT_TRUE(LoadTemplates(EncodeTemplates({}), list));
	EXPECT_TRUE(list.empty());
}

// The resource cache loaders of the gesture assets, on bytes written by the test

TEST(GestureTemplatesLoader, readsTheRecordsOfTheBytes)
{
	std::vector<GestureData> source;
	source.push_back(TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}}, true));
	source.push_back(TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}}, false));
	const auto list =
	    resources::GestureTemplatesLoader {}(resources::GestureTemplatesLoader::FromBufferTag {}, EncodeTemplates(source));
	ASSERT_NE(list, nullptr);
	ASSERT_EQ(list->size(), 2u);
	EXPECT_EQ((*list)[0].gesture, k_Circle);
	EXPECT_EQ((*list)[0].count, 3);
	EXPECT_TRUE((*list)[0].allowReverse);
	EXPECT_EQ((*list)[1].gesture, k_Spiral);
	EXPECT_EQ((*list)[1].count, 2);
}

TEST(GestureTemplatesLoader, shortBytesGiveAnEmptyList)
{
	const resources::GestureTemplatesLoader loader;
	const auto list = loader(resources::GestureTemplatesLoader::FromBufferTag {}, std::vector<uint8_t>(3, 0));
	ASSERT_NE(list, nullptr);
	EXPECT_TRUE(list->empty());
}

namespace
{
/// A .cam file: u32 file size, u32 (unused), u32 count, then count points of 6 floats (x, y, z, and 3 unused)
std::vector<uint8_t> EncodeShape(const std::vector<glm::vec3>& points)
{
	std::vector<uint8_t> bytes(12 + points.size() * 24, 0);
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(0, static_cast<uint32_t>(bytes.size()));
	put(8, static_cast<uint32_t>(points.size()));
	for (size_t i = 0; i < points.size(); ++i)
	{
		put(12 + i * 24, points[i].x);
		put(12 + i * 24 + 4, points[i].y);
		put(12 + i * 24 + 8, points[i].z);
	}
	return bytes;
}
} // namespace

TEST(GestureShapeLoader, readsAShapeOfTheBytes)
{
	// x (-100..100) -> (x + 100) / 200 and z -> (100 - z) / 200: a square from (0, 1) to (1, 0)
	const auto shape = resources::GestureShapeLoader {}(
	    resources::GestureShapeLoader::FromBufferTag {},
	    EncodeShape({{-100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, -100.0f}, {-100.0f, 0.0f, -100.0f}}));
	ASSERT_NE(shape, nullptr);
	EXPECT_EQ(shape->points.size(), 4u);
	EXPECT_FLOAT_EQ(shape->minX, 0.0f);
	EXPECT_FLOAT_EQ(shape->maxX, 1.0f);
	EXPECT_FLOAT_EQ(shape->minZ, 0.0f);
	EXPECT_FLOAT_EQ(shape->maxZ, 1.0f);
	EXPECT_FLOAT_EQ(shape->aspect, 1.0f);
	EXPECT_FLOAT_EQ(shape->length, 3.0f);
}

TEST(GestureShapeLoader, bytesThatAreNotAShapeGiveNull)
{
	const resources::GestureShapeLoader loader;
	EXPECT_EQ(loader(resources::GestureShapeLoader::FromBufferTag {}, std::vector<uint8_t>(11, 0)), nullptr);
	// a count of 0
	EXPECT_EQ(loader(resources::GestureShapeLoader::FromBufferTag {}, EncodeShape({})), nullptr);
	// a count that promises more points than there are
	auto bytes = EncodeShape({{0.0f, 0.0f, 0.0f}});
	bytes.resize(bytes.size() - 1);
	EXPECT_EQ(loader(resources::GestureShapeLoader::FromBufferTag {}, bytes), nullptr);
}
