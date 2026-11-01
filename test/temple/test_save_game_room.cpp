/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <3D/TempleToolTips.h>
#include <SavePictures.h>
#include <Temple/SaveGameRoomRules.h>
#include <gtest/gtest.h>

namespace rules = openblack::temple::save_game_room;
using openblack::TempleRoom;
using openblack::TempleSaveGameControl;
using openblack::TempleToolTip;
using openblack::TempleToolTipInput;
using rules::Click;
using rules::Control;
using rules::Part;
using rules::View;

namespace
{
constexpr uint32_t k_Index(uint32_t text)
{
	return text - 0xE73;
}
} // namespace

TEST(SaveGameRoom, TheControlsAreFoundByTheirNames)
{
	EXPECT_EQ(rules::ParseControl("LH_PICTURE_savegame_01"), (Control {.part = Part::Picture, .slot = 0}));
	EXPECT_EQ(rules::ParseControl("LH_PICTURE_SAVEGAME_21"), (Control {.part = Part::Picture, .slot = 20}));
	EXPECT_EQ(rules::ParseControl("LH_BUTTON_SAVE00"), (Control {.part = Part::Save, .slot = 0}));
	EXPECT_EQ(rules::ParseControl("LH_BUTTON_LOAD20"), (Control {.part = Part::Load, .slot = 20}));
	EXPECT_EQ(rules::ParseControl("LH_BUTTON_DELETE13"), (Control {.part = Part::Delete, .slot = 13}));
	// The pictures start at 01 and the buttons stop at 20
	EXPECT_FALSE(rules::ParseControl("LH_PICTURE_savegame_00").has_value());
	EXPECT_FALSE(rules::ParseControl("LH_BUTTON_SAVE21").has_value());
	EXPECT_FALSE(rules::ParseControl("LH_FRAME_savegame_01").has_value());
	EXPECT_FALSE(rules::ParseControl("LH_SCROLL_Savegame").has_value());
}

TEST(SaveGameRoom, TheButtonsShowOnlyCloseToAPicture)
{
	const View far {};
	const View close {.closeUp = true, .pictureLookedAt = true};
	EXPECT_TRUE(rules::IsShown({.part = Part::Picture, .slot = 3}, far, false));
	EXPECT_FALSE(rules::IsShown({.part = Part::Save, .slot = 3}, far, false));
	EXPECT_TRUE(rules::IsShown({.part = Part::Save, .slot = 3}, close, false));
	// Only the player's slots save, and not while the player is kept in the room
	EXPECT_FALSE(rules::IsShown({.part = Part::Save, .slot = 14}, close, false));
	EXPECT_FALSE(rules::IsShown({.part = Part::Save, .slot = 3},
	                            View {.closeUp = true, .pictureLookedAt = true, .keptInRoom = true}, false));
	// Close to the scroll, no button shows
	EXPECT_FALSE(rules::IsShown({.part = Part::Save, .slot = 3}, View {.closeUp = true}, false));
	// Load and delete need a save
	EXPECT_FALSE(rules::IsShown({.part = Part::Load, .slot = 3}, close, false));
	EXPECT_TRUE(rules::IsShown({.part = Part::Load, .slot = 15}, close, true));
	EXPECT_TRUE(rules::IsShown({.part = Part::Delete, .slot = 15}, close, true));
}

