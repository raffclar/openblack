/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VideoViewer.h"

#include <algorithm>
#include <span>

#include "Debug/ImGuiUtils.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
constexpr std::chrono::milliseconds k_LongestUpdate {250};
} // namespace

VideoViewer::VideoViewer() noexcept
    : Window("Video", ImVec2(820.0f, 680.0f))
{
}

void VideoViewer::DrawFilms() noexcept
{
	// The game's own films: off, they are skipped as a build without the decoder skips them. The switch lives with the
	// video system, so closing the window leaves it as it is
	if (Locator::videoSystem::has_value())
	{
		bool films = Locator::videoSystem::value().FilmsEnabled();
		if (MenuClick(ImGui::Checkbox("Game films", &films)))
		{
			Locator::videoSystem::value().SetFilmsEnabled(films);
		}
		ImGui::Separator();
	}
	const bool hasFiles = Locator::filesystem::has_value();
	for (const auto film : k_Films)
	{
		const std::string name(film);
		const bool found = hasFiles && Locator::filesystem::value().Exists(name);
		if (MenuClick(ImGui::Selectable(name.c_str(), name == _film, found ? 0 : ImGuiSelectableFlags_Disabled)))
		{
			Load(film);
		}
	}
	ImGui::Separator();
}

void VideoViewer::Load(std::string_view film) noexcept
{
	_film = film;
	_error.clear();
	_texture.Reset();
	_shown = 0;
	_caughtUp = 0;
	_decodeMs = 0.0;
	_lastUpdate.reset();
	_playback.Reset(0, 1, 1);
	if (!_file.Open(_film))
	{
		_error = _file.GetError();
		return;
	}
	if (!_decoder.Open(_file))
	{
		_error = _decoder.GetError();
		return;
	}
	_playback.Reset(_file.FrameCount(), _file.FpsNumerator(), _file.FpsDenominator());
	_texture.Reset(bgfx::createTexture2D(static_cast<uint16_t>(_file.Width()), static_cast<uint16_t>(_file.Height()), false, 1,
	                                     bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
	bgfx::setName(_texture.Get(), "Video viewer");
	CatchUp();
}

void VideoViewer::CatchUp() noexcept
{
	_caughtUp = 0;
	std::span<const uint8_t> picture;
	while (_shown < _playback.Due())
	{
		const auto start = std::chrono::steady_clock::now();
		const auto rgba = _decoder.DecodeNext(_shown);
		_decodeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		++_shown;
		++_caughtUp;
		if (rgba.empty())
		{
			_error = _decoder.GetError();
			continue;
		}
		picture = rgba;
	}
	if (!picture.empty() && _texture.IsValid())
	{
		bgfx::updateTexture2D(_texture.Get(), 0, 0, 0, 0, static_cast<uint16_t>(_file.Width()),
		                      static_cast<uint16_t>(_file.Height()),
		                      bgfx::copy(picture.data(), static_cast<uint32_t>(picture.size())));
	}
}

void VideoViewer::Update() noexcept
{
	const auto now = std::chrono::steady_clock::now();
	if (!_playback.Playing() || !_lastUpdate)
	{
		_lastUpdate = now;
	}
	else
	{
		// Whole milliseconds; the rest waits for the next update. A longer gap (the window was closed) is not played
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - *_lastUpdate);
		_playback.Advance(static_cast<uint64_t>(std::min(ms, k_LongestUpdate).count()));
		*_lastUpdate = ms > k_LongestUpdate ? now : *_lastUpdate + ms;
	}
	if (_texture.IsValid())
	{
		CatchUp();
	}
}

void VideoViewer::Draw() noexcept
{
	DrawFilms();
	if (_film.empty())
	{
		ImGui::TextUnformatted("Choose a film above.");
		return;
	}
	ImGui::Text("%s: %ux%u, %u/%u fps, %u frames, %u key frames", _film.c_str(), _file.Width(), _file.Height(),
	            _file.FpsNumerator(), _file.FpsDenominator(), _file.FrameCount(), _file.KeyFrameCount());
	if (!_error.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", _error.c_str());
	}
	if (!_texture.IsValid())
	{
		return;
	}

	if (MenuClick(ImGui::Button(_playback.Playing() ? "Pause" : "Play")))
	{
		if (_playback.Playing())
		{
			_playback.Pause();
		}
		else
		{
			_playback.Play();
			_lastUpdate.reset();
		}
	}
	ImGui::SameLine();
	if (MenuClick(ImGui::Button("Step")))
	{
		_playback.Step();
	}
	ImGui::SameLine();
	if (MenuClick(ImGui::Button("Restart")))
	{
		_playback.Restart();
		_shown = 0;
	}

	const uint32_t frame = _shown > 0 ? _shown - 1 : 0;
	const double fps = static_cast<double>(_file.FpsNumerator()) / std::max(1u, _file.FpsDenominator());
	ImGui::Text("frame %u / %u, t = %.3f s", frame, _file.FrameCount(), frame / fps);
	ImGui::Text("played %.3f s, frame due %u, decoded this update %u, last decode %.2f ms",
	            static_cast<double>(_playback.PlayedMs()) / 1000.0, _playback.Due() > 0 ? _playback.Due() - 1 : 0, _caughtUp,
	            _decodeMs);

	// The picture, as large as the window allows with its shape kept
	const ImVec2 room = ImGui::GetContentRegionAvail();
	const float scale = std::min(room.x / static_cast<float>(_file.Width()), room.y / static_cast<float>(_file.Height()));
	if (scale > 0.0f)
	{
		ImGui::Image(_texture.Get(),
		             ImVec2(static_cast<float>(_file.Width()) * scale, static_cast<float>(_file.Height()) * scale));
	}
}

void VideoViewer::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void VideoViewer::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
