/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RealWeatherRules.h"

#include <cstring>

#include <algorithm>

#include <fmt/format.h>

namespace openblack::ecs::real_weather
{
namespace
{

/// The places in a preset of the file
namespace offsets
{
constexpr size_t k_Type = 0x00;
constexpr size_t k_MakesStorm = 0x02;
constexpr size_t k_RainOperation = 0x03;
constexpr size_t k_RainAmount = 0x04;
constexpr size_t k_SnowCover = 0x08;
constexpr size_t k_Snowfall = 0x10;
constexpr size_t k_WindX = 0x18;
constexpr size_t k_WindZ = 0x20;
constexpr size_t k_RainSpeed = 0x28;
constexpr size_t k_ThunderLeast = 0x30;
constexpr size_t k_ThunderMost = 0x38;
constexpr size_t k_BoltLeast = 0x40;
constexpr size_t k_BoltMost = 0x48;
constexpr size_t k_Overcast = 0x50;
constexpr size_t k_CloudShade = 0x58;
constexpr size_t k_CloudCount = 0x60;
constexpr size_t k_CloudHeight = 0x68;
} // namespace offsets

float ReadFloat(std::span<const uint8_t> bytes, size_t offset)
{
	float value = 0.0f;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}

/// A change whose operation is in the byte at the place, and whose amount is in the 4 bytes at the place plus 4
Change ReadChange(std::span<const uint8_t> bytes, size_t offset)
{
	return {.operation = bytes[offset], .amount = ReadFloat(bytes, offset + 4)};
}

/// The number as the game makes a byte of it: the number truncated, then its low byte, at most 100
int8_t CappedByte(float value)
{
	const auto low = static_cast<int8_t>(static_cast<uint8_t>(static_cast<uint32_t>(static_cast<int32_t>(value)) & 0xFFU));
	return low > k_MostAmount ? static_cast<int8_t>(k_MostAmount) : low;
}

} // namespace

float Change::Apply(float value) const
{
	switch (static_cast<Operation>(operation))
	{
	case Operation::Add:
		return amount + value;
	case Operation::Subtract:
		return value - amount;
	default:
		return amount;
	}
}

std::vector<Preset> ParsePresets(std::span<const uint8_t> bytes)
{
	std::vector<Preset> presets;
	for (size_t start = 0; start + k_PresetBytes <= bytes.size(); start += k_PresetBytes)
	{
		const auto record = bytes.subspan(start, k_PresetBytes);
		uint16_t type = 0;
		std::memcpy(&type, record.data() + offsets::k_Type, sizeof(type));
		presets.push_back(Preset {
		    .type = type,
		    .makesStorm = record[offsets::k_MakesStorm] == 1,
		    .rain = {.operation = record[offsets::k_RainOperation], .amount = ReadFloat(record, offsets::k_RainAmount)},
		    .snowCover = ReadChange(record, offsets::k_SnowCover),
		    .snowfall = ReadChange(record, offsets::k_Snowfall),
		    .windX = ReadChange(record, offsets::k_WindX),
		    .windZ = ReadChange(record, offsets::k_WindZ),
		    .overcast = ReadChange(record, offsets::k_Overcast),
		    .rainSpeed = ReadChange(record, offsets::k_RainSpeed),
		    .thunderLeast = ReadChange(record, offsets::k_ThunderLeast),
		    .thunderMost = ReadChange(record, offsets::k_ThunderMost),
		    .boltLeast = ReadChange(record, offsets::k_BoltLeast),
		    .boltMost = ReadChange(record, offsets::k_BoltMost),
		    .cloudShade = ReadChange(record, offsets::k_CloudShade),
		    .cloudCount = ReadChange(record, offsets::k_CloudCount),
		    .cloudHeight = ReadChange(record, offsets::k_CloudHeight),
		});
	}
	return presets;
}

const Preset* FindPreset(std::span<const Preset> presets, int32_t type)
{
	// The game compares the type as a 16-bit number
	const auto wanted = static_cast<int16_t>(type);
	if (wanted == k_NoType)
	{
		return nullptr;
	}
	const auto found = std::ranges::find_if(presets.rbegin(), presets.rend(), [wanted](const Preset& preset) {
		return static_cast<int32_t>(preset.type) == static_cast<int32_t>(wanted);
	});
	return found != presets.rend() ? &*found : nullptr;
}

script_weather::LandStorm PresetStorm(glm::vec3 centre)
{
	script_weather::LandStorm storm;
	storm.position = centre;
	storm.destination = centre;
	storm.innerRadius = k_StormInnerRadius;
	storm.outerRadius = k_StormOuterRadius;
	storm.fadeSeconds = k_StormFadeSeconds;
	storm.lastsFor = k_StormLastsFor;
	return storm;
}

Applied ApplyPreset(const Preset* preset, components::WeatherInfo background, int32_t temperature, int32_t visibility,
                    script_weather::LandStorm storm)
{
	background.temperature = static_cast<int8_t>(temperature);
	background.overcast = static_cast<int8_t>(visibility);
	Applied applied {.background = background, .storm = std::nullopt};
	if (preset == nullptr)
	{
		return applied;
	}

	// The one number that each change of the background starts from
	float last = 0.0f;
	if (preset->makesStorm)
	{
		storm.cloudShade = preset->cloudShade.Apply(storm.cloudShade);
		if (storm.cloudShade > k_MostCloudShade)
		{
			storm.cloudShade = k_MostCloudShade;
		}
		storm.cloudHeight = preset->cloudHeight.Apply(storm.cloudHeight);
		storm.boltWait.y = preset->boltMost.Apply(storm.boltWait.y);
		storm.boltWait.x = preset->boltLeast.Apply(storm.boltWait.x);
		storm.thunderWait.y = preset->thunderMost.Apply(storm.thunderWait.y);
		storm.thunderWait.x = preset->thunderLeast.Apply(storm.thunderWait.x);
		last = preset->cloudCount.Apply(last);
		storm.cloudCount = CappedByte(last);
		storm.rainSpeed = preset->rainSpeed.Apply(storm.rainSpeed);
	}

	auto& weather = applied.background;
	last = preset->overcast.Apply(last);
	weather.overcast = CappedByte(last);
	last = preset->snowCover.Apply(last);
	weather.snowCover = CappedByte(last);
	last = preset->snowfall.Apply(last);
	weather.snow = CappedByte(last);
	last = preset->rain.Apply(last);
	weather.rain = CappedByte(last);
	last = preset->windX.Apply(last);
	weather.windX = CappedByte(last);
	last = preset->windZ.Apply(last);
	weather.windZ = CappedByte(last);

	if (preset->makesStorm)
	{
		// The storm brings the new background weather
		storm.effect = weather;
		storm.effect.stamp = 0;
		applied.storm = storm;
	}
	return applied;
}

uint8_t PictureOf(int32_t type)
{
	switch (type)
	{
	case 1:
		return 0;
	case 8:
	case 12:
		return 2;
	case 9:
		return 3;
	case 2:
	case 6:
		return 4;
	case 3:
		return 5;
	case 11:
		return 6;
	case 7:
		return 7;
	case 5:
	case 10:
		return 8;
	default:
		return 1;
	}
}

std::string PlaceText(std::string_view city, std::string_view area, std::string_view country)
{
	if (area.empty() || area == "(null)")
	{
		return fmt::format("{} ({})", city, country);
	}
	return fmt::format("{} ({},{})", city, area, country);
}

} // namespace openblack::ecs::real_weather
