/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureWebPage.h"

#include <cmath>

#include <algorithm>
#include <ranges>

#include <fmt/format.h>
#include <stb_image_write.h>

#include "ECS/ChallengePictures.h"
#include "Graphics/ScreenshotCapture.h"

using namespace openblack;
using namespace openblack::creature_web_page;

namespace
{

constexpr uint32_t k_BytesPerScreenPixel = 4;
constexpr uint32_t k_BytesPerPixel = 3;
constexpr uint32_t k_MillisecondsPerSecond = 1000;
/// The ages at which the score of a picture drops, and the share of the score kept from each
constexpr uint32_t k_YoungAge = 30;
constexpr uint32_t k_MiddleAge = 100;
constexpr float k_MiddleAgeShare = 0.9f;
constexpr float k_OldAgeShare = 0.8f;
/// The desire that a creature with no plan has: the one after the last desire
constexpr size_t k_NoDesire = 40;
/// The picture steps across the screen by its size times these
constexpr float k_RowStepPerPixel = 1.0f / static_cast<float>(k_PictureHeight);
constexpr float k_ColumnStepPerPixel = 1.0f / static_cast<float>(k_PictureWidth);
/// The page writes a number to the hundredth
constexpr double k_Hundred = 100.0;

/// The characters of code page 1252 from 0x80 to 0x9F, 0 for a byte that it does not use
constexpr std::array<char16_t, 32> k_HighCharacters {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};
constexpr char16_t k_FirstHighCharacter = 0x80;
constexpr char16_t k_FirstLatinCharacter = 0xA0;
constexpr char16_t k_LastLatinCharacter = 0xFF;

[[nodiscard]] float KeptInRange(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}
	return value > 1.0f ? 1.0f : value;
}

/// A pixel of the screen, BGRA, as red, green and blue
[[nodiscard]] std::array<uint32_t, k_BytesPerPixel> ScreenPixel(const graphics::ScreenView& screen, uint32_t x, uint32_t row)
{
	const auto offset = static_cast<size_t>(row) * screen.pitch + static_cast<size_t>(x) * k_BytesPerScreenPixel;
	return {screen.bgra[offset + 2], screen.bgra[offset + 1], screen.bgra[offset]};
}

void WriteBytes(void* context, void* data, int size)
{
	auto* bytes = static_cast<std::vector<uint8_t>*>(context);
	const auto* first = static_cast<const uint8_t*>(data);
	bytes->insert(bytes->end(), first, first + size);
}

} // namespace

uint32_t creature_web_page::WholeSecondsBetween(uint32_t lastTurn, uint32_t turn, uint32_t turnMilliseconds)
{
	const auto turnsPerSecond = turnMilliseconds == 0 ? 0u : k_MillisecondsPerSecond / turnMilliseconds;
	// The turns between are counted without a sign
	return turnsPerSecond == 0 ? 0u : (turn - lastTurn) / turnsPerSecond;
}

bool creature_web_page::TakesPicture(std::span<const Picture> pictures, const Chance& chance)
{
	return chance.lookedAt && chance.seconds > k_SecondsBetweenPictures && chance.actionMoment &&
	       std::ranges::none_of(pictures, [&chance](const Picture& picture) { return picture.action == chance.action; });
}

float creature_web_page::Score(float actionWorth, float planPriority, uint32_t age)
{
	float ageShare = 1.0f;
	if (age >= k_MiddleAge)
	{
		ageShare = k_OldAgeShare;
	}
	else if (age >= k_YoungAge)
	{
		ageShare = k_MiddleAgeShare;
	}
	return ageShare * KeptInRange(planPriority) * KeptInRange(actionWorth);
}

std::optional<uint32_t> creature_web_page::Keep(std::vector<Picture>& pictures, const Picture& picture)
{
	if (pictures.size() >= k_MostPictures)
	{
		// The lowest score below 1, the first of equal scores
		std::optional<size_t> lowest;
		float score = 1.0f;
		for (size_t i = 0; i < pictures.size(); ++i)
		{
			if (pictures[i].score < score)
			{
				score = pictures[i].score;
				lowest = i;
			}
		}
		// With all the scores at 1, the game finds no picture to replace and reads a picture that is not there
		// (vanilla-bugs). Here, the creature does not keep the new picture.
		if (!lowest.has_value() || !(pictures[*lowest].score < picture.score))
		{
			return std::nullopt;
		}
		pictures.erase(pictures.begin() + static_cast<std::ptrdiff_t>(*lowest));
	}
	const auto slot = static_cast<uint32_t>(pictures.size());
	pictures.push_back(picture);
	return slot;
}

uint32_t creature_web_page::CaptionText(std::optional<size_t> desire)
{
	return k_FirstDesireText + static_cast<uint32_t>(desire.value_or(k_NoDesire));
}

std::u16string creature_web_page::Caption(std::u16string_view text)
{
	return u" " + std::u16string(text);
}

std::string creature_web_page::PictureFile(uint32_t slot)
{
	return fmt::format("creatureshot_{}.jpg", slot);
}

std::string creature_web_page::PageFile(std::u16string_view creatureName)
{
	return ToWindowsText(creatureName) + ".html";
}

