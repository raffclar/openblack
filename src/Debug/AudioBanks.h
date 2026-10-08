/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Window.h"

namespace openblack::pack
{
class PackFile;
}

namespace openblack::debug::gui
{

/// The Audio banks debug window: every .sad bank of the game folder, its samples (or music segments) with their
/// format, and each one decoded and played with openblack's own decoders, as the game decodes them, on a channel of
/// its own. Only a debug tool: the game's sounds and music are not affected (the switches that silence them for testing
/// are in the Audio Player window).
class AudioBanks final: public Window
{
public:
	AudioBanks() noexcept;
	~AudioBanks() noexcept override;
	AudioBanks(const AudioBanks&) = delete;
	AudioBanks& operator=(const AudioBanks&) = delete;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	struct Decoded
	{
		uint16_t channels {0};
		int sampleRate {0};
		size_t frames {0};
		double decodeMs {0.0};
	};
	struct Row
	{
		std::string name;
		size_t bytes {0};
		uint32_t headerRate {0}; ///< the rate the bank's table gives
		std::string_view format;
		std::optional<Decoded> decoded; ///< empty until decoded; frames 0 when it does not decode
	};

	/// Every .sad under the game's audio folders, sorted
	void FindBanks() noexcept;
	/// Reads bank `index` and lists its samples
	void Load(size_t index) noexcept;
	/// Decodes sample `row` and keeps what the decoder gave; with `play`, plays it too
	void Decode(size_t row, bool play) noexcept;
	void StopPreview() noexcept;

	std::vector<std::filesystem::path> _banks;
	bool _searched {false};
	std::optional<size_t> _bank;
	std::unique_ptr<pack::PackFile> _pack;
	bool _musicBank {false};
	std::vector<Row> _rows;
	std::string _error;
	double _decodeAllMs {0.0};

	uint32_t _source {0}; ///< the preview's own source and buffer
	uint32_t _buffer {0};
	std::optional<size_t> _playing;
};

} // namespace openblack::debug::gui
