/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "ECS/Systems/TattooEditorSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TattooEditorSystem final: public TattooEditorSystemInterface
{
public:
	/// Where the creature's tattoos are read from and written to: its skin, painted again as they change
	class Skin
	{
	public:
		virtual ~Skin() = default;
		[[nodiscard]] virtual std::optional<creature_tattoo::Slots> Read(entt::entity creature) const = 0;
		virtual void Write(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) = 0;
	};

	/// The creatures' skins in the registry
	TattooEditorSystem();
	explicit TattooEditorSystem(std::unique_ptr<Skin> skin);

	void Open(entt::entity creature, const creature_tattoo_editor::Orbit& view) override;
	void Close() override;
	[[nodiscard]] bool IsOpen() const override { return _creature.has_value(); }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return _creature; }
	[[nodiscard]] const creature_tattoo_editor::Orbit& GetView() const override { return _view; }
	[[nodiscard]] const creature_tattoo_editor::Session& GetSession() const override { return _session; }
	void Update(float milliseconds) override;
	void Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds) override;
	void Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer) override;
	[[nodiscard]] std::optional<uint8_t> GetHoveredSite() const override { return _hovered; }
	[[nodiscard]] std::optional<glm::ivec2> GetHoveredPoint() const override { return _hoveredPoint; }
	bool Drop(uint8_t design, const glm::u8vec3& colour) override;
	std::optional<creature_tattoo::Slot> Lift() override;
	creature_tattoo_editor::Accept Ok(bool networkGame) override;
	void Answered() override { _session.changed = false; }
	void Cancel() override;

private:
	/// The creature wears the tattoos as they are now
	void Write(const creature_tattoo::Slots& slots);

	std::unique_ptr<Skin> _skin;
	std::optional<entt::entity> _creature;
	creature_tattoo_editor::Session _session;
	creature_tattoo_editor::Orbit _view;
	std::optional<uint8_t> _hovered;
	std::optional<glm::ivec2> _hoveredPoint;
};

} // namespace openblack::ecs::systems
