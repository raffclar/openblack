/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldProviders.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <numbers>
#include <string>
#include <utility>

#include "Graphics/Moon.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{

/// The days of the moon's month, new moon to new moon
constexpr double k_MoonMonthDays = 29.530588853;

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

bool ContainsAnyCase(std::string_view text, std::string_view part)
{
	return Lower(text).find(Lower(part)) != std::string::npos;
}

Json Point(const glm::vec3& point)
{
	return {point.x, point.y, point.z};
}

Json EffectItem(const inspector::ParticleEffectInfo& effect)
{
	return {
	    {"kind", "effect"},
	    {"id", effect.id},
	    {"file", effect.file},
	    {"position", Point(effect.origin)},
	    {"age", effect.age},
	    {"particles", effect.atoms},
	    {"collections", effect.collections},
	    {"closing", effect.closing},
	    {"owned_by_spell", effect.ownedBySpell},
	    {"seconds_left", effect.secondsLeft.has_value() ? Json(*effect.secondsLeft) : Json(nullptr)},
	};
}

} // namespace

Json openblack::inspector::MoonState(float scriptHour, int64_t unixTime, glm::vec3 cameraOrigin)
{
	const auto placement = graphics::moon::Place(scriptHour);
	const float phase = graphics::moon::Phase(unixTime);
	// The phase runs down from a full turn at the new moon
	const double fraction = 1.0 - (static_cast<double>(phase) / (2.0 * std::numbers::pi));
	const auto day = static_cast<int>(std::floor(std::clamp(fraction, 0.0, 1.0) * k_MoonMonthDays));
	return {
	    {"position", placement.has_value() ? Point(cameraOrigin + placement->offset) : Json(nullptr)},
	    {"phase", phase},
	    {"cycle_day", day},
	    {"visible", placement.has_value()},
	};
}

SkyProvider::SkyProvider(SkySources sources)
    : _sources(std::move(sources))
{
}

std::vector<QueryDescription> SkyProvider::Describe() const
{
	return {
	    {.name = "moon",
	     .description = "The moon: its position in the world (null while down), phase, day of its month and whether it is "
	                    "up",
	     .parameters = {},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	};
}

QueryResult SkyProvider::Run(std::string_view query, const QueryContext& /*context*/)
{
	if (query != "moon")
	{
		return QueryResult::Error("no query sky." + std::string(query));
	}
	const auto hour = _sources.scriptHour ? _sources.scriptHour() : std::nullopt;
	if (!hour.has_value())
	{
		return QueryResult::Error("there is no sky: no land is loaded");
	}
	const auto camera = _sources.cameraOrigin ? _sources.cameraOrigin() : std::nullopt;
	const auto now = _sources.unixTime ? _sources.unixTime() : 0;
	return QueryResult::Value(MoonState(*hour, now, camera.value_or(glm::vec3(0.0f))));
}

Json openblack::inspector::SplashItems(std::span<const water_rings::Ring> rings, std::span<const ParticleEffectInfo> effects)
{
	Json items = Json::array();
	for (size_t i = 0; i < rings.size(); ++i)
	{
		const auto& ring = rings[i];
		items.push_back({
		    {"kind", "ring"},
		    {"id", i},
		    {"position", Point(ring.position)},
		    {"age_ms", ring.age},
		    {"life_ms", water_rings::k_Life},
		    {"growth", ring.growth},
		    {"alpha", water_rings::Alpha(ring)},
		});
	}
	for (const auto& effect : effects)
	{
		if (ContainsAnyCase(effect.file, "splash"))
		{
			items.push_back(EffectItem(effect));
		}
	}
	return items;
}

Json openblack::inspector::EffectItems(std::span<const ParticleEffectInfo> effects, std::optional<std::string_view> file)
{
	Json items = Json::array();
	for (const auto& effect : effects)
	{
		if (!file.has_value() || ContainsAnyCase(effect.file, *file))
		{
			items.push_back(EffectItem(effect));
		}
	}
	return items;
}

ParticlesProvider::ParticlesProvider(ParticleSources sources)
    : _sources(std::move(sources))
{
}

std::vector<QueryDescription> ParticlesProvider::Describe() const
{
	return {
	    {.name = "splash",
	     .description = "The splashes within the radius of a point, nearest first: rings on the water (age, life, "
	                    "growth, alpha) and splash effects (age, particle count, closing)",
	     .parameters = {},
	     .kind = ResultKind::List,
	     .needsNear = true},
	    {.name = "effects",
	     .description = "The running particle effects: file, place, age, particle count, closing",
	     .parameters = {{.name = "file",
	                     .type = "string",
	                     .description = "Only effects whose file name holds this text, in any case",
	                     .required = false}},
	     .kind = ResultKind::List,
	     .needsNear = false},
	};
}

QueryResult ParticlesProvider::Run(std::string_view query, const QueryContext& context)
{
	const auto effects = _sources.effects ? _sources.effects() : std::vector<ParticleEffectInfo> {};
	if (query == "splash")
	{
		const auto rings = _sources.rings ? _sources.rings() : std::span<const water_rings::Ring> {};
		return QueryResult::Value(SplashItems(rings, effects));
	}
	if (query == "effects")
	{
		const auto file = StringMember(context.params, "file");
		return QueryResult::Value(
		    EffectItems(effects, file.has_value() ? std::optional<std::string_view>(*file) : std::nullopt));
	}
	return QueryResult::Error("no query particles." + std::string(query));
}
