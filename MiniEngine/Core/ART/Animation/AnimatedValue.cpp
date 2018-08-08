#pragma once

#include "pch.h"

#include "AnimatedValue.inl"

#include <algorithm>

namespace ART {

	//////////////////////////////////////////////////////////////////////////
	// AnimatedValueBase

	void AnimatedValueBase::SetCatRomTension(float value) {
		// We also pre-compute the C-R coefficients
		_tension = value;
		_interpolationValid = false;

		float tau = value;
		
		_CatRomCoefficients[0] = Math::Vector4(0.0f, -tau, 2.0f * tau, -tau);
		_CatRomCoefficients[1] = Math::Vector4(1.0f, 0.0f, tau - 3.0f, 2.0f - tau);
		_CatRomCoefficients[2] = Math::Vector4(0.0f, tau, 3.0f - 2.0f * tau, tau - 2.0f);
		_CatRomCoefficients[3] = Math::Vector4(0.0f, 0.0f, -tau, tau);
	}

	float AnimatedValueBase::GetKeyframeTime(size_t idx) const {
		ART_ASSERT(idx < _keyframeTimes.size());
		return _keyframeTimes[idx];
	}

	void AnimatedValueBase::SetKeyframeTime(size_t idx, float time) {
		ART_ASSERT(idx < _keyframeTimes.size());
		_keyframeTimes[idx] = time;
		_interpolationValid = false;
	}

	float AnimatedValueBase::GetStartOffset() const {
		if (_keyframeTimes.size() > 0) return _keyframeTimes[0];
		return 0.0f;
	}

	float AnimatedValueBase::GetDuration() const {
		if (_keyframeTimes.size() < 2) return 0;
		return _keyframeTimes[_keyframeTimes.size() - 1] - _keyframeTimes[0];
	}

	size_t AnimatedValueBase::InsertKeyframe(float time) {

		// add the keyframe to the animation using insertion sort
		
		// by default we insert to the beginning of the key vector,
		// but if an earlier key exists we need to insert right afterwards
		size_t insertIdx;

		bool foundPrevKeyframe = GetLastKeyframe(time, insertIdx);

		_interpolationValid = false;

		// same time?
		if (insertIdx < GetKeyframeCount() && GetKeyframeTime(insertIdx) == time) return insertIdx;

		if (!foundPrevKeyframe)
			insertIdx = 0;
		else
			insertIdx++;
		
		insertKeyframeAt(insertIdx, time);

		return insertIdx;
	}

	void AnimatedValueBase::insertKeyframeAt(size_t idx, float time) {
		_keyframeTimes.insert(_keyframeTimes.begin() + idx, time);
	}

	void AnimatedValueBase::DeleteKeyframe(size_t idx) {
		ART_ASSERT(idx < _keyframeTimes.size());

		deleteKeyframeAt(idx);

		_interpolationValid = false;
	}

	void AnimatedValueBase::deleteKeyframeAt(size_t idx) {
		_keyframeTimes.erase(_keyframeTimes.begin() + idx);
	}

	bool AnimatedValueBase::GetLastKeyframe(float time, size_t& idx) const {

		if (_keyframeTimes.size() == 0) return false;

		auto lowerIt = std::lower_bound(_keyframeTimes.begin(), _keyframeTimes.end(), time);

		if (lowerIt == _keyframeTimes.end()) {
			idx = _keyframeTimes.size() - 1;
		}
		else {
			idx = lowerIt - _keyframeTimes.begin();

			if (_keyframeTimes[idx] > time) {
				if (idx == 0) return false; // even the first keyframe in the series is later than the quieried time
				idx--;
			}
		}

		return true;
	}

	bool AnimatedValueBase::GetNextKeyframe(float time, size_t& idx) const {
		auto upperIt = std::upper_bound(_keyframeTimes.begin(), _keyframeTimes.end(), time);

		idx = upperIt - _keyframeTimes.begin();

		return (upperIt != _keyframeTimes.end());
	}

}
