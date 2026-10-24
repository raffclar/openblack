/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <CreatureUploadCfg.h>
#include <cxxopts.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "Common/UserData.h"
#include "CreatureUploadApp.h"
#include "Gui/TextDatabase.h"
#include "Online/CreatureUpload.h"
#include "Online/Desktop/CurlHttpClient.h"

using namespace openblack;
using namespace openblack::creature_upload;

namespace
{

/// The file of the profiles in the profile folder, the same file as the game reads
constexpr std::string_view k_ProfilesFile = "Profiles.txt";
/// The lobby of the game
constexpr std::string_view k_DefaultLobby = "https://bw.zanidrak.com";
/// The longest wait for the sign-out after the dialog closes
constexpr auto k_FinishWait = std::chrono::seconds(3);
constexpr auto k_PollWait = std::chrono::milliseconds(15);

std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::filesystem::path PathOf(const std::string& utf8)
{
	return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

/// The folder of the game from the registry, as the game finds it
std::optional<std::filesystem::path> InstalledGamePath()
{
#if defined(_WIN32)
	DWORD size = 0;
	constexpr auto k_Key = L"SOFTWARE\\Lionhead Studios Ltd\\Black & White";
	if (RegGetValueW(HKEY_CURRENT_USER, k_Key, L"GameDir", RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS ||
	    size == 0)
	{
		return std::nullopt;
	}
	std::wstring path(size / sizeof(wchar_t), L'\0');
	if (RegGetValueW(HKEY_CURRENT_USER, k_Key, L"GameDir", RRF_RT_REG_SZ, nullptr, path.data(), &size) != ERROR_SUCCESS)
	{
		return std::nullopt;
	}
	path.resize(wcsnlen(path.c_str(), path.size()));
	return std::filesystem::path(path);
#else
	return std::nullopt;
#endif
}

/// Reads one line from the console with no echo of the characters
std::string ReadSecretLine()
{
#if defined(_WIN32)
	const auto input = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;
	const bool console = GetConsoleMode(input, &mode) != 0;
	if (console)
	{
		SetConsoleMode(input, mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
	}
#else
	termios before {};
	const bool console = isatty(STDIN_FILENO) != 0 && tcgetattr(STDIN_FILENO, &before) == 0;
	if (console)
	{
		termios quiet = before;
		quiet.c_lflag &= ~static_cast<tcflag_t>(ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
	}
#endif
	std::string line;
	std::getline(std::cin, line);
	if (!line.empty() && line.back() == '\r')
	{
		line.pop_back();
	}
#if defined(_WIN32)
	if (console)
	{
		SetConsoleMode(input, mode);
		std::cout << '\n';
	}
#else
	if (console)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &before);
		std::cout << '\n';
	}
#endif
	return line;
}

/// Runs the flow with no window: the status lines and the messages go to the console. Returns the exit code.
int RunConsole(const app::Setup& setup, online::HttpClientInterface& http, dialog::UploadFlow& flow, const dialog::Texts& texts,
               const std::string& profileName, const std::string& loginName)
{
	std::cout << texts.Get(dialog::keys::k_LoginPassword) << ' ' << std::flush;
	auto password = ReadSecretLine();
	flow.Upload(loginName, std::move(password), app::GatherFor(setup, profileName));
	const auto sentText = texts.Get(dialog::keys::k_SentToServer);
	bool sent = false;
	auto nextTick = std::chrono::steady_clock::now();
	do
	{
		http.Poll();
		if (std::chrono::steady_clock::now() >= nextTick)
		{
			flow.Tick();
			nextTick += std::chrono::milliseconds(dialog::k_SendTimerMs);
		}
		for (auto& effect : flow.TakeEffects())
		{
			if (const auto* status = std::get_if<dialog::SetStatus>(&effect))
			{
				std::cout << status->text << '\n';
				sent = sent || status->text == sentText;
			}
			else if (const auto* message = std::get_if<dialog::ShowMessage>(&effect))
			{
				std::cout << message->caption << ": " << message->text << '\n';
			}
		}
		std::this_thread::sleep_for(k_PollWait);
	} while (flow.IsBusy());
	flow.Close();
	app::FinishRequests(http);
	return sent ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

dialog::GatherFiles app::GatherFor(const Setup& setup, const std::string& profileName)
{
	const auto* profile = player_profiles::Find(setup.profiles, gui::ToUtf16(profileName));
	auto paths = profile != nullptr ? dialog::PathsFor(setup.store, *profile) : std::nullopt;
	return [paths = std::move(paths)]() -> std::variant<lobby_api::CreatureUpload, Problem> {
		if (!paths.has_value())
		{
			return Problem::NoPackage;
		}
		return Gather(*paths);
	};
}

void app::FinishRequests(online::HttpClientInterface& http)
{
	const auto end = std::chrono::steady_clock::now() + k_FinishWait;
	while (http.Pending() > 0 && std::chrono::steady_clock::now() < end)
	{
		http.Poll();
		std::this_thread::sleep_for(k_PollWait);
	}
}

int main(int argc, char** argv)
{
	cxxopts::Options options("creatureupload", "The creature upload tool of openblack: sends the creature of a profile to "
	                                           "the lobby, which shows the creature on a web page.");
	// clang-format off
	options.add_options()
		("g,game-path", "The folder of the game, for the language file of the game's upload tool. With none, the folder that the registry gives.", cxxopts::value<std::string>())
		("profile-folder", "The folder of the profiles, as the game's --profile-folder. With none, the Profiles folder in the working folder.", cxxopts::value<std::string>())
		("lobby", "The https:// origin of the lobby.", cxxopts::value<std::string>()->default_value(std::string(k_DefaultLobby)))
		("extra-authorities", "For tests on this computer only: a file of more certificate authorities, for example of a local lobby.", cxxopts::value<std::string>())
		("console", "Run with no window. The tool asks for the password on the console.")
		("profile", "With --console: the name of the profile. With none, the profile that the game played last.", cxxopts::value<std::string>())
		("login-name", "With --console: the login name of the lobby.", cxxopts::value<std::string>())
		("look-pictures", "For the look page: write pictures of the dialog in the stages of an upload to this folder, with no network, then stop.", cxxopts::value<std::string>())
		("log-file", "Write the log to this file. The log has no names, passwords or tokens.", cxxopts::value<std::string>())
		("h,help", "Show this help.");
	// clang-format on

	try
	{
		const auto result = options.parse(argc, argv);
		if (result.count("help") != 0)
		{
			std::cout << options.help() << '\n';
			return EXIT_SUCCESS;
		}
		const bool console = result.count("console") != 0;
#if defined(_WIN32)
		// The tool is a window program. With --console and no file or pipe for the output, it writes to the console
		// that started it.
		if (console && GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN &&
		    AttachConsole(ATTACH_PARENT_PROCESS) != 0)
		{
			FILE* stream = nullptr;
			freopen_s(&stream, "CONOUT$", "w", stdout);
			freopen_s(&stream, "CONOUT$", "w", stderr);
			freopen_s(&stream, "CONIN$", "r", stdin);
		}
#endif

		std::vector<spdlog::sink_ptr> sinks;
		if (console)
		{
			sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
		}
		if (result.count("log-file") != 0)
		{
			sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(result["log-file"].as<std::string>(), true));
		}
		if (!sinks.empty())
		{
			spdlog::register_logger(std::make_shared<spdlog::logger>("creatureupload", sinks.begin(), sinks.end()));
		}

		app::Setup setup;
		setup.gamePath =
		    result.count("game-path") != 0 ? std::optional(PathOf(result["game-path"].as<std::string>())) : InstalledGamePath();
		setup.store = player_profiles::StoreFor(player_profiles::RunOptions {
		    .profileFolder = result.count("profile-folder") != 0
		                         ? std::optional(PathOf(result["profile-folder"].as<std::string>()))
		                         : std::nullopt,
		    .userFolder = user_data::ProfileFolder(),
		    .automated = false,
		});
		if (setup.store.folder.has_value())
		{
			if (const auto text = ReadFile(*setup.store.folder / k_ProfilesFile); text.has_value())
			{
				setup.profiles =
				    player_profiles::Read(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
			}
		}
		setup.origin = result["lobby"].as<std::string>();
		while (!setup.origin.empty() && setup.origin.back() == '/')
		{
			setup.origin.pop_back();
		}

		creatureuploadcfg::Texts fileTexts;
		if (setup.gamePath.has_value())
		{
			if (const auto bytes = ReadFile(*setup.gamePath / std::string(dialog::k_LanguageFile)); bytes.has_value())
			{
				fileTexts = creatureuploadcfg::Read(*bytes);
			}
		}
		const dialog::Texts texts(std::move(fileTexts));

		online::CurlHttpClient http(result.count("extra-authorities") != 0
		                                ? std::optional(result["extra-authorities"].as<std::string>())
		                                : std::nullopt);
		const auto seed = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
		dialog::UploadFlow flow(http, setup.origin, texts, seed);

#if defined(_WIN32)
		if (result.count("look-pictures") != 0)
		{
			return app::WriteLookPictures(setup, texts, PathOf(result["look-pictures"].as<std::string>()));
		}
#endif
		if (console)
		{
			if (result.count("login-name") == 0)
			{
				std::cerr << "--console needs --login-name\n";
				return EXIT_FAILURE;
			}
			const auto profile = result.count("profile") != 0 ? result["profile"].as<std::string>() : setup.profiles.current;
			return RunConsole(setup, http, flow, texts, profile, result["login-name"].as<std::string>());
		}
#if defined(_WIN32)
		return app::RunDialog(setup, http, flow, texts);
#else
		std::cerr << "The dialog is for Windows only. Use --console.\n";
		return EXIT_FAILURE;
#endif
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
