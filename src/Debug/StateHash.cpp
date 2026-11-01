/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StateHash.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include <fmt/format.h>
#include <imgui.h>

#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
const ImVec4 k_Good {0.4f, 0.85f, 0.4f, 1.0f};
const ImVec4 k_Bad {0.95f, 0.45f, 0.35f, 1.0f};
constexpr size_t k_MostDifferences = 24;
} // namespace

StateHash::StateHash() noexcept
    : Window("State Hash", ImVec2(420.0f, 360.0f))
    , _context(std::make_unique<entt::meta_ctx>())
{
	ecs::reflection::RegisterComponents(*_context);
}

StateHash::~StateHash() noexcept = default;

void StateHash::Take() noexcept
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::time::has_value())
	{
		return;
	}
	const auto hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_context,
	                                       synced_state::PresentationParts(), false);
	_current = state_hash::Snapshot {.turn = Locator::time::value().GetTurn(), .hash = hash.hash, .parts = hash.parts};
}

void StateHash::UpdateAlways() noexcept
{
	if (_every <= 0 || !Locator::time::has_value())
	{
		return;
	}
	const auto turn = Locator::time::value().GetTurn();
	if (state_hash::Due(static_cast<uint32_t>(_every), turn,
	                    _current.has_value() ? std::optional(_current->turn) : std::nullopt))
	{
		Take();
	}
}

void StateHash::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::time::has_value())
	{
		ImGui::TextUnformatted("No land");
		return;
	}
	ImGui::Text("Turn %u", Locator::time::value().GetTurn());
	if (ImGui::Button("Take hash"))
	{
		Take();
	}
	ImGui::SetItemTooltip("Hashes the synced entities and components now. It takes a moment on a big land.");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::InputInt("Every turns", &_every, 0, 0);
	_every = std::max(_every, 0);
	ImGui::SetItemTooltip("Takes the hash at each turn that is a multiple of this number. 0 takes it only when asked.");

	if (!_current.has_value())
	{
		ImGui::TextDisabled("No hash taken yet");
		return;
	}
	ImGui::TextUnformatted(state_hash::Line(*_current).c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton("Copy"))
	{
		ImGui::SetClipboardText(state_hash::Line(*_current).c_str());
	}
	ImGui::SetItemTooltip("Copies the turn and the hash, to compare with another run");
	if (ImGui::Button("Pin this hash"))
	{
		_pinned = _current;
	}
	ImGui::SetItemTooltip("Keeps this hash to compare the next ones with");
	ImGui::SameLine();
	ImGui::BeginDisabled(!_pinned.has_value());
	if (ImGui::Button("Unpin"))
	{
		_pinned.reset();
	}
	ImGui::EndDisabled();

	// A hash from another run, pasted as a line that Copy gave, is the pinned one
	_pasteLine.resize(96);
	if (ImGui::InputTextWithHint("##paste", "Paste a line from another run", _pasteLine.data(), _pasteLine.size(),
	                             ImGuiInputTextFlags_EnterReturnsTrue))
	{
		if (auto parsed = state_hash::FromLine(_pasteLine.c_str()); parsed.has_value())
		{
			_pinned = std::move(parsed);
		}
	}

	if (!_pinned.has_value())
	{
		return;
	}
	ImGui::Text("Pinned: %s", state_hash::Line(*_pinned).c_str());
	if (state_hash::Compare(_pinned, *_current) == state_hash::Match::Same)
	{
		ImGui::TextColored(k_Good, "Same hash%s", _pinned->turn == _current->turn ? "" : " (at another turn)");
		return;
	}
	ImGui::TextColored(k_Bad, "Different hash");
	if (_pinned->parts.empty())
	{
		ImGui::TextDisabled("The pinned hash has no parts to compare (it was pasted)");
		return;
	}
	const auto differences = state_hash::Differences(*_pinned, *_current);
	ImGui::Text("%zu parts differ", differences.size());
	if (ImGui::BeginTable("Differences", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
	                      ImVec2(0.0f, 160.0f)))
	{
		ImGui::TableSetupColumn("Part");
		ImGui::TableSetupColumn("Pinned");
		ImGui::TableSetupColumn("Now");
		ImGui::TableHeadersRow();
		for (const auto& difference : differences | std::views::take(k_MostDifferences))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(difference.part.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(difference.pinned.has_value() ? state_hash::HexOf(*difference.pinned).c_str() : "-");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(difference.current.has_value() ? state_hash::HexOf(*difference.current).c_str() : "-");
		}
		ImGui::EndTable();
	}
}
