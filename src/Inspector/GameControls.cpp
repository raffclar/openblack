/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameControls.h"

#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <InspectorDiscovery.h>
#include <LHVM.h>
#include <bgfx/bgfx.h>
#include <glm/trigonometric.hpp>

#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/EditorCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Camera/TempleCameraModel.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "EntityDescription.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Level.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{

constexpr std::string_view k_Menu = "menu";
constexpr std::string_view k_Cave = "cave";

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

/// The menu's labels are the game's own text; the inspector names them in plain letters
std::string Plain(std::u16string_view text)
{
	std::string plain;
	for (const auto c : text)
	{
		plain.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return plain;
}

std::string_view ModelName(const CameraModel& model)
{
	if (dynamic_cast<const DefaultWorldCameraModel*>(&model) != nullptr)
	{
		return "world";
	}
	if (dynamic_cast<const CreatureCameraModel*>(&model) != nullptr)
	{
		return "creature";
	}
	if (dynamic_cast<const FightCameraModel*>(&model) != nullptr)
	{
		return "fight";
	}
	if (dynamic_cast<const TempleCameraModel*>(&model) != nullptr)
	{
		return "temple";
	}
	if (dynamic_cast<const EditorCameraModel*>(&model) != nullptr)
	{
		return "editor";
	}
	return "other";
}

bool PathHoldsCamera()
{
	return Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera();
}

std::string_view PageName(gui::GameMenu::Page page)
{
	constexpr std::array<std::string_view, 5> k_Pages {"main", "options", "players", "advanced", "controls"};
	const auto index = static_cast<size_t>(page);
	return index < k_Pages.size() ? k_Pages.at(index) : "unknown";
}

/// The debug window of a name, any case
std::optional<std::string> DebugWindowNamed(std::string_view name)
{
	if (!Locator::debugGui::has_value())
	{
		return std::nullopt;
	}
	for (const auto& window : Locator::debugGui::value().ListWindows())
	{
		if (Lower(window.name) == Lower(name))
		{
			return window.name;
		}
	}
	return std::nullopt;
}

ScriptValue::Type TypeOf(lhvm::DataType type)
{
	switch (type)
	{
	case lhvm::DataType::Int:
		return ScriptValue::Type::Int;
	case lhvm::DataType::Vector:
		return ScriptValue::Type::Vector;
	case lhvm::DataType::Object:
		return ScriptValue::Type::Object;
	case lhvm::DataType::Boolean:
		return ScriptValue::Type::Boolean;
	default:
		return ScriptValue::Type::Float;
	}
}

ScriptValue FromVm(lhvm::VMValue value, lhvm::DataType type)
{
	ScriptValue result {.type = TypeOf(type)};
	switch (result.type)
	{
	case ScriptValue::Type::Int:
		result.integer = value.intVal;
		break;
	case ScriptValue::Type::Object:
		result.object = value.uintVal;
		break;
	case ScriptValue::Type::Boolean:
		// The script machine keeps a truth as a number
		result.boolean = value.floatVal != 0.0f;
		break;
	default:
		result.number = value.floatVal;
		break;
	}
	return result;
}

std::pair<lhvm::VMValue, lhvm::DataType> ToVm(const ScriptValue& value)
{
	switch (value.type)
	{
	case ScriptValue::Type::Int:
		return {lhvm::VMValue(value.integer), lhvm::DataType::Int};
	case ScriptValue::Type::Vector:
		return {lhvm::VMValue(value.number), lhvm::DataType::Vector};
	case ScriptValue::Type::Object:
		return {lhvm::VMValue(value.object), lhvm::DataType::Object};
	case ScriptValue::Type::Boolean:
		return {lhvm::VMValue(value.boolean ? 1.0f : 0.0f), lhvm::DataType::Boolean};
	case ScriptValue::Type::Float:
		break;
	}
	return {lhvm::VMValue(value.number), lhvm::DataType::Float};
}

} // namespace

// The camera

std::optional<CameraState> GameCamera::State() const
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& camera = Locator::camera::value();
	return CameraState {
	    .origin = camera.GetOrigin(),
	    .focus = camera.GetFocus(),
	    .rotation = glm::degrees(camera.GetRotation()),
	    .forward = camera.GetForward(),
	    .horizontalFieldOfView = camera.GetHorizontalFieldOfView(),
	    .nearClip = camera.GetNearClip(),
	    .model = std::string(ModelName(camera.GetModel())),
	    .heldByPath = PathHoldsCamera(),
	};
}

