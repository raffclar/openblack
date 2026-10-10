/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VideoViewer.h"

#include <array>
#include <string_view>

#include <BinkYuv.h>
#include <entt/core/hashed_string.hpp>

#include "Debug/ImGuiUtils.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
struct Film
{
	std::string_view label;
	std::string_view path;
};

constexpr std::array<Film, 5> k_Films = {{
    {"Intro", "Data/INTRO.bik"},
    {"Falling spell", "Data/Spells/fall/fall.bik"},
    {"Logo", "Data/logo.bik"},
    {"Pre-intro", "Data/pre_intro.bik"},
    {"Tips", "Data/tips.bik"},
}};

entt::id_type IdOf(const std::filesystem::path& path)
{
	return entt::hashed_string(path.generic_string().c_str()).value();
}
} // namespace

VideoViewer::VideoViewer() noexcept
    : Window("Videos", ImVec2(720.0f, 640.0f))
{
}

VideoViewer::~VideoViewer() noexcept = default;

void VideoViewer::OpenFilm(const std::filesystem::path& path)
{
	CloseFilm();
	if (!Locator::filesystem::value().Exists(path))
	{
		return;
	}
	auto& videos = Locator::resources::value().GetVideos();
	try
	{
		const auto [it, loaded] = videos.Load(IdOf(path), resources::VideoLoader::FromDiskTag {}, path);
		if (!it->second)
		{
			return;
		}
		std::shared_ptr<const bink::BinkFile> file = it->second.handle();
		_reader = bink::FrameReader::Create(file);
	}
	catch (const std::exception&)
	{
		return;
	}
	if (!_reader)
	{
		return;
	}
	const auto& header = _reader->GetFile().GetHeader();
	_playback.emplace(header.frameCount, header.fpsNumerator, header.fpsDenominator);
	_rgba.assign(static_cast<size_t>(header.width) * header.height * 4, 0);
	_texture = std::make_unique<graphics::Texture2D>("VideoViewer");
	_texture->Create(static_cast<uint16_t>(header.width), static_cast<uint16_t>(header.height), 1,
	                 graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge, graphics::Filter::Linear, nullptr);
	_path = path;
	_last = std::chrono::steady_clock::now();
	ShowFrame(0);
}

void VideoViewer::CloseFilm()
{
	_texture.reset();
	_reader.reset();
	_playback.reset();
	_shown.reset();
	if (_path)
	{
		// The full-screen player may hold the same file; the cache only lets its own reference go
		Locator::resources::value().GetVideos().Erase(IdOf(*_path));
		_path.reset();
	}
}

void VideoViewer::ShowFrame(uint32_t frame)
{
	if (!_reader || _shown == frame)
	{
		return;
	}
	[[maybe_unused]] const bool decoded = _reader->DecodeFrame(frame);
	bink::ConvertPicture(_reader->GetPicture(), bink::PixelLayout::Rgba8, _rgba);
	_texture->Update(_rgba.data(), static_cast<uint32_t>(_rgba.size()));
	_shown = frame;
}

void VideoViewer::Draw() noexcept
{
	auto& videos = Locator::videoSystem::value();
	bool films = videos.AreFilmsEnabled();
	if (ImGui::Checkbox("Game films", &films))
	{
		videos.SetFilmsEnabled(films);
	}
	ImGui::SameLine();
	bool sixteenBit = videos.IsSixteenBitColour();
	if (ImGui::Checkbox("16-bit colour, as the game showed it", &sixteenBit))
	{
		videos.SetSixteenBitColour(sixteenBit);
	}
#if !defined(OPENBLACK_BINK_DECODER)
	ImGui::TextUnformatted("Built without the decoder: videos keep their timing and show black");
#endif

	ImGui::SeparatorText("Full screen");
	for (const auto& film : k_Films)
	{
		ImGui::PushID(film.path.data());
		if (ImGui::Button(film.label.data()))
		{
			if (film.label == "Falling spell")
			{
				videos.StartFallingSpell();
			}
			else
			{
				videos.Play(std::filesystem::path(film.path));
				if (film.label == "Intro")
				{
					videos.ScheduleIntro();
				}
			}
		}
		ImGui::PopID();
		ImGui::SameLine();
	}
	if (ImGui::Button("Stop"))
	{
		videos.Stop();
	}
	if (const auto status = videos.GetStatus())
	{
		ImGui::Text("Frame %d of %d at %d fps, fading from %d, ending at %d, alpha %.3f%s", status->frame, status->frameCount,
		            status->fps, status->schedule.fadeStart, status->schedule.end, status->alpha,
		            status->fallingSpell ? " (falling spell)" : "");
		if (status->fallingSpell)
		{
			ImGui::Text("Falling spell state %d", status->fallingSpellState);
		}
	}
	else
	{
		ImGui::TextUnformatted("No video playing");
	}

	ImGui::SeparatorText("Preview");
#if defined(OPENBLACK_BINK_DECODER)
	DrawPreview();
#else
	ImGui::TextUnformatted("Needs the decoder");
#endif
}

void VideoViewer::DrawPreview()
{
	for (const auto& film : k_Films)
	{
		ImGui::PushID(film.label.data());
		if (ImGui::RadioButton(film.label.data(), _path == std::filesystem::path(film.path)))
		{
			OpenFilm(std::filesystem::path(film.path));
		}
		ImGui::PopID();
		ImGui::SameLine();
	}
	ImGui::NewLine();
	if (!_reader || !_playback)
	{
		return;
	}
	if (ImGui::Button(_playback->IsPlaying() ? "Pause" : "Play"))
	{
		_playback->IsPlaying() ? _playback->Pause() : _playback->Play();
		_last = std::chrono::steady_clock::now();
	}
	ImGui::SameLine();
	if (ImGui::Button("Step"))
	{
		_playback->Step();
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart"))
	{
		_playback->Restart();
	}
	const auto& header = _reader->GetFile().GetHeader();
	ImGui::SameLine();
	ImGui::Text("frame %u of %u, %u/%u fps, %u key frames", _playback->Frame(), header.frameCount, header.fpsNumerator,
	            header.fpsDenominator, _reader->GetFile().KeyFrameCount());
	ShowFrame(_playback->Frame());
	const float width = ImGui::GetContentRegionAvail().x;
	const float scale = width / static_cast<float>(header.width);
	ImGui::Image(graphics::toBgfx(_texture->GetNativeHandle()), ImVec2(width, static_cast<float>(header.height) * scale));
}

void VideoViewer::Update() noexcept
{
	if (!_playback)
	{
		return;
	}
	const auto now = std::chrono::steady_clock::now();
	_playback->Advance(std::chrono::duration_cast<std::chrono::microseconds>(now - _last));
	_last = now;
}

void VideoViewer::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void VideoViewer::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
