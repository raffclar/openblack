/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "ECS/Systems/AdvisorSystemInterface.h"
#include "Help/AdvisorModel.h"
#include "Help/SpiritView.h"
#include "Help/Spirits.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class AdvisorSystem final: public AdvisorSystemInterface
{
public:
	AdvisorSystem();
	~AdvisorSystem() override;

	void Load() override;
	[[nodiscard]] bool IsLoaded() const override { return _loaded; }
	void Reset() override;

	void Update(const Frame& frame) override;
	void ProcessTurn() override;

	[[nodiscard]] help::spirits::AdvisorSpiritController& GetController() override { return *_controller; }
	[[nodiscard]] const help::spirits::AdvisorSpiritController& GetController() const override { return *_controller; }
	[[nodiscard]] const help::spirits::AdvisorModel* GetModel(int advisor) const override;

	[[nodiscard]] std::span<const Draw> GetDraws() const override { return _draws; }
	[[nodiscard]] std::span<const TrailVertex> GetTrails() const override { return _trails; }

private:
	/// The advisors' logic over the models' data, asking this system what it needs of the game
	void MakeController();
	[[nodiscard]] help::spirits::Queries MakeQueries();
	/// The advisor's bones in the world as its anims stand so far this frame
	[[nodiscard]] std::vector<glm::mat4> PoseSoFar(int advisor) const;
	void MakeDraws(int32_t stepMs);
	void MakeTrails();

	bool _loaded {false};
	std::array<std::shared_ptr<const help::spirits::AdvisorModel>, help::spirits::k_Dudes> _models;
	/// Without the files, the logic runs on empty data
	std::array<help::spirits::DudeData, help::spirits::k_Dudes> _emptyData;
	std::unique_ptr<help::spirits::AdvisorSpiritController> _controller;
	help::spirits::SpiritView _view;
	/// One clock for both halos, moved on by the frame's step each time a halo is drawn
	int32_t _haloClockMs {0};
	std::vector<Draw> _draws;
	std::vector<TrailVertex> _trails;
};

} // namespace openblack::ecs::systems
