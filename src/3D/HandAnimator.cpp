/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandAnimator.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <limits>

#include <MorphFile.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"

using namespace openblack;

namespace
{
using R3 = std::array<float, 9>;
using Affine = HandAnimator::Affine;
using Pose = HandAnimator::Pose;

constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();
/// hh.HBN is a Lionhead pack: "LiOnHeAd" + block name "Hand" (32 bytes) + block size.
/// The morph data (and all its offsets) start right after it.
constexpr size_t k_PackHeaderSize = 0x2C;

R3 Mul3(const R3& a, const R3& b)
{
	R3 o {};
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			o[i * 3 + j] = a[i * 3 + 0] * b[0 * 3 + j] + a[i * 3 + 1] * b[1 * 3 + j] + a[i * 3 + 2] * b[2 * 3 + j];
		}
	}
	return o;
}

R3 Transpose3(const R3& a)
{
	return {a[0], a[3], a[6], a[1], a[4], a[7], a[2], a[5], a[8]};
}

glm::vec3 RowVecMul(const glm::vec3& v, const R3& r)
{
	return {v.x * r[0] + v.y * r[3] + v.z * r[6], v.x * r[1] + v.y * r[4] + v.z * r[7], v.x * r[2] + v.y * r[5] + v.z * r[8]};
}

/// Row-vector composition: apply a, then b.
Affine MulAffine(const Affine& a, const Affine& b)
{
	return {Mul3(a.r, b.r), RowVecMul(a.t, b.r) + b.t};
}

/// The Y-X-Z Euler rotation (affine::RotationYXZ) by rows. The animation passes the stored float3 (x, y, z) as Y, X, Z.
R3 EulerYXZRows(const glm::vec3& v)
{
	const auto m = affine::RotationYXZ(v.y, v.x, v.z);
	return {m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]};
}

R3 NormalizeRows(R3 r)
{
	for (int row = 0; row < 3; ++row)
	{
		const float l = std::hypot(r[row * 3], r[row * 3 + 1], r[row * 3 + 2]);
		if (l > 0.0f)
		{
			r[row * 3] /= l;
			r[row * 3 + 1] /= l;
			r[row * 3 + 2] /= l;
		}
	}
	return r;
}

R3 LerpRotation(const R3& a, const R3& b, float t)
{
	R3 o {};
	for (size_t i = 0; i < 9; ++i)
	{
		o[i] = a[i] + (b[i] - a[i]) * t;
	}
	return NormalizeRows(o);
}

Pose BlendPoses(const Pose& a, const Pose& b, float t)
{
	if (t <= 0.0f)
	{
		return a;
	}
	if (t >= 1.0f)
	{
		return b;
	}
	Pose out(a.size());
	for (size_t i = 0; i < a.size(); ++i)
	{
		out[i] = {LerpRotation(a[i].r, b[i].r, t), a[i].t + (b[i].t - a[i].t) * t};
	}
	return out;
}

/// Applies an L layer as a delta with respect to its central frame, so the C animation is untouched while the hand
/// is still and the directional pose appears when it moves.
Pose ApplyDirectionalLayer(const Pose& base, const Pose& layer, const Pose& neutral, float translationScale)
{
	Pose out(base.size());
	for (size_t i = 0; i < base.size(); ++i)
	{
		const auto delta = Mul3(Transpose3(neutral[i].r), layer[i].r);
		out[i] = {NormalizeRows(Mul3(base[i].r, delta)), base[i].t + (layer[i].t - neutral[i].t) * translationScale};
	}
	return out;
}

float Ease(float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

/// L3DMesh keeps bone matrices in column-vector form built as mat4(o0,o1,o2,0, o3,o4,o5,0, o6,o7,o8,0, p,1).
Affine FromColumnMatrix(const glm::mat4& m)
{
	return {{m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]}, glm::vec3(m[3])};
}

glm::mat4 ToColumnMatrix(const Affine& a)
{
	return {a.r[0], a.r[1], a.r[2], 0.0f, a.r[3], a.r[4], a.r[5], 0.0f,
	        a.r[6], a.r[7], a.r[8], 0.0f, a.t.x,  a.t.y,  a.t.z,  1.0f};
}
} // namespace

