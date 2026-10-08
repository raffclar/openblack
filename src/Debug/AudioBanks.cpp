/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AudioBanks.h"

#include <cctype>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <span>

#include <PackFile.h>
#include <glm/vec3.hpp>

#include "Audio/Device/Device.h"
#include "Audio/Device/WaveBuffers.h"
#include "AudioBankSample.h"
#include "Debug/ImGuiUtils.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
/// The audio folders of the game: the base game's and the Creature Isle add-on's
constexpr std::array<std::string_view, 2> k_AudioFolders = {"Audio", "CreatureIsle/Audio"};
} // namespace

AudioBanks::AudioBanks() noexcept
    : Window("Audio banks", ImVec2(900.0f, 600.0f))
{
}

AudioBanks::~AudioBanks() noexcept
{
	StopPreview();
	if (_source != 0)
	{
		audio::device::DeleteSource(_source);
	}
}

void AudioBanks::FindBanks() noexcept
{
	_searched = true;
	_banks.clear();
	if (!Locator::filesystem::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	for (const auto folder : k_AudioFolders)
	{
		const std::filesystem::path path(folder);
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		fileSystem.Iterate(path, true, [this](const std::filesystem::path& f) {
			auto extension = f.extension().string();
			std::ranges::transform(extension, extension.begin(),
			                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (extension == ".sad")
			{
				_banks.push_back(f);
			}
		});
	}
	std::ranges::sort(_banks);
}

void AudioBanks::Load(size_t index) noexcept
{
	StopPreview();
	_bank = index;
	_rows.clear();
	_error.clear();
	_decodeAllMs = 0.0;
	_musicBank = false;
	_pack = std::make_unique<pack::PackFile>();
	auto stream = Locator::filesystem::value().GetData(_banks[index]);
	const auto result = stream != nullptr ? _pack->ReadFile(*stream) : pack::PackResult::ErrCantOpen;
	if (result != pack::PackResult::Success)
	{
		_error = std::string(pack::ResultToStr(result));
		_pack.reset();
		return;
	}
	_musicBank = _pack->IsAudioMusicBank();
	const auto& headers = _pack->GetAudioSampleHeaders();
	const auto& data = _pack->GetAudioSamplesData();
	_rows.reserve(headers.size());
	for (size_t i = 0; i < headers.size(); ++i)
	{
		const auto& header = headers[i];
		const std::span<const uint8_t> bytes =
		    i < data.size() ? std::span<const uint8_t>(data[i]) : std::span<const uint8_t> {};
		_rows.push_back({
		    .name = std::string(header.name.data(), strnlen(header.name.data(), header.name.size())),
		    .bytes = bytes.size(),
		    .headerRate = header.sampleRate,
		    .format = bytes.empty() ? std::string_view("empty") : SampleFormatName(bytes),
		    .decoded = std::nullopt,
		});
	}
}

void AudioBanks::Decode(size_t row, bool play) noexcept
{
	if (_pack == nullptr || row >= _pack->GetAudioSamplesData().size())
	{
		return;
	}
	// The sample as the game's wave buffers decode it
	audio::Sound sound {};
	sound.buffer.push_back(_pack->GetAudioSamplesData()[row]);
	audio::wave_buffers::Pcm pcm;
	const auto start = std::chrono::steady_clock::now();
	const bool ok = audio::wave_buffers::Decode(sound, pcm);
	const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	const int rate = pcm.sampleRate > 0 ? pcm.sampleRate : static_cast<int>(_rows[row].headerRate);
	_rows[row].decoded = Decoded {
	    .channels = static_cast<uint16_t>(pcm.layout == audio::ChannelLayout::Stereo ? 2 : 1),
	    .sampleRate = rate,
	    .frames = ok ? pcm.Frames() : 0,
	    .decodeMs = ms,
	};
	if (!play || !ok || rate <= 0 || !audio::device::IsOpen())
	{
		return;
	}
	StopPreview();
	_buffer = audio::device::CreateBuffer(pcm.layout, pcm.samples, rate);
	if (_source == 0)
	{
		// A 2D channel at full gain, outside the game's 16 channels
		_source = audio::device::CreateSource();
		audio::device::SetSourceRelative(_source, true);
		audio::device::SetSourcePosition(_source, glm::vec3(0.0f));
		audio::device::SetSourceRolloff(_source, 0.0f);
		audio::device::SetSourceGain(_source, 1.0f);
	}
	audio::device::SetSourceBuffer(_source, _buffer);
	audio::device::PlaySource(_source);
	_playing = row;
}

void AudioBanks::StopPreview() noexcept
{
	_playing.reset();
	// After the audio has closed, the context took its sources and buffers with it
	if (!Locator::audioState::has_value() || !audio::device::IsOpen())
	{
		_source = 0;
		_buffer = 0;
		return;
	}
	if (_source != 0)
	{
		audio::device::StopSource(_source);
		audio::device::SetSourceBuffer(_source, 0);
	}
	if (_buffer != 0)
	{
		audio::device::DeleteBuffer(_buffer);
		_buffer = 0;
	}
}

void AudioBanks::Update() noexcept
{
	if (_playing && _source != 0 && audio::device::IsOpen() &&
	    audio::device::SourceStatus(_source) == audio::AudioStatus::Stopped)
	{
		_playing.reset();
	}
}

void AudioBanks::Draw() noexcept
{
	if (!_searched)
	{
		FindBanks();
	}
	if (_banks.empty())
	{
		ImGui::TextUnformatted("No .sad bank in the game folder.");
		return;
	}

	// The banks on the left, the chosen bank's samples on the right
	ImGui::BeginChild("Banks", ImVec2(260.0f, 0.0f), true);
	const auto& root = Locator::filesystem::value().GetGamePath();
	for (size_t i = 0; i < _banks.size(); ++i)
	{
		const auto name = _banks[i].lexically_relative(root).generic_string();
		if (MenuClick(ImGui::Selectable(name.empty() ? _banks[i].generic_string().c_str() : name.c_str(), _bank == i)))
		{
			Load(i);
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("Samples");
	if (!_bank)
	{
		ImGui::TextUnformatted("Choose a bank.");
		ImGui::EndChild();
		return;
	}
	if (!_error.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", _error.c_str());
		ImGui::EndChild();
		return;
	}
	ImGui::Text("%zu %s%s", _rows.size(), _musicBank ? "music segments" : "samples",
	            audio::device::IsOpen() ? "" : " (no audio device: decode only)");
	if (MenuClick(ImGui::Button("Decode all")))
	{
		const auto start = std::chrono::steady_clock::now();
		for (size_t i = 0; i < _rows.size(); ++i)
		{
			Decode(i, false);
		}
		_decodeAllMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
	if (_decodeAllMs > 0.0)
	{
		ImGui::SameLine();
		ImGui::Text("the whole bank in %.1f ms", _decodeAllMs);
	}
	if (_playing)
	{
		ImGui::SameLine();
		if (MenuClick(ImGui::Button("Stop")))
		{
			StopPreview();
		}
	}

	constexpr auto k_Flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
	if (ImGui::BeginTable("BankSamples", 9, k_Flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		for (const char* name : {"#", "Name", "Bytes", "Format", "Ch", "Rate", "Length", "Decode", ""})
		{
			ImGui::TableSetupColumn(name);
		}
		ImGui::TableHeadersRow();
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(_rows.size()));
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const auto index = static_cast<size_t>(i);
				const auto& row = _rows[index];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row.name.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%zu", row.bytes);
				ImGui::TableNextColumn();
				ImGui::Text("%.*s", static_cast<int>(row.format.size()), row.format.data());
				if (row.decoded && row.decoded->frames > 0)
				{
					ImGui::TableNextColumn();
					ImGui::Text("%u", row.decoded->channels);
					ImGui::TableNextColumn();
					ImGui::Text("%d", row.decoded->sampleRate);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f s", static_cast<double>(row.decoded->frames) / row.decoded->sampleRate);
				}
				else
				{
					ImGui::TableNextColumn();
					ImGui::TableNextColumn();
					ImGui::Text("%u", row.headerRate);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(row.decoded ? "does not decode" : "");
				}
				ImGui::TableNextColumn();
				if (row.decoded)
				{
					ImGui::Text("%.2f ms", row.decoded->decodeMs);
				}
				ImGui::TableNextColumn();
				if (_playing == index)
				{
					if (MenuClick(ImGui::SmallButton("Stop")))
					{
						StopPreview();
					}
				}
				else if (MenuClick(ImGui::SmallButton(audio::device::IsOpen() ? "Play" : "Decode")))
				{
					Decode(index, true);
				}
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();
}

void AudioBanks::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void AudioBanks::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
