/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Gestures.h"

#include <string>

#include <glm/vec2.hpp>

#include "GesturesModel.h"
#include "Locator.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::gestures_window;
namespace gestures = openblack::magic::gestures;

namespace
{
constexpr ImU32 k_PathColour = IM_COL32(255, 255, 255, 160);
constexpr ImU32 k_CornerColour = IM_COL32(255, 220, 60, 255);
constexpr ImU32 k_StartColour = IM_COL32(80, 255, 80, 255);
constexpr ImU32 k_EndColour = IM_COL32(255, 80, 80, 255);
constexpr ImU32 k_MatchColour = IM_COL32(60, 200, 255, 220);
/// How wide a template is drawn, in pixels
constexpr float k_DrawnSize = 320.0f;

/// The game window's size in pixels, which the mouse positions are measured in
glm::vec2 WindowSize()
{
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		return {static_cast<float>(size.x), static_cast<float>(size.y)};
	}
	return {ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y};
}

/// From the game window's pixels to ImGui's
ImVec2 Display(float x, float y)
{
	const auto window = WindowSize();
	const auto& display = ImGui::GetIO().DisplaySize;
	const float scaleX = window.x > 0.0f ? display.x / window.x : 1.0f;
	const float scaleY = window.y > 0.0f ? display.y / window.y : 1.0f;
	return {x * scaleX, y * scaleY};
}

ImU32 KeyPointColour(KeyPoint key)
{
	switch (key)
	{
	case KeyPoint::Start:
		return k_StartColour;
	case KeyPoint::Corner:
		return k_CornerColour;
	case KeyPoint::End:
		return k_EndColour;
	default:
		return 0u;
	}
}
} // namespace

Gestures::Gestures() noexcept
    : Window("Gestures", ImVec2(460.0f, 520.0f))
{
}

void Gestures::Draw() noexcept
{
	ImGui::Checkbox("Draw the hand's path over the screen", &_overlay);
	if (_overlay)
	{
		DrawOverlay();
	}
	DrawState();
	ImGui::Separator();
	DrawTools();
}

void Gestures::DrawOverlay() const noexcept
{
	const auto& path = gestures::State().system;
	auto* list = ImGui::GetForegroundDrawList();
	for (int i = 1; i < path.Count(); ++i)
	{
		const auto& from = path.At(i - 1);
		const auto& to = path.At(i);
		list->AddLine(Display(from.sx, from.sz), Display(to.sx, to.sz), k_PathColour, 2.0f);
	}
	for (int i = 0; i < path.Count(); ++i)
	{
		const auto& sample = path.At(i);
		if (const auto colour = KeyPointColour(KeyPointOf(sample.flags)); colour != 0u)
		{
			list->AddCircleFilled(Display(sample.sx, sample.sz), 5.0f, colour);
		}
	}

	// The gestures the path matches now, by its newest sample
	if (path.Count() == 0)
	{
		return;
	}
	const auto& templates = gestures::Templates();
	const auto ratio = gestures::sampling::ScreenRatio();
	const auto matches = MatchesNow(templates, gestures::BuildFromSystem(path, ratio), ratio);
	auto position = Display(path.At(path.Count() - 1).sx, path.At(path.Count() - 1).sz);
	position.x += 10.0f;
	for (const auto& match : matches)
	{
		const auto label = std::string(GestureName(match.gesture)) + (match.mirrored ? " mirrored" : "");
		list->AddText(position, k_MatchColour, label.c_str());
		position.y += ImGui::GetTextLineHeight();
	}
}

