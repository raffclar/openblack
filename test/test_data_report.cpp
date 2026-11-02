/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The text of datatool, which lists the game's smaller files. The files here are made up in the game's formats.

#include <cstdint>
#include <cstring>

#include <string>
#include <vector>

#include <DataReport.h>
#include <SaveGameInfo.h>
#include <VanillaSaveFile.h>
#include <gtest/gtest.h>

#include "Common/PlayerProfiles.h"
#include "gui/FakeGameFont.h"

using namespace openblack;
using namespace openblack::datatool;

namespace
{
bool Contains(const std::string& text, const std::string& part)
{
	return text.find(part) != std::string::npos;
}

void Append32(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

/// The compressed form of a particle file: the length of the text, then a zlib stream of one stored block
std::vector<uint8_t> Compressed(const std::string& text)
{
	std::vector<uint8_t> bytes;
	Append32(bytes, static_cast<uint32_t>(text.size()));
	bytes.insert(bytes.end(), {0x78, 0x01, 0x01});
	const auto length = static_cast<uint16_t>(text.size());
	bytes.push_back(static_cast<uint8_t>(length & 0xFF));
	bytes.push_back(static_cast<uint8_t>(length >> 8));
	bytes.push_back(static_cast<uint8_t>(~length & 0xFF));
	bytes.push_back(static_cast<uint8_t>((~length >> 8) & 0xFF));
	uint32_t a = 1;
	uint32_t b = 0;
	for (const char c : text)
	{
		bytes.push_back(static_cast<uint8_t>(c));
		a = (a + static_cast<uint8_t>(c)) % 65521;
		b = (b + a) % 65521;
	}
	const uint32_t sum = (b << 16) | a;
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		bytes.push_back(static_cast<uint8_t>(sum >> shift));
	}
	return bytes;
}

const std::string k_Particles = "BEGINPROPERTIES\n"
                                "PROPERTY MaxSpellAge FLOAT -1\n"
                                "ENDPROPERTIES\n"
                                "BEGINCLASS DiskEmitter DiskEmitter0\n"
                                "BEGINPROPERTIES\n"
                                "PROPERTY EmissionFreq FLOAT 3\n"
                                "PROPERTY PCreator PERSIS_PNTR ParticleSpriteCreator0\n"
                                "ENDPROPERTIES\n"
                                "ENDCLASS\n";
} // namespace

TEST(DataReport, GestureTemplatesAreListedWithTheirFields)
{
	gestures::GestureFile file;
	gestures::GestureTemplate entry;
	entry.pointCountField = 2;
	entry.gestureField = 4;
	entry.positionModeField = 2;
	entry.allowMirror = 1;
	entry.points.at(0) = {.x = 0.0f, .y = 0.0f, .z = 0.5f, .turn = 0.0f, .direction = 2};
	entry.points.at(1) = {.x = 1.0f, .y = 0.0f, .z = 0.5f, .turn = 0.0f, .direction = 2};
	file.AddTemplate(entry);
	const auto text = DescribeGestures(file);
	EXPECT_TRUE(Contains(text, "1 templates"));
	EXPECT_TRUE(Contains(text, "gesture 4, 2 points, position mode 2"));
	EXPECT_TRUE(Contains(text, "mirror allowed"));
	EXPECT_TRUE(Contains(text, "(1.000, 0.500)"));
}

TEST(DataReport, ParticleTextComesLooseOrInflated)
{
	const std::vector<uint8_t> loose(k_Particles.begin(), k_Particles.end());
	EXPECT_EQ(ParticleText(loose, false), k_Particles);
	EXPECT_EQ(ParticleText(Compressed(k_Particles), true), k_Particles);
	EXPECT_FALSE(ParticleText(std::vector<uint8_t> {1, 2}, true).has_value());
	// A stream that does not inflate is refused, and does not throw
	auto damaged = Compressed(k_Particles);
	damaged[4] = 0xFF;
	EXPECT_FALSE(ParticleText(damaged, true).has_value());
}

