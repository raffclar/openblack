/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MemoryUse.h"

#include <string>

#include <entt/meta/resolve.hpp>
#include <fmt/format.h>
#include <imgui.h>

#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
/// The window reads the storages again this many frames after it last read them
constexpr int k_FramesBetweenReads = 30;

std::string Bytes(std::size_t bytes)
{
	constexpr double k_Kibibyte = 1024.0;
	return fmt::format("{:.1f} KiB", static_cast<double>(bytes) / k_Kibibyte);
}
} // namespace

MemoryUse::MemoryUse() noexcept
    : Window("Memory Use", ImVec2(460.0f, 520.0f))
    , _context(std::make_unique<entt::meta_ctx>())
{
	ecs::reflection::RegisterComponents(*_context);
}

MemoryUse::~MemoryUse() noexcept = default;

void MemoryUse::Take() noexcept
{
	_rows.clear();
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value().Underlying();
	std::vector<memory_use::Storage> storages;
	for (const auto& [id, storage] : registry.storage())
	{
		const auto& info = ecs::reflection::StorageType(storage);
		const auto type = entt::resolve(*_context, info);
		storages.push_back({.name = ecs::reflection::ShortTypeName(info),
		                    .count = storage.size(),
		                    .capacity = storage.capacity(),
		                    .componentSize = type ? type.size_of() : 0});
	}
	_rows = memory_use::Rows(storages, sizeof(entt::entity));
}

void MemoryUse::Draw() noexcept
{
	const bool asked = ImGui::Button("Read again");
	if (asked || _frames <= 0)
	{
		Take();
		_frames = k_FramesBetweenReads;
	}
	--_frames;
	ImGui::SameLine();
	ImGui::Text("Total %s in %zu storages", Bytes(memory_use::TotalBytes(_rows)).c_str(), _rows.size());
	ImGui::TextDisabled("A component of a type that the reflection does not know counts only its entity");
	if (!ImGui::BeginTable("memory_use", 3,
	                       ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV |
	                           ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Component");
	ImGui::TableSetupColumn("Entities");
	ImGui::TableSetupColumn("Memory");
	ImGui::TableHeadersRow();
	for (const auto& row : _rows)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(row.name.c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%zu", row.count);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Bytes(row.bytes).c_str());
	}
	ImGui::EndTable();
}