std::string GameCamera::Set(const CameraPose& pose)
{
	if (!Locator::camera::has_value())
	{
		return "there is no camera";
	}
	if (PathHoldsCamera())
	{
		return "a camera path (a miracle's or a script's) holds the camera";
	}
	Locator::camera::value().SetOrigin(pose.origin).SetFocus(pose.focus);
	return {};
}

std::string GameCamera::Pin(const CameraPose& pose)
{
	if (!Locator::camera::has_value())
	{
		return "there is no camera";
	}
	auto& camera = Locator::camera::value();
	// Its own state, kept from the first pin of the frame: a second pin (a picture's over an override) keeps it
	if (!_own.has_value())
	{
		_own = camera.GetMotion();
	}
	camera.SetOrigin(pose.origin).SetFocus(pose.focus);
	return {};
}

void GameCamera::Unpin()
{
	if (_own.has_value() && Locator::camera::has_value())
	{
		Locator::camera::value().SetMotion(*_own);
	}
	_own.reset();
}

std::string GameCamera::Fly(const CameraPose& pose)
{
	if (!Locator::camera::has_value())
	{
		return "there is no camera";
	}
	if (PathHoldsCamera())
	{
		return "a camera path (a miracle's or a script's) holds the camera";
	}
	Locator::camera::value().GetModel().SetFlight(pose.origin, pose.focus);
	return {};
}

float GameCamera::GroundHeight(glm::vec2 point) const
{
	return editor::LandHeight(point);
}

std::optional<glm::vec3> GameCamera::EntityPosition(uint32_t id) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(id);
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	// Where it is drawn, so that a camera framing a body moving in the physics looks at it rather than where it stood
	return DrawnPosition(registry, entity);
}

// The windows

GameGui::GameGui(InputTargetInterface& input)
    : _input(input)
{
}

std::vector<WindowInfo> GameGui::Windows() const
{
	std::vector<WindowInfo> windows;
	if (auto* game = Game::Instance(); game != nullptr && game->GetInterface() != nullptr)
	{
		const auto& menu = game->GetInterface()->GetMenu();
		WindowInfo info {.name = std::string(k_Menu),
		                 .kind = "game",
		                 .open = menu.IsOpen(),
		                 .page = menu.IsAskingToQuit() ? "question" : std::string(PageName(menu.GetPage())),
		                 .buttons = {}};
		// Every page's buttons, check boxes, sliders and tabs, or the question's answers while it is asked
		for (const auto& control : menu.GetNamedControls())
		{
			info.buttons.push_back(Plain(control.name));
		}
		windows.push_back(std::move(info));
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		windows.push_back({.name = std::string(k_Cave),
		                   .kind = "game",
		                   .open = Locator::creatureCaveSystem::value().IsOpen(),
		                   .page = {},
		                   .buttons = {}});
	}
	if (Locator::debugGui::has_value())
	{
		for (const auto& window : Locator::debugGui::value().ListWindows())
		{
			windows.push_back({.name = window.name, .kind = "debug", .open = window.open, .page = {}, .buttons = {}});
		}
	}
	return windows;
}

std::string GameGui::Open(std::string_view window)
{
	if (Lower(window) == k_Menu)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return "the game's menu isn't made yet";
		}
		// Opened as the player opens it, with the Escape key
		if (!game->GetInterface()->GetMenu().IsOpen())
		{
			_input.Apply({.kind = InputEvent::Kind::KeyDown, .key = "Escape"});
			_input.Apply({.kind = InputEvent::Kind::KeyUp, .key = "Escape"});
		}
		return {};
	}
	if (Lower(window) == k_Cave)
	{
		if (!Locator::creatureCaveSystem::has_value())
		{
			return "there is no Creature Cave";
		}
		Locator::creatureCaveSystem::value().Open();
		return {};
	}
	const auto name = DebugWindowNamed(window);
	if (!name.has_value())
	{
		return "no window " + std::string(window) + "; ask gui.windows";
	}
	Locator::debugGui::value().OpenWindow(*name);
	return {};
}

