/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Weather.h"

#include <cmath>

#include <algorithm>

#include <fmt/format.h>

#include "Camera/Camera.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/LightningFlash.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/Weather.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::weather_window;

namespace
{

ImVec4 Colour(const std::array<float, 4>& colour)
{
	return {colour[0], colour[1], colour[2], colour[3]};
}

ImVec4 Darker(ImVec4 colour, float by)
{
	return {colour.x * by, colour.y * by, colour.z * by, colour.w};
}

/// A labelled bar for an amount of 0 to 100
void AmountBar(const char* label, int value, ImVec4 colour)
{
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, colour);
	ImGui::ProgressBar(static_cast<float>(value) / 100.0f, ImVec2(-80.0f, 0.0f), fmt::format("{}%", value).c_str());
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextUnformatted(label);
}

/// A compass showing which way the wind blows and how hard
void WindCompass(float windX, float windZ, float radius)
{
	auto* drawList = ImGui::GetWindowDrawList();
	const auto corner = ImGui::GetCursorScreenPos();
	const ImVec2 centre {corner.x + radius, corner.y + radius};
	drawList->AddCircleFilled(centre, radius, IM_COL32(30, 40, 55, 255));
	drawList->AddCircle(centre, radius, IM_COL32(120, 140, 170, 255), 32, 1.5f);
	drawList->AddText(ImVec2(centre.x - 3.0f, corner.y + 1.0f), IM_COL32(160, 170, 190, 255), "N");
	const auto strength = std::sqrt((windX * windX) + (windZ * windZ));
	if (strength > 0.0f)
	{
		// Longer the harder it blows, at most to the rim at 100
		const auto length = std::min(strength / 100.0f, 1.0f) * (radius - 4.0f);
		const ImVec2 tip {centre.x + (windX / strength * length), centre.y - (windZ / strength * length)};
		drawList->AddLine(centre, tip, IM_COL32(120, 220, 255, 255), 2.5f);
		drawList->AddCircleFilled(tip, 3.5f, IM_COL32(120, 220, 255, 255));
	}
	ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}

/// Every storm on the island fades out, as the scripts' KILL_STORMS_IN_AREA does over an area
void ClearStorms()
{
	weather::storms::KillStormsInArea({k_IslandCentre.x, 0.0f, k_IslandCentre.y}, k_IslandOuterRadius);
}

} // namespace

Weather::Weather() noexcept
    : Window("Weather", ImVec2(460.0f, 470.0f))
{
}

void Weather::Draw() noexcept
{
	if (!Locator::weatherSystem::has_value())
	{
		ImGui::TextUnformatted("There is no weather on this island");
		return;
	}
	DrawAtCamera();
	ImGui::Separator();
	DrawPresets();
	ImGui::Separator();
	DrawCustom();
	ImGui::Separator();
	DrawActions();
	ImGui::Separator();
	DrawStorms();
}

void Weather::DrawAtCamera() noexcept
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	const auto camera = Locator::camera::value().GetOrigin();
	// Sampled smoothly, as the camera's weather is
	const auto here = weather::ComputeWeather(camera, true);

	ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "At the camera");
	ImGui::BeginGroup();
	WindCompass(static_cast<float>(here.windX), static_cast<float>(here.windZ), 36.0f);
	ImGui::EndGroup();
	ImGui::SameLine();
	ImGui::BeginGroup();
	AmountBar("Rain", here.rain, ImVec4(0.30f, 0.55f, 0.95f, 1.0f));
	AmountBar("Snow", here.snow, ImVec4(0.85f, 0.90f, 0.98f, 1.0f));
	// The snow lying on the ground, out of the most it lies
	AmountBar("Lying", LyingPercent(here.snowCover), ImVec4(0.70f, 0.80f, 0.95f, 1.0f));
	AmountBar("Cloud", here.overcast, ImVec4(0.55f, 0.58f, 0.62f, 1.0f));
	AmountBar("Flash", FlashPercent(weather::LightningFlashAtCamera(camera)), ImVec4(1.0f, 0.95f, 0.55f, 1.0f));
	ImGui::EndGroup();
	const auto temperature = static_cast<int>(here.temperature);
	const auto cold = temperature < 0;
	ImGui::TextColored(cold ? ImVec4(0.6f, 0.8f, 1.0f, 1.0f) : ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "%d C", temperature);
	ImGui::SameLine();
	ImGui::TextDisabled("wind (%d, %d)", here.windX, here.windZ);
}

