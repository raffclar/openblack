/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>

#include "ECS/Systems/IncomingMessageSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class IncomingMessageSystem final: public IncomingMessageSystemInterface
{
public:
	/// The real time in milliseconds, going round as the machine's count does
	using Clock = std::function<uint32_t()>;

	explicit IncomingMessageSystem(Clock clock);

	void SetTexts(const gui::TextDatabase* texts) override { _texts = texts; }
	void Add(incoming_messages::Kind kind, std::u16string_view text, uint8_t picture) override;
	void AddWeather(std::u16string_view place, uint8_t picture) override;
	void Update(bool buttonShows) override;
	void Dismiss() override;
	[[nodiscard]] const incoming_messages::Queue& GetQueue() const override { return _queue; }
	[[nodiscard]] uint32_t GetMilliseconds() const override { return _clock(); }

private:
	Clock _clock;
	const gui::TextDatabase* _texts {nullptr};
	incoming_messages::Queue _queue;
};

} // namespace openblack::ecs::systems
