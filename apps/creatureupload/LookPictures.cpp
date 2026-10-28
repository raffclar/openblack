/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#if defined(_WIN32)

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

// clang-format off
// The Windows header must come before the header of the common controls
#include <windows.h>
#include <commctrl.h>
// clang-format on
#include <PhysiqueFile.h>
#include <fmt/format.h>
#include <stb_image_write.h>

#include "Win32Dialog.h"
#include "resource.h"

namespace openblack::creature_upload::app
{
namespace
{

/// A client that answers each request when the pictures ask for the answer. It does not use the network.
class ScriptedHttp final: public online::HttpClientInterface
{
public:
	online::HttpRequestId Send(online::HttpRequest /*request*/, online::HttpCallback callback) override
	{
		_pending.push_back(std::move(callback));
		return _nextId++;
	}
	void Cancel(online::HttpRequestId /*id*/) override {}
	void Poll() override {}
	[[nodiscard]] size_t Pending() const override { return _pending.size(); }
	[[nodiscard]] std::optional<uint64_t> SentBytes(online::HttpRequestId /*id*/) const override { return sentBytes; }

	void Answer(int32_t status, std::string_view body)
	{
		if (_pending.empty())
		{
			return;
		}
		auto callback = std::move(_pending.front());
		_pending.pop_front();
		callback(online::HttpResponse {.status = status, .body = {body.begin(), body.end()}});
	}

