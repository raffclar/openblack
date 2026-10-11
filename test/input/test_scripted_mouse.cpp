/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <SDL.h>
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Input/GameActionMap.h>
#include <Input/InjectedInput.h>
#include <Input/ScriptedMouse.h>
#include <Inspector.h>
#include <Inspector/GameInput.h>
#include <Inspector/InputControl.h>
#include <Locator.h>
#include <Windowing/WindowingInterface.h>

using namespace openblack;

namespace
{

constexpr glm::ivec2 k_Window {800, 600};

/// A window of 800 by 600 that is never drawn
class StillWindow final: public windowing::WindowingInterface
{
public:
	[[nodiscard]] void* GetHandle() const override { return nullptr; }
	[[nodiscard]] NativeHandles GetNativeHandles() const override { return {}; }
	[[nodiscard]] uint32_t GetID() const override { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const override { return k_Window; }
	[[nodiscard]] float GetAspectRatio() const override
	{
		return static_cast<float>(k_Window.x) / static_cast<float>(k_Window.y);
	}
	[[nodiscard]] bool IsMinimised() const override { return false; }
	[[nodiscard]] bool IsWindowed() const override { return true; }
	WindowingInterface& SetDisplayMode(windowing::DisplayMode /*mode*/) override { return *this; }
};

/// What the game saw of one frame: where the action map read the pointer as the frame began, and the moves and presses
/// its events brought
struct Seen
{
	glm::ivec2 readAtFrameStart {0, 0};
	uint32_t buttonsAtFrameStart {0};
	glm::ivec2 eventsMoved {0, 0};
	std::vector<glm::ivec2> motionsAt;
	std::vector<glm::ivec2> pressesAt;
	glm::ivec2 pointerAfterEvents {0, 0};
};

/// The game's action map with the window's event queue, run frame by frame as the game runs it, and the inspector's
/// input made on it as the game's inspector makes it
class ScriptedMouse: public testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_EQ(SDL_InitSubSystem(SDL_INIT_EVENTS), 0) << SDL_GetError();
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		Locator::windowing::emplace<StillWindow>();
		_actions = &static_cast<input::GameActionMap&>(Locator::gameActionSystem::emplace<input::GameActionMap>());
		auto provider = std::make_unique<inspector::InputProvider>(_input);
		_provider = provider.get();
		_inspector.Add(std::move(provider));
	}

	void TearDown() override
	{
		Locator::gameActionSystem::reset();
		Locator::windowing::reset();
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		SDL_QuitSubSystem(SDL_INIT_EVENTS);
	}

	void Ask(const std::string& line)
	{
		const auto decoded = inspector::DecodeRequest(line);
		ASSERT_TRUE(std::holds_alternative<inspector::Request>(decoded));
		const auto answer = _inspector.Answer(std::get<inspector::Request>(decoded));
		ASSERT_TRUE(answer.Ok()) << line << ": " << answer.error;
	}

	/// One frame as the game makes it: the inspector's input due, the action map's frame, the scripted mouse pumped,
	/// then the window's events
	Seen Frame()
	{
		_provider->Frame(++_frame);
		_actions->Frame();
		Seen seen {.readAtFrameStart = glm::ivec2(_actions->GetMousePosition()),
		           .buttonsAtFrameStart = _actions->GetPointerButtons()};
		_actions->PumpScriptedPointer();
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_MOUSEMOTION)
			{
				seen.eventsMoved += glm::ivec2(event.motion.xrel, event.motion.yrel);
				seen.motionsAt.emplace_back(event.motion.x, event.motion.y);
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				seen.pressesAt.emplace_back(event.button.x, event.button.y);
			}
			_actions->ProcessEvent(event);
		}
		seen.pointerAfterEvents = _actions->GetPointerPosition();
		return seen;
	}

	input::GameActionMap* _actions {nullptr};
	inspector::GameInput _input;
	inspector::Inspector _inspector;
	inspector::InputProvider* _provider {nullptr};
	uint64_t _frame {0};
};

} // namespace

// A real mouse moved now is read by the action map only on the next frame, as SDL takes its state while pumping the
// window's messages, after the action map's frame. The move's event reaches the game this frame. The inspector's
// pointer does the same
TEST_F(ScriptedMouse, AMoveIsReadByTheActionMapOnePumpLate)
{
	Ask(R"({"query": "input.pointer", "params": {"screen": [100, 100]}})");
	Frame();
	Frame();

	Ask(R"({"query": "input.pointer", "params": {"screen": [300, 250]}})");
	const auto moved = Frame();
	EXPECT_EQ(moved.readAtFrameStart, glm::ivec2(100, 100));
	EXPECT_EQ(moved.eventsMoved, glm::ivec2(200, 150));
	ASSERT_EQ(moved.motionsAt.size(), 1);
	EXPECT_EQ(moved.motionsAt[0], glm::ivec2(300, 250));
	EXPECT_EQ(moved.pointerAfterEvents, glm::ivec2(300, 250));

	const auto next = Frame();
	EXPECT_EQ(next.readAtFrameStart, glm::ivec2(300, 250));
	EXPECT_EQ(next.eventsMoved, glm::ivec2(0, 0));
}

