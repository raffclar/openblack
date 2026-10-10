/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HelpSpeechSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HelpSpeechSystem final: public HelpSpeechSystemInterface
{
public:
	void SetTable(audio::HelpSpeechTable table) override;
	void Say(uint32_t text, audio::SpeechVoice voice, std::optional<glm::vec3> position) override;
	[[nodiscard]] bool IsSaying(uint32_t text, audio::SpeechVoice voice) override;
	void Stop(audio::SpeechVoice voice, audio::SpeechBank bank) override;
	[[nodiscard]] const audio::HelpSpeechTable& GetTable() const override { return _table; }

private:
	audio::HelpSpeechTable _table;
	audio::SpeechVoices _voices;
};

} // namespace openblack::ecs::systems