void Weather::DrawPresets() noexcept
{
	ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Over the whole island");
	const auto columns = static_cast<float>(k_Presets.size());
	const auto width = (ImGui::GetContentRegionAvail().x - (ImGui::GetStyle().ItemSpacing.x * (columns - 1.0f))) / columns;
	for (size_t i = 0; i < k_Presets.size(); ++i)
	{
		const auto& preset = k_Presets.at(i);
		if (i != 0)
		{
			ImGui::SameLine();
		}
		const auto colour = Colour(preset.colour);
		ImGui::PushStyleColor(ImGuiCol_Button, Darker(colour, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Darker(colour, 0.75f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, colour);
		const auto pressed = ImGui::Button(preset.name.data(), ImVec2(width, 32.0f));
		ImGui::PopStyleColor(3);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", preset.tooltip.data());
		}
		if (!pressed)
		{
			continue;
		}
		// A preset becomes the island's weather: the other storms clear and the climates stop breeding new ones, so
		// only a thunderstorm brings thunder
		ClearStorms();
		weather::climate::SetStormCreationEnabled(false);
		if (!ForcesStorm(preset))
		{
			continue;
		}
		// The made-to-measure storm takes the preset, keeping its own length, so it can be tweaked from there
		ApplyPreset(preset, _storm);
		weather::storms::Create(IslandStorm(_storm));
	}
}

void Weather::DrawCustom() noexcept
{
	if (!ImGui::CollapsingHeader("Made to measure"))
	{
		return;
	}
	const auto byteSlider = [](const char* label, int8_t& value, int min, int max, const char* format) {
		int wide = value;
		if (ImGui::SliderInt(label, &wide, min, max, format))
		{
			value = static_cast<int8_t>(wide);
		}
	};
	byteSlider("Rain", _storm.rain, 0, 100, "%d%%");
	byteSlider("Cloud", _storm.overcast, 0, 100, "%d%%");
	byteSlider("Temperature", _storm.temperature, -30, 45, "%d C");
	ImGui::SliderFloat("Wind from", &_storm.windDegrees, 0.0f, 360.0f, "%.0f deg");
	ImGui::SliderInt("Wind strength", &_storm.windStrength, 0, 100);

	auto lightning = _storm.lightning;
	if (ImGui::Checkbox("Lightning", &lightning))
	{
		SetLightning(_storm, lightning);
	}
	if (_storm.lightning)
	{
		ImGui::DragFloatRange2("Thunder every", &_storm.thunderWait.x, &_storm.thunderWait.y, 0.1f, 0.5f, 120.0f, "%.1f s",
		                       "%.1f s");
		ImGui::DragFloatRange2("Bolt every", &_storm.boltWait.x, &_storm.boltWait.y, 0.1f, 0.5f, 120.0f, "%.1f s", "%.1f s");
	}

	ImGui::SliderFloat("Lasts", &_storm.seconds, 10.0f, 3600.0f, "%.0f s", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Comes in over", &_storm.fadeSeconds, 0.1f, 60.0f, "%.1f s", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Cloud height", &_storm.cloudHeight, 0.0f, 500.0f, "%.0f");
	ImGui::SliderFloat("Rain speed", &_storm.rainSpeed, 0.0f, 5.0f, "%.2f");

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.30f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.38f, 1.0f));
	if (ImGui::Button("Force this storm", ImVec2(-1.0f, 28.0f)))
	{
		weather::storms::Create(IslandStorm(_storm));
	}
	ImGui::PopStyleColor(2);
}

void Weather::DrawActions() noexcept
{
	if (ImGui::Button("Clear every storm"))
	{
		ClearStorms();
	}

	auto climates = weather::climate::IsClimateSystemEnabled();
	if (ImGui::Checkbox("Climates", &climates))
	{
		weather::climate::SetClimateSystemEnabled(climates);
	}
	ImGui::SameLine();
	auto storms = weather::climate::IsStormCreationEnabled();
	if (ImGui::Checkbox("Climates breed storms", &storms))
	{
		weather::climate::SetStormCreationEnabled(storms);
	}
}

void Weather::DrawStorms() noexcept
{
	size_t alive = 0;
	weather::storms::ForEach([&alive](const weather::storms::Storm& storm) { alive += storm.deleteCounter == 0 ? 1 : 0; });
	if (!ImGui::CollapsingHeader(fmt::format("Storms ({})###Storms", alive).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	constexpr auto k_Flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("storms", 6, k_Flags))
	{
		return;
	}
	ImGui::TableSetupColumn("Where");
	ImGui::TableSetupColumn("Radius");
	ImGui::TableSetupColumn("Life");
	ImGui::TableSetupColumn("Rain/snow/cloud");
	ImGui::TableSetupColumn("Lightning");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();
	// Ended once the walk over the storms is done
	weather::storms::StormId toEnd = weather::storms::k_NoStorm;
	weather::storms::ForEach([&toEnd](const weather::storms::Storm& storm) {
		const auto phase = PhaseOf(storm);
		if (phase == StormPhase::Ending)
		{
			return;
		}
		const auto& descriptor = storm.descriptor;
		ImGui::PushID(static_cast<int>(storm.id));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%.0f, %.0f", storm.drawPosition.x, storm.drawPosition.z);
		ImGui::TableNextColumn();
		ImGui::Text("%.0f", descriptor.outerRadius);
		ImGui::TableNextColumn();
		ImGui::ProgressBar(descriptor.lifeTime > 0.0f ? storm.age / descriptor.lifeTime : 0.0f, ImVec2(-1.0f, 0.0f),
		                   fmt::format("{:.0f}/{:.0f}s", storm.age, descriptor.lifeTime).c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%d/%d/%d", descriptor.weather.rain, descriptor.weather.snow, descriptor.weather.overcast);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(HasLightning(descriptor) ? "yes" : "-");
		ImGui::TableNextColumn();
		// A storm that is clearing has no more than its fading time left
		if (phase == StormPhase::Clearing)
		{
			ImGui::TextDisabled("clearing");
		}
		else if (ImGui::SmallButton("End"))
		{
			toEnd = storm.id;
		}
		ImGui::PopID();
	});
	ImGui::EndTable();
	if (toEnd != weather::storms::k_NoStorm)
	{
		weather::storms::MarkForDeletion(toEnd);
	}
}

void Weather::Update() noexcept {}

void Weather::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Weather::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