	std::optional<uint64_t> sentBytes;

private:
	std::deque<online::HttpCallback> _pending;
	online::HttpRequestId _nextId {1};
};

/// What the dialog shows in one picture
struct Stage
{
	std::string name;
	std::vector<dialog::Effect> effects;
	/// The text in the login name box and the password box
	std::optional<std::pair<std::wstring, std::wstring>> boxes;
};

/// The dialog template of a module, with no WS_VISIBLE style, so that the dialog does not show when it opens
std::vector<uint8_t> HiddenTemplate(HMODULE module)
{
	const auto resource = FindResourceW(module, MAKEINTRESOURCEW(IDD_CREATUREUPLOAD), MAKEINTRESOURCEW(5));
	if (resource == nullptr)
	{
		return {};
	}
	const auto* data = static_cast<const uint8_t*>(LockResource(LoadResource(module, resource)));
	std::vector<uint8_t> bytes(data, data + SizeofResource(module, resource));
	// The style of an extended dialog template is the 32-bit number at byte 12
	constexpr size_t k_StyleOffset = 12;
	if (bytes.size() > k_StyleOffset + sizeof(DWORD))
	{
		DWORD style = 0;
		std::memcpy(&style, bytes.data() + k_StyleOffset, sizeof(style));
		style &= ~static_cast<DWORD>(WS_VISIBLE);
		std::memcpy(bytes.data() + k_StyleOffset, &style, sizeof(style));
	}
	return bytes;
}

void PumpMessages(std::chrono::milliseconds time)
{
	const auto end = std::chrono::steady_clock::now() + time;
	while (std::chrono::steady_clock::now() < end)
	{
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0)
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

/// Writes a picture of the whole window, with its frame, to a PNG file
bool WritePicture(HWND window, const std::filesystem::path& file)
{
	PumpMessages(std::chrono::milliseconds(300));
	RECT rect {};
	GetWindowRect(window, &rect);
	const auto width = rect.right - rect.left;
	const auto height = rect.bottom - rect.top;
	BITMAPINFO info {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	const auto screen = GetDC(nullptr);
	const auto memory = CreateCompatibleDC(screen);
	const auto bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	const auto old = SelectObject(memory, bitmap);
	// The full content includes the frame that the desktop window manager draws
	constexpr UINT k_RenderFullContent = 2;
	const bool printed = PrintWindow(window, memory, k_RenderFullContent) != 0;
	std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
	const auto* source = static_cast<const uint8_t*>(bits);
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		pixels[i] = source[i + 2];
		pixels[i + 1] = source[i + 1];
		pixels[i + 2] = source[i];
		pixels[i + 3] = 0xFF;
	}
	SelectObject(memory, old);
	DeleteObject(bitmap);
	DeleteDC(memory);
	ReleaseDC(nullptr, screen);
	return printed && stbi_write_png(file.string().c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}

/// What the dialogs of the pictures show when they open
struct Opening
{
	const Setup& setup;
	const dialog::Texts& texts;
	HICON icon {nullptr};
};

INT_PTR CALLBACK OpeningProc(HWND dialog, UINT message, WPARAM /*wParam*/, LPARAM lParam)
{
	if (message != WM_INITDIALOG)
	{
		return FALSE;
	}
	const auto& opening = *reinterpret_cast<const Opening*>(lParam);
	win32::Prepare(dialog, opening.setup, opening.texts, opening.icon);
	return TRUE;
}

/// The stages of an upload, from the flow, which runs on answers that this function gives
std::vector<Stage> UploadStages(const Setup& setup, const dialog::Texts& texts, std::vector<std::string>& messages)
{
	ScriptedHttp http;
	dialog::UploadFlow flow(http, setup.origin, texts, 1);
	std::vector<Stage> stages;
	stages.push_back({.name = "1_open", .effects = {}, .boxes = std::nullopt});
	const auto keep = [&](std::string name, std::optional<std::pair<std::wstring, std::wstring>> boxes) {
		std::vector<dialog::Effect> shown;
		for (auto& effect : flow.TakeEffects())
		{
			if (const auto* message = std::get_if<dialog::ShowMessage>(&effect))
			{
				messages.push_back(fmt::format("{}: {}", message->caption, message->text));
			}
			else
			{
				shown.push_back(std::move(effect));
			}
		}
		stages.push_back({.name = std::move(name), .effects = std::move(shown), .boxes = std::move(boxes)});
	};

	flow.Upload("uploadtester", "password", []() {
		return lobby_api::CreatureUpload {
		    .package = std::vector<uint8_t>(40000, 1),
		    .mind = std::vector<uint8_t>(6000, 2),
		    .mindFileName = "Player.erc",
		    .physique = std::vector<uint8_t>(28, 3),
		    .physiqueFileName = creaturemind::PhysiqueFileName("Player.erc"),
		};
	});
	keep("2_validating", std::pair {std::wstring(L"uploadtester"), std::wstring(L"password")});
	http.Answer(201, R"({"access_token":"a","access_token_expires":"2026-10-24T10:15:00Z","refresh_token":"r",)"
	                 R"("refresh_token_expires":"2026-11-23T10:00:00Z","account":{"id":1,"name":"uploadtester"}})");
	http.sentBytes = 30000;
	flow.Tick();
	keep("3_sending", std::nullopt);
	http.Answer(201, fmt::format(R"({{"id":1,"name":"Rex","url":"https://{}/creatures/uploadtester/",)"
	                             R"("uploaded":"2026-10-24T10:00:00Z"}})",
	                             dialog::HostOf(setup.origin)));
	keep("4_sent", std::nullopt);
	flow.Close();
	return stages;
}

/// Opens the dialog of a template where the player cannot see it, and writes a picture of each stage
int WriteStages(const std::vector<uint8_t>& dialogTemplate, const Opening& opening, const std::vector<Stage>& stages,
                const std::filesystem::path& folder, std::string_view prefix)
{
	if (dialogTemplate.empty())
	{
		return 0;
	}
	const auto window =
	    CreateDialogIndirectParamW(GetModuleHandleW(nullptr), reinterpret_cast<LPCDLGTEMPLATEW>(dialogTemplate.data()), nullptr,
	                               &OpeningProc, reinterpret_cast<LPARAM>(&opening));
	if (window == nullptr)
	{
		return 0;
	}
	// The window is on the screen, so that Windows draws it, but almost fully transparent, at the back and with no
	// activation. Thus the player does not see it and it does not take the focus.
	SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(window, 0, 1, LWA_ALPHA);
	SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	int written = 0;
	for (const auto& stage : stages)
	{
		if (stage.boxes.has_value())
		{
			SetDlgItemTextW(window, IDC_LOGINNAME, stage.boxes->first.c_str());
			SetDlgItemTextW(window, IDC_LOGINPASSWORD, stage.boxes->second.c_str());
		}
		for (const auto& effect : stage.effects)
		{
			win32::Apply(window, effect);
		}
		written += WritePicture(window, folder / fmt::format("{}_{}.png", prefix, stage.name)) ? 1 : 0;
	}
	DestroyWindow(window);
	return written;
}

} // namespace

int WriteLookPictures(const Setup& setup, const dialog::Texts& texts, const std::filesystem::path& folder)
{
	const INITCOMMONCONTROLSEX controls {.dwSize = sizeof(INITCOMMONCONTROLSEX), .dwICC = ICC_PROGRESS_CLASS};
	InitCommonControlsEx(&controls);
	std::filesystem::create_directories(folder);

	std::vector<std::string> messages;
	const auto stages = UploadStages(setup, texts, messages);
	HMODULE gameTool = nullptr;
	const Opening opening {.setup = setup, .texts = texts, .icon = win32::GameToolIcon(setup, gameTool)};

	// The dialog of this tool, then the dialog of the game's tool from the resources of the game's file
	int written = WriteStages(HiddenTemplate(GetModuleHandleW(nullptr)), opening, stages, folder, "openblack");
	if (gameTool != nullptr)
	{
		written += WriteStages(HiddenTemplate(gameTool), opening, stages, folder, "vanilla");
		FreeLibrary(gameTool);
	}

	std::ofstream text(folder / "messages.txt", std::ios::binary);
	for (const auto& message : messages)
	{
		text << message << "\n";
	}
	std::cout << "Wrote " << written << " pictures to " << folder.string() << '\n';
	return written > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace openblack::creature_upload::app

#endif
