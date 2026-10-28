/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The warning that OK of the tattoo editor shows in a network game, and the message of a setup box that shows it

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <ECS/Systems/TattooEditorSystemInterface.h>
#include <Gui/DialogPainter.h>
#include <Gui/SetupMessage.h>
#include <Gui/TattooEditorDialog.h>
#include <Gui/TextDatabase.h>
#include <SDL_keycode.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack;
using namespace openblack::gui;
using namespace openblack::gui::test;

namespace
{
TextDatabase EditorTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_DIALOG_TATOODRAG", u"AB"},
	    {u"HELP_TEXT_REQUESTER_BOXES_03", u"AB"},
	    {u"HELP_TEXT_REQUESTER_BOXES_04", u"AW"},
	    {u"HELP_TEXT_SYMBOL_EXTRA_01", u"BA"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

/// An editor that only keeps whether the tattoos changed, and whether it is open
class FakeEditor final: public ecs::systems::TattooEditorSystemInterface
{
public:
	void Open(entt::entity /*creature*/, const creature_tattoo_editor::Orbit& /*view*/) override { open = true; }
	void OpenFromMenu(std::optional<entt::entity> /*playerCreature*/, entt::entity /*shown*/,
	                  const creature_tattoo_editor::Orbit& /*view*/) override
	{
		open = true;
	}
	[[nodiscard]] bool IsFromMenu() const override { return false; }
	[[nodiscard]] std::optional<entt::entity> GetShown() const override { return std::nullopt; }
	void Close() override { open = false; }
	[[nodiscard]] bool IsOpen() const override { return open; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return std::nullopt; }
	[[nodiscard]] const creature_tattoo_editor::Orbit& GetView() const override { return _view; }
	[[nodiscard]] const creature_tattoo_editor::Session& GetSession() const override { return session; }
	void Update(float /*milliseconds*/) override {}
	void Steer(glm::ivec2 /*grab*/, glm::ivec2 /*pointer*/, float /*milliseconds*/) override {}
	void Hover(std::span<const creature_tattoo_editor::SiteOnScreen> /*sites*/, glm::ivec2 /*pointer*/) override {}
	[[nodiscard]] std::optional<uint8_t> GetHoveredSite() const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::ivec2> GetHoveredPoint() const override { return std::nullopt; }
	bool Drop(uint8_t /*design*/, const glm::u8vec3& /*colour*/) override { return false; }
	std::optional<creature_tattoo::Slot> Lift() override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_face::Face> TakeFace() override { return std::nullopt; }
	creature_tattoo_editor::Accept Ok(bool networkGame) override
	{
		const auto accept = creature_tattoo_editor::Ok(session, networkGame);
		if (accept == creature_tattoo_editor::Accept::Close)
		{
			open = false;
		}
		return accept;
	}
	void Answered() override
	{
		session.changed = false;
		++answers;
	}
	void Cancel() override { open = false; }
	void SetPictures(const creature_tattoo::Pictures& pictures) override { _pictures = pictures; }
	[[nodiscard]] const creature_tattoo::Pictures& GetPictures() const override { return _pictures; }

	creature_tattoo_editor::Session session;
	bool open {true};
	int answers {0};

private:
	creature_tattoo_editor::Orbit _view {};
	creature_tattoo::Pictures _pictures {};
};

struct EditorFixture
{
	TextDatabase texts = EditorTexts();
	GameFont font = LoadFont();
	TattooEditorDialog dialog {texts, font};
	FakeEditor editor;

	explicit EditorFixture(bool networkGame)
	{
		editor.session.changed = true;
		dialog.SetNetworkGame(networkGame);
		dialog.Open(false);
		Update(1.0f);
	}

	void Update(float seconds) { dialog.Update(seconds, editor, {}, false); }

	bool Click(glm::ivec2 point)
	{
		dialog.MouseMove(point);
		dialog.MouseDown(point);
		return dialog.MouseUp(point, editor);
	}
};

constexpr glm::ivec2 k_EditorOk = TattooEditorDialog::k_OkPosition + (TattooEditorDialog::k_ArrowSize / 2);
} // namespace

TEST(TattooEditorWarning, InANetworkGameOkAfterAChangeWarnsFirst)
{
	EditorFixture f(true);
	EXPECT_TRUE(f.Click(k_EditorOk));
	// The editor stays open, and the warning shows with one OK button
	EXPECT_TRUE(f.editor.IsOpen());
	EXPECT_TRUE(f.dialog.IsOpen());
	EXPECT_TRUE(f.dialog.IsShowingMessage());
	ASSERT_EQ(f.dialog.GetNamedControls().size(), 1u);
	EXPECT_EQ(f.dialog.GetNamedControls().front().name, u"AW");
	// The OK of the warning clears the change, and the next OK closes the editor
	EXPECT_TRUE(f.Click(SetupMessage::k_Ok.Centre()));
	EXPECT_FALSE(f.dialog.IsShowingMessage());
	EXPECT_EQ(f.editor.answers, 1);
	EXPECT_FALSE(f.editor.session.changed);
	EXPECT_TRUE(f.dialog.IsOpen());
	EXPECT_TRUE(f.Click(k_EditorOk));
	EXPECT_FALSE(f.editor.IsOpen());
	EXPECT_FALSE(f.dialog.IsOpen());
}

TEST(TattooEditorWarning, InAGameAloneOrWithNoChangeOkCloses)
{
	EditorFixture alone(false);
	EXPECT_TRUE(alone.Click(k_EditorOk));
	EXPECT_FALSE(alone.dialog.IsShowingMessage());
	EXPECT_FALSE(alone.dialog.IsOpen());

	EditorFixture unchanged(true);
	unchanged.editor.session.changed = false;
	EXPECT_TRUE(unchanged.Click(k_EditorOk));
	EXPECT_FALSE(unchanged.dialog.IsShowingMessage());
	EXPECT_FALSE(unchanged.dialog.IsOpen());
}

TEST(TattooEditorWarning, ReturnAndEscapePressTheOkOfTheWarning)
{
	EditorFixture f(true);
	EXPECT_TRUE(f.dialog.KeyDown(SDLK_RETURN, f.editor));
	EXPECT_TRUE(f.dialog.IsShowingMessage());
	// Escape answers the warning. It does not cancel the editor.
	EXPECT_TRUE(f.dialog.KeyDown(SDLK_ESCAPE, f.editor));
	EXPECT_FALSE(f.dialog.IsShowingMessage());
	EXPECT_TRUE(f.editor.IsOpen());
	EXPECT_EQ(f.editor.answers, 1);
	// With the change cleared, Return is OK of the editor again, and it closes the editor
	EXPECT_TRUE(f.dialog.KeyDown(SDLK_RETURN, f.editor));
	EXPECT_FALSE(f.editor.IsOpen());
}

TEST(TattooEditorWarning, TheControlsBehindTheWarningDoNotAct)
{
	EditorFixture f(true);
	f.Click(k_EditorOk);
	ASSERT_TRUE(f.dialog.IsShowingMessage());
	// A click on the OK of the editor behind the warning does nothing
	EXPECT_FALSE(f.Click(k_EditorOk));
	EXPECT_TRUE(f.dialog.IsShowingMessage());
	EXPECT_EQ(f.editor.answers, 0);
}

TEST(SetupMessage, TheBoxBehindDimsToAQuarterOverHalfASecondAndComesBackAtOnce)
{
	SetupMessage message;
	int answers = 0;
	message.onAnswer = [&answers]() { ++answers; };
	EXPECT_FLOAT_EQ(message.BehindAlpha(1.0f), 1.0f);
	message.Show(u"A", u"OK");
	EXPECT_FLOAT_EQ(message.BehindAlpha(1.0f), 1.0f);
	message.Update(0.25f);
	EXPECT_GT(message.BehindAlpha(1.0f), 0.25f);
	EXPECT_LT(message.BehindAlpha(1.0f), 1.0f);
	message.Update(0.5f);
	// A quarter of 255 is 63.75, and the game cuts the fraction off
	EXPECT_FLOAT_EQ(message.BehindAlpha(1.0f), 63.0f / 255.0f);
	EXPECT_FLOAT_EQ(message.MessageAlpha(), 1.0f);
	// A key other than Return or Escape does not answer
	EXPECT_FALSE(message.KeyDown(SDLK_a));
	EXPECT_TRUE(message.KeyDown(SDLK_KP_ENTER));
	EXPECT_FALSE(message.IsShown());
	EXPECT_EQ(answers, 1);
	EXPECT_FLOAT_EQ(message.BehindAlpha(1.0f), 1.0f);
}

TEST(SetupMessage, TheOpacitiesAreWholeStepsCutDown)
{
	EXPECT_EQ(SetupMessage::BehindSteps(1.0f, 0.0f), 255);
	EXPECT_EQ(SetupMessage::BehindSteps(1.0f, 1.0f), 63);
	EXPECT_EQ(SetupMessage::BehindSteps(1.0f, 0.5f), 159);
	EXPECT_EQ(SetupMessage::BehindSteps(0.5f, 1.0f), 0);
	EXPECT_EQ(SetupMessage::BehindSteps(0.0f, 1.0f), 0);
	EXPECT_EQ(SetupMessage::BehindSteps(1.5f, 0.0f), 255);
	EXPECT_EQ(SetupMessage::MessageSteps(0.0f), 0);
	EXPECT_EQ(SetupMessage::MessageSteps(0.999f), 254);
	EXPECT_EQ(SetupMessage::MessageSteps(1.0f), 255);
}

TEST(DialogPainter, TheEdgeLinesAreTheEdgeColourTimesTheBoxColourIn256ths)
{
	constexpr auto white = glm::u8vec3(255);
	EXPECT_EQ(DialogPainter::EdgeLineColour(white, white), glm::u8vec3(254));
	EXPECT_EQ(DialogPainter::EdgeLineColour({255, 128, 0}, white), glm::u8vec3(254, 127, 0));
	EXPECT_EQ(DialogPainter::EdgeLineColour({0, 0, 0}, white), glm::u8vec3(0));
	EXPECT_EQ(DialogPainter::EdgeLineColour(white, {128, 64, 1}), glm::u8vec3(127, 63, 0));
}
