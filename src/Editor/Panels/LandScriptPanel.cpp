/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandScriptPanel.h"

#include <algorithm>
#include <exception>
#include <span>
#include <string_view>
#include <system_error>

#include <OnlineMapFile.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Common/FileDialog.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "Editor/EditorStyle.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Online/OnlineMapLand.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::editor
{

namespace
{
constexpr std::string_view k_LandScriptFolderKey = "land_script_folder";

[[nodiscard]] const char* StateText(scripts::LandScriptStepper::State state)
{
	using State = scripts::LandScriptStepper::State;
	switch (state)
	{
	case State::Done:
		return "done";
	case State::Skipped:
		return "skipped";
	case State::Failed:
		return "failed";
	default:
		return "";
	}
}
} // namespace

void LandScriptPanel::ReadCurrentLand() noexcept
{
	if (!Locator::landSystem::has_value())
	{
		_message = "No land is loaded";
		return;
	}
	ReadFile(Locator::landSystem::value().GetLandPath());
}

void LandScriptPanel::ReadFile(const std::filesystem::path& path) noexcept
{
	_path = path;
	_read = {};
	_selected.reset();
	_refilter = true;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		if (path.empty() || !fileSystem.Exists(path))
		{
			_stepper.Reset(0);
			_message = fmt::format("The land script {} is not a file", path.generic_string());
			return;
		}
		const auto data = fileSystem.ReadAll(path);
		auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());
		// A land of a network game holds its script in the same file as its landscape
		if (online_map_land::IsOnlineMap(path.generic_string()))
		{
			const auto file = onlinemap::Parse(std::as_bytes(std::span(data)));
			source = file.has_value() ? std::string(file->script) : std::string {};
		}
		_read = lhscriptx::Script::Read(source);
		_stepper.Reset(_read.commands.size());
		_message = fmt::format("Read {} commands of {}", _read.commands.size(), path.filename().generic_string());
	}
	catch (const std::exception& error)
	{
		_stepper.Reset(0);
		_message = fmt::format("Could not read {}: {}", path.generic_string(), error.what());
	}
}

void LandScriptPanel::Draw() noexcept
{
	// The panel reads the script of the loaded land when it shows for the first time
	if (!_triedCurrentLand)
	{
		_triedCurrentLand = true;
		ReadCurrentLand();
	}
	DrawLoading();
	DrawControls();
	DrawCommands();
}

void LandScriptPanel::DrawLoading() noexcept
{
	if (ImGui::SmallButton("Loaded land"))
	{
		ReadCurrentLand();
	}
	ImGui::SetItemTooltip("Reads the script of the land that the game loaded last");
	ImGui::SameLine();
	if (ImGui::SmallButton("Open land script..."))
	{
		std::filesystem::path scriptsFolder;
		if (Locator::filesystem::has_value())
		{
			scriptsFolder = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>(true);
		}
		const auto exists = [](const std::filesystem::path& folder) {
			std::error_code error;
			return std::filesystem::is_directory(folder, error);
		};
		const auto outcome = file_dialog::Show({
		    .mode = file_dialog::Mode::Open,
		    .title = "Open a land script",
		    .filters = {{.name = "Land scripts (*.txt)", .patterns = {"*.txt"}},
		                {.name = "Lands of network games (*.map)", .patterns = {"*.map"}},
		                {.name = "All files", .patterns = {"*"}}},
		    .startFolder = file_dialog::StartFolder(file_dialog::RememberedPath(k_LandScriptFolderKey), scriptsFolder, exists),
		    .owner = Locator::windowing::has_value() ? Locator::windowing::value().GetNativeHandles().nativeWindow : nullptr,
		});
		switch (outcome.status)
		{
		case file_dialog::Status::Chosen:
			file_dialog::RememberPath(k_LandScriptFolderKey, outcome.path.parent_path());
			ReadFile(outcome.path);
			break;
		case file_dialog::Status::Cancelled:
			break;
		default:
			_message = "No file dialog is available here";
			break;
		}
	}
	ImGui::SameLine();
	ImGui::TextUnformatted(_path.generic_string().c_str());
	if (!_message.empty())
	{
		ImGui::TextColored(style::k_Muted, "%s", _message.c_str());
	}
	if (_read.error.has_value())
	{
		ImGui::TextColored(style::k_Warning, "The script stops after command %zu: %s", _read.commands.size(),
		                   _read.error->c_str());
	}
}

bool LandScriptPanel::RunCommand(size_t index) noexcept
{
	if (index >= _read.commands.size())
	{
		return false;
	}
	const auto& command = _read.commands[index];
	try
	{
		lhscriptx::Script script;
		script.Run(command);
		_message = fmt::format("Line {}: {} done", command.line, command.name);
		return true;
	}
	catch (const std::exception& error)
	{
		_message = fmt::format("Line {}: {} failed: {}", command.line, command.name, error.what());
		return false;
	}
}

