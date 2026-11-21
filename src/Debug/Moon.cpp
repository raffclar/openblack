/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Moon.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <numbers>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "3D/DayNightClock.h"
#include "3D/SkyFrame.h"
#include "Common/DebugActRuns.h"
#include "Common/DebugMagicWeatherActRuns.h"
#include "Common/IntegerSteps.h"
#include "Common/MachineClock.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "Graphics/Moon.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
namespace moon = graphics::moon;
using player_commands::DebugSky;

/// Days in a moon month
constexpr double k_MonthDays = 1.0 / moon::k_MonthsPerDay;

/// The points of the moon month the phase buttons jump to
struct NamedPhase
{
	std::string_view name;
	double fraction;
};
constexpr std::array<NamedPhase, 4> k_NamedPhases {{
    {"New", 0.0},
    {"First quarter", 0.25},
    {"Full", 0.5},
    {"Last quarter", 0.75},
}};
constexpr std::array<std::string_view, 8> k_PhaseNames {
    "new moon",  "waxing crescent", "first quarter", "waxing gibbous",
    "full moon", "waning gibbous",  "last quarter",  "waning crescent",
};

/// The moon month's fraction kept between 0 and 1, as before 2000 it counts back from 0
double Wrapped(double fraction)
{
	return fraction - std::floor(fraction);
}

std::string_view PhaseName(double fraction)
{
	const auto index = static_cast<size_t>(std::floor((Wrapped(fraction) * 8.0) + 0.5)) % k_PhaseNames.size();
	return k_PhaseNames.at(index);
}

/// A seconds since 1970 time as a date and time of day, in universal time
std::string DateText(int64_t unixTime)
{
	// Days to a civil date, counting in 400 year eras from 1 March of year 0
	const int64_t days = (unixTime >= 0 ? unixTime : unixTime - (moon::k_SecondsPerDay - 1)) / moon::k_SecondsPerDay;
	const int64_t seconds = unixTime - (days * moon::k_SecondsPerDay);
	const int64_t shifted = days + 719468;
	const int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
	const int64_t dayOfEra = shifted - (era * 146097);
	const int64_t yearOfEra = (dayOfEra - (dayOfEra / 1460) + (dayOfEra / 36524) - (dayOfEra / 146096)) / 365;
	const int64_t dayOfYear = dayOfEra - ((365 * yearOfEra) + (yearOfEra / 4) - (yearOfEra / 100));
	const int64_t monthIndex = ((5 * dayOfYear) + 2) / 153;
	const int64_t day = dayOfYear - (((153 * monthIndex) + 2) / 5) + 1;
	const int64_t month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
	const int64_t year = yearOfEra + (era * 400) + (month <= 2 ? 1 : 0);
	return fmt::format("{:04}-{:02}-{:02} {:02}:{:02} UTC", year, month, day, seconds / 3600, (seconds / 60) % 60);
}

/// A small picture of how much of the moon is lit at a point of the moon month, waxing from the right
void DrawPhasePicture(double fraction)
{
	constexpr float k_Radius = 28.0f;
	constexpr int k_Steps = 32;
	auto* drawList = ImGui::GetWindowDrawList();
	const ImVec2 corner = ImGui::GetCursorScreenPos();
	const ImVec2 centre {corner.x + k_Radius + 2.0f, corner.y + k_Radius + 2.0f};
	drawList->AddCircleFilled(centre, k_Radius, IM_COL32(40, 44, 56, 255), k_Steps);

	// The lit side is the right while waxing and the left while waning; the line between light and dark is half an
	// ellipse whose width follows the cosine of the month's angle
	const double wrapped = Wrapped(fraction);
	const float side = wrapped < 0.5 ? 1.0f : -1.0f;
	const auto terminator = static_cast<float>(std::cos(wrapped * 2.0 * std::numbers::pi));
	std::array<ImVec2, (k_Steps + 1) * 2> points {};
	for (int i = 0; i <= k_Steps; ++i)
	{
		const float angle = std::numbers::pi_v<float> * (static_cast<float>(i) / static_cast<float>(k_Steps) - 0.5f);
		const float x = std::cos(angle) * k_Radius;
		const float y = std::sin(angle) * k_Radius;
		points.at(static_cast<size_t>(i)) = {centre.x + (side * x), centre.y + y};
		points.at(static_cast<size_t>((2 * k_Steps) + 1 - i)) = {centre.x + (side * x * terminator), centre.y + y};
	}
	// Filled as thin strips between the limb and the terminator, which keeps each piece convex
	for (int i = 0; i < k_Steps; ++i)
	{
		const std::array<ImVec2, 4> strip {points.at(static_cast<size_t>(i)), points.at(ToIndex(i + 1)),
		                                   points.at(static_cast<size_t>((2 * k_Steps) - i)),
		                                   points.at(static_cast<size_t>((2 * k_Steps) + 1 - i))};
		drawList->AddConvexPolyFilled(strip.data(), static_cast<int>(strip.size()), IM_COL32(235, 232, 210, 255));
	}
	drawList->AddCircle(centre, k_Radius, IM_COL32(120, 125, 140, 255), k_Steps);
	ImGui::Dummy(ImVec2((k_Radius * 2.0f) + 4.0f, (k_Radius * 2.0f) + 4.0f));
}