TEST(DataReport, ParticleFileListsItsClassesAndProperties)
{
	const auto file = psys::ParticleFile::Parse(k_Particles);
	ASSERT_TRUE(file.has_value());
	const auto brief = DescribeParticleFile(*file, false);
	EXPECT_TRUE(Contains(brief, "1 objects"));
	EXPECT_TRUE(Contains(brief, "DiskEmitter DiskEmitter0: 2 properties"));
	EXPECT_FALSE(Contains(brief, "EmissionFreq"));
	const auto full = DescribeParticleFile(*file, true);
	EXPECT_TRUE(Contains(full, "EmissionFreq FLOAT 3"));
	EXPECT_TRUE(Contains(full, "PCreator PERSIS_PNTR ParticleSpriteCreator0"));
}

TEST(DataReport, RawImagesAreSummedUpByChannel)
{
	const std::vector<uint8_t> bytes {10, 20, 30, 30, 40, 50, 0, 0, 0, 255, 255, 255};
	const auto rgb = rawimage::DecodeRgb(bytes, 2, 2);
	ASSERT_TRUE(rgb.has_value());
	const auto text = DescribeRgbImage(*rgb);
	EXPECT_TRUE(Contains(text, "2 x 2 pixels, 3 bytes a pixel"));
	EXPECT_TRUE(Contains(text, "red: lowest 0, highest 255, mean 73.75"));
	const auto grey = rawimage::DecodeGrey(std::vector<uint8_t> {4, 8, 12, 16}, 2, 2);
	ASSERT_TRUE(grey.has_value());
	EXPECT_TRUE(Contains(DescribeGreyImage(*grey), "level: lowest 4, highest 16, mean 10.00"));
}

TEST(DataReport, ARawImageWithNoSizeIsTakenAsSquare)
{
	EXPECT_EQ(SquareSide(256 * 256 * 3, 3), 256u);
	EXPECT_EQ(SquareSide(64 * 64, 1), 64u);
	EXPECT_FALSE(SquareSide(10, 3).has_value());
	EXPECT_FALSE(SquareSide(30, 3).has_value());
	EXPECT_FALSE(SquareSide(0, 1).has_value());
	EXPECT_FALSE(SquareSide(16, 0).has_value());
}

TEST(DataReport, SizesAreParsedFromText)
{
	EXPECT_EQ(ParseSize("640x480"), (std::pair<uint32_t, uint32_t> {640, 480}));
	EXPECT_FALSE(ParseSize("640").has_value());
	EXPECT_FALSE(ParseSize("x480").has_value());
	EXPECT_FALSE(ParseSize("0x480").has_value());
	EXPECT_FALSE(ParseSize("64ax48").has_value());
}

TEST(DataReport, TheInfoTablesCoverTheWholeBlock)
{
	const std::vector<uint8_t> block(sizeof(InfoConstants), 0);
	const auto text = DescribeInfoBlock(block);
	EXPECT_TRUE(Contains(text, "layout of the patch 1.20"));
	EXPECT_TRUE(Contains(text, "102 tables"));
	EXPECT_TRUE(Contains(text, "magicGeneral                         offset       0 rows   10"));
	const auto total = std::to_string(sizeof(InfoConstants));
	EXPECT_TRUE(Contains(text, "the tables cover " + total + " of " + total + " bytes"));
}

TEST(DataReport, AnInfoBlockOfAnotherSizeIsRefused)
{
	const auto text = DescribeInfoBlock(std::vector<uint8_t>(100, 0));
	EXPECT_TRUE(Contains(text, "100 bytes: neither"));
	EXPECT_FALSE(DumpInfoTable(std::vector<uint8_t>(100, 0), "magicGeneral", 0, 1).has_value());
}

