/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#if defined(_WIN32)

#include "Win32Dialog.h"

#include <cstdlib>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <commctrl.h>

#include "resource.h"

namespace openblack::creature_upload::app
{
namespace
{

/// The timer that moves the requests on
constexpr UINT_PTR k_PollTimer = 1;
constexpr UINT k_PollMs = 15;
/// The timer of the progress bar while the tool sends the files
constexpr UINT_PTR k_SendTimer = 0x30;

std::string Utf8(const std::wstring& wide)
{
	if (wide.empty())
	{
		return {};
	}
	const auto size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
	std::string text(static_cast<size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), text.data(), size, nullptr, nullptr);
	return text;
}

std::wstring ItemText(HWND dialog, int item)
{
	const auto control = GetDlgItem(dialog, item);
	const auto length = GetWindowTextLengthW(control);
	std::wstring text(static_cast<size_t>(length) + 1, L'\0');
	GetWindowTextW(control, text.data(), length + 1);
	text.resize(static_cast<size_t>(length));
	return text;
}

/// The state of the open dialog
struct Window
{
	const Setup& setup;
	online::HttpClientInterface& http;
	dialog::UploadFlow& flow;
	const dialog::Texts& texts;
	HMODULE gameTool {nullptr};
	/// True while a message box is open. The timers then do nothing, so that the dialog keeps the order of the changes.
	bool inMessage {false};
	bool sendTimer {false};

	void Open(HWND dialog)
	{
		win32::Prepare(dialog, setup, texts, win32::GameToolIcon(setup, gameTool));
		SetTimer(dialog, k_PollTimer, k_PollMs, nullptr);
	}

	void Close(HWND dialog)
	{
		KillTimer(dialog, k_PollTimer);
		if (sendTimer)
		{
			KillTimer(dialog, k_SendTimer);
		}
		flow.Close();
		EndDialog(dialog, IDCANCEL);
	}

	void Upload(HWND dialog)
	{
		const auto profiles = GetDlgItem(dialog, IDC_PROFILE);
		std::string profile;
		const auto selected = SendMessageW(profiles, CB_GETCURSEL, 0, 0);
		if (selected != CB_ERR)
		{
			const auto length = SendMessageW(profiles, CB_GETLBTEXTLEN, static_cast<WPARAM>(selected), 0);
			std::wstring name(static_cast<size_t>(length) + 1, L'\0');
			SendMessageW(profiles, CB_GETLBTEXT, static_cast<WPARAM>(selected), reinterpret_cast<LPARAM>(name.data()));
			name.resize(static_cast<size_t>(length));
			profile = Utf8(name);
		}
		flow.Upload(Utf8(ItemText(dialog, IDC_LOGINNAME)), Utf8(ItemText(dialog, IDC_LOGINPASSWORD)),
		            GatherFor(setup, profile));
		Show(dialog);
	}

	void Timer(HWND dialog, UINT_PTR timer)
	{
		if (inMessage)
		{
			return;
		}
		if (timer == k_PollTimer)
		{
			http.Poll();
		}
		else if (timer == k_SendTimer)
		{
			flow.Tick();
		}
		Show(dialog);
	}

	/// Shows the changes of the flow in their order. A message box stops the changes after it until the player closes it.
	void Show(HWND dialog)
	{
		for (auto& effect : flow.TakeEffects())
		{
			const auto* message = std::get_if<dialog::ShowMessage>(&effect);
			if (message == nullptr)
			{
				win32::Apply(dialog, effect);
				continue;
			}
			UpdateWindow(dialog);
			inMessage = true;
			MessageBoxW(dialog, win32::Wide(message->text).c_str(), win32::Wide(message->caption).c_str(),
			            MB_OK | (message->icon == dialog::MessageIcon::Error ? MB_ICONERROR : 0U));
			inMessage = false;
		}
		if (flow.IsSending() != sendTimer)
		{
			sendTimer = flow.IsSending();
			if (sendTimer)
			{
				SetTimer(dialog, k_SendTimer, dialog::k_SendTimerMs, nullptr);
			}
			else
			{
				KillTimer(dialog, k_SendTimer);
			}
		}
	}
};

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(dialog, DWLP_USER));
	switch (message)
	{
	case WM_INITDIALOG:
		window = reinterpret_cast<Window*>(lParam);
		SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(window));
		window->Open(dialog);
		return TRUE;
	case WM_COMMAND:
		if (window == nullptr)
		{
			return FALSE;
		}
		if (LOWORD(wParam) == IDC_DO_UPLOAD && HIWORD(wParam) == BN_CLICKED)
		{
			window->Upload(dialog);
			return TRUE;
		}
		if (LOWORD(wParam) == IDCANCEL)
		{
			window->Close(dialog);
			return TRUE;
		}
		return FALSE;
	case WM_TIMER:
		if (window != nullptr)
		{
			window->Timer(dialog, wParam);
		}
		return TRUE;
	default:
		return FALSE;
	}
}

} // namespace

