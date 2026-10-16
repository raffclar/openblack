/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <utility>
#include <vector>

#include <IFRFile.h>
#include <gtest/gtest.h>

using namespace openblack::ifr;

namespace
{

using Keys = std::vector<std::pair<std::string, std::string>>;

void PutU32(std::vector<uint8_t>& out, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		out.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutString(std::vector<uint8_t>& out, const std::string& text)
{
	out.insert(out.end(), text.begin(), text.end());
	out.push_back(0);
}

/// One object as the game's files keep it: the ID in the first group of keys, the other keys after it
std::vector<uint8_t> MakeObject(const std::string& name, const std::string& type, const std::string& id, const Keys& keys)
{
	std::vector<uint8_t> body;
	PutString(body, name);
	PutString(body, type);
	PutString(body, "");
	std::vector<uint8_t> first;
	PutString(first, "ID");
	PutString(first, id);
	PutU32(body, static_cast<uint32_t>(first.size() + 4));
	body.insert(body.end(), first.begin(), first.end());
	for (const auto& [key, value] : keys)
	{
		PutString(body, key);
		PutString(body, value);
	}
	std::vector<uint8_t> object;
	PutU32(object, static_cast<uint32_t>(body.size() + 4));
	object.insert(object.end(), body.begin(), body.end());
	return object;
}

std::vector<uint8_t> MakeFile(const std::vector<std::vector<uint8_t>>& objects)
{
	std::vector<uint8_t> file {'i', 'f', 'p', 'r'};
	PutU32(file, static_cast<uint32_t>(objects.size()));
	for (const auto& object : objects)
	{
		file.insert(file.end(), object.begin(), object.end());
	}
	return file;
}

} // namespace

TEST(IFRFile, ReadsTheObjectsWithTheirKeys)
{
	const auto data = MakeFile({
	    MakeObject("Slap", "Periodic", "{A}", {{"Magnitude", "7209"}, {"Waveform", "1"}}),
	    MakeObject("Both", "Compound", "{C}", {{"ContainedObjects", "{A};{B}"}}),
	    MakeObject("Bumps", "Texture", "{B}", {{"Axes", "1"}}),
	});
	IFRFile file;
	ASSERT_EQ(file.Open(data), IFRResult::Success);
	ASSERT_EQ(file.GetObjects().size(), 3);
	const auto* slap = file.Find("Slap");
	ASSERT_NE(slap, nullptr);
	EXPECT_EQ(slap->type, "Periodic");
	EXPECT_EQ(slap->Find("ID"), "{A}");
	EXPECT_EQ(slap->Integer("Magnitude", 0), 7209);
	EXPECT_EQ(slap->Integer("Period", 42), 42);
	EXPECT_EQ(file.FindById("{B}"), file.Find("Bumps"));

	// A compound plays its effects in the order that it names them; a single effect plays itself
	const auto effects = file.Effects(*file.Find("Both"));
	ASSERT_EQ(effects.size(), 2);
	EXPECT_EQ(effects[0]->name, "Slap");
	EXPECT_EQ(effects[1]->name, "Bumps");
	ASSERT_EQ(file.Effects(*slap).size(), 1);
}

TEST(IFRFile, RefusesBadFiles)
{
	IFRFile file;
	EXPECT_EQ(file.Open(std::vector<uint8_t> {'i', 'f'}), IFRResult::ErrTooSmall);
	EXPECT_EQ(file.Open(std::vector<uint8_t> {'n', 'o', 'p', 'e', 0, 0, 0, 0}), IFRResult::ErrBadMagic);
	auto data = MakeFile({MakeObject("Slap", "Periodic", "{A}", {})});
	data.resize(data.size() - 3);
	EXPECT_EQ(file.Open(data), IFRResult::ErrBadObject);
}

TEST(IFRFile, PeriodicTakesTheGameValuesForMissingKeys)
{
	const Object object {.name = "Plain", .type = "Periodic", .properties = {}};
	const auto periodic = ReadPeriodic(object);
	EXPECT_EQ(periodic.waveform, Waveform::Sine);
	EXPECT_EQ(periodic.magnitude, 5000);
	EXPECT_EQ(periodic.period, 1000000);
	EXPECT_EQ(periodic.timing.iterations, 1);
	EXPECT_EQ(periodic.timing.gain, 10000);
	EXPECT_FALSE(periodic.timing.duration.has_value());
	// Without the keys, the effect starts from nothing in a tenth of a second; it plays until stopped, so it has no end
	ASSERT_TRUE(periodic.envelope.has_value());
	EXPECT_EQ(periodic.envelope->attackLevel, 0);
	EXPECT_EQ(periodic.envelope->attackTime, 100000);
	EXPECT_EQ(periodic.envelope->fadeLevel, 5000);
	EXPECT_EQ(periodic.envelope->fadeTime, 0);
}

TEST(IFRFile, PeriodicEnvelopeOnlyWhenTheStrengthChanges)
{
	// The slap of the game's files: full strength from the start, and no fade
	const Object slap {.name = "CreatureSlap",
	                   .type = "Periodic",
	                   .properties = {{"Magnitude", "7209"},
	                                  {"Period", "9343"},
	                                  {"Waveform", "1"},
	                                  {"Duration", "20000"},
	                                  {"FadeDuration", "0"},
	                                  {"AttackLevel", "10000"},
	                                  {"InfiniteDuration", "0"},
	                                  {"AttackDuration", "0"}}};
	const auto periodic = ReadPeriodic(slap);
	EXPECT_EQ(periodic.waveform, Waveform::Square);
	EXPECT_EQ(periodic.timing.duration, 20000u);
	EXPECT_FALSE(periodic.envelope.has_value());

	const Object fades {.name = "Fades", .type = "Periodic", .properties = {{"Duration", "1000"}, {"AttackDuration", "0"}}};
	ASSERT_TRUE(ReadPeriodic(fades).envelope.has_value());
	EXPECT_EQ(ReadPeriodic(fades).envelope->fadeTime, 100000);
}

TEST(IFRFile, TextureAxesAndBumps)
{
	const Object fur {.name = "Fur",
	                  .type = "Texture",
	                  .properties = {{"Axes", "1"}, {"Offset", "512"}, {"PosMagnitude", "460"}, {"PosBumpSpace2", "7"}}};
	const auto texture = ReadTexture(fur);
	EXPECT_EQ(texture.axes, Axes::Y);
	EXPECT_EQ(texture.first.offset, 512);
	EXPECT_EQ(texture.first.positiveMagnitude, 460);
	EXPECT_EQ(texture.first.negativeMagnitude, 10000);
	EXPECT_EQ(texture.first.positiveWidth, 5);
	EXPECT_EQ(texture.first.positiveSpace, 20);
	EXPECT_EQ(texture.second.positiveSpace, 7);
	EXPECT_FALSE(texture.timing.duration.has_value());

	const Object both {.name = "Both", .type = "Texture", .properties = {{"Axes", "2"}}};
	EXPECT_EQ(ReadTexture(both).axes, Axes::Both);
	const Object none {.name = "X", .type = "Texture", .properties = {}};
	EXPECT_EQ(ReadTexture(none).axes, Axes::X);
}

TEST(IFRFile, ConditionValues)
{
	const Object damper {.name = "Grip", .type = "Damper", .properties = {{"Axes", "2"}, {"DeadBand", "25"}}};
	const auto condition = ReadCondition(damper);
	EXPECT_EQ(condition.type, "Damper");
	EXPECT_EQ(condition.axes, Axes::Both);
	EXPECT_EQ(condition.first.deadBand, 25);
	EXPECT_EQ(condition.first.positiveCoefficient, 10000);
	EXPECT_EQ(condition.second.deadBand, 0);
}