TEST(DataReport, AnInfoBlockOfTheOldLayoutIsShownInTheNewOne)
{
	const std::vector<uint8_t> block(sizeof(v100::InfoConstants), 0);
	EXPECT_TRUE(Contains(DescribeInfoBlock(block), "layout of version 1.0"));
}

TEST(DataReport, ATablesRowsAreDumpedAsBytes)
{
	std::vector<uint8_t> block(sizeof(InfoConstants), 0);
	block[0] = 0xAB;
	const auto text = DumpInfoTable(block, "magicGeneral", 0, 1);
	ASSERT_TRUE(text.has_value());
	EXPECT_TRUE(Contains(*text, "magicGeneral[0]"));
	EXPECT_TRUE(Contains(*text, "0000: ab 00"));
	// Rows stop at the end of the table
	const auto last = DumpInfoTable(block, "magicGeneral", 9, 5);
	ASSERT_TRUE(last.has_value());
	EXPECT_TRUE(Contains(*last, "magicGeneral[9]"));
	EXPECT_FALSE(Contains(*last, "magicGeneral[10]"));
	EXPECT_FALSE(DumpInfoTable(block, "magicGeneral", 10, 1).has_value());
	EXPECT_FALSE(DumpInfoTable(block, "noSuchTable", 0, 1).has_value());
}

TEST(DataReport, AFontIsDescribedAndItsGlyphsAreDrawn)
{
	const auto font = gui::test::LoadFont();
	const auto brief = DescribeFont(font, false);
	EXPECT_TRUE(Contains(brief, "font \"Fake Sans\""));
	EXPECT_TRUE(Contains(brief, "bitmap height 4, 6 glyphs"));
	EXPECT_FALSE(Contains(brief, "U+0041"));
	EXPECT_TRUE(Contains(DescribeFont(font, true), "U+0041 width 2 left 1 ink 2 right 1"));
	EXPECT_EQ(DrawGlyph(font, u'A'), "##\n##\n##\n##\n");
	EXPECT_EQ(DrawGlyph(font, u'B'), "#.\n.#\n#.\n.#\n");
	// A character the font lacks is drawn as its question mark
	EXPECT_EQ(DrawGlyph(font, u'Z'), DrawGlyph(font, u'?'));
}

TEST(DataReport, ProfilesAreListedWithTheirSettings)
{
	player_profiles::Profiles profiles;
	player_profiles::Add(profiles, u"Ann", u"", "C1.erc", 3);
	const auto text = DescribeProfiles(player_profiles::Write(profiles));
	EXPECT_TRUE(Contains(text, "1 profiles, playing as \"Ann\""));
	EXPECT_TRUE(Contains(text, "\"Ann\": creature \"Ann\" in C1.erc, symbol 3"));
	EXPECT_TRUE(Contains(text, "help level 3"));
	EXPECT_TRUE(Contains(DescribeProfiles(""), "0 profiles"));
}

TEST(DataReport, ASaveSlotsInfoIsListed)
{
	gamesave::SaveGameInfo info {.slot = 2, .date = u"12:30 1/2/2001", .name = u"Before the temple"};
	const auto text = DescribeSaveInfo(gamesave::Write(info));
	EXPECT_TRUE(Contains(text, "slot 2, saved \"12:30 1/2/2001\", named \"Before the temple\""));
	EXPECT_TRUE(Contains(DescribeSaveInfo(std::vector<uint8_t>(10, 0)), "fewer than 260"));
}

TEST(DataReport, ASavedWorldShowsItsStart)
{
	const auto start = gamesave::WriteVanillaFraming("Land1.lnd");
	const auto text = DescribeSavedWorld(start);
	// A start with nothing after it holds no creatures
	if (Contains(text, "revision"))
	{
		EXPECT_TRUE(Contains(text, "land \"Land1.lnd\""));
		EXPECT_TRUE(Contains(text, "0 creatures"));
		EXPECT_TRUE(Contains(text, "0 leashes"));
	}
	EXPECT_TRUE(Contains(DescribeSavedWorld(std::vector<uint8_t>(3, 0)), "not a saved world"));
}

