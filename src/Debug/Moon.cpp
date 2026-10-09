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
#include "3D/SkyInterface.h"
#include "ECS/Systems/MoonSystemInterface.h"
#include "Graphics/Moon.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
namespace moon = graphics::moon;

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
		const std::array<ImVec2, 4> strip {points.at(static_cast<size_t>(i)), points.at(static_cast<size_t>(i + 1)),
		                                   points.at(static_cast<size_t>((2 * k_Steps) - i)),
		                                   points.at(static_cast<size_t>((2 * k_Steps) + 1 - i))};
		drawList->AddConvexPolyFilled(strip.data(), static_cast<int>(strip.size()), IM_COL32(235, 232, 210, 255));
	}
	drawList->AddCircle(centre, k_Radius, IM_COL32(120, 125, 140, 255), k_Steps);
	ImGui::Dummy(ImVec2((k_Radius * 2.0f) + 4.0f, (k_Radius * 2.0f) + 4.0f));
}
} // namespace

Moon::Moon() noexcept
    : Window("Moon", ImVec2(420.0f, 560.0f))
{
}

void Moon::Draw() noexcept
{
	if (!Locator::moonSystem::has_value() || !Locator::skySystem::has_value())
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
	const auto& moonSystem = Locator::moonSystem::value();
	const auto date = moonSystem.GetDate();
	const double fraction = moon::MonthFraction(date);

	ImGui::TextUnformatted("Phase");
	DrawPhasePicture(fraction);
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::Text("%s", std::string(PhaseName(fraction)).c_str());
	ImGui::Text("Day %.1f of %.2f in the moon month", Wrapped(fraction) * k_MonthDays, k_MonthDays);
	ImGui::Text("Phase %.4f (%.1f degrees)", static_cast<double>(moonSystem.GetPhase()),
	            static_cast<double>(moonSystem.GetPhase()) * 180.0 / std::numbers::pi);
	ImGui::EndGroup();
	ImGui::ProgressBar(static_cast<float>(Wrapped(fraction)), ImVec2(-1.0f, 0.0f),
	                   fmt::format("{:.1f}% through the moon month", Wrapped(fraction) * 100.0).c_str());

	ImGui::Text("Date used: %s%s", DateText(date).c_str(), moonSystem.GetDateOverride() ? " (overridden)" : "");
	ImGui::Text("Scripts are told %.3f (from the phase it last showed with, %.4f)",
	            static_cast<double>(moonSystem.GetScriptPercentage()), static_cast<double>(moonSystem.GetShownPhase()));
}

void Moon::DrawSky() noexcept
{
	const auto& moonSystem = Locator::moonSystem::value();
	const auto& clock = Locator::skySystem::value().GetClock();
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
	const auto placement = moonSystem.GetPlacement();
	if (placement)
	{
		ImGui::Text("Showing: strength %.0f of 200, %.0f through the overcast", static_cast<double>(placement->alpha),
		            static_cast<double>(moonSystem.GetStrength()));
	}
	else
	{
		ImGui::TextUnformatted("Down: it shows from about 19:20 to 04:40 script time");
	}
}

void Moon::DrawTimeControls() noexcept
{
	auto& clock = Locator::skySystem::value().GetClock();
	ImGui::TextUnformatted("Time of day");
	float hour = clock.GetScriptTime();
	if (ImGui::SliderFloat("Script hour", &hour, 0.0f, 24.0f, "%.2f"))
	{
		clock.SetScriptTime(hour);
	}
	bool running = clock.IsRunning();
	if (ImGui::Checkbox("Clock runs", &running))
	{
		clock.SetRunning(running);
	}
	ImGui::SameLine();
	if (ImGui::Button("Midnight"))
	{
		clock.SetScriptTime(0.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Evening (19:30)"))
	{
		clock.SetScriptTime(19.5f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Noon"))
	{
		clock.SetScriptTime(12.0f);
	}
}

void Moon::DrawDateControls() noexcept
{
	auto& moonSystem = Locator::moonSystem::value();
	const auto date = moonSystem.GetDate();
	ImGui::TextUnformatted("Date for the phase");
	bool overridden = moonSystem.GetDateOverride().has_value();
	if (ImGui::Checkbox("Override the computer's date", &overridden))
	{
		moonSystem.SetDateOverride(overridden ? std::optional(date) : std::nullopt);
	}

	auto day = static_cast<float>(Wrapped(moon::MonthFraction(date)) * k_MonthDays);
	if (ImGui::SliderFloat("Day of the moon month", &day, 0.0f, static_cast<float>(k_MonthDays), "%.0f"))
	{
		moonSystem.SetDateOverride(moon::DateAtFraction(date, static_cast<double>(day) / k_MonthDays));
	}
	if (ImGui::Button("< Day"))
	{
		moonSystem.SetDateOverride(date - moon::k_SecondsPerDay);
	}
	ImGui::SameLine();
	if (ImGui::Button("Day >"))
	{
		moonSystem.SetDateOverride(date + moon::k_SecondsPerDay);
	}
	for (const auto& phase : k_NamedPhases)
	{
		ImGui::SameLine();
		if (ImGui::Button(std::string(phase.name).c_str()))
		{
			moonSystem.SetDateOverride(moon::DateAtFraction(date, phase.fraction));
		}
	}

	if (ImGui::Button("Reset to the game's values"))
	{
		moonSystem.SetDateOverride(std::nullopt);
		Locator::skySystem::value().GetClock().SetRunning(true);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The computer's date for the phase again, and the clock running");
	}
}