TEST(SaveGameRoom, AClickLooksCloserThenAsks)
{
	const View close {.closeUp = true, .pictureLookedAt = true};
	EXPECT_EQ(rules::Clicked({.part = Part::Picture, .slot = 2}, View {}, true), Click::LookCloser);
	EXPECT_EQ(rules::Clicked({.part = Part::Picture, .slot = 2}, close, true), Click::AskLoad);
	EXPECT_EQ(rules::Clicked({.part = Part::Picture, .slot = 2}, close, false), Click::AskName);
	EXPECT_EQ(rules::Clicked({.part = Part::Picture, .slot = 16}, close, false), Click::Nothing);
	// A close click on an empty picture saves even while the player is kept in the room
	EXPECT_EQ(rules::Clicked({.part = Part::Picture, .slot = 2},
	                         View {.closeUp = true, .pictureLookedAt = true, .keptInRoom = true}, false),
	          Click::AskName);
	EXPECT_EQ(rules::Clicked({.part = Part::Save, .slot = 2}, close, true), Click::AskOverwrite);
	EXPECT_EQ(rules::Clicked({.part = Part::Save, .slot = 2}, close, false), Click::AskName);
	EXPECT_EQ(rules::Clicked({.part = Part::Load, .slot = 2}, close, true), Click::AskLoad);
	EXPECT_EQ(rules::Clicked({.part = Part::Delete, .slot = 2}, close, true), Click::AskDelete);
}

TEST(SaveGameRoom, AHeldButtonMovesIntoTheWallAndAPictureDoesNot)
{
	const glm::vec3 frameZ {0.0f, 0.0f, 2.0f};
	EXPECT_FLOAT_EQ(rules::HeldOffset(Part::Load, frameZ).z, -0.6f);
	EXPECT_EQ(rules::HeldOffset(Part::Picture, frameZ), glm::vec3(0.0f));
}

TEST(SaveGameRoom, TheEmptyPictureIsHalfByAlignment)
{
	EXPECT_EQ(rules::EmptyPicture(0.0f), (rules::UvRect {.u0 = 0.5f, .v0 = 0.0f, .u1 = 1.0f, .v1 = 1.0f}));
	EXPECT_EQ(rules::EmptyPicture(-0.1f), (rules::UvRect {.u0 = 0.0f, .v0 = 0.0f, .u1 = 0.5f, .v1 = 1.0f}));
}

TEST(SaveGameRoom, ALongNameIsCutWithDots)
{
	EXPECT_EQ(rules::ToolTipName(u"Land One"), u"Land One");
	EXPECT_EQ(rules::ToolTipName(u"123456789012345"), u"123456789012345");
	EXPECT_EQ(rules::ToolTipName(u"1234567890123456"), u"1234567890123...");
	EXPECT_EQ(rules::ToolTipName(u"1234567890123456789"), u"1234567890123...");
}

TEST(SaveGameRoom, APictureChoosesItsTooltip)
{
	rules::PictureToolTipInput input {.slot = 4,
	                                  .hovered = true,
	                                  .zoom = 0.0f,
	                                  .pictureLookedAt = false,
	                                  .highDetail = std::nullopt,
	                                  .buttonHovered = false,
	                                  .saveName = u"Ogre",
	                                  .multiplayer = false};
	// From afar, a named save zooms in with its name, an unnamed one or an empty slot without
	auto chosen = rules::PictureToolTipOf(input);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(chosen->text, rules::k_ZoomInNamedText);
	EXPECT_EQ(chosen->name, u"Ogre");
	input.saveName = u"";
	EXPECT_EQ(rules::PictureToolTipOf(input)->text, rules::k_ZoomInText);
	input.saveName = std::nullopt;
	EXPECT_EQ(rules::PictureToolTipOf(input)->text, rules::k_ZoomInText);
	// Close up as the picture in high detail: an empty slot saves, a save loads
	input.zoom = 1.0f;
	input.pictureLookedAt = true;
	input.highDetail = 4;
	EXPECT_EQ(rules::PictureToolTipOf(input)->text, rules::k_SaveText);
	input.saveName = u"Ogre";
	EXPECT_EQ(rules::PictureToolTipOf(input)->text, rules::k_LoadText);
	// A button hovered before it leaves it be
	input.buttonHovered = true;
	EXPECT_FALSE(rules::PictureToolTipOf(input).has_value());
	// Not hovered, it zooms out
	input.buttonHovered = false;
	input.hovered = false;
	EXPECT_EQ(rules::PictureToolTipOf(input)->text, rules::k_ZoomOutText);
	// An empty slot that the player cannot save into says nothing close up
	input.hovered = true;
	input.saveName = std::nullopt;
	input.slot = 15;
	input.highDetail = 15;
	EXPECT_FALSE(rules::PictureToolTipOf(input).has_value());
}

