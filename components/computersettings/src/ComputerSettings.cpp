/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerSettings.h"

#include <cmath>

#include <array>
#include <limits>

#include <nlohmann/json.hpp>

namespace openblack::computer_settings
{
namespace
{

using Json = nlohmann::json;

constexpr std::array<std::string_view, 3> k_WindowModes = {"windowed", "fullscreen", "borderless"};
constexpr std::string_view k_CustomDetail = "custom_detail";

/// Reads the members of an object one at a time, and keeps a line for each value that it refuses
class Reader
{
public:
	Reader(const Json& object, std::string_view prefix, std::vector<std::string>& problems)
	    : _object(object)
	    , _prefix(prefix)
	    , _problems(problems)
	{
	}

	void Bool(std::string_view key, std::optional<bool>& out)
	{
		if (const auto* value = Find(key); value != nullptr)
		{
			if (value->is_boolean())
			{
				out = value->get<bool>();
			}
			else
			{
				Refuse(key, "is not true or false");
			}
		}
	}

	template <typename T>
	void Integer(std::string_view key, std::optional<T>& out, int64_t lowest, int64_t highest)
	{
		if (const auto* value = Find(key); value != nullptr)
		{
			if (!value->is_number_integer())
			{
				Refuse(key, "is not a whole number");
				return;
			}
			const auto number = value->get<int64_t>();
			if (number < lowest || number > highest)
			{
				Refuse(key, "is out of its range");
				return;
			}
			out = static_cast<T>(number);
		}
	}

	/// A number from above lowest (or from lowest when it may be equal) up to highest
	void Number(std::string_view key, std::optional<float>& out, double lowest, bool lowestAllowed, double highest)
	{
		if (const auto* value = Find(key); value != nullptr)
		{
			if (!value->is_number())
			{
				Refuse(key, "is not a number");
				return;
			}
			const auto number = value->get<double>();
			const bool aboveLowest = lowestAllowed ? number >= lowest : number > lowest;
			if (!std::isfinite(number) || !aboveLowest || number > highest)
			{
				Refuse(key, "is out of its range");
				return;
			}
			out = static_cast<float>(number);
		}
	}

	void WindowMode(std::string_view key, std::optional<std::string>& out)
	{
		if (const auto* value = Find(key); value != nullptr)
		{
			if (!value->is_string() || std::ranges::find(k_WindowModes, value->get<std::string>()) == k_WindowModes.end())
			{
				Refuse(key, "is not windowed, fullscreen or borderless");
				return;
			}
			out = value->get<std::string>();
		}
	}

private:
	[[nodiscard]] const Json* Find(std::string_view key) const
	{
		const auto found = _object.find(key);
		return found != _object.end() ? &*found : nullptr;
	}

	void Refuse(std::string_view key, std::string_view why)
	{
		_problems.push_back(std::string(_prefix) + std::string(key) + " " + std::string(why));
	}

