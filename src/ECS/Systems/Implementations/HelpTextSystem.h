/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HelpTextSystem final: public HelpTextSystemInterface
{
public:
	HelpTextSystem();

	void Start(const gui::TextDatabase& texts, int screenHeight) override;
	[[nodiscard]] bool IsStarted() const override { return _dialogue != nullptr; }
	void Reset() override;
	void Update(const Frame& frame) override;
	bool RunText(bool singleLine, uint32_t text, int32_t withInteraction) override;
	[[nodiscard]] bool IsTextRead() const override;
	void ClearAllText() override;
	void CloseDialogue() override;
	void InterruptAdvisors() override;
	[[nodiscard]] int32_t GetNarrator(uint32_t text) const override;
	[[nodiscard]] const help::TextFrame* Layout(glm::ivec2 screen, int barPixels, const help::WidthFn& widthFn) const override;
	[[nodiscard]] help::AdvisorVoices& GetVoices() override { return _voices; }
	[[nodiscard]] const help::AdvisorVoices& GetVoices() const override { return _voices; }
	[[nodiscard]] const help::DialogueText* GetDialogue() const override { return _dialogue.get(); }

private:
	/// The voices' sounds and clock, from the game's audio
	[[nodiscard]] help::AdvisorVoices::Audio VoiceAudio();
	/// The voices' gestures, given to the advisors' bodies
	[[nodiscard]] static help::AdvisorVoices::Hooks VoiceHooks();
	/// What the dialogue reads and has done
	[[nodiscard]] help::DialogueText::Queries DialogueQueries();
	[[nodiscard]] help::DialogueText::Hooks DialogueHooks();
	/// An advisor of the narrator (2 good, 3 evil) says a help text's line, both having stopped first
	void AdvisorSays(int32_t narrator, uint32_t text);

	const gui::TextDatabase* _texts {nullptr};
	help::AdvisorVoices _voices;
	std::unique_ptr<help::DialogueText> _dialogue;
	/// The sound saying the advisors' line
	entt::entity _advisorLine {entt::null};
	/// That sound's length in milliseconds
	float _advisorLineMs {0.0f};
	/// The game clock the texts are timed by, in milliseconds
	int32_t _clockMs {0};
	bool _inTemple {false};
};

} // namespace openblack::ecs::systems
