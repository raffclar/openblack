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
#include <optional>

#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Help/DialogueText.h"
#include "Sound/SilentPlayback.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HelpTextSystem final: public HelpTextSystemInterface
{
public:
	void Start(const gui::TextDatabase& texts, int screenHeight) override;
	[[nodiscard]] bool IsStarted() const override { return _dialogue != nullptr; }
	void SetTextChoices(const TextChoices& choices) override;
	void Reset() override;
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

	void StopVoices() override;
	void Update(const Frame& frame) override;
	void ProcessTurn(uint32_t milliseconds) override;
	bool RunText(bool singleLine, uint32_t text, int32_t withInteraction) override;
	bool RunTextWithNumber(bool singleLine, uint32_t text, float number, int32_t withInteraction) override;
	void TempText(bool singleLine, std::string_view words, int32_t withInteraction) override;
	[[nodiscard]] bool IsTextRead() const override;
	void ClearAllText() override;
	void CloseDialogue() override;
	[[nodiscard]] help::DialogueText::Held HoldText() const override;
	void CloseTextBox() override;
	void RestoreText(const help::DialogueText::Held& held) override;
	void ShowTextAgain() override;
	void InterruptAdvisors() override;
	void InterruptAdvisorsByEscape() override;
	[[nodiscard]] int32_t GetNarrator(uint32_t text) const override;
	[[nodiscard]] std::u16string GetWords(uint32_t text) const override;
	[[nodiscard]] const help::TextFrame* Layout(glm::ivec2 screen, int barPixels, const help::WidthFn& widthFn) const override;
	[[nodiscard]] const help::DialogueText* GetDialogue() const override { return _dialogue.get(); }
	[[nodiscard]] const help::StoryHistory& GetStoryHistory() const override { return _storyHistory; }

private:
	/// Keeps a help text that a script showed in the story history, an invalid text as the first text
	void RecordStoryLine(uint32_t text, int32_t withInteraction, float number);

	/// What the dialogue reads and has done
	[[nodiscard]] help::DialogueText::Queries DialogueQueries();
	[[nodiscard]] help::DialogueText::Hooks DialogueHooks();
	/// An advisor of the narrator (2 good, 3 evil) says a help text's line, both having stopped first
	void AdvisorSays(int32_t narrator, uint32_t text);

	const gui::TextDatabase* _texts {nullptr};
	std::unique_ptr<help::DialogueText> _dialogue;
	help::StoryHistory _storyHistory;
	TextChoices _textChoices;
	/// The advisors' line as the scripts time it on their clock: from its start delay, for the length of its sample
	std::optional<audio::SilentPlayback> _scriptAdvisorLine;
	bool _inTemple {false};
	/// The player's click and key of the frames since the last turn
	bool _pendingClick {false};
	bool _pendingSkipKey {false};
};

} // namespace openblack::ecs::systems
