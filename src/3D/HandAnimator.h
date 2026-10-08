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
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack
{

/// Evaluates the player hand animations stored in Data/CTR/hh.HBN (spec: Data/hndspec5.txt).
///
/// Nodes named C* are the central animation of a hand state (Cwiggle, Ccan_pickup, Cgrip...).
/// Nodes named L*_lr / L*_fb are directional layers: they are not played over time but sampled
/// from the hand motion (-1..+1) and applied as a delta on top of the matching C node.
///
/// Evaluation, row-vector Lionhead matrices:
///   stored float3 (x, y, z) -> Euler matrix Y-X-Z
///   linear interpolation of the 9 rotation components + row normalisation
///   localRot = bindGlobalRot * animRot * inverse(bindParentGlobalRot)
class HandAnimator
{
public:
	/// Row-vector 3x3 rotation (Lionhead convention) + translation.
	struct Affine
	{
		std::array<float, 9> r;
		glm::vec3 t;
	};
	using Pose = std::vector<Affine>;

	struct ClipInfo
	{
		std::string name;
		std::string set;
		uint32_t durationMs;
		uint32_t frameCount;
		bool loop;
	};

	/// @param bindGlobals default (bind) global bone matrices, column-vector form as stored by L3DMesh.
	/// @param parents parent index per bone, UINT32_MAX for the root.
	bool Load(const std::vector<uint8_t>& hbnFile, const std::filesystem::path& specsDirectory,
	          const std::vector<glm::mat4>& bindGlobals, const std::vector<uint32_t>& parents) noexcept;

	[[nodiscard]] bool IsLoaded() const noexcept { return !_clips.empty(); }
	[[nodiscard]] bool Has(std::string_view name) const noexcept;
	[[nodiscard]] std::vector<ClipInfo> ListClips() const noexcept;
	[[nodiscard]] const std::string& GetCurrentClip() const noexcept { return _clipName; }

	/// Cross-fade to a C node. Playing the current clip again does nothing.
	bool Play(std::string_view name, std::chrono::milliseconds blend) noexcept;
	/// special_hold (holding an object): blend of the static poses Cphile (0) -> Chorn (1) with the
	/// Lhold_fingers layers on top. nullopt returns to the current C node.
	void SetSpecialHold(std::optional<float> fill, std::chrono::milliseconds blend) noexcept;
	[[nodiscard]] bool IsSpecialHold() const noexcept { return _specialHold.has_value(); }
	/// Hold the current clip at a (fractional) frame index instead of playing it over time, like the original hand does
	/// for Chold_side / Chold_above (frame = numFrames / 2 * grip). nullopt plays the clip normally.
	void SetFrame(std::optional<float> frame) noexcept { _frame = frame; }
	/// Hold the current clip at a time in ms (the animation's "frame" argument in the holding state), sampled like Play.
	void SetTime(std::optional<float> timeMs) noexcept { _fixedTime = timeMs; }
	[[nodiscard]] uint32_t GetDurationMs(std::string_view name) const noexcept;
	[[nodiscard]] uint32_t GetFrameCount(std::string_view name) const noexcept;

	/// Directional motion in -1..+1 used to sample the L*_lr / L*_fb layers.
	void SetMotion(float leftRight, float forwardBack) noexcept;
	/// How much of the directional layers' translation is applied (0 = rotations only).
	void SetLayerTranslationScale(float scale) noexcept { _layerTranslationScale = scale; }
	/// Keep the root bone in its bind pose (the animated root turns/moves the whole hand).
	void SetRootLocked(bool locked) noexcept { _rootLocked = locked; }

	void Update(std::chrono::microseconds dt) noexcept;
	/// The current pose becomes the blend source and the blend timer restarts at 0
	void StartStateBlend() noexcept;
	/// The blend timer, after the state's update: while timer < 0.13, timer += dt (s) and t = timer / 0.13
	/// (StateBlendFraction); the frame it reaches 0.13, no t. The matrices are blended in world space
	/// by the hand system (HandSystem::BlendHandWorld)
	void AdvanceStateBlend(float dtSeconds) noexcept;
	/// The bone matrices to be blended in world space (draw data)
	[[nodiscard]] std::vector<glm::mat4>& BoneMatricesForBlend() noexcept { return _boneMatrices; }
	/// The state blend's t for the hand's draw position, nullopt when not blending this frame
	[[nodiscard]] std::optional<float> StateBlendFraction() const noexcept { return _stateBlendT; }

	/// Stateless evaluation of a clip (plus its L layers) at a given time, as global bone matrices. For tools/tests.
	[[nodiscard]] std::vector<glm::mat4> Evaluate(std::string_view name, float timeMs, float leftRight = 0.0f,
	                                              float forwardBack = 0.0f, float layerTranslationScale = 0.0f) const noexcept;

	/// Global bone matrices (column-vector form), ready for bgfx::setTransform.
	[[nodiscard]] const std::vector<glm::mat4>& GetBoneMatrices() const noexcept { return _boneMatrices; }

private:
	struct Clip
	{
		ClipInfo info;
		/// [frame][bone]
		std::vector<std::vector<std::optional<glm::vec3>>> rotations;
		std::vector<std::vector<std::optional<glm::vec3>>> translations;
	};

	[[nodiscard]] Pose SampleFrames(const Clip& clip, uint32_t i0, uint32_t i1, float u) const noexcept;
	[[nodiscard]] Pose Sample(const Clip& clip, float timeMs) const noexcept;
	[[nodiscard]] std::optional<Pose> SampleDirectional(std::string_view name, float value) const noexcept;
	[[nodiscard]] Pose ApplyMotion(const Pose& base, std::string_view clipName) const noexcept;
	void ComputeBoneMatrices() noexcept;
	[[nodiscard]] std::vector<glm::mat4> ToGlobals(const Pose& pose) const noexcept;

	std::unordered_map<std::string, Clip> _clips;
	std::vector<uint32_t> _parents;
	Pose _bindLocal;
	std::vector<std::array<float, 9>> _bindGlobalRot;
	std::vector<std::array<float, 9>> _invBindGlobalRot;

	std::string _clipName;
	float _timeMs {0.0f};
	Pose _pose;
	Pose _fromPose;
	float _blendMs {0.0f};
	float _blendDurationMs {0.0f};
	float _motionLeftRight {0.0f};
	float _motionForwardBack {0.0f};
	float _layerTranslationScale {0.0f};
	bool _rootLocked {false};
	std::optional<float> _specialHold;
	std::optional<float> _frame;
	std::optional<float> _fixedTime;
	std::vector<glm::mat4> _boneMatrices;
	float _stateBlendSeconds {0.13f}; ///< 0.13: no blend running
	std::optional<float> _stateBlendT;
};

} // namespace openblack
