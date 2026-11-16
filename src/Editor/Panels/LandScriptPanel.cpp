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
#include <string_view>
#include <system_error>

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Common/DeveloperToolsGate.h"
#include "Common/FileDialog.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "Editor/EditorStyle.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::editor
{

namespace
{
constexpr std::string_view k_LandScriptFolderKey = "land_script_folder";

using LandAct = player_commands::EditorLandScript::Act;

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

void LandScriptPanel::Act(player_commands::EditorLandScript act, std::span<const uint8_t> text) noexcept
{
	_scrollToNext = true;
	if (developer_tools::NetworkSessionLive())
	{
		if (!Locator::playerCommandSystem::has_value())
		{
			_sendMessage = "The game has no commands: the act did not go";
			return;
		}
		if (act.act == LandAct::Open)
		{
			// The text goes first on the game socket. The server sends it to every computer before the turn of the command.
			const auto hash = script_acts::SendFile(game_socket::EditorScriptFile::Kind::LandScript, text);
			if (!hash.has_value())
			{
				_sendMessage = "The game socket is not open, or the file is too large: the land script did not go";
				return;
			}
			act.file = *hash;
		}
		Locator::playerCommandSystem::value().Submit(act);
		_sendMessage = fmt::format("Sent: {} at the next turn, for every computer", script_acts::NameOf(act.act));
		return;
	}
	if (!Locator::editorSystem::has_value())
	{
		return;
	}
	auto& run = Locator::editorSystem::value().GetLandScriptRun();
	// A game alone keeps the name of the file that the editor opened
	if (act.act == LandAct::Open)
	{
		script_acts::ReadLandScript(run, _openedPath.filename().generic_string(),
		                            std::string(reinterpret_cast<const char*>(text.data()), text.size()));
		return;
	}
	std::ignore = script_acts::ApplyLandScript(act, {}, run, script_acts::LocatorLandScriptWorld());
}

void LandScriptPanel::ReadFile(const std::filesystem::path& path) noexcept
{
	_openedPath = path;
	const auto text = script_acts::LandScriptText(path);
	if (!text.has_value())
	{
		_sendMessage = fmt::format("The land script {} is not a file that reads", path.generic_string());
		return;
	}
	const std::span bytes(reinterpret_cast<const uint8_t*>(text->data()), text->size());
	Act({.act = LandAct::Open}, bytes);
}

void LandScriptPanel::Draw() noexcept
{
	if (!Locator::editorSystem::has_value())
	{
		ImGui::TextDisabled("The game has no editor");
		return;
	}
	const auto& run = Locator::editorSystem::value().GetLandScriptRun();
	// The panel reads the script of the loaded land when it shows for the first time. In a network game a read is a
	// command, so the panel waits for the click.
	if (!_triedCurrentLand)
	{
		_triedCurrentLand = true;
		if (!developer_tools::NetworkSessionLive() && run.changes == 0)
		{
			Act({.act = LandAct::LoadedLand});
		}
	}
	DrawLoading(run);
	DrawControls(run);
	DrawCommands(run);
}

void LandScriptPanel::DrawLoading(const script_acts::LandScriptRun& run) noexcept
{
	if (ImGui::SmallButton("Loaded land"))
	{
		Act({.act = LandAct::LoadedLand});
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
		const file_dialog::Request request {
		    .mode = file_dialog::Mode::Open,
		    .title = "Open a land script",
		    .filters = {{.name = "Land scripts (*.txt)", .patterns = {"*.txt"}},
		                {.name = "Lands of network games (*.map)", .patterns = {"*.map"}},
		                {.name = "All files", .patterns = {"*"}}},
		    .startFolder = file_dialog::StartFolder(file_dialog::RememberedPath(k_LandScriptFolderKey), scriptsFolder, exists),
		    .owner = Locator::windowing::has_value() ? Locator::windowing::value().GetNativeHandles().nativeWindow : nullptr,
		};
		const auto outcome = file_dialog::Show(request);
		switch (outcome.status)
		{
		case file_dialog::Status::Chosen:
			file_dialog::RememberPath(k_LandScriptFolderKey, outcome.path.parent_path());
			ReadFile(outcome.path);
			break;
		case file_dialog::Status::Cancelled:
			break;
		case file_dialog::Status::Unavailable:
			// No dialog on this platform, such as in a web browser: the panel shows its own file browser
			_fileBrowser.Open(request);
			break;
		}
	}
	if (const auto chosen = _fileBrowser.Draw())
	{
		file_dialog::RememberPath(k_LandScriptFolderKey, chosen->parent_path());
		ReadFile(*chosen);
	}
	ImGui::SameLine();
	ImGui::TextUnformatted(run.name.c_str());
	if (!_sendMessage.empty())
	{
		ImGui::TextColored(style::k_Muted, "%s", _sendMessage.c_str());
	}
	if (!run.message.empty())
	{
		ImGui::TextColored(style::k_Muted, "%s", run.message.c_str());
	}
	if (run.read.error.has_value())
	{
		ImGui::TextColored(style::k_Warning, "The script stops after command %zu: %s", run.read.commands.size(),
		                   run.read.error->c_str());
	}
}

void LandScriptPanel::DrawControls(const script_acts::LandScriptRun& run) noexcept
{
	const auto& stepper = run.stepper;
	const bool hasNext = stepper.Next().has_value();
	ImGui::BeginDisabled(!hasNext);
	if (ImGui::Button("Step"))
	{
		Act({.act = LandAct::Step});
	}
	ImGui::SetItemTooltip("Does the next command in the world");
	ImGui::SameLine();
	if (ImGui::Button("Continue"))
	{
		Act({.act = LandAct::Continue});
	}
	ImGui::SetItemTooltip("Does the commands up to the next breakpoint, a command that fails or the end");
	ImGui::SameLine();
	ImGui::BeginDisabled(!_selected.has_value());
	if (ImGui::Button("Run to selected") && _selected.has_value())
	{
		Act({.act = LandAct::RunTo, .command = static_cast<uint32_t>(*_selected)});
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Does the commands up to the selected command. The selected command is not done.");
	ImGui::SameLine();
	if (ImGui::Button("Skip"))
	{
		Act({.act = LandAct::Skip});
	}
	ImGui::SetItemTooltip("Moves past the next command and does not do it");
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Restart"))
	{
		Act({.act = LandAct::Restart});
	}
	ImGui::SetItemTooltip("Starts again at the first command. The world keeps all that the commands made.");
	ImGui::SameLine();
	if (const auto next = stepper.Next(); next.has_value())
	{
		ImGui::Text("Next: line %d, %s", run.read.commands[*next].line, run.read.commands[*next].name.c_str());
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
	ImGui::Text("%zu of %zu commands", _visible.size(), run.read.commands.size());
}

void LandScriptPanel::Refilter(const script_acts::LandScriptRun& run)
{
	_refilter = false;
	_visible.clear();
	for (size_t i = 0; i < run.read.commands.size(); ++i)
	{
		if (scripts::CommandMatches(run.read.commands[i], _filter))
		{
			_visible.push_back(i);
		}
	}
}

void LandScriptPanel::DrawCommands(const script_acts::LandScriptRun& run) noexcept
{
	// An act of this frame can change the commands, so the list follows the run before the table reads it
	if (_refilter || run.changes != _shownChanges)
	{
		_shownChanges = run.changes;
		Refilter(run);
		if (_selected.has_value() && *_selected >= run.read.commands.size())
		{
			_selected.reset();
		}
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

	const auto& stepper = run.stepper;
	const auto next = stepper.Next();
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
	std::optional<size_t> toggle;
	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
			const auto index = _visible[static_cast<size_t>(row)];
			const auto& command = run.read.commands[index];
			const auto state = stepper.StateOf(index);
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
			const bool breakpoint = stepper.HasBreakpoint(index);
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      breakpoint ? ImGui::ColorConvertU32ToFloat4(style::k_Breakpoint) : style::k_Muted);
			if (ImGui::SmallButton(breakpoint ? "stop" : " .  "))
			{
				toggle = index;
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
	// The act changes the run, so it goes after the table that reads the run
	if (toggle.has_value())
	{
		Act({.act = LandAct::Breakpoint, .command = static_cast<uint32_t>(*toggle)});
	}
}

} // namespace openblack::editor