// The timing of recordings made before: the action map reads the pointer where it was put that same frame
TEST_F(ScriptedMouse, ImmediateTimingIsReadTheSameFrame)
{
	Ask(R"({"query": "input.pointer", "params": {"screen": [100, 100], "timing": "immediate"}})");
	Frame();
	Ask(R"({"query": "input.pointer", "params": {"screen": [300, 250], "timing": "immediate"}})");
	const auto moved = Frame();
	EXPECT_EQ(moved.readAtFrameStart, glm::ivec2(300, 250));
	EXPECT_EQ(moved.eventsMoved, glm::ivec2(200, 150));
}

// A button pressed now is held for the action map's frame only from the next frame, and its press comes where the
// pointer is once the moves before it are pumped
TEST_F(ScriptedMouse, ButtonsAreTakenAsTheyArePumpedAfterTheMovesBeforeThem)
{
	Ask(R"({"query": "input.pointer", "params": {"screen": [100, 100]}})");
	Frame();
	Ask(R"({"query": "input.pointer", "params": {"screen": [400, 300]}})");
	Ask(R"({"query": "input.button", "params": {"button": "left", "action": "press"}})");
	const auto pressed = Frame();
	EXPECT_EQ(pressed.buttonsAtFrameStart, 0u);
	ASSERT_EQ(pressed.pressesAt.size(), 1);
	EXPECT_EQ(pressed.pressesAt[0], glm::ivec2(400, 300));
	EXPECT_TRUE(_actions->GetBindable(input::BindableActionMap::MOVE));

	const auto held = Frame();
	EXPECT_EQ(held.buttonsAtFrameStart, static_cast<uint32_t>(SDL_BUTTON_LMASK));
	EXPECT_TRUE(_actions->GetBindable(input::BindableActionMap::MOVE));

	Ask(R"({"query": "input.button", "params": {"button": "left", "action": "release"}})");
	const auto letGo = Frame();
	EXPECT_EQ(letGo.buttonsAtFrameStart, static_cast<uint32_t>(SDL_BUTTON_LMASK));
	EXPECT_FALSE(_actions->GetBindable(input::BindableActionMap::MOVE));
	EXPECT_EQ(Frame().buttonsAtFrameStart, 0u);
}

// When the game puts the cursor somewhere (as the camera holds it on the ring dragging round the screen's edge), a
// drag's next step moves the pointer on from there by the step, as a hand moving a real mouse does
TEST_F(ScriptedMouse, ADragsStepsGoOnFromWhereTheGamePutTheCursor)
{
	Ask(R"({"query": "input.path", "params": {"points": [[100, 100], [110, 100], [120, 100]]}})");
	Frame();
	_actions->WarpCursor({500, 400});
	const auto stepped = Frame();
	EXPECT_EQ(stepped.readAtFrameStart, glm::ivec2(500, 400));
	EXPECT_EQ(stepped.pointerAfterEvents, glm::ivec2(510, 400));
	EXPECT_EQ(Frame().pointerAfterEvents, glm::ivec2(520, 400));
}

// Two moves sent before a pump go one after the other, and the pointer stays in the window
TEST_F(ScriptedMouse, MovesSentTogetherFollowEachOtherAndStayInTheWindow)
{
	Ask(R"({"query": "input.pointer", "params": {"screen": [700, 500]}})");
	Ask(R"({"query": "input.pointer", "params": {"screen": [790, 590]}})");
	const auto both = Frame();
	ASSERT_EQ(both.motionsAt.size(), 2);
	EXPECT_EQ(both.motionsAt[1], glm::ivec2(790, 590));

	auto pointer = input::ScriptedPointer {.position = {790, 590}};
	auto out = input::PumpedMotion({50, 50}, 0);
	pointer = input::Pump(pointer, out, k_Window);
	EXPECT_EQ(pointer.position, k_Window - 1);
	EXPECT_EQ(glm::ivec2(out.motion.x, out.motion.y), k_Window - 1);
	EXPECT_EQ(glm::ivec2(out.motion.xrel, out.motion.yrel), glm::ivec2(50, 50));
}

// The scripted mouse's pumped events are the game's own: the input lock lets them in, the player's own are told apart
TEST(ScriptedMouseEvents, PumpedEventsAreInjected)
{
	auto motion = input::PumpedMotion({1, 2}, 0);
	auto press = input::PumpedButton(SDL_BUTTON_LEFT, true, 1, 0);
	EXPECT_TRUE(input::IsPumped(motion));
	EXPECT_TRUE(input::IsInjected(motion));
	EXPECT_TRUE(input::IsPumped(press));
	EXPECT_TRUE(input::IsInjected(press));

	SDL_Event old {};
	old.type = SDL_MOUSEMOTION;
	input::MarkInjected(old);
	EXPECT_TRUE(input::IsInjected(old));
	EXPECT_FALSE(input::IsPumped(old));
	// An event not the scripted mouse's leaves it as it was
	const auto pointer = input::ScriptedPointer {.position = {5, 5}, .buttons = 0};
	EXPECT_EQ(input::Pump(pointer, old, k_Window).position, glm::ivec2(5, 5));
}