bool HandAnimator::Load(const std::vector<uint8_t>& hbnFile, const std::filesystem::path& specsDirectory,
                        const std::vector<glm::mat4>& bindGlobals, const std::vector<uint32_t>& parents) noexcept
{
	auto logger = spdlog::get("game");
	_clips.clear();
	if (bindGlobals.empty() || bindGlobals.size() != parents.size())
	{
		SPDLOG_LOGGER_ERROR(logger, "HandAnimator: hand mesh has no bones");
		return false;
	}
	if (hbnFile.size() <= k_PackHeaderSize || std::memcmp(hbnFile.data(), "LiOnHeAd", 8) != 0)
	{
		SPDLOG_LOGGER_ERROR(logger, "HandAnimator: hh.HBN is not a LiOnHeAd pack");
		return false;
	}

	const std::vector<uint8_t> block(hbnFile.begin() + k_PackHeaderSize, hbnFile.end());
	morph::MorphFile morph;
	if (const auto result = morph.Open(block, specsDirectory); result != morph::MorphResult::Success)
	{
		SPDLOG_LOGGER_ERROR(logger, "HandAnimator: failed to read hh.HBN: {}", morph::ResultToStr(result));
		return false;
	}

	// Bind pose: locals from the mesh's global default matrices.
	const auto boneCount = bindGlobals.size();
	_parents = parents;
	_bindLocal.resize(boneCount);
	_bindGlobalRot.resize(boneCount);
	_invBindGlobalRot.resize(boneCount);
	for (size_t i = 0; i < boneCount; ++i)
	{
		const auto p = parents[i];
		const auto local = p == k_NoParent ? bindGlobals[i] : glm::affineInverse(bindGlobals[p]) * bindGlobals[i];
		_bindLocal[i] = FromColumnMatrix(local);
		_bindGlobalRot[i] = FromColumnMatrix(bindGlobals[i]).r;
		_invBindGlobalRot[i] = Transpose3(_bindGlobalRot[i]);
	}

	for (const auto& anim : morph.GetBaseAnimationSet())
	{
		if (anim.header.meshBoneCount != boneCount)
		{
			SPDLOG_LOGGER_WARN(logger, "HandAnimator: {} has {} bones, mesh has {}", anim.name, anim.header.meshBoneCount,
			                   boneCount);
			continue;
		}
		Clip clip;
		// The header gives the length in milliseconds and whether the clip loops (cycles, the C nodes).
		clip.info = {anim.name, anim.setName, anim.header.duration, anim.header.frameCount, anim.header.looping != 0};
		clip.rotations.resize(anim.keyframes.size(), std::vector<std::optional<glm::vec3>>(boneCount));
		clip.translations.resize(anim.keyframes.size(), std::vector<std::optional<glm::vec3>>(boneCount));
		for (size_t f = 0; f < anim.keyframes.size(); ++f)
		{
			const auto& frame = anim.keyframes[f];
			for (size_t k = 0; k < anim.rotatedJointIndices.size(); ++k)
			{
				const auto& e = frame.eulerAngles[k];
				clip.rotations[f][anim.rotatedJointIndices[k]] = glm::vec3(e[0], e[1], e[2]);
			}
			for (size_t k = 0; k < anim.translatedJointIndices.size(); ++k)
			{
				const auto& t = frame.translations[k];
				clip.translations[f][anim.translatedJointIndices[k]] = glm::vec3(t[0], t[1], t[2]);
			}
		}
		// Names such as Cspare repeat across sets: keep the first one.
		_clips.try_emplace(anim.name, std::move(clip));
	}

	SPDLOG_LOGGER_INFO(logger, "HandAnimator: loaded {} hand animations from hh.HBN", _clips.size());
	_pose = _bindLocal;
	_clipName.clear();
	Play("Cwiggle", std::chrono::milliseconds(0));
	ComputeBoneMatrices();
	return IsLoaded();
}

bool HandAnimator::Has(std::string_view name) const noexcept
{
	return _clips.contains(std::string(name));
}

std::vector<HandAnimator::ClipInfo> HandAnimator::ListClips() const noexcept
{
	std::vector<ClipInfo> out;
	out.reserve(_clips.size());
	for (const auto& [_, clip] : _clips)
	{
		out.push_back(clip.info);
	}
	std::sort(out.begin(), out.end(),
	          [](const auto& a, const auto& b) { return a.set != b.set ? a.set < b.set : a.name < b.name; });
	return out;
}

uint32_t HandAnimator::GetDurationMs(std::string_view name) const noexcept
{
	const auto it = _clips.find(std::string(name));
	return it == _clips.end() ? 0 : it->second.info.durationMs;
}

uint32_t HandAnimator::GetFrameCount(std::string_view name) const noexcept
{
	const auto it = _clips.find(std::string(name));
	return it == _clips.end() ? 0 : static_cast<uint32_t>(it->second.rotations.size());
}