TEST(SaveGameRoom, TheLoadButtonKeepsTheArrowsBeforeIt)
{
	const std::array controls {
	    TempleSaveGameControl {.subMesh = 1, .control = {.part = Part::Load, .slot = 0}, .saveName = u"A"},
	    TempleSaveGameControl {.subMesh = 2, .control = {.part = Part::Save, .slot = 0}, .saveName = u"A"},
	};
	TempleToolTip toolTip {};
	TempleToolTipInput input {.room = TempleRoom::SaveGame, .inControl = true, .hoveredSubMesh = 1};
	input.savedGames = controls;
	std::u16string name;
	openblack::UpdateTempleToolTip(toolTip, input, &name);
	EXPECT_EQ(toolTip.index, k_Index(rules::k_LoadText));
	EXPECT_EQ(toolTip.arrows, openblack::gui::ToolTipArrows::k_All);
	input.hoveredSubMesh = 2;
	openblack::UpdateTempleToolTip(toolTip, input, &name);
	EXPECT_EQ(toolTip.index, k_Index(rules::k_SaveText));
	EXPECT_EQ(toolTip.arrows, openblack::gui::ToolTipArrows::k_None);
}

TEST(SaveGameRoom, ANamedZoomGivesTheSavesName)
{
	const std::array controls {
	    TempleSaveGameControl {.subMesh = 5, .control = {.part = Part::Picture, .slot = 3}, .saveName = u"A long name for it"},
	};
	TempleToolTip toolTip {};
	TempleToolTipInput input {.room = TempleRoom::SaveGame, .inControl = true, .hoveredSubMesh = 5};
	input.savedGames = controls;
	std::u16string name;
	openblack::UpdateTempleToolTip(toolTip, input, &name);
	EXPECT_EQ(toolTip.index, k_Index(rules::k_ZoomInNamedText));
	EXPECT_EQ(name, u"A long name f...");
}

TEST(SavePictures, TheFilesAndCellsOfTheSlots)
{
	using namespace openblack::gamesave;
	EXPECT_EQ(LargePictureFileName(7), "save_game_picture_hires_3.raw");
	EXPECT_EQ(SmallPictureFileName(7), "save_game_picture_lores_0.raw");
	EXPECT_EQ(SmallPictureFileName(20), "save_game_picture_lores_2.raw");
	EXPECT_EQ(LargePictureCell(7), (PictureCell {.x = 0, .y = 128, .width = 256, .height = 128}));
	EXPECT_EQ(SmallPictureCell(7), (PictureCell {.x = 128, .y = 192, .width = 128, .height = 64}));
	EXPECT_EQ(SmallPictureCell(10), (PictureCell {.x = 0, .y = 64, .width = 128, .height = 64}));
}

TEST(SavePictures, ACellIsItsRowsOfBlocks)
{
	using namespace openblack::gamesave;
	std::vector<uint8_t> bytes(k_PictureFileSize, 0);
	bytes[0] = 0x80;
	bytes[1] = 0x80;
	bytes[84] = 'D';
	bytes[85] = 'X';
	bytes[86] = 'T';
	bytes[87] = '1';
	// Each block's first byte is its column, its second its row
	for (uint32_t row = 0; row < 64; ++row)
	{
		for (uint32_t column = 0; column < 64; ++column)
		{
			const auto at = 128 + ((static_cast<size_t>(row) * 64 + column) * 8);
			bytes[at] = static_cast<uint8_t>(column);
			bytes[at + 1] = static_cast<uint8_t>(row);
		}
	}
	const auto file = ReadPictureFile(bytes);
	ASSERT_TRUE(file.has_value());
	const auto blocks = CellBlocks(*file, SmallPictureCell(3));
	ASSERT_EQ(blocks.size(), 32u * 16u * 8u);
	// The cell of slot 3 starts at column 32, row 16
	EXPECT_EQ(blocks[0], 32);
	EXPECT_EQ(blocks[1], 16);
	EXPECT_EQ(blocks[(32 * 8) + 1], 17);
	// Another kind of picture is not read
	bytes[87] = '3';
	EXPECT_FALSE(ReadPictureFile(bytes).has_value());
	EXPECT_FALSE(ReadPictureFile(std::vector<uint8_t>(100)).has_value());
}

