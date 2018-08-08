#pragma once

#include "Math/Vector.h"

#include "../CommonDefs.h"

#include "document.h"

#include <vector>

namespace ART {

	//////////////////////////////////////////////////////////////////////////
	// Keyframe interpolation modes that we support
	enum EInterpolationMode {
		INTERPOLATION_MODE_CONSTANT,
		INTERPOLATION_MODE_LINEAR,
		INTERPOLATION_MODE_CATMULL_ROM
	};
		
	//////////////////////////////////////////////////////////////////////////
	// Base interface for animated values
	//
	// An animated value contains an array of keyframes, each of which has the same type
	// There is no direct access to the keyframe vector, that way we can ensure that the vector stays sorted
	// and the time values of the keyframes are not allowed to change.
	//
	// NOTE: currently the interpolation mode is set globally for the entire animated value.
	//		 This is not very practical, it would be better to set this per keyframe.

	class AnimatedValueBase {
	public:
		AnimatedValueBase() : 
			_interpolationMode(INTERPOLATION_MODE_CATMULL_ROM), _tension(0.5f) {}
		AnimatedValueBase(EInterpolationMode interpolationMode) :
			_interpolationMode(interpolationMode), _tension(0.5f), _interpolationValid(false) {}
		
		virtual ~AnimatedValueBase() {}

		void SetInterpolationMode(EInterpolationMode mode) {
			_interpolationMode = mode;
		}
		EInterpolationMode GetInterpolationMode() const {
			return _interpolationMode;
		}

		// If Catmull-Rom interpolation is used, this parameter defines the smoothness
		// NOTE: it is not practical to set this parameter globally
		//      In the future we would control the interpolation mode per keyframe
		void SetCatRomTension(float value);
		float GetCatRomTension() const {
			return _tension;
		}

		size_t GetKeyframeCount() const {
			return _keyframeTimes.size();
		}
		virtual void ClearKeyframes() {
			_keyframeTimes.clear();
		}

		// returns the time parameter of the ith keyframe
		float GetKeyframeTime(size_t idx) const;
		void SetKeyframeTime(size_t idx, float time);

		float GetStartOffset() const;
		float GetDuration() const;

		// Evaluates the animated attribute at a specific time
		// the result of the interpolation will be stored in an internal variable
		// so it is cheap to query it multiple times if the animation is paused, etc.
		// The evaluation method uses the interpolation mode specified in the base class
		// If there are no keyframes specified, the method returns false (and an assertion fails...)
		virtual bool EvaluateAt(float time) = 0;

		// Creates a new keyframe at the given time
		// if a keyframe already exists at the same time, the operation does not create a new one
		// The method returns the index of the keyframe that can be useful for further processing, e.g. setting the value of the keyframe
		size_t InsertKeyframe(float time);
		void DeleteKeyframe(size_t idx);
		
		// There are also methods to find neighboring keyframes for a specific time
		// The return values are false if such a key does not exist
		
		// This method returns the index of the last keyframe where 'keyFrame.Time <= time'
		// If no such key exists the method returns false and the idx is invalid
		bool GetLastKeyframe(float time, size_t& idx) const;

		// This method returns the index of the first keyframe where 'keyFrame.Time > time'
		// If no such key exists the method returns false and the idx is invalid
		bool GetNextKeyframe(float time, size_t& idx) const;

	protected:

		virtual void insertKeyframeAt(size_t idx, float time);
		virtual void deleteKeyframeAt(size_t idx);

		std::vector<float>						_keyframeTimes;

		EInterpolationMode						_interpolationMode;
		float									_tension;
		Math::Vector4							_CatRomCoefficients[4];
		bool									_interpolationValid;
	};

	DECLARE_PTR_TYPES(AnimatedValueBase);


	//////////////////////////////////////////////////////////////////////////
	// Type-specific class for an animated value

	template<typename T>
	class AnimatedValue : public AnimatedValueBase{
	public:

		virtual ~AnimatedValue() {}

		virtual void ClearKeyframes() override;

		// Support for keyframe value manipulation
		void SetKeyframeValue(size_t idx, const T& value);
		void GetKeyframeValue(size_t idx, T& value) const;

		// Evaluates the animated attribute at a specific time
		// the result of the interpolation will be stored in an internal variable
		// so it is cheap to query it multiple times if the animation is paused, etc.
		// The evaluation method uses the interpolation mode specified in the base class
		// If there are no keyframes specified, the method returns false (and an assertion fails...)
		virtual bool EvaluateAt(float time);

		// returns the last stored result of the interpolation in the argument
		// returns false if the interpolatzed value is not recent (keyframes changed or there are no keyframes)
		bool GetLastInterpolatedValue(T& value) const {
			value = _lastInterpolatedValue;
			return _interpolationValid;
		}

		template<class TWriter>
		void SerializeJSON(TWriter& writer) const;

		void DeserializeJSON(rapidjson::Value& value);

	protected:

		virtual void insertKeyframeAt(size_t idx, float time) override;
		virtual void deleteKeyframeAt(size_t idx) override;

		std::vector<T>				_keyframeValues;

		T							_lastInterpolatedValue;
	};

	DECLARE_PTR_TYPES_T1(AnimatedValue);

	//////////////////////////////////////////////////////////////////////////
	// Limited animated value that cannot be interpolated
	// For example a text can change over time, but does not support arithmetic operations required by the standard animated value

	template<typename T>
	class PointAnimatedValue : public AnimatedValueBase {
	public:
		virtual ~PointAnimatedValue() {}

		virtual void ClearKeyframes() override;
	
		// Support for keyframe value manipulation
		void SetKeyframeValue(size_t idx, const T& value);
		void GetKeyframeValue(size_t idx, T& value) const;

		virtual bool EvaluateAt(float time);

		bool GetLastInterpolatedValue(T& value) const {
			value = _lastInterpolatedValue;
			return _interpolationValid;
		}

		template<class TWriter>
		void SerializeJSON(TWriter& writer) const;

		void DeserializeJSON(rapidjson::Value& value);

	protected:

		virtual void insertKeyframeAt(size_t idx, float time) override;
		virtual void deleteKeyframeAt(size_t idx) override;

		std::vector<T>				_keyframeValues;

		T							_lastInterpolatedValue;
	};

}