std::wstring win32::Wide(std::string_view utf8)
{
	if (utf8.empty())
	{
		return {};
	}
	const auto size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring text(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), text.data(), size);
	return text;
}

void win32::Apply(HWND dialog, const dialog::Effect& effect)
{
	const auto progress = GetDlgItem(dialog, IDC_PROGRESS);
	if (const auto* status = std::get_if<dialog::SetStatus>(&effect))
	{
		SetDlgItemTextW(dialog, IDC_STATUS, Wide(status->text).c_str());
	}
	else if (const auto* range = std::get_if<dialog::SetProgressRange>(&effect))
	{
		SendMessageW(progress, PBM_SETRANGE32, 0, static_cast<LPARAM>(range->most));
	}
	else if (const auto* position = std::get_if<dialog::SetProgress>(&effect))
	{
		SendMessageW(progress, PBM_SETPOS, static_cast<WPARAM>(position->position), 0);
	}
	else if (const auto* enable = std::get_if<dialog::EnableUpload>(&effect))
	{
		EnableWindow(GetDlgItem(dialog, IDC_DO_UPLOAD), enable->enabled ? TRUE : FALSE);
	}
}

void win32::Prepare(HWND dialog, const Setup& setup, const dialog::Texts& texts, HICON icon)
{
	if (icon != nullptr)
	{
		SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
		SendMessageW(dialog, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
	}
	// The list sorts the names. The profile that the game played last is the selected one.
	const auto profiles = GetDlgItem(dialog, IDC_PROFILE);
	for (const auto& profile : setup.profiles.profiles)
	{
		SendMessageW(profiles, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(profile.name).c_str()));
	}
	SendMessageW(profiles, CB_SETCURSEL, 0, 0);
	if (!setup.profiles.current.empty())
	{
		SendMessageW(profiles, CB_SELECTSTRING, static_cast<WPARAM>(-1),
		             reinterpret_cast<LPARAM>(Wide(setup.profiles.current).c_str()));
	}
	const std::array<std::pair<int, std::string_view>, 6> labels {{
	    {IDC_STATIC_GROUPBOX, dialog::keys::k_GroupBox},
	    {IDC_STATIC_PROFILE, dialog::keys::k_Profile},
	    {IDC_STATIC_LOGINNAME, dialog::keys::k_LoginName},
	    {IDC_STATIC_LOGINPASSWORD, dialog::keys::k_LoginPassword},
	    {IDC_DO_UPLOAD, dialog::keys::k_DoUpload},
	    {IDCANCEL, dialog::keys::k_Cancel},
	}};
	for (const auto& [item, key] : labels)
	{
		SetDlgItemTextW(dialog, item, Wide(texts.Get(key)).c_str());
	}
	SetWindowTextW(dialog, Wide(texts.Get(dialog::keys::k_AppTitle)).c_str());
	// The status line and the boxes start empty
	SetDlgItemTextW(dialog, IDC_STATUS, L"");
}

HICON win32::GameToolIcon(const Setup& setup, HMODULE& module)
{
	if (!setup.gamePath.has_value())
	{
		return nullptr;
	}
	if (module == nullptr)
	{
		const auto path = *setup.gamePath / "CreatureUpload.exe";
		module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	}
	return module != nullptr ? LoadIconW(module, MAKEINTRESOURCEW(IDI_GAME_TOOL)) : nullptr;
}

int RunDialog(const Setup& setup, online::HttpClientInterface& http, dialog::UploadFlow& flow, const dialog::Texts& texts)
{
	const INITCOMMONCONTROLSEX controls {.dwSize = sizeof(INITCOMMONCONTROLSEX), .dwICC = ICC_PROGRESS_CLASS};
	InitCommonControlsEx(&controls);
	Window window {.setup = setup, .http = http, .flow = flow, .texts = texts};
	DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CREATUREUPLOAD), nullptr, &DialogProc,
	                reinterpret_cast<LPARAM>(&window));
	if (window.gameTool != nullptr)
	{
		FreeLibrary(window.gameTool);
	}
	FinishRequests(http);
	return EXIT_SUCCESS;
}

} // namespace openblack::creature_upload::app

#endif
