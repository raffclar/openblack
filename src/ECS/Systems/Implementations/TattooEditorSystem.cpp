/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TattooEditorSystem.h"

#include <algorithm>
#include <vector>

#include "ECS/Components/CreatureSkin.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/ForceFeedbackSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// A force-feedback mouse gives a pulse as a tattoo goes on or comes off
void Pulse()
{
	if (Locator::forceFeedbackSystem::has_value())
	{
		Locator::forceFeedbackSystem::value().Start(ImmersionEffectType::CommandSuccess);
	}
}

/// The skins of the creatures in the registry, painted again by the skin system as their tattoos change
class RegistrySkin final: public TattooEditorSystem::Skin
{
public:
	[[nodiscard]] std::optional<creature_tattoo::Slots> Read(entt::entity creature) const override
	{
		if (!Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureTattoos>(creature);
		return tattoos != nullptr ? std::optional(tattoos->slots) : std::nullopt;
	}

	void Write(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) override
	{
		if (Locator::creatureSkinSystem::has_value())
		{
			Locator::creatureSkinSystem::value().SetTattoo(creature, slot, tattoo);
		}
	}
	void WritePictures(entt::entity creature, std::vector<uint8_t> pictures) override
	{
		if (Locator::creatureSkinSystem::has_value())
		{
			Locator::creatureSkinSystem::value().SetTattooPictures(creature, std::move(pictures));
		}
	}
};
} // namespace

TattooEditorSystem::TattooEditorSystem()
    : TattooEditorSystem(std::make_unique<RegistrySkin>())
{
}

TattooEditorSystem::TattooEditorSystem(std::unique_ptr<Skin> skin)
    : _skin(std::move(skin))
{
}

void TattooEditorSystem::Open(entt::entity creature, const creature_tattoo_editor::Orbit& view)
{
	_open = true;
	_creature = creature;
	_shown.reset();
	_session = creature_tattoo_editor::Open(_skin->Read(creature).value_or(creature_tattoo::Slots {}));
	_view = view;
	_hovered.reset();
	_hoveredPoint.reset();
}

void TattooEditorSystem::OpenFromMenu(std::optional<entt::entity> playerCreature, entt::entity shown,
                                      const creature_tattoo_editor::Orbit& view)
{
	_open = true;
	_creature = playerCreature;
	_shown = shown;
	const auto slots = playerCreature.has_value() ? _skin->Read(*playerCreature) : std::nullopt;
	_session = creature_tattoo_editor::Open(slots.value_or(creature_tattoo::Slots {}));
	_view = view;
	_hovered.reset();
	_hoveredPoint.reset();
	_face.reset();
	// The creature that the menu shows wears the tattoos being edited from the start
	for (size_t i = 0; i < _session.slots.size(); ++i)
	{
		_skin->Write(shown, i, _session.slots.at(i));
	}
	_skin->WritePictures(shown, creature_tattoo::PicturesBlock(_session.slots, _pictures));
}

void TattooEditorSystem::Close()
{
	_open = false;
	_creature.reset();
	_shown.reset();
	_hovered.reset();
	_hoveredPoint.reset();
	_face.reset();
}

void TattooEditorSystem::Update(float milliseconds)
{
	if (IsOpen())
	{
		creature_tattoo_editor::Turn(_view, milliseconds);
	}
}

void TattooEditorSystem::Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds)
{
	creature_tattoo_editor::Steer(_view, grab, pointer, milliseconds);
}

void TattooEditorSystem::Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer)
{
	_hovered = IsOpen() ? creature_tattoo_editor::SiteUnderPointer(sites, pointer) : std::nullopt;
	_hoveredPoint.reset();
	if (_hovered.has_value())
	{
		const auto site = std::ranges::find(sites, *_hovered, &creature_tattoo_editor::SiteOnScreen::site);
		_hoveredPoint = site->screen;
	}
}

bool TattooEditorSystem::Drop(uint8_t design, const glm::u8vec3& colour)
{
	if (!IsOpen())
	{
		return false;
	}
	const auto putOn = creature_tattoo_editor::Drop(_session, design, colour, _hovered);
	Write(_session.slots);
	// Each drop makes the creature's block of pictures again from its tattoos
	WritePictures(_session.slots);
	if (putOn)
	{
		_face = creature_tattoo_editor::k_PutOnFace;
		Pulse();
	}
	return putOn;
}

std::optional<creature_tattoo::Slot> TattooEditorSystem::Lift()
{
	if (!IsOpen() || !_hovered.has_value())
	{
		return std::nullopt;
	}
	auto lifted = creature_tattoo_editor::Lift(_session, *_hovered);
	if (lifted.has_value())
	{
		Write(_session.slots);
		_face = creature_tattoo_editor::k_LiftFace;
		Pulse();
	}
	return lifted;
}

creature_tattoo_editor::Accept TattooEditorSystem::Ok(bool networkGame)
{
	const auto accept = creature_tattoo_editor::Ok(_session, networkGame);
	if (accept == creature_tattoo_editor::Accept::Close)
	{
		Close();
	}
	return accept;
}

void TattooEditorSystem::Cancel()
{
	if (IsOpen())
	{
		Write(_session.opened);
		// The game does not make the block of pictures again here (vanilla-bugs 379), and openblack fixes this
		WritePictures(_session.opened);
	}
	Close();
}

std::vector<entt::entity> TattooEditorSystem::Wearers() const
{
	std::vector<entt::entity> wearers;
	if (_creature.has_value())
	{
		wearers.push_back(*_creature);
	}
	if (_shown.has_value())
	{
		wearers.push_back(*_shown);
	}
	return wearers;
}

void TattooEditorSystem::Write(const creature_tattoo::Slots& slots)
{
	for (const auto creature : Wearers())
	{
		for (size_t i = 0; i < slots.size(); ++i)
		{
			_skin->Write(creature, i, slots.at(i));
		}
	}
}

void TattooEditorSystem::WritePictures(const creature_tattoo::Slots& slots)
{
	for (const auto creature : Wearers())
	{
		_skin->WritePictures(creature, creature_tattoo::PicturesBlock(slots, _pictures));
	}
}