std::vector<uint8_t> creature_web_page::TakePicture(const graphics::ScreenView& screen)
{
	const auto region = ecs::challenge_pictures::PictureRegion(screen.width, screen.height);
	if (region.width == 0 || region.height == 0 || screen.bgra.size() < static_cast<size_t>(screen.pitch) * screen.height ||
	    screen.pitch < screen.width * k_BytesPerScreenPixel)
	{
		return {};
	}
	// The steps are in single precision. Each position drops its fraction to give a whole pixel.
	const auto rowStep = static_cast<float>(region.height) * k_RowStepPerPixel;
	const auto columnStep = static_cast<float>(region.width) * k_ColumnStepPerPixel;
	std::array<uint32_t, k_PictureWidth> columns {};
	for (uint32_t x = 0; x < k_PictureWidth; ++x)
	{
		columns.at(x) = std::min(region.x + static_cast<uint32_t>(static_cast<float>(x) * columnStep), screen.width - 1);
	}

	std::vector<uint8_t> picture(static_cast<size_t>(k_PictureWidth) * k_PictureHeight * k_BytesPerPixel);
	auto out = picture.begin();
	for (uint32_t y = 0; y < k_PictureHeight; ++y)
	{
		const auto down = std::min(region.y + static_cast<uint32_t>(static_cast<float>(y) * rowStep), screen.height - 1);
		const auto row = screen.yflip ? screen.height - 1 - down : down;
		auto previous = ScreenPixel(screen, region.x, row);
		for (const auto across : columns)
		{
			const auto pixel = ScreenPixel(screen, across, row);
			for (size_t channel = 0; channel < k_BytesPerPixel; ++channel)
			{
				*out++ = static_cast<uint8_t>((previous.at(channel) + pixel.at(channel)) / 2);
			}
			previous = pixel;
		}
	}
	return picture;
}

std::vector<uint8_t> creature_web_page::EncodeJpeg(std::span<const uint8_t> rgb, uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0 || rgb.size() < static_cast<size_t>(width) * height * k_BytesPerPixel)
	{
		return {};
	}
	std::vector<uint8_t> bytes;
	const auto written = stbi_write_jpg_to_func(&WriteBytes, &bytes, static_cast<int>(width), static_cast<int>(height),
	                                            static_cast<int>(k_BytesPerPixel), rgb.data(), k_JpegQuality);
	if (written == 0)
	{
		return {};
	}
	return bytes;
}

std::string creature_web_page::FormatHundredths(float value)
{
	// A float times a hundred is exact in double precision, so a value exactly halfway is easy to find
	const auto hundredths = static_cast<double>(value) * k_Hundred;
	const auto magnitude = std::abs(hundredths);
	if (std::isfinite(magnitude) && magnitude - std::floor(magnitude) == 0.5)
	{
		const auto whole = static_cast<uint64_t>(std::floor(magnitude)) + 1;
		return fmt::format("{}{}.{:02}", std::signbit(value) ? "-" : "", whole / 100, whole % 100);
	}
	return fmt::format("{:.2f}", static_cast<double>(value));
}

std::string creature_web_page::ToWindowsText(std::u16string_view text)
{
	std::string written;
	written.reserve(text.size());
	for (const char16_t c : text)
	{
		if (c < k_FirstHighCharacter || (c >= k_FirstLatinCharacter && c <= k_LastLatinCharacter))
		{
			written.push_back(static_cast<char>(c));
			continue;
		}
		const auto high = std::ranges::find(k_HighCharacters, c);
		written.push_back(high == k_HighCharacters.end()
		                      ? '?'
		                      : static_cast<char>(k_FirstHighCharacter + std::distance(k_HighCharacters.begin(), high)));
	}
	return written;
}

std::vector<creaturehtml::Value> creature_web_page::PageValues(const PageFacts& facts)
{
	const auto whole = [](uint32_t value) { return fmt::format("{}", static_cast<int32_t>(value)); };
	std::vector<creaturehtml::Value> values {
	    {"name", ToWindowsText(facts.name)},
	    {"health", FormatHundredths(facts.health)},
	    {"alignment", FormatHundredths(facts.alignment)},
	    {"age", whole(facts.age)},
	    {"energy", FormatHundredths(facts.energy)},
	    {"strength", FormatHundredths(facts.strength)},
	    {"fatness", FormatHundredths(facts.fatness)},
	    {"exhaustion", FormatHundredths(facts.exhaustion)},
	    {"dehydration", FormatHundredths(facts.dehydration)},
	    {"itchiness", FormatHundredths(facts.itchiness)},
	    {"amount-of-poo", FormatHundredths(facts.poo)},
	    {"num-people-killed", whole(facts.peopleKilled)},
	    {"num-animals-killed", whole(facts.animalsKilled)},
	    {"num-creatures-killed", whole(facts.creaturesKilled)},
	    {"num-battles-fought", whole(facts.battlesFought)},
	    {"num-battles-won", whole(facts.battlesWon)},
	    {"num-poos", whole(facts.poos)},
	    {"num-mushrooms", whole(facts.mushrooms)},
	};
	for (size_t slot = 0; slot < facts.captions.size(); ++slot)
	{
		values.push_back({fmt::format("picture_dialog_{}", slot), ToWindowsText(facts.captions.at(slot))});
	}
	for (const auto& [name, value] : facts.desires)
	{
		values.push_back({fmt::format("desire-{}", name), FormatHundredths(value)});
	}
	for (size_t kind = 0; kind < facts.beliefs.size(); ++kind)
	{
		values.push_back({fmt::format("beliefs_{}", kind), fmt::format("{}", facts.beliefs.at(kind))});
	}
	values.push_back(
	    {"how_nice_player_has_been", fmt::format("{}", static_cast<int32_t>(k_FirstNicenessText) + facts.niceness)});
	values.push_back({"how_much_attention_player_has_given_me",
	                  fmt::format("{}", static_cast<int32_t>(k_FirstAttentionText) + facts.attention)});
	return values;
}