void LandScriptPanel::DrawControls() noexcept
{
	const auto run = [this](size_t index) { return RunCommand(index); };
	const bool hasNext = _stepper.Next().has_value();
	ImGui::BeginDisabled(!hasNext);
	if (ImGui::Button("Step"))
	{
		_stepper.Step(run);
		_scrollToNext = true;
	}
	ImGui::SetItemTooltip("Does the next command in the world");
	ImGui::SameLine();
	if (ImGui::Button("Continue"))
	{
		_stepper.Continue(run);
		_scrollToNext = true;
	}
	ImGui::SetItemTooltip("Does the commands up to the next breakpoint, a command that fails or the end");
	ImGui::SameLine();
	ImGui::BeginDisabled(!_selected.has_value());
	if (ImGui::Button("Run to selected") && _selected.has_value())
	{
		_stepper.RunTo(*_selected, run);
		_scrollToNext = true;
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Does the commands up to the selected command. The selected command is not done.");
	ImGui::SameLine();
	if (ImGui::Button("Skip"))
	{
		_stepper.Skip();
		_scrollToNext = true;
	}
	ImGui::SetItemTooltip("Moves past the next command and does not do it");
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Restart"))
	{
		_stepper.Restart();
		_scrollToNext = true;
	}
	ImGui::SetItemTooltip("Starts again at the first command. The world keeps all that the commands made.");
	ImGui::SameLine();
	if (const auto next = _stepper.Next(); next.has_value())
	{
		ImGui::Text("Next: line %d, %s", _read.commands[*next].line, _read.commands[*next].name.c_str());
	}
	else
	{
		ImGui::TextUnformatted("No command is left");
	}
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20.0f);
	if (ImGui::InputTextWithHint("##LandScriptFilter", "Filter commands", &_filter))
	{
		_refilter = true;
	}
	ImGui::SameLine();
	ImGui::Text("%zu of %zu commands", _visible.size(), _read.commands.size());
}

void LandScriptPanel::Refilter()
{
	_refilter = false;
	_visible.clear();
	for (size_t i = 0; i < _read.commands.size(); ++i)
	{
		if (scripts::CommandMatches(_read.commands[i], _filter))
		{
			_visible.push_back(i);
		}
	}
}

void LandScriptPanel::DrawCommands() noexcept
{
	if (_refilter)
	{
		Refilter();
	}
	constexpr auto k_TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV |
	                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("LandScriptCommands", 5, k_TableFlags))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed);
	ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed);
	ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed);
	ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed);
	ImGui::TableSetupColumn("Arguments", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableHeadersRow();

	const auto next = _stepper.Next();
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(_visible.size()));
	// The clipper must draw the row of the next command, so that the table can scroll to it
	std::optional<int> nextRow;
	if (_scrollToNext && next.has_value())
	{
		const auto found = std::ranges::lower_bound(_visible, *next);
		if (found != _visible.end() && *found == *next)
		{
			nextRow = static_cast<int>(found - _visible.begin());
			clipper.IncludeItemByIndex(*nextRow);
		}
	}
	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
			const auto index = _visible[static_cast<size_t>(row)];
			const auto& command = _read.commands[index];
			const auto state = _stepper.StateOf(index);
			ImGui::PushID(static_cast<int>(index));
			ImGui::TableNextRow();
			if (next == index)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, style::k_CurrentLine);
				if (nextRow == row)
				{
					ImGui::SetScrollHereY(0.3f);
				}
			}
			ImGui::TableNextColumn();
			const bool breakpoint = _stepper.HasBreakpoint(index);
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      breakpoint ? ImGui::ColorConvertU32ToFloat4(style::k_Breakpoint) : style::k_Muted);
			if (ImGui::SmallButton(breakpoint ? "stop" : " .  "))
			{
				_stepper.ToggleBreakpoint(index);
			}
			ImGui::PopStyleColor();
			ImGui::TableNextColumn();
			ImGui::TextColored(style::k_Address, "%d", command.line);
			ImGui::TableNextColumn();
			const bool selected = _selected == index;
			if (ImGui::Selectable(command.name.c_str(), selected))
			{
				_selected = selected ? std::nullopt : std::optional(index);
			}
			ImGui::TableNextColumn();
			const auto stateColour = state == scripts::LandScriptStepper::State::Failed ? style::k_Error
			                         : state == scripts::LandScriptStepper::State::Done ? style::k_Good
			                                                                            : style::k_Muted;
			ImGui::TextColored(stateColour, "%s", StateText(state));
			ImGui::TableNextColumn();
			ImGui::TextColored(style::k_String, "%s", scripts::ArgumentsText(command).c_str());
			ImGui::PopID();
		}
	}
	_scrollToNext = false;
	ImGui::EndTable();
}

} // namespace openblack::editor