TEST(SaveGameRoom, AfterASaveThePictureInHighDetailIsReadAgain)
{
	// The small pictures are read again after every save
	EXPECT_EQ(rules::PicturesAfterSave(4, std::nullopt),
	          (rules::PicturesToRead {.smallPictures = true, .largePicture = false}));
	EXPECT_EQ(rules::PicturesAfterSave(4, 7u), (rules::PicturesToRead {.smallPictures = true, .largePicture = false}));
	// The picture in high detail is read again when the save goes into its slot
	EXPECT_EQ(rules::PicturesAfterSave(4, 4u), (rules::PicturesToRead {.smallPictures = true, .largePicture = true}));
	EXPECT_EQ(rules::PicturesAfterSave(0, 0u), (rules::PicturesToRead {.smallPictures = true, .largePicture = true}));
}

TEST(SaveGameRoom, TheCloseLookAtASaveGivesItsDateAndName)
{
	const rules::HelpTextFn helpText = [](uint32_t number) { return number == 6690 ? u"Auto Save Land: %3.0f" : u"Text"; };
	const auto words =
	    rules::CloseLookWordsOf(3, rules::SaveWords {.date = u"10:00:00 Monday", .name = u"My Save"}, 1, helpText);
	ASSERT_TRUE(words.has_value());
	EXPECT_EQ(words->title, std::u16string(u"10:00:00 Monday"));
	EXPECT_EQ(words->details, u"My Save");
	// A save in a slot that has a label also gives its date and name
	const auto landSave = rules::CloseLookWordsOf(15, rules::SaveWords {.date = u"Date", .name = u""}, 2, helpText);
	ASSERT_TRUE(landSave.has_value());
	EXPECT_EQ(landSave->title, std::u16string(u"Date"));
	EXPECT_EQ(landSave->details, u"");
}

TEST(SaveGameRoom, TheCloseLookAtAnEmptySlotGivesItsLabel)
{
	const rules::HelpTextFn helpText = [](uint32_t number) -> std::u16string {
		switch (number)
		{
		case 6688:
			return u"Previous Land";
		case 6689:
			return u"Next Land";
		case 6690:
			return u"Auto Save Land: %3.0f";
		case 6661:
			return u"Quick Save";
		case 6691:
			return u"Auto Save On Exit";
		default:
			return u"Wrong";
		}
	};
	const std::array<std::u16string, 7> expected {u"Previous Land",       u"Auto Save Land:   2", u"Auto Save Land:   2",
	                                              u"Auto Save Land:   2", u"Next Land",           u"Quick Save",
	                                              u"Auto Save On Exit"};
	for (uint32_t slot = rules::k_FirstLabelSlot; slot < rules::k_Slots; ++slot)
	{
		const auto words = rules::CloseLookWordsOf(slot, std::nullopt, 2, helpText);
		ASSERT_TRUE(words.has_value()) << slot;
		EXPECT_FALSE(words->title.has_value()) << slot;
		EXPECT_EQ(words->details, expected.at(slot - rules::k_FirstLabelSlot)) << slot;
	}
	// The slots that the player saves into have no label
	EXPECT_FALSE(rules::CloseLookWordsOf(0, std::nullopt, 2, helpText).has_value());
	EXPECT_FALSE(rules::CloseLookWordsOf(rules::k_LastPlayerSlot, std::nullopt, 2, helpText).has_value());
	EXPECT_FALSE(rules::CloseLookWordsOf(rules::k_Slots, std::nullopt, 2, helpText).has_value());
}

