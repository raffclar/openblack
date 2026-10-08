/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include "ECS/Systems/ScreenshotRequestSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Frame 0 and no request when made
class ScreenshotRequestSystem final: public ScreenshotRequestSystemInterface
{
public:
	[[nodiscard]] uint32_t Frame() const noexcept override { return _frame; }
	void ResetFrame() noexcept override { _frame = 0; }
	void Request(const std::filesystem::path& path) noexcept override { _request = std::make_pair(_frame, path); }
	void RequestAt(uint32_t frame, const std::filesystem::path& path) noexcept override
	{
		_request = std::make_pair(frame, path);
	}
	[[nodiscard]] std::optional<std::filesystem::path> Due() const override
	{
		if (_request.has_value() && _request->first == _frame)
		{
			return _request->second;
		}
		return std::nullopt;
	}
	void FinishFrame() noexcept override
	{
		if (_request.has_value() && _request->first <= _frame)
		{
			_request = std::nullopt;
		}
		++_frame;
	}

private:
	uint32_t _frame {0};
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> _request;
};
} // namespace openblack::ecs::systems