TEST(DataReport, SoundPlacementsAreListedPerClip)
{
	sas::SASFile file;
	file.version = 2;
	file.clips.push_back({.clip = "Walk", .soundType = 1, .sounds = {{.time = 100, .action = 7, .mode = 0}}});
	file.clips.push_back({.clip = "Idle", .soundType = 1, .sounds = {}});
	const auto brief = DescribeSounds(file, false);
	EXPECT_TRUE(Contains(brief, "version 2, 2 clips, 1 sounds"));
	EXPECT_TRUE(Contains(brief, "Walk (kind 1): 1 sounds"));
	EXPECT_FALSE(Contains(brief, "at 100 ms"));
	EXPECT_TRUE(Contains(DescribeSounds(file, true), "at 100 ms: sound 7 mode 0"));
}

TEST(DataReport, AnEnumHeaderIsListedInTheOrderOfItsValues)
{
	const auto text = DescribeEnumHeader("enum Sounds\n{\n\tSND_B = 5, // b\n\tSND_A = 2,\n\tSND_C\n};\n");
	EXPECT_TRUE(Contains(text, "3 names"));
	// A name without a value takes the one after the name before it
	EXPECT_TRUE(Contains(text, "     3 SND_C"));
	EXPECT_LT(text.find("SND_A"), text.find("SND_C"));
	EXPECT_LT(text.find("SND_C"), text.find("SND_B"));
}

TEST(DataReport, ADanceAndItsShapeAreListed)
{
	dance::DanceFile file;
	file.version = 2;
	file.groupCount = 1;
	file.loops = 4;
	file.angle = 0.5f;
	file.groupNames = {"Front"};
	dance::DanceKeyFrame frame;
	frame.time = 8.0f;
	frame.actions.push_back({.groups = {0}, .type = 3, .arguments = {}});
	file.keyFrames.push_back(frame);
	const auto text = DescribeDance(file);
	EXPECT_TRUE(Contains(text, "version 2, 1 key frames, 1 groups, loops 4"));
	EXPECT_TRUE(Contains(text, "angle 0.5"));
	EXPECT_TRUE(Contains(text, "group 0: Front"));
	EXPECT_TRUE(Contains(text, "key frame 0 at beat 8: 1 actions"));
	EXPECT_TRUE(Contains(text, "action 3 on 1 groups"));

	std::vector<uint8_t> shapeBytes;
	Append32(shapeBytes, 2);
	for (const uint32_t value : {1u, 2u, 65535u, 0u})
	{
		Append32(shapeBytes, value);
	}
	dance::DanceShapeFile shape;
	ASSERT_EQ(shape.Open(shapeBytes), dance::DanceResult::Success);
	const auto shapeText = DescribeDanceShape(shape);
	EXPECT_TRUE(Contains(shapeText, "2 points"));
	EXPECT_TRUE(Contains(shapeText, "[1] (65535, 0)"));
}

TEST(DataReport, FootpathsAreListedWithTheirNodes)
{
	serializer::GameThingSerializer::Footpath path {};
	serializer::GameThingSerializer::FootpathNode node {};
	node.coords = {.x = 10, .z = 20, .altitude = 1.5f};
	path.nodes.push_back(node);
	serializer::GameThingSerializer::FootpathLinkSave link {};
	link.coords = {.x = 3, .z = 4, .altitude = 0.0f};
	link.link.footpaths.push_back(path);
	const auto text = DescribeFootpaths(std::span(&link, 1), std::span(&path, 1));
	EXPECT_TRUE(Contains(text, "1 links, 1 paths"));
	EXPECT_TRUE(Contains(text, "link 0 at (3, 4) holds 1 paths"));
	EXPECT_TRUE(Contains(text, "path 0: 1 nodes"));
	EXPECT_TRUE(Contains(text, "(10, 20) altitude 1.5"));
}