	const Json& _object;
	std::string_view _prefix;
	std::vector<std::string>& _problems;
};

void ReadCustomDetail(const Json& object, CustomDetail& detail, std::vector<std::string>& problems)
{
	Reader read(object, "custom_detail.", problems);
	read.Number("model_detail", detail.modelDetail, 0.0, false, 1.0);
	read.Bool("small_bump", detail.smallBump);
	read.Bool("clouds", detail.clouds);
	read.Bool("cloud_shadows", detail.cloudShadows);
	read.Number("water_tiling", detail.waterTiling, 0.0, true, 1.0);
	read.Bool("land_reflection", detail.landReflection);
	read.Bool("temple_reflections", detail.templeReflections);
	read.Bool("temple_lightmaps", detail.templeLightmaps);
	read.Bool("temple_glows", detail.templeGlows);
	read.Bool("temple_volume_light", detail.templeVolumeLight);
	read.Bool("temple_people", detail.templePeople);
	read.Integer("rain_splashes", detail.rainSplashes, 0, std::numeric_limits<uint8_t>::max());
	read.Bool("fog", detail.fog);
	read.Bool("weather", detail.weather);
	read.Bool("object_lighting", detail.objectLighting);
	read.Bool("object_shadows", detail.objectShadows);
	read.Bool("low_res_textures", detail.lowResTextures);
	read.Bool("coarse_land", detail.coarseLand);
}

template <typename T>
void Put(Json& object, std::string_view key, const std::optional<T>& value)
{
	if (value.has_value())
	{
		object[std::string(key)] = *value;
	}
}

/// A float as the shortest decimal that reads back as the same float
double Decimal(float value)
{
	for (int digits = 1; digits < 10; ++digits)
	{
		const double scale = std::pow(10.0, digits);
		const double rounded = std::round(static_cast<double>(value) * scale) / scale;
		if (static_cast<float>(rounded) == value)
		{
			return rounded;
		}
	}
	return static_cast<double>(value);
}

void PutFloat(Json& object, std::string_view key, const std::optional<float>& value)
{
	if (value.has_value())
	{
		object[std::string(key)] = Decimal(*value);
	}
}

} // namespace

ParseResult Parse(std::string_view text)
{
	ParseResult result;
	const auto json = Json::parse(text.begin(), text.end(), nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		result.problems.emplace_back("the file is not a JSON object");
		return result;
	}

	auto& settings = result.settings;
	Reader read(json, "", result.problems);
	read.Integer("detail_level", settings.detailLevel, 0, 6);
	read.Integer("max_detail_level", settings.maxDetailLevel, 1, 7);
	read.Number("effects_volume", settings.effectsVolume, 0.0, true, 1.0);
	read.Number("music_volume", settings.musicVolume, 0.0, true, 1.0);
	read.Integer("width", settings.width, 1, std::numeric_limits<uint16_t>::max());
	read.Integer("height", settings.height, 1, std::numeric_limits<uint16_t>::max());
	read.WindowMode("window_mode", settings.windowMode);
	read.Bool("vsync", settings.vsync);
	if (const auto custom = json.find(k_CustomDetail); custom != json.end())
	{
		if (custom->is_object())
		{
			ReadCustomDetail(*custom, settings.customDetail, result.problems);
		}
		else
		{
			result.problems.emplace_back("custom_detail is not an object");
		}
	}
	return result;
}

std::string Update(std::string_view oldText, const Settings& changed)
{
	auto json = Json::parse(oldText.begin(), oldText.end(), nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		json = Json::object();
	}

	Put(json, "detail_level", changed.detailLevel);
	Put(json, "max_detail_level", changed.maxDetailLevel);
	PutFloat(json, "effects_volume", changed.effectsVolume);
	PutFloat(json, "music_volume", changed.musicVolume);
	Put(json, "width", changed.width);
	Put(json, "height", changed.height);
	Put(json, "window_mode", changed.windowMode);
	Put(json, "vsync", changed.vsync);

	const auto& detail = changed.customDetail;
	if (detail != CustomDetail {})
	{
		auto& custom = json[std::string(k_CustomDetail)];
		if (!custom.is_object())
		{
			custom = Json::object();
		}
		PutFloat(custom, "model_detail", detail.modelDetail);
		Put(custom, "small_bump", detail.smallBump);
		Put(custom, "clouds", detail.clouds);
		Put(custom, "cloud_shadows", detail.cloudShadows);
		PutFloat(custom, "water_tiling", detail.waterTiling);
		Put(custom, "land_reflection", detail.landReflection);
		Put(custom, "temple_reflections", detail.templeReflections);
		Put(custom, "temple_lightmaps", detail.templeLightmaps);
		Put(custom, "temple_glows", detail.templeGlows);
		Put(custom, "temple_volume_light", detail.templeVolumeLight);
		Put(custom, "temple_people", detail.templePeople);
		Put(custom, "rain_splashes", detail.rainSplashes);
		Put(custom, "fog", detail.fog);
		Put(custom, "weather", detail.weather);
		Put(custom, "object_lighting", detail.objectLighting);
		Put(custom, "object_shadows", detail.objectShadows);
		Put(custom, "low_res_textures", detail.lowResTextures);
		Put(custom, "coarse_land", detail.coarseLand);
	}

	// Text that is not valid UTF-8 is replaced, because the library must not throw
	return json.dump(4, ' ', false, Json::error_handler_t::replace) + "\n";
}

StartValues Resolve(const StartValues& commandLine, const GivenOnCommandLine& given, const Settings& file)
{
	StartValues values = commandLine;
	if (!given.width && file.width.has_value())
	{
		values.width = *file.width;
	}
	if (!given.height && file.height.has_value())
	{
		values.height = *file.height;
	}
	if (!given.windowMode && file.windowMode.has_value())
	{
		values.windowMode = *file.windowMode;
	}
	if (!given.vsync && file.vsync.has_value())
	{
		values.vsync = *file.vsync;
	}
	if (!given.detailLevel && file.detailLevel.has_value())
	{
		values.detailLevel = *file.detailLevel;
	}
	if (file.maxDetailLevel.has_value())
	{
		values.maxDetailLevel = *file.maxDetailLevel;
	}
	return values;
}

} // namespace openblack::computer_settings