std::string GameGui::Close(std::string_view window)
{
	if (Lower(window) == k_Menu)
	{
		auto* game = Game::Instance();
		if (game != nullptr && game->GetInterface() != nullptr && game->GetInterface()->GetMenu().IsOpen())
		{
			_input.Apply({.kind = InputEvent::Kind::KeyDown, .key = "Escape"});
			_input.Apply({.kind = InputEvent::Kind::KeyUp, .key = "Escape"});
		}
		return {};
	}
	if (Lower(window) == k_Cave)
	{
		if (Locator::creatureCaveSystem::has_value())
		{
			Locator::creatureCaveSystem::value().Close();
		}
		return {};
	}
	const auto name = DebugWindowNamed(window);
	if (!name.has_value())
	{
		return "no window " + std::string(window) + "; ask gui.windows";
	}
	Locator::debugGui::value().CloseWindow(*name);
	return {};
}

std::string GameGui::Press(std::string_view window, const std::vector<ButtonPathStep>& path)
{
	if (Lower(window) == k_Menu)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetMenu().IsOpen())
		{
			return "the game's menu isn't open: gui.open it first";
		}
		const auto& menu = game->GetInterface()->GetMenu();
		// A control by its name on the page shown, and which of those of the same name if there are several
		if (path.empty() || path.size() > 2 || !std::holds_alternative<std::string>(path.back()) ||
		    (path.size() == 2 && !std::holds_alternative<int32_t>(path.front())))
		{
			return "press a control of the menu's page by its name, with path [n] for the n-th of that name from 0; "
			       "gui.windows lists them";
		}
		const auto label = Lower(std::get<std::string>(path.back()));
		auto which = path.size() == 2 ? std::get<int32_t>(path.front()) : 0;
		for (const auto& control : menu.GetNamedControls())
		{
			if (Lower(Plain(control.name)) != label || which-- > 0)
			{
				continue;
			}
			// Clicked in the middle of the control, through the game's input as the player clicks it
			const auto& rect = control.rect;
			const auto at = game->GetInterface()->DialogToScreen((rect.min + rect.max) / 2);
			for (const auto& event : {InputEvent {.kind = InputEvent::Kind::PointerTo, .position = at},
			                          InputEvent {.kind = InputEvent::Kind::ButtonDown, .button = 1},
			                          InputEvent {.kind = InputEvent::Kind::ButtonUp, .button = 1}})
			{
				if (auto why = _input.Apply(event); !why.empty())
				{
					return why;
				}
			}
			return {};
		}
		return "the menu's " + std::string(menu.IsAskingToQuit() ? "question" : PageName(menu.GetPage())) + " has no control " +
		       std::get<std::string>(path.back()) + "; gui.windows lists them";
	}
	const auto name = DebugWindowNamed(window);
	if (!name.has_value())
	{
		return "no window " + std::string(window) + "; ask gui.windows";
	}
	std::vector<debug::gui::DebugGuiInterface::ButtonPathStep> steps(path.begin(), path.end());
	if (!Locator::debugGui::value().PressButton(*name, steps))
	{
		return "the window " + *name + " isn't open: gui.open it first";
	}
	return {};
}

// The scripts

bool GameScripts::Loaded() const
{
	return Locator::vm::has_value() && !Locator::vm::value().GetScripts().empty();
}

std::vector<ScriptInfo> GameScripts::Scripts() const
{
	std::vector<ScriptInfo> scripts;
	if (!Locator::vm::has_value())
	{
		return scripts;
	}
	constexpr std::array<std::pair<lhvm::ScriptType, std::string_view>, 6> k_Types {{
	    {lhvm::ScriptType::Script, "script"},
	    {lhvm::ScriptType::Help, "help"},
	    {lhvm::ScriptType::ChallengeHelp, "challenge_help"},
	    {lhvm::ScriptType::TempleHelp, "temple_help"},
	    {lhvm::ScriptType::TempleSpecial, "temple_special"},
	    {lhvm::ScriptType::MultiplayerHelp, "multiplayer_help"},
	}};
	for (const auto& script : Locator::vm::value().GetScripts())
	{
		const auto type = std::ranges::find(k_Types, script.type, &std::pair<lhvm::ScriptType, std::string_view>::first);
		scripts.push_back({.name = script.name,
		                   .file = script.filename,
		                   .type = std::string(type != k_Types.end() ? type->second : "other"),
		                   .parameters = script.parameterCount});
	}
	return scripts;
}

std::variant<uint32_t, std::string> GameScripts::Run(std::string_view name)
{
	auto& vm = Locator::vm::value();
	const auto& scripts = vm.GetScripts();
	if (std::ranges::none_of(scripts, [name](const auto& script) { return script.name == name; }))
	{
		return "no script " + std::string(name) + "; script.vm lists how many there are, script.tasks those running";
	}
	const auto task = vm.StartScript(std::string(name), lhvm::ScriptType::All);
	if (task == 0)
	{
		return "the script " + std::string(name) + " didn't start";
	}
	return task;
}

