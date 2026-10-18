/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <CreatureHtml.h>
#include <LhpFile.h>

namespace openblack::graphics
{
struct ScreenView;
}

/// The web page about the player's creature and the pictures on it.
///
/// Each 600 turns of its life, when the game writes the creature's files, it also fills the template
/// Scripts/creature_html.tmpl into html/<creature name>.html in the player's profile folder. The page shows the
/// creature's values and four pictures of it.
///
/// The game takes the pictures as the player looks at the creature: at most one each ten seconds, at the moment of an
/// action, and one for each action. It keeps the five pictures with the best scores, each written as
/// creatureshot_<slot>.jpg in the player's profile folder, at 400 by 300 pixels.
namespace openblack::creature_web_page
{

/// The template, in the scripts folder of the game
constexpr std::string_view k_Template = "creature_html.tmpl";
/// The folder of the page in the player's profile folder
constexpr std::string_view k_PageFolder = "html";
/// The package of the page's values and the pictures, in the player's profile folder, for the creature upload tool
constexpr std::string_view k_UploadFile = "creature.lhp";
/// The category and name of the package's block of values, and the category of its blocks of pictures
constexpr std::string_view k_ValuesCategory = "CREATURE";
constexpr std::string_view k_ValuesName = "creature";
constexpr std::string_view k_PicturesCategory = "PICTURES";
/// The size of each value in the package: its name, then its value, each a text that ends in a zero byte in a field of
/// half this size
constexpr size_t k_PackedValueSize = 500;
/// The stamp of the package is a random number below this
constexpr int32_t k_StampRange = 0x8000;
/// The text that the template puts before the folder of the game, in the links to its decorations
constexpr std::string_view k_GameFolderLink = "../../../";

/// The size of a picture
constexpr uint32_t k_PictureWidth = 400;
constexpr uint32_t k_PictureHeight = 300;
/// The most pictures that a creature keeps
constexpr size_t k_MostPictures = 5;
/// A picture comes only when more whole seconds than this went by since the last one
constexpr uint32_t k_SecondsBetweenPictures = 10;
/// The quality of the pictures' JPEG files, from 1 to 100
constexpr int k_JpegQuality = 75;
/// The help text of the first desire: "Your Creature wants to impress somebody." The caption of a picture is the text
/// of the desire of the plan, the text after this one for each desire after the first.
constexpr uint32_t k_FirstDesireText = 624;
/// The help texts that say how much attention the player gave the creature, from "no" to "loads of"
constexpr uint32_t k_FirstAttentionText = 1137;
/// The help texts that say how good the player was to the creature, from "totally evil" to the most good
constexpr uint32_t k_FirstNicenessText = 1130;
/// The help texts that say which spell is on the creature, one for each spell in the order of the game's tables
constexpr uint32_t k_FirstSpellText = 1046;
/// The kinds of belief that the page counts
constexpr size_t k_BeliefKinds = 24;

/// A picture that the creature keeps
struct Picture
{
	float score {0.0f};
	/// The age of the creature when the picture came
	uint32_t age {0};
	/// The action that it did in the picture
	uint32_t action {0};
};

/// The whole seconds between two game turns, as the game counts them: the turns between them divided by the whole
/// turns in a second
[[nodiscard]] uint32_t WholeSecondsBetween(uint32_t lastTurn, uint32_t turn, uint32_t turnMilliseconds);

/// What the game knows about the creature on the turn that it can take a picture
struct Chance
{
	/// The player's camera looks at the creature
	bool lookedAt {false};
	/// The whole seconds since the last picture
	uint32_t seconds {0};
	/// The body of the creature got to the moment of its action this turn
	bool actionMoment {false};
	/// The action of the plan of the creature
	uint32_t action {0};
};

/// Whether the game takes a picture this turn: the player looks at the creature, more than ten seconds went by since
/// the last picture, the body gets to the moment of its action, and no picture that the creature keeps shows the action
[[nodiscard]] bool TakesPicture(std::span<const Picture> pictures, const Chance& chance);

/// The score of a picture: how much the action is worth, times the priority of the plan, each kept from 0 to 1, times
/// 1 for a creature younger than 30, 0.9 for one younger than 100, and 0.8 for an older one
[[nodiscard]] float Score(float actionWorth, float planPriority, uint32_t age);

/// Keeps a new picture, and gives the slot of its file. When the creature keeps five pictures already, the new picture
/// takes the place of the one with the lowest score below 1, but only if the new score is higher. Then the slot of the
/// file is the count of the other pictures, as the game counts it. Thus a new picture always goes to the last slot,
/// and the files of the other slots stay as they were. None when the picture is not kept.
[[nodiscard]] std::optional<uint32_t> Keep(std::vector<Picture>& pictures, const Picture& picture);

/// The help text of a picture's caption, for the desire of the plan; none counts as the desire after the last
[[nodiscard]] uint32_t CaptionText(std::optional<size_t> desire);
/// The caption of a picture from its help text: a space, then the text
[[nodiscard]] std::u16string Caption(std::u16string_view text);

/// The name of the file of a picture in the player's profile folder
[[nodiscard]] std::string PictureFile(uint32_t slot);
/// The name of the page's file in the html folder: the creature's name, as Windows writes it, and ".html"
[[nodiscard]] std::string PageFile(std::u16string_view creatureName);

/// The picture of a frame of the screen, RGB, 400 by 300 pixels, the top row first. It picks pixels evenly across the
/// part of the screen that challenge_pictures::PictureRegion gives, and each pixel of the picture is the mean of the
/// pixel picked and the pixel picked before it in its row. Empty if the frame is too small to hold its rows. It uses no
/// game data, so the renderer can call it on its own thread.
[[nodiscard]] std::vector<uint8_t> TakePicture(const graphics::ScreenView& screen);
/// A picture as a JPEG file, with the quality that the game gives. Empty if it cannot be made.
[[nodiscard]] std::vector<uint8_t> EncodeJpeg(std::span<const uint8_t> rgb, uint32_t width, uint32_t height);

/// A number with two places after the point, as the game writes it. A number halfway between two values goes to the
/// value further from zero.
[[nodiscard]] std::string FormatHundredths(float value);
/// Text as an English Windows writes it in a single byte to a character (code page 1252). A character that the code page
/// does not have becomes "?".
[[nodiscard]] std::string ToWindowsText(std::u16string_view text);

/// The values of the creature that the page shows
struct PageFacts
{
	std::u16string name;
	uint32_t age {0};
	float health {0.0f};
	float alignment {0.0f};
	float energy {0.0f};
	float strength {0.0f};
	float fatness {0.0f};
	float exhaustion {0.0f};
	float dehydration {0.0f};
	float itchiness {0.0f};
	float poo {0.0f};
	uint32_t peopleKilled {0};
	uint32_t animalsKilled {0};
	uint32_t creaturesKilled {0};
	uint32_t battlesFought {0};
	uint32_t battlesWon {0};
	uint32_t poos {0};
	uint32_t mushrooms {0};
	/// The captions of the five slots of pictures
	std::array<std::u16string, k_MostPictures> captions {};
	/// Each desire's name and value
	std::vector<std::pair<std::string, float>> desires;
	/// How many beliefs the creature has of each kind
	std::array<size_t, k_BeliefKinds> beliefs {};
	/// How good the player was to the creature, from 0 (totally evil) to 6
	int32_t niceness {0};
	/// How much attention the player gave the creature, from 0 (none) to 4
	int32_t attention {0};
	/// The first spell in the order of the game's tables that is on the creature now (cast and not finished), none for
	/// none
	std::optional<uint32_t> spellOn;
};

/// The values for the template, in the order that the game makes them. The marks of the template that the game ships
/// are name, age, alignment, strength, health, energy, num-people-killed and num-battles-won.
[[nodiscard]] std::vector<creaturehtml::Value> PageValues(const PageFacts& facts);

/// The values as the package keeps them: the last value first, each in a field of k_PackedValueSize bytes
[[nodiscard]] std::vector<uint8_t> PackValues(std::span<const creaturehtml::Value> values);

/// The package of the values and the pictures. The pictures are the bytes of the files of the five slots, in the order
/// of the slots; a slot with no file adds no block.
[[nodiscard]] lhp::Package UploadPackage(std::span<const creaturehtml::Value> values,
                                         std::span<const std::optional<std::vector<uint8_t>>> pictures, uint16_t stamp);

/// The name of the package's block of the picture of a slot
[[nodiscard]] std::string PictureBlockName(uint32_t slot);

/// A file URL of a folder, with a "/" at its end
[[nodiscard]] std::string FolderUrl(const std::filesystem::path& folder);

/// The page with each link to the game folder that the template has (k_GameFolderLink) replaced by the URL of the
/// game folder, so that the decorations show wherever the profile folder is
[[nodiscard]] std::string PointLinksAtGameFolder(std::string_view page, const std::filesystem::path& gameFolder);

} // namespace openblack::creature_web_page
