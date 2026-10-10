/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <glm/vec3.hpp>

#include "Audio/HelpSpeech.h"

namespace openblack::ecs::systems
{

/// The spoken lines of the game's help texts that scripts have said, such as the family's talk in the opening
class HelpSpeechSystemInterface
{
public:
	virtual ~HelpSpeechSystemInterface() = default;

	/// Which sample says each help text, once the texts and the speech banks are loaded
	virtual void SetTable(audio::HelpSpeechTable table) = 0;
	/// Says a help text's line on a voice, heard everywhere or, given a position, from there. Texts without a line say
	/// nothing.
	virtual void Say(uint32_t text, audio::SpeechVoice voice, std::optional<glm::vec3> position) = 0;
	/// Whether the voice is still saying the help text's line
	[[nodiscard]] virtual bool IsSaying(uint32_t text, audio::SpeechVoice voice) = 0;
	/// How many help texts have a spoken line, 0 until the texts and the speech banks are loaded
	[[nodiscard]] virtual size_t GetSpokenTextCount() const = 0;
	/// How many lines the scripts have had said that weren't yet found finished
	[[nodiscard]] virtual size_t GetLineCount() const = 0;
};

} // namespace openblack::ecs::systems