std::string GameScripts::StopTask(uint32_t task)
{
	if (!Locator::vm::has_value() || !Locator::vm::value().GetTasks().contains(task))
	{
		return "no task " + std::to_string(task) + "; ask script.tasks";
	}
	Locator::vm::value().StopTask(task);
	return {};
}

std::vector<ScriptGlobal> GameScripts::Globals() const
{
	std::vector<ScriptGlobal> globals;
	if (!Locator::vm::has_value())
	{
		return globals;
	}
	for (const auto& variable : Locator::vm::value().GetVariables())
	{
		globals.push_back({.name = variable.name, .value = FromVm(variable.value, variable.type)});
	}
	return globals;
}

std::string GameScripts::SetGlobal(std::string_view name, const ScriptValue& value)
{
	auto& vm = Locator::vm::value();
	const auto& variables = vm.GetVariables();
	const auto found = std::ranges::find_if(variables, [name](const auto& variable) { return variable.name == name; });
	if (found == variables.end())
	{
		return "no global " + std::string(name);
	}
	vm.SetVariable(static_cast<uint32_t>(std::distance(variables.begin(), found)), ToVm(value).first);
	return {};
}

std::vector<ScriptNative> GameScripts::Natives() const
{
	std::vector<ScriptNative> natives;
	const auto* functions = Locator::vm::has_value() ? Locator::vm::value().GetFunctions() : nullptr;
	if (functions == nullptr)
	{
		return natives;
	}
	for (size_t id = 1; id < functions->size(); ++id)
	{
		const auto& function = functions->at(id);
		natives.push_back({.id = static_cast<uint32_t>(id),
		                   .name = function.name,
		                   .in = function.stackIn,
		                   .out = function.stackOut,
		                   .implemented = function.impl != nullptr,
		                   .slots = NativeSlots(function.name, function.stackIn)});
	}
	return natives;
}

std::variant<std::vector<ScriptValue>, std::string> GameScripts::CallNative(uint32_t id, const std::vector<ScriptValue>& args)
{
	std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> arguments;
	arguments.reserve(args.size());
	std::ranges::transform(args, std::back_inserter(arguments), ToVm);
	std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> results;
	if (!Locator::vm::value().CallNative(id, arguments, results))
	{
		return "the native couldn't be called now";
	}
	std::vector<ScriptValue> values;
	for (const auto& [value, type] : results)
	{
		values.push_back(FromVm(value, type));
	}
	return values;
}

// The lands

std::vector<LevelInfo> GameLevels::Levels() const
{
	std::vector<LevelInfo> levels;
	if (!Locator::resources::has_value())
	{
		return levels;
	}
	Locator::resources::value().GetLevels().Each([&levels](entt::id_type /*id*/, const Level& level) {
		levels.push_back({.name = level.GetName(),
		                  .kind = level.GetType() == Level::LandType::Campaign ? "story" : "playground",
		                  .valid = level.IsValid()});
	});
	std::ranges::sort(levels, {}, &LevelInfo::name);
	return levels;
}

std::string GameLevels::Load(std::string_view name, LoadHow how)
{
	auto* game = Game::Instance();
	if (game == nullptr || !Locator::resources::has_value())
	{
		return "there is no game to load into";
	}
	std::optional<std::filesystem::path> path;
	Locator::resources::value().GetLevels().Each([&path, name](entt::id_type /*id*/, const Level& level) {
		if (level.GetName() == name)
		{
			path = level.GetScriptPath();
		}
	});
	if (!path.has_value())
	{
		return "no land " + std::string(name);
	}
	const bool loaded = how == LoadHow::Story ? game->LoadMap(*path) : game->LoadMapWithFreshScripts(*path);
	return loaded ? std::string {} : "the land " + std::string(name) + " didn't load";
}

std::string GameLevels::LoadTestbed()
{
	auto* game = Game::Instance();
	if (game == nullptr)
	{
		return "there is no game to load into";
	}
	game->LoadTestbed();
	return {};
}

std::string GameLevels::NewGame(std::string_view start)
{
	auto* game = Game::Instance();
	if (game == nullptr)
	{
		return "there is no game to start";
	}
	std::optional<new_game_choice::NewGameStart> how;
	if (!start.empty())
	{
		how = new_game_choice::ParseNewGameStart(start);
		if (!how.has_value())
		{
			return "no way to start a new game called " + std::string(start);
		}
	}
	return game->StartNewGame(how) ? std::string {} : "the new game didn't start";
}

