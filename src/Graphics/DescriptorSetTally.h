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

#include <array>
#include <span>
#include <vector>

namespace openblack::graphics
{

/// How a view orders its draws, as bgfx's view modes do
enum class ViewOrder : uint8_t
{
	/// By program, then by depth, ties in the order they came
	ByProgram,
	/// In the order they came
	Sequential,
	/// The nearest (smallest depth) first
	DepthAscending,
	/// The farthest (greatest depth) first
	DepthDescending,
};

/// The descriptor sets one view's draws took in a frame
struct ViewDescriptorSets
{
	uint16_t view;
	uint32_t draws;
	uint32_t sets;
	/// The sets the view's draws would take drawn in the order they came
	uint32_t setsInOrder;
};

/// The descriptor sets a frame's draws took, all views together and view by view (the views with draws, in order)
struct FrameDescriptorSets
{
	uint32_t draws {0};
	uint32_t sets {0};
	std::vector<ViewDescriptorSets> views;
};

/// What a frame's descriptor sets call for
enum class DescriptorBudget : uint8_t
{
	/// Well within what the renderer has
	Fine,
	/// Nearing it: worth a warning
	Near,
	/// Too near to risk another frame like it: skip what can be skipped
	Over,
};

/// bgfx's Vulkan renderer gives every frame in flight a share of one pool of descriptor sets: 1024 a frame. A draw
/// takes a new set whenever its program or textures differ from the draw before it in the order the views sort them,
/// and when the pool runs dry the renderer aborts. These count against that share, leaving room for the draws that
/// don't go through the game's programs (the interface, the debug windows, bgfx's own blits).
constexpr uint32_t k_DescriptorSetsPerFrame = 1024;
constexpr uint32_t k_DescriptorSetsNear = 640;
constexpr uint32_t k_DescriptorSetsOver = 896;

[[nodiscard]] constexpr DescriptorBudget JudgeDescriptorSets(uint32_t sets) noexcept
{
	if (sets >= k_DescriptorSetsOver)
	{
		return DescriptorBudget::Over;
	}
	return sets >= k_DescriptorSetsNear ? DescriptorBudget::Near : DescriptorBudget::Fine;
}

/// What the next frames do after one took too many sets
struct DescriptorSetRelief
{
	/// Views to draw in the order their draws come, which takes far fewer sets than their sorted order
	std::vector<uint16_t> inOrder;
	/// Whether to leave out the view the scene can do without, the rest still taking too many
	bool skipOptional {false};
};

/// The relief for a frame that took too many sets: the views that may be drawn in the order their draws come where
/// that halves their sets at least, then the optional view if what is left still nears the budget
[[nodiscard]] DescriptorSetRelief PlanDescriptorSetRelief(const FrameDescriptorSets& frame,
                                                          std::span<const uint16_t> reorderable);

/// Counts the descriptor sets bgfx's Vulkan renderer will take for a frame, from the textures bound and the draws
/// submitted, as they are given to bgfx. Bindings stay from one draw to the next until a discard drops them, as bgfx
/// keeps them, so a run of draws that binds a texture part way leaves two sets of bindings behind it: if the view then
/// sorts those draws so that the two alternate, every draw takes a set of its own.
class DescriptorSetTally
{
public:
	static constexpr uint8_t k_Stages = 16;
	static constexpr uint16_t k_Views = 256;

	DescriptorSetTally() noexcept { DiscardBindings(); }

	/// A texture bound at a stage, by its bgfx handle
	void SetTexture(uint8_t stage, uint16_t texture) noexcept;
	/// The bindings were dropped
	void DiscardBindings() noexcept;
	/// How a view orders its draws from now on
	void SetViewOrder(uint16_t view, ViewOrder order) noexcept;
	/// A draw with the program (its bgfx handle) at the depth given, with the textures bound now. dropsBindings: the
	/// submit discards the bindings after it.
	void Submit(uint16_t view, uint16_t program, uint32_t depth, bool dropsBindings);
	/// The frame's sets, counted in the order bgfx draws them. The draws and bindings are dropped for the next frame,
	/// as bgfx drops them; the views keep their order.
	[[nodiscard]] FrameDescriptorSets EndFrame();

private:
	struct Draw
	{
		uint16_t view;
		uint16_t program;
		uint32_t depth;
		uint32_t sequence;
		uint32_t bindings;
	};

	std::array<uint16_t, k_Stages> _textures {};
	std::array<ViewOrder, k_Views> _orders {};
	std::vector<Draw> _draws;
};

} // namespace openblack::graphics