TEST(SaveGameRoom, TheCloseLookDetailsGoAcrossTheMiddleHalf)
{
	EXPECT_EQ(rules::DetailsSpanOf(0, 800), (rules::DetailsSpan {.left = 200.0f, .right = 600.0f}));
	// The quarter is rounded down
	EXPECT_EQ(rules::DetailsSpanOf(0, 1023), (rules::DetailsSpan {.left = 255.0f, .right = 765.0f}));
	// The middle of a wide screen
	EXPECT_EQ(rules::DetailsSpanOf(640, 1920), (rules::DetailsSpan {.left = 1120.0f, .right = 2080.0f}));
	// Each line is centred in the span
	EXPECT_FLOAT_EQ(rules::DetailsLineLeft({.left = 200.0f, .right = 600.0f}, 100.0f), 350.0f);
	EXPECT_FLOAT_EQ(rules::DetailsLineLeft({.left = 200.0f, .right = 600.0f}, 401.0f), 199.5f);
}

// The room reads its list and its small pictures at the first visit, and again only after a save, a delete or a slot
// picture, or for another player or another land
TEST(SaveGameRoom, TheRoomReadsTheSavedGamesAgainOnlyWhenTheyChanged)
{
	using openblack::temple::save_game_room::ReadsSavedGamesAgain;
	using openblack::temple::save_game_room::SavedGamesStamp;
	const SavedGamesStamp read {.changes = 3, .player = "players/one", .land = 1};
	EXPECT_TRUE(ReadsSavedGamesAgain(std::nullopt, read));
	EXPECT_FALSE(ReadsSavedGamesAgain(read, read));
	EXPECT_TRUE(ReadsSavedGamesAgain(read, {.changes = 4, .player = "players/one", .land = 1}));
	EXPECT_TRUE(ReadsSavedGamesAgain(read, {.changes = 3, .player = "players/two", .land = 1}));
	EXPECT_TRUE(ReadsSavedGamesAgain(read, {.changes = 3, .player = "players/one", .land = 2}));
}

// The text of the scroll moves in front of the scroll one frame after the camera comes past half way. The room then
// writes it again in each frame, and writes it on the texture again in the frame that the camera is half way or farther.
TEST(SaveGameRoom, TheScrollTextMovesInFrontOneFrameAfterTheCameraComesPastHalfWay)
{
	using rules::ScrollSwitch;
	using rules::StepScrollSwitch;
	// Far away: nothing changes
	EXPECT_EQ(StepScrollSwitch(false, 0.0f),
	          (ScrollSwitch {.drawInFront = false, .inFrontAfter = false, .writeOnTexture = false}));
	// Exactly half way is not close
	EXPECT_EQ(StepScrollSwitch(false, 0.5f),
	          (ScrollSwitch {.drawInFront = false, .inFrontAfter = false, .writeOnTexture = false}));
	// The first close frame only keeps the change
	EXPECT_EQ(StepScrollSwitch(false, 0.51f),
	          (ScrollSwitch {.drawInFront = false, .inFrontAfter = true, .writeOnTexture = false}));
	// While close, the text is in front in each frame
	EXPECT_EQ(StepScrollSwitch(true, 1.0f),
	          (ScrollSwitch {.drawInFront = true, .inFrontAfter = true, .writeOnTexture = false}));
	// In the frame that the camera is half way, the text is in front once more and then goes on the texture again
	EXPECT_EQ(StepScrollSwitch(true, 0.5f),
	          (ScrollSwitch {.drawInFront = true, .inFrontAfter = false, .writeOnTexture = true}));
	EXPECT_EQ(StepScrollSwitch(true, 0.0f),
	          (ScrollSwitch {.drawInFront = true, .inFrontAfter = false, .writeOnTexture = true}));
}
