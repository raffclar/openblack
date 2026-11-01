/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Advisors.h"

#include <cstddef>

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "Camera/Camera.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Help/Spirits.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::help::spirits;

namespace
{
constexpr std::array<const char*, 4> k_ControlStates {"Home", "Going home", "Out", "Clinging"};
/// The window lists this many of the newest help messages
constexpr size_t k_ShownHelpMessages = 12;

/// The help spirit type the scripts name an advisor by
int32_t TypeOf(int advisor)
{
	return advisor == k_GoodDude ? 1 : 2;
}
} // namespace

Advisors::Advisors() noexcept
    : Window("Advisors", ImVec2(420.0f, 560.0f))
{
}

void Advisors::Draw() noexcept
{
	if (!Locator::advisorSystem::has_value())
	{
		ImGui::TextUnformatted("No advisors");
		return;
	}
	auto& system = Locator::advisorSystem::value();
	if (!system.IsLoaded())
	{
		ImGui::TextWrapped("The advisors' files did not load (Data/HelpSprite/MarkGood.Hd and MarkEvil.Hd)");
	}
	auto& control = system.GetController();
	if (ImGui::Button("Both out"))
	{
		control.SpiritEject(1, false);
		control.SpiritEject(2, false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Both appear"))
	{
		control.SpiritEject(1, true);
		control.SpiritEject(2, true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Both home"))
	{
		control.SpiritHome(1, false);
		control.SpiritHome(2, false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Both vanish"))
	{
		control.SpiritHome(1, true);
		control.SpiritHome(2, true);
	}
	if (ImGui::Button("Cling at the bottom"))
	{
		control.SpiritCling(1, 0.25f, 0.95f);
		control.SpiritCling(2, 0.75f, 0.95f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Cling at the sides"))
	{
		control.SpiritCling(1, 0.0f, 0.0f);
		control.SpiritCling(2, 0.0f, 0.0f);
	}
	ImGui::Text("Focus: %s", control.Focus() == k_GoodDude ? "good" : (control.Focus() == k_EvilDude ? "evil" : "none"));
	for (int advisor = 0; advisor < k_Dudes; ++advisor)
	{
		ImGui::PushID(advisor);
		if (ImGui::CollapsingHeader(advisor == k_GoodDude ? "The good advisor" : "The evil advisor",
		                            ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawAdvisor(advisor);
		}
		ImGui::PopID();
	}
	if (ImGui::CollapsingHeader("The scripts' dialogue and the help messages"))
	{
		DrawHelp();
	}
}

void Advisors::DrawHelp() noexcept
{
	if (!Locator::helpTextSystem::has_value())
	{
		ImGui::TextUnformatted("No help system");
		return;
	}
	const auto& help = Locator::helpTextSystem::value();
	ImGui::Text("Dialogue started: %s, text read: %s", help.IsStarted() ? "yes" : "no", help.IsTextRead() ? "yes" : "no");
	if (Locator::dialogueControlSystem::has_value())
	{
		ImGui::Text("Dialogue held by script task %u", Locator::dialogueControlSystem::value().GetOwner());
	}
	if (const auto* dialogue = help.GetDialogue(); dialogue != nullptr)
	{
		const auto text = dialogue->GetCurrentText();
		ImGui::Text("Text %u, narrator %d", text, help.GetNarrator(text));
		ImGui::Text("Waits for a click: %s, drawn: %s", dialogue->IsWaitingForClick() ? "yes" : "no",
		            dialogue->IsDrawn() ? "yes" : "no");
		ImGui::Text("Started in turn %u, ends in turn %u", dialogue->GetStartTurn(), dialogue->GetEndTurn());
		ImGui::TextWrapped("%s", openblack::gui::ToUtf8(help.GetWords(text)).c_str());
	}
	else
	{
		ImGui::TextUnformatted("No text shows");
	}
	if (!Locator::helpMessageSystem::has_value())
	{
		return;
	}
	const auto& started = Locator::helpMessageSystem::value().GetStarted();
	ImGui::SeparatorText(fmt::format("Help messages started: {}", started.size()).c_str());
	// The newest first
	const auto shown = std::min(started.size(), k_ShownHelpMessages);
	for (size_t i = 0; i < shown; ++i)
	{
		const auto& message = started[started.size() - 1 - i];
		ImGui::Text("Turn %u: %s, texts %u to %u", message.turn, message.script.c_str(), message.first, message.last);
		if (message.statText.has_value())
		{
			ImGui::SameLine();
			ImGui::Text("(town need text %u, %.0f%%)", *message.statText, message.need.value_or(0.0f));
		}
	}
}

void Advisors::DrawAdvisor(int advisor) noexcept
{
	auto& control = Locator::advisorSystem::value().GetController();
	const auto type = TypeOf(advisor);
	const AdvisorSpirit& dude = control.Dude(advisor);
	const auto state = static_cast<size_t>(control.State(advisor));
	ImGui::Text("%s, state 0x%X (queued 0x%X) for %.2f s", state < k_ControlStates.size() ? k_ControlStates.at(state) : "?",
	            dude.State(), dude.QueuedState(), dude.StateTime());
	ImGui::Text("Hover (%.3f, %.3f), depth %.3f, closeness %.2f", dude.Hover().x, dude.Hover().y,
	            dude.DepthChannel().GetValue(), dude.Closeness());
	ImGui::Text("Alpha %.2f to %.2f, in the world %.2f, scale %.3f", dude.Alpha(), dude.AlphaTarget(), dude.InWorld(),
	            dude.ModelScale());
	ImGui::Text("Emotion %s (%.2f, peak %.2f) going to %s", EmotionName(dude.Emotion()).data(), dude.EmotionWeight(),
	            dude.EmotionPeak(), EmotionName(dude.EmotionTarget()).data());
	ImGui::Text("Position (%.1f, %.1f, %.1f), %zu layers", dude.Position().x, dude.Position().y, dude.Position().z,
	            dude.Layers().size());

	if (ImGui::Button("Out"))
	{
		control.SpiritEject(type, false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Appear"))
	{
		control.SpiritEject(type, true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Home"))
	{
		control.SpiritHome(type, false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Vanish"))
	{
		control.SpiritHome(type, true);
	}

	auto& place = _place.at(static_cast<size_t>(advisor));
	ImGui::SliderFloat2("On the screen", place.data(), 0.0f, 1.0f);
	if (ImGui::Button("Fly there"))
	{
		control.SpiritFly(type, place[0], place[1]);
	}
	ImGui::SameLine();
	if (ImGui::Button("Cling there"))
	{
		control.SpiritCling(type, place[0], place[1]);
	}
	ImGui::SameLine();
	if (ImGui::Button("Point there"))
	{
		const auto& screen = control.GetScreen();
		control.SpiritScreenPoint(type, glm::ivec2(static_cast<int32_t>(static_cast<float>(screen.width) * place[0]),
		                                           static_cast<int32_t>(static_cast<float>(screen.height) * place[1])));
	}
	ImGui::Checkbox("Out in the world", &_inWorld.at(static_cast<size_t>(advisor)));
	ImGui::SameLine();
	if (ImGui::Button("Point at the camera's focus"))
	{
		control.SpiritPointPosition(type, Locator::camera::value().GetFocus(), _inWorld.at(static_cast<size_t>(advisor)));
	}
	ImGui::SameLine();
	if (ImGui::Button("Look at it"))
	{
		control.SpiritLookAtPosition(type, Locator::camera::value().GetFocus());
	}
	if (ImGui::Button("Stop pointing"))
	{
		control.SpiritStopPointing(type);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop looking"))
	{
		control.SpiritStopLooking(type);
	}

	auto& anim = _anim.at(static_cast<size_t>(advisor));
	if (ImGui::BeginCombo("Anim", fmt::format("{} {}", anim, AnimName(static_cast<uint32_t>(anim))).c_str()))
	{
		for (uint32_t i = 0; i < k_AnimSlots; ++i)
		{
			if (ImGui::Selectable(fmt::format("{} {}", i, AnimName(i)).c_str(), static_cast<uint32_t>(anim) == i))
			{
				anim = static_cast<int>(i);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SliderFloat("Speed", &_animSpeed.at(static_cast<size_t>(advisor)), 0.1f, 3.0f);
	if (ImGui::Button("Play there"))
	{
		control.SpiritPlayAnim(type, place[0], place[1], static_cast<uint32_t>(anim),
		                       _animSpeed.at(static_cast<size_t>(advisor)));
	}
	ImGui::SameLine();
	ImGui::Text(control.SpiritPlayingAnim(type) ? "playing" : "played");

	auto& emotion = _emotion.at(static_cast<size_t>(advisor));
	ImGui::SliderInt("Emotion", &emotion, 0, 7, EmotionName(static_cast<uint32_t>(emotion)).data());
	ImGui::SameLine();
	if (ImGui::Button("Feel it"))
	{
		control.Dude(advisor).SetEmotion(static_cast<uint32_t>(emotion), 1.0f);
	}
}