std::string GameLevels::Current() const
{
	const auto* game = Game::Instance();
	if (game == nullptr || game->GetLandPath().empty())
	{
		return {};
	}
	const auto& path = game->GetLandPath();
	if (path == "testbed")
	{
		return "testbed";
	}
	std::string name = path.stem().string();
	if (Locator::resources::has_value())
	{
		Locator::resources::value().GetLevels().Each([&name, &path](entt::id_type /*id*/, const Level& level) {
			if (level.GetScriptPath().lexically_normal() == path.lexically_normal())
			{
				name = level.GetName();
			}
		});
	}
	return name;
}

// Pictures of the screen

std::string GameScreenshots::Capture(const std::filesystem::path& path, bool hideDebugGui)
{
	auto* game = Game::Instance();
	if (game == nullptr)
	{
		return "there is no game";
	}
	std::error_code error;
	if (path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path(), error);
		if (error)
		{
			return "can't make the folder " + path.parent_path().generic_string() + ": " + error.message();
		}
	}
	game->RequestScreenshot(path, hideDebugGui);
	return {};
}

void GameScreenshots::HideDebugGui()
{
	if (auto* game = Game::Instance(); game != nullptr)
	{
		game->HideDebugGuiThisFrame();
	}
}

std::optional<std::filesystem::path> GameScreenshots::Root() const
{
	const auto* game = Game::Instance();
	// NOLINTNEXTLINE(concurrency-mt-unsafe): read once a picture is asked for, on the game's thread
	const char* environment = std::getenv("OPENBLACK_SCREENSHOT_ROOT");
	return ResolveShotRoot(game != nullptr ? game->GetScreenshotRoot() : std::nullopt, environment,
	                       [](const std::filesystem::path& path) {
		                       std::error_code error;
		                       return std::filesystem::exists(path, error);
	                       });
}

ShotSource GameScreenshots::Source() const
{
	ShotSource source;
	namespace discovery = inspector::discovery;
#if defined(OPENBLACK_BUILT_SOURCE_DIR)
	const std::filesystem::path builtFrom = OPENBLACK_BUILT_SOURCE_DIR;
#else
	const std::filesystem::path builtFrom;
#endif
	if (const auto worktree = discovery::GameWorktree(builtFrom, discovery::ExecutablePath()); worktree.has_value())
	{
		const auto revision = discovery::ReadRevision(*worktree);
		source.branch = revision.branch;
		source.commit = revision.commit.substr(0, 9);
	}
	switch (bgfx::getRendererType())
	{
	case bgfx::RendererType::Vulkan:
		source.backend = "vulkan";
		break;
	case bgfx::RendererType::Direct3D12:
		source.backend = "d3d12";
		break;
	case bgfx::RendererType::Direct3D11:
		source.backend = "d3d11";
		break;
	case bgfx::RendererType::OpenGL:
		source.backend = "opengl";
		break;
	case bgfx::RendererType::Metal:
		source.backend = "metal";
		break;
	default:
		break;
	}
	source.date = DateOf(std::chrono::system_clock::now());
	return source;
}

bool GameScreenshots::Exists(const std::filesystem::path& path) const
{
	std::error_code error;
	return std::filesystem::exists(path, error);
}

std::string GameScreenshots::Remove(const std::filesystem::path& path)
{
	std::error_code error;
	std::filesystem::remove(path, error);
	return error ? error.message() : std::string {};
}

std::string GameScreenshots::AppendLine(const std::filesystem::path& file, std::string_view line)
{
	std::error_code error;
	if (file.has_parent_path())
	{
		std::filesystem::create_directories(file.parent_path(), error);
	}
	std::ofstream out(file, std::ios::app | std::ios::binary);
	if (!out)
	{
		return "can't open " + file.generic_string();
	}
	// One write for the line and its end, so that games appending at once don't mix their lines
	const std::string whole = std::string(line) + "\n";
	out.write(whole.data(), static_cast<std::streamsize>(whole.size()));
	return out ? std::string {} : "can't write to " + file.generic_string();
}

std::filesystem::path GameScreenshots::Directory() const
{
	// The game's own folder, so that two games never write the same file; it goes when the game does
	return discovery::ShotsFolder(discovery::DefaultFolder(), discovery::CurrentProcessId());
}