/// The date the moon takes its phase from, or the computer's before it has read one
int64_t DateInUse(const ecs::components::Moon& theMoon)
{
	const auto date = sky_frame::MoonDate(theMoon);
	return date != 0 ? date : machine_clock::UnixTime();
}

/// The moon entity's state, none without a sky
ecs::components::Moon* MoonOfTheSky()
{
	if (!Locator::skySystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	return Locator::entitiesRegistry::value().TryGet<ecs::components::Moon>(Locator::skySystem::value().GetMoon());
}
} // namespace

Moon::Moon() noexcept
    : Window("Moon", ImVec2(420.0f, 560.0f))
{
}

void Moon::Draw() noexcept
{
	if (MoonOfTheSky() == nullptr)
	{
		ImGui::TextUnformatted("No moon: the game has not started.");
		return;
	}
	DrawPhase();
	ImGui::Separator();
	DrawSky();
	ImGui::Separator();
	DrawTimeControls();
	ImGui::Separator();
	DrawDateControls();
}

void Moon::DrawPhase() noexcept
{
	const auto& theMoon = *MoonOfTheSky();
	const auto date = DateInUse(theMoon);
	const double fraction = moon::MonthFraction(date);
	const float phase = moon::Phase(date);

	ImGui::TextUnformatted("Phase");
	DrawPhasePicture(fraction);
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::Text("%s", std::string(PhaseName(fraction)).c_str());
	ImGui::Text("Day %.1f of %.2f in the moon month", Wrapped(fraction) * k_MonthDays, k_MonthDays);
	ImGui::Text("Phase %.4f (%.1f degrees)", static_cast<double>(phase), static_cast<double>(phase) * 180.0 / std::numbers::pi);
	ImGui::EndGroup();
	ImGui::ProgressBar(static_cast<float>(Wrapped(fraction)), ImVec2(-1.0f, 0.0f),
	                   fmt::format("{:.1f}% through the moon month", Wrapped(fraction) * 100.0).c_str());

	const char* source =
	    theMoon.dateOverride ? " (overridden)" : (theMoon.date == 0 ? " (not read yet: the moon has not shown)" : "");
	ImGui::Text("Date used: %s%s", DateText(date).c_str(), source);
	ImGui::Text("Scripts are told %.3f (from the phase it last showed with, %.4f)",
	            static_cast<double>(moon::ScriptPercentage(theMoon.phase)), static_cast<double>(theMoon.phase));
}

