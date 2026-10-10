/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <map>
#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/TattooEditorSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using creature_tattoo::Slot;
using creature_tattoo::Slots;
using creature_tattoo_editor::SiteOnScreen;

namespace
{
constexpr auto k_Creature = static_cast<entt::entity>(7);
constexpr glm::u8vec3 k_Green {0, 200, 0};

/// The skins the editor writes to, a creature's slots at a time
class FakeSkin final: public TattooEditorSystem::Skin
{
public:
	explicit FakeSkin(std::map<entt::entity, Slots>& skins)
	    : _skins(skins)
	{
	}
	[[nodiscard]] std::optional<Slots> Read(entt::entity creature) const override
	{
		const auto found = _skins.find(creature);
		return found != _skins.end() ? std::optional(found->second) : std::nullopt;
	}
	void Write(entt::entity creature, size_t slot, const Slot& tattoo) override { _skins.at(creature).at(slot) = tattoo; }

private:
	std::map<entt::entity, Slots>& _skins;
};

struct Fixture
{
	std::map<entt::entity, Slots> skins {{k_Creature, Slots {}}};
	TattooEditorSystem editor {std::make_unique<FakeSkin>(skins)};
	/// One place, at the middle of the screen, facing the camera
	std::array<SiteOnScreen, 1> sites {SiteOnScreen {.site = 4, .screen = {400, 300}, .facingCamera = 1.0f}};

	void OverThePlace() { editor.Hover(sites, {400, 300}); }
	void AwayFromIt() { editor.Hover(sites, {0, 0}); }
};
} // namespace

TEST(TattooEditorSystem, OpensOnACreatureWithItsTattoosAndView)
{
	Fixture f;
	f.skins.at(k_Creature).at(2) = {.design = 3, .site = 1, .colour = k_Green};
	const creature_tattoo_editor::Orbit view {.eye = {1.0f, 2.0f, 3.0f}, .focus = {0.0f, 1.0f, 0.0f}};
	f.editor.Open(k_Creature, view);
	EXPECT_TRUE(f.editor.IsOpen());
	EXPECT_EQ(f.editor.GetCreature(), k_Creature);
	EXPECT_EQ(f.editor.GetSession().slots, f.skins.at(k_Creature));
	EXPECT_EQ(f.editor.GetView().eye, view.eye);
}

TEST(TattooEditorSystem, ASymbolDroppedOverAPlaceGoesIntoTheSkin)
{
	Fixture f;
	f.editor.Open(k_Creature, {});
	f.AwayFromIt();
	EXPECT_FALSE(f.editor.Drop(9, k_Green));
	EXPECT_TRUE(f.skins.at(k_Creature).at(0).Empty());
	f.OverThePlace();
	EXPECT_EQ(f.editor.GetHoveredSite(), 4);
	EXPECT_TRUE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.skins.at(k_Creature).at(0), (Slot {.design = 9, .site = 4, .colour = k_Green}));
}

TEST(TattooEditorSystem, LiftingTakesTheTattooOffTheSkin)
{
	Fixture f;
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	const auto lifted = f.editor.Lift();
	ASSERT_TRUE(lifted.has_value());
	EXPECT_EQ(lifted->design, 9);
	EXPECT_EQ(lifted->colour, k_Green);
	EXPECT_TRUE(f.skins.at(k_Creature).at(0).Empty());
	f.AwayFromIt();
	EXPECT_FALSE(f.editor.Lift().has_value());
}

TEST(TattooEditorSystem, CancelPutsBackTheTattoosItOpenedWith)
{
	Fixture f;
	f.skins.at(k_Creature).at(5) = {.design = 1, .site = 0, .colour = k_Green};
	const auto before = f.skins.at(k_Creature);
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	ASSERT_NE(f.skins.at(k_Creature), before);
	f.editor.Cancel();
	EXPECT_EQ(f.skins.at(k_Creature), before);
	EXPECT_FALSE(f.editor.IsOpen());
}

TEST(TattooEditorSystem, OkKeepsTheTattoosAndClosesOrWarnsFirst)
{
	Fixture f;
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.editor.Ok(true), creature_tattoo_editor::Accept::Warn);
	EXPECT_TRUE(f.editor.IsOpen());
	f.editor.Answered();
	EXPECT_EQ(f.editor.Ok(true), creature_tattoo_editor::Accept::Close);
	EXPECT_FALSE(f.editor.IsOpen());
	EXPECT_EQ(f.skins.at(k_Creature).at(0).design, 9);
}

TEST(TattooEditorSystem, TheViewTurnsOnlyWhileOpen)
{
	Fixture f;
	const creature_tattoo_editor::Orbit view {.eye = {10.0f, 5.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	f.editor.Open(k_Creature, view);
	f.editor.Steer({100, 100}, {0, 100}, 16.0f);
	f.editor.Update(16.0f);
	EXPECT_NE(f.editor.GetView().eye, view.eye);
	f.editor.Close();
	const auto stopped = f.editor.GetView().eye;
	f.editor.Update(16.0f);
	EXPECT_EQ(f.editor.GetView().eye, stopped);
}