void Gestures::DrawState() noexcept
{
	const auto& state = gestures::State();
	const auto& templates = gestures::Templates();
	const auto ratio = gestures::sampling::ScreenRatio();
	const auto keyPoints = gestures::BuildFromSystem(state.system, ratio);
	ImGui::Text("%zu templates, %d points recorded, %d key points", templates.size(), static_cast<int>(state.system.Count()),
	            static_cast<int>(keyPoints.count));
	if (state.circlePending)
	{
		ImGui::Text("Circle (%s) at %.1f, %.1f, %.1f, size %.1f, held for %.1f s", GestureName(state.circleGesture).data(),
		            static_cast<double>(state.circlePosition.x), static_cast<double>(state.circlePosition.y),
		            static_cast<double>(state.circlePosition.z), static_cast<double>(state.circleSize),
		            static_cast<double>(state.circleTimer));
	}
	if (state.selection.open)
	{
		ImGui::Text("Miracle selection open: %s, stage %u, %.1f s", GestureName(state.selection.category).data(),
		            state.selection.stage, static_cast<double>(state.selection.timer));
	}
	if (state.cooldown > 0.0f)
	{
		ImGui::Text("Resting for %.2f s after a gesture", static_cast<double>(state.cooldown));
	}

	ImGui::SeparatorText("Waiting for");
	bool waiting = false;
	for (size_t gesture = 0; gesture < state.lookingFor.size(); ++gesture)
	{
		const auto purpose = LookingForName(state.lookingFor.at(gesture).type);
		if (!purpose.empty())
		{
			waiting = true;
			ImGui::BulletText("%s: %s", GestureName(static_cast<gestures::Gesture>(gesture)).data(), purpose.c_str());
		}
	}
	if (!waiting)
	{
		ImGui::TextDisabled("nothing");
	}

	ImGui::SeparatorText("Matches now");
	const auto matches = MatchesNow(templates, keyPoints, ratio);
	if (matches.empty())
	{
		ImGui::TextDisabled("nothing");
	}
	for (const auto& match : matches)
	{
		ImGui::BulletText("%s (template %d)%s", GestureName(match.gesture).data(), static_cast<int>(match.templateIndex),
		                  match.mirrored ? " mirrored" : "");
	}

	ImGui::SeparatorText("Last recognised");
	if (state.result.gesture != gestures::k_None)
	{
		ImGui::Text("%s by template %d%s, key points %d to %d", GestureName(state.result.gesture).data(),
		            static_cast<int>(state.result.templateIndex), state.result.reversed ? " mirrored" : "",
		            static_cast<int>(state.result.start), static_cast<int>(state.result.end));
		const auto& packet = state.gesture;
		ImGui::Text("at %.1f, %.1f, %.1f, size %.1f", static_cast<double>(packet.position.x),
		            static_cast<double>(packet.position.y), static_cast<double>(packet.position.z),
		            static_cast<double>(packet.size));
	}
	else
	{
		ImGui::TextDisabled("nothing yet");
	}
}

void Gestures::DrawTools() noexcept
{
	// Draw a gesture through the recogniser, from its first template, as the mouse would
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##gesture", GestureName(static_cast<gestures::Gesture>(_gesture)).data()))
	{
		for (int i = 1; i < static_cast<int>(gestures::k_GestureCount); ++i)
		{
			if (ImGui::Selectable(GestureName(static_cast<gestures::Gesture>(i)).data(), i == _gesture))
			{
				_gesture = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	const auto* gestureTemplate = FirstTemplate(gestures::Templates(), static_cast<gestures::Gesture>(_gesture));
	const bool playing = gestures::sampling::PlayingStroke();
	ImGui::BeginDisabled(gestureTemplate == nullptr || playing);
	if (ImGui::Button("Draw it") && gestureTemplate != nullptr)
	{
		const auto points =
		    TemplateStroke(*gestureTemplate, k_DrawnSize, WindowSize() * 0.5f, gestures::sampling::ScreenRatio());
		if (points.size() >= 2)
		{
			gestures::sampling::PlayStroke(MousePositions(points));
		}
	}
	ImGui::EndDisabled();
	if (playing)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("drawing...");
	}
	if (ImGui::Button("Forget the path"))
	{
		gestures::ClearBuffer();
	}
}
