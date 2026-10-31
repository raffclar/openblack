/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "TestbedGameStageInterface.h"

namespace openblack::testbed_scenarios
{

/// The game on the player's computer as the scenarios stage it. With no game, each command fails as the dedicated
/// server's commands fail.
class TestbedGameStage final: public GameStageInterface
{
public:
	[[nodiscard]] bool HasInterface() const override;
	[[nodiscard]] const gui::TextDatabase* GetTexts() const override;

	[[nodiscard]] std::string OpenTattooMenu() override;
	[[nodiscard]] std::string TattooEditorAsNetworkGame(bool asNetworkGame) override;
	[[nodiscard]] std::string OpenControlsBox(size_t row) override;
	void OpenChat(std::u16string_view line) override;
	[[nodiscard]] std::string OpenMultiplayerBox(size_t stage) override;
	[[nodiscard]] std::string OpenMenuPage(size_t page, bool noSound) override;
	[[nodiscard]] std::string PressMenuKey(size_t key, bool shift) override;
	[[nodiscard]] std::string OpenSkirmishMenu(bool askLeave) override;
	[[nodiscard]] std::string OpenSkirmishBox(bool selectSecond) override;
	[[nodiscard]] std::string OpenEndBox(size_t value) override;
	[[nodiscard]] std::string ShowCustomMusicControls(size_t value) override;

	void MakePlayersSymbols(std::optional<uint32_t> symbol) override;
	void SetDemoRoundTrip(std::optional<uint64_t> microseconds) override;
	void SetMousePosition(glm::ivec2 position) override;
	bool RequestJoinOnline() override;
	void PlayLogoEffect() override;
	[[nodiscard]] bool ContinueFromExitSave() override;
	void RequestQuit() override;
	void ReportChecks(checks::Report report, bool quit) override;
};

} // namespace openblack::testbed_scenarios