bool HandAnimator::Play(std::string_view name, std::chrono::milliseconds blend) noexcept
{
	if (!Has(name))
	{
		return false;
	}
	if (name == _clipName)
	{
		return true;
	}
	_fromPose = _pose;
	_clipName = name;
	_timeMs = 0.0f;
	_blendMs = 0.0f;
	_blendDurationMs = static_cast<float>(blend.count());
	return true;
}

void HandAnimator::SetSpecialHold(std::optional<float> fill, std::chrono::milliseconds blend) noexcept
{
	if (fill.has_value() == _specialHold.has_value())
	{
		if (fill)
		{
			_specialHold = std::clamp(*fill, 0.0f, 1.0f);
		}
		return;
	}
	if (fill && (!Has("Cphile") || !Has("Chorn")))
	{
		return;
	}
	_specialHold = fill ? std::optional(std::clamp(*fill, 0.0f, 1.0f)) : std::nullopt;
	_fromPose = _pose;
	_blendMs = 0.0f;
	_blendDurationMs = static_cast<float>(blend.count());
	if (!_specialHold)
	{
		// Restart the C node from its first frame after letting go.
		_timeMs = 0.0f;
	}
}

void HandAnimator::SetMotion(float leftRight, float forwardBack) noexcept
{
	_motionLeftRight = std::clamp(leftRight, -1.0f, 1.0f);
	_motionForwardBack = std::clamp(forwardBack, -1.0f, 1.0f);
}

Pose HandAnimator::SampleFrames(const Clip& clip, uint32_t i0, uint32_t i1, float u) const noexcept
{
	Pose out(_bindLocal.size());
	for (size_t bi = 0; bi < _bindLocal.size(); ++bi)
	{
		auto r = _bindLocal[bi].r;
		auto t = _bindLocal[bi].t;
		auto rv0 = clip.rotations[i0][bi];
		auto rv1 = clip.rotations[i1][bi];
		if (rv0 || rv1)
		{
			const auto a = rv0 ? *rv0 : *rv1;
			const auto b = rv1 ? *rv1 : *rv0;
			const auto e = LerpRotation(EulerYXZRows(a), EulerYXZRows(b), u);
			const auto g = Mul3(_bindGlobalRot[bi], e);
			const auto p = _parents[bi];
			r = p == k_NoParent ? g : Mul3(g, _invBindGlobalRot[p]);
		}
		auto tv0 = clip.translations[i0][bi];
		auto tv1 = clip.translations[i1][bi];
		if (tv0 || tv1)
		{
			const auto a = tv0 ? *tv0 : *tv1;
			const auto b = tv1 ? *tv1 : *tv0;
			t = a + (b - a) * u;
		}
		out[bi] = {r, t};
	}
	return out;
}

Pose HandAnimator::Sample(const Clip& clip, float timeMs) const noexcept
{
	const auto n = static_cast<uint32_t>(clip.rotations.size());
	if (n == 0)
	{
		return _bindLocal;
	}
	uint32_t i0 = 0;
	uint32_t i1 = 0;
	float u = 0.0f;
	if (n > 1)
	{
		const float duration = std::max(1.0f, static_cast<float>(clip.info.durationMs));
		if (clip.info.loop)
		{
			const float t = std::fmod(std::fmod(timeMs, duration) + duration, duration);
			const float ff = t * static_cast<float>(n) / duration;
			i0 = static_cast<uint32_t>(ff) % n;
			i1 = (i0 + 1) % n;
			u = ff - std::floor(ff);
		}
		else
		{
			const float effective = duration * static_cast<float>(n) / static_cast<float>(n - 1);
			const float ff = std::clamp(timeMs, 0.0f, effective) * static_cast<float>(n) / effective;
			i0 = std::min(n - 1, static_cast<uint32_t>(ff));
			i1 = std::min(n - 1, i0 + 1);
			u = i0 == i1 ? 0.0f : ff - static_cast<float>(i0);
		}
	}
	return SampleFrames(clip, i0, i1, u);
}

std::optional<Pose> HandAnimator::SampleDirectional(std::string_view name, float value) const noexcept
{
	const auto it = _clips.find(std::string(name));
	if (it == _clips.end() || it->second.rotations.empty())
	{
		return std::nullopt;
	}
	// -1 = first frame, 0 = central frame, +1 = last frame.
	const auto n = static_cast<uint32_t>(it->second.rotations.size());
	const float ff = std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f) * static_cast<float>(n - 1);
	const auto i0 = static_cast<uint32_t>(ff);
	const auto i1 = std::min(n - 1, i0 + 1);
	return SampleFrames(it->second, i0, i1, ff - static_cast<float>(i0));
}