void Moon::DrawSky() noexcept
{
	const auto& theMoon = *MoonOfTheSky();
	const auto& clock = Locator::skyClockSystem::value().GetClock();
	ImGui::TextUnformatted("In the sky");
	ImGui::Text("Script hour %.2f, visual hour %.2f, clock %s", static_cast<double>(clock.GetScriptTime()),
	            static_cast<double>(clock.GetVisualTime()), clock.IsRunning() ? "running" : "stopped");
	// Where it would stand at this hour, whether or not it shows
	const auto where = moon::Offset(clock.GetScriptTime());
	const auto angles = moon::Angles(where);
	ImGui::Text("From the camera: %.0f east, %.0f up, %.0f north", static_cast<double>(where.x), static_cast<double>(where.y),
	            static_cast<double>(where.z));
	ImGui::Text("Azimuth %.1f degrees, elevation %.1f degrees", static_cast<double>(angles.azimuth),
	            static_cast<double>(angles.elevation));
	if (const auto& placement = theMoon.placement)
	{
		ImGui::Text("Showing: strength %.0f of 200, %.0f through the overcast", static_cast<double>(placement->alpha),
		            static_cast<double>(theMoon.strength));
	}
	else
	{
		ImGui::TextUnformatted("Down: it shows from about 19:20 to 04:40 script time");
	}
}

void Moon::DrawTimeControls() noexcept
{
	const auto& clock = Locator::skyClockSystem::value().GetClock();
	const bool network = debug_acts::SendsCommands();
	const auto setHour = [](float hour) { debug_acts::Do(DebugSky {.act = DebugSky::Act::ScriptHour, .hour = hour}); };
	ImGui::TextUnformatted("Time of day");
	float hour = _hourDrag.value_or(clock.GetScriptTime());
	if (ImGui::SliderFloat("Script hour", &hour, 0.0f, 24.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
	{
		if (network)
		{
			_hourDrag = hour;
		}
		else
		{
			setHour(hour);
		}
	}
	if (ImGui::IsItemDeactivated() && _hourDrag.has_value())
	{
		setHour(*_hourDrag);
		_hourDrag.reset();
	}
	bool running = clock.IsRunning();
	if (ImGui::Checkbox("Clock runs", &running))
	{
		debug_acts::Do(DebugSky {.act = DebugSky::Act::ClockRuns, .on = running});
	}
	ImGui::SameLine();
	if (ImGui::Button("Midnight"))
	{
		setHour(0.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Evening (19:30)"))
	{
		setHour(19.5f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Noon"))
	{
		setHour(12.0f);
	}
}

void Moon::DrawDateControls() noexcept
{
	const auto& theMoon = *MoonOfTheSky();
	const auto date = DateInUse(theMoon);
	const bool network = debug_acts::SendsCommands();
	const auto setDate = [](int64_t value) { debug_acts::Do(DebugSky {.act = DebugSky::Act::MoonDate, .date = value}); };
	ImGui::TextUnformatted("Date for the phase");
	bool overridden = theMoon.dateOverride.has_value();
	if (ImGui::Checkbox("Override the computer's date", &overridden))
	{
		if (overridden)
		{
			setDate(date);
		}
		else
		{
			debug_acts::Do(DebugSky {.act = DebugSky::Act::ComputerDate});
		}
	}

	auto day = _dayDrag.value_or(static_cast<float>(Wrapped(moon::MonthFraction(date)) * k_MonthDays));
	if (ImGui::SliderFloat("Day of the moon month", &day, 0.0f, static_cast<float>(k_MonthDays), "%.0f"))
	{
		if (network)
		{
			_dayDrag = day;
		}
		else
		{
			setDate(moon::DateAtFraction(date, static_cast<double>(day) / k_MonthDays));
		}
	}
	if (ImGui::IsItemDeactivated() && _dayDrag.has_value())
	{
		setDate(moon::DateAtFraction(date, static_cast<double>(*_dayDrag) / k_MonthDays));
		_dayDrag.reset();
	}
	if (ImGui::Button("< Day"))
	{
		setDate(date - moon::k_SecondsPerDay);
	}
	ImGui::SameLine();
	if (ImGui::Button("Day >"))
	{
		setDate(date + moon::k_SecondsPerDay);
	}
	for (const auto& phase : k_NamedPhases)
	{
		ImGui::SameLine();
		if (ImGui::Button(std::string(phase.name).c_str()))
		{
			setDate(moon::DateAtFraction(date, phase.fraction));
		}
	}

	if (ImGui::Button("Reset to the game's values"))
	{
		debug_acts::Do(DebugSky {.act = DebugSky::Act::Reset});
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The computer's date for the phase again, and the clock running");
	}
}