Pose HandAnimator::ApplyMotion(const Pose& base, std::string_view clipName) const noexcept
{
	if (clipName.empty() || clipName.front() != 'C')
	{
		return base;
	}
	const auto stem = std::string(clipName.substr(1));
	auto pose = base;
	for (const auto& [suffix, value] : {std::pair {"_lr", _motionLeftRight}, std::pair {"_fb", _motionForwardBack}})
	{
		const auto layerName = "L" + stem + suffix;
		const auto layer = SampleDirectional(layerName, value);
		const auto neutral = SampleDirectional(layerName, 0.0f);
		if (layer && neutral)
		{
			pose = ApplyDirectionalLayer(pose, *layer, *neutral, _layerTranslationScale);
		}
	}
	return pose;
}

void HandAnimator::Update(std::chrono::microseconds dt) noexcept
{
	const auto it = _clips.find(_clipName);
	if (it == _clips.end())
	{
		return;
	}
	const float dtMs = static_cast<float>(dt.count()) / 1000.0f;
	_timeMs += dtMs;
	Pose target;
	if (_specialHold)
	{
		// 0% = Cphile, 100% = Chorn. special_hold has no L layers: reuse hold_fingers so the hand still reacts.
		const auto base = BlendPoses(Sample(_clips.at("Cphile"), 0.0f), Sample(_clips.at("Chorn"), 0.0f), *_specialHold);
		target = ApplyMotion(base, "Chold_fingers");
	}
	else if (_fixedTime)
	{
		target = ApplyMotion(Sample(it->second, *_fixedTime), _clipName);
	}
	else if (_frame && !it->second.rotations.empty())
	{
		const auto n = static_cast<uint32_t>(it->second.rotations.size());
		const float f = std::clamp(*_frame, 0.0f, static_cast<float>(n - 1));
		const auto i0 = static_cast<uint32_t>(f);
		const auto i1 = std::min(n - 1, i0 + 1);
		target = ApplyMotion(SampleFrames(it->second, i0, i1, f - static_cast<float>(i0)), _clipName);
	}
	else
	{
		target = ApplyMotion(Sample(it->second, _timeMs), _clipName);
	}
	if (!_fromPose.empty() && _blendDurationMs > 0.0f && _blendMs < _blendDurationMs)
	{
		_blendMs += dtMs;
		_pose = BlendPoses(_fromPose, target, Ease(_blendMs / _blendDurationMs));
	}
	else
	{
		_pose = target;
		_fromPose.clear();
	}
	if (_rootLocked)
	{
		for (size_t i = 0; i < _pose.size(); ++i)
		{
			if (_parents[i] == k_NoParent)
			{
				_pose[i] = _bindLocal[i];
			}
		}
	}
	ComputeBoneMatrices();
}

std::vector<glm::mat4> HandAnimator::ToGlobals(const Pose& pose) const noexcept
{
	std::vector<glm::mat4> out(pose.size());
	for (size_t i = 0; i < pose.size(); ++i)
	{
		// Parents always precede their children in Lionhead skeletons.
		const auto local = ToColumnMatrix(pose[i]);
		const auto p = _parents[i];
		out[i] = p == k_NoParent ? local : out[p] * local;
	}
	return out;
}

void HandAnimator::StartStateBlend() noexcept
{
	_stateBlendSeconds = 0.0f;
}

void HandAnimator::AdvanceStateBlend(float dtSeconds) noexcept
{
	// (approximate) the original lerps its world-space transformed matrices (the first one the hand's matrix); here the
	// model-space bone matrices, the hand's own matrix being the Transform
	_stateBlendT.reset();
	constexpr float k_Duration = 0.13f;
	if (_stateBlendSeconds >= k_Duration)
	{
		return;
	}
	_stateBlendSeconds += dtSeconds;
	if (_stateBlendSeconds >= k_Duration)
	{
		return; // the frame the blend ends: nothing (as the original)
	}
	_stateBlendT = _stateBlendSeconds / k_Duration;
}

void HandAnimator::ComputeBoneMatrices() noexcept
{
	_boneMatrices = ToGlobals(_pose);
}

std::vector<glm::mat4> HandAnimator::Evaluate(std::string_view name, float timeMs, float leftRight, float forwardBack,
                                              float layerTranslationScale) const noexcept
{
	const auto it = _clips.find(std::string(name));
	if (it == _clips.end())
	{
		return {};
	}
	auto copy = *this;
	copy.SetMotion(leftRight, forwardBack);
	copy.SetLayerTranslationScale(layerTranslationScale);
	return ToGlobals(copy.ApplyMotion(copy.Sample(it->second, timeMs), name));
}
