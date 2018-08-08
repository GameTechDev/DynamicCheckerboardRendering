#pragma once

#include "AnimatedValue.h"
#include "SerializationUtil.h"

#include "VectorMath.h"

namespace ART {

	//////////////////////////////////////////////////////////////////////////
	// AnimatedValue

	template<typename T>
	void AnimatedValue<T>::ClearKeyframes() {
		AnimatedValueBase::ClearKeyframes();
		_keyframeValues.clear();
		_interpolationValid = false;
	}

	template<typename T>
	void AnimatedValue<T>::insertKeyframeAt(size_t idx, float time) {
		AnimatedValueBase::insertKeyframeAt(idx, time);
		_keyframeValues.insert(_keyframeValues.begin() + idx, T());
		_interpolationValid = false;
	}

	template<typename T>
	void AnimatedValue<T>::deleteKeyframeAt(size_t idx) {
		AnimatedValueBase::deleteKeyframeAt(idx);
		_keyframeValues.erase(_keyframeValues.begin() + idx);
		_interpolationValid = false;
	}

	template<typename T>
	void AnimatedValue<T>::SetKeyframeValue(size_t idx, const T& value) {
		ART_ASSERT(idx < _keyframeValues.size());
		_keyframeValues[idx] = value;
		_interpolationValid = false;
	}

	template<typename T>
	void AnimatedValue<T>::GetKeyframeValue(size_t idx, T& value) const {
		ART_ASSERT(idx < _keyframeValues.size());
		value = _keyframeValues[idx];
	}

	template<typename T>
	bool AnimatedValue<T>::EvaluateAt(float time) {
		_interpolationValid = false;
		if (_keyframeValues.empty()) return false;

		// find surrounding keyframes.
		// if no previous keyframe was found, we snap the previous keyframe to the next keyframe
		// similarly, if no next keyframe was found, we snap the value of the next keyframe to the previous keyframe

		size_t prevIdx, nextIdx;
		bool hasPrevIdx = GetLastKeyframe(time, prevIdx);
		bool hasNextIdx = GetNextKeyframe(time, nextIdx);
		ART_ASSERT(hasPrevIdx || hasNextIdx); // one of them had to be found because the keyframes are not empty

		if (!hasPrevIdx) prevIdx = nextIdx;
		else if (!hasNextIdx) nextIdx = prevIdx;

		// the interpolation weight depends on the time parameter of the surrounding keyframes
		const float tPrev = _keyframeTimes[prevIdx];
		const float tNext = _keyframeTimes[nextIdx];
		const float w = (tNext > tPrev) ? (time - tPrev) / (tNext - tPrev) : 1.0f;

		switch (_interpolationMode) {
		case INTERPOLATION_MODE_CONSTANT:
			_lastInterpolatedValue = _keyframeValues[prevIdx];
			break;
		case INTERPOLATION_MODE_LINEAR:
			_lastInterpolatedValue = _keyframeValues[nextIdx] * w + _keyframeValues[prevIdx] * (1.0f - w);
			break;
		case INTERPOLATION_MODE_CATMULL_ROM:
			{
				// for the Catmull-Rom interpolation we need two more keyframe values, extending the neighborhood
				size_t prev2Idx = (prevIdx > 0) ? prevIdx - 1 : prevIdx;
				size_t next2Idx = (nextIdx < _keyframeValues.size() - 1) ? nextIdx + 1 : nextIdx;


				// compute Catmull-Rom coefficients
				Math::Vector4 wVec(1.0f, w, w * w, w * w * w);
				float w2 = w * w;
				float w3 = w2 * w;
				float coeffs[4];
				
				coeffs[0] = Dot(wVec, _CatRomCoefficients[0]);
				coeffs[1] = Dot(wVec, _CatRomCoefficients[1]);
				coeffs[2] = Dot(wVec, _CatRomCoefficients[2]);
				coeffs[3] = Dot(wVec, _CatRomCoefficients[3]);
				
				_lastInterpolatedValue = (coeffs[0] * _keyframeValues[prev2Idx] +
					coeffs[1] * _keyframeValues[prevIdx] +
					coeffs[2] * _keyframeValues[nextIdx] +
					coeffs[3] * _keyframeValues[next2Idx]);
			}
			break;
		default: 
			ART_ASSERT(false);
			return false; // should not get here
		}
	
		_interpolationValid = true;
		return true;
	}

	template<typename T>
	template<class TWriter>
	void AnimatedValue<T>::SerializeJSON(TWriter& writer) const {

		writer.StartObject();

			writer.String("interpolation");
			writer.Uint((uint32_t)_interpolationMode);

			writer.String("tension");
			writer.Double(_tension);

			writer.String("keyframes");
			writer.StartArray();

			for (size_t iKeyframe = 0; iKeyframe < _keyframeValues.size(); iKeyframe++) {
				writer.StartObject();

					writer.String("time");
					writer.Double(_keyframeTimes[iKeyframe]);

					writer.String("value");

					JSONSerializableValue<T> serVal(_keyframeValues[iKeyframe]);
					serVal.SerializeJSON(writer);

				writer.EndObject();
			}

			writer.EndArray();

		writer.EndObject();

	}

	template<typename T>
	void AnimatedValue<T>::DeserializeJSON(rapidjson::Value& value) {

		ClearKeyframes();

		if (!value.IsObject()) {
			throw SerializationException("JSON value not recognized as object");
			return;
		}

		for (auto it = value.MemberBegin(); it != value.MemberEnd(); it++) {
			const char* name = it->name.GetString();

			if (STRMATCH(name, "interpolation")) {
				if (!it->value.IsUint())
					throw SerializationException("'interpolation' : expected UINT value");
				SetInterpolationMode((EInterpolationMode)it->value.GetUint());
			}
			else if (STRMATCH(name, "tension")) {
				if (!it->value.IsDouble())
					throw SerializationException("'tension' : expected FLOAT value");
				SetCatRomTension((float) it->value.GetDouble());
			}
			else if (STRMATCH(name, "keyframes")) {
				if (!it->value.IsArray())
					throw SerializationException("'keyframes' : expected ARRAY value");

				auto& keyframeArray = it->value;
				// Deserialize each keyframe
				for (rapidjson::SizeType i = 0; i < keyframeArray.Size(); i++) {
					if (!keyframeArray[i].IsObject())
						throw SerializationException("'keyframes' array must contain JSON OBJECTS");
					bool foundTime = false;
					bool foundValue = false;

					for (auto it = keyframeArray[i].MemberBegin(); it != keyframeArray[i].MemberEnd(); it++) {
						const char* name = it->name.GetString();

						if (STRMATCH(name, "time")) {
							foundTime = true;
							if (!it->value.IsDouble())
								throw SerializationException("'time' : expected FLOAT value");
							_keyframeTimes.push_back((float) it->value.GetDouble());
						}
						else if (STRMATCH(name, "value")) {
							foundValue = true;
							JSONSerializableValue<T> serVal;
							serVal.DeserializeJSON(it->value);
							_keyframeValues.push_back(serVal.Value);
						}
					}
					if (!foundTime || !foundValue) {
						ClearKeyframes();
						throw SerializationException("All keyframes must contain a 'time' and 'value' field");
					}
				}
			}
		}

	}		

	//////////////////////////////////////////////////////////////////////////
	// PointAnimatedValue

	template<typename T>
	void PointAnimatedValue<T>::ClearKeyframes() {
		AnimatedValueBase::ClearKeyframes();
		_keyframeValues.clear();
		_interpolationValid = false;
	}

	template<typename T>
	void PointAnimatedValue<T>::insertKeyframeAt(size_t idx, float time) {
		AnimatedValueBase::insertKeyframeAt(idx, time);
		_keyframeValues.insert(_keyframeValues.begin() + idx, T());
		_interpolationValid = false;
	}

	template<typename T>
	void PointAnimatedValue<T>::deleteKeyframeAt(size_t idx) {
		AnimatedValueBase::deleteKeyframeAt(idx);
		_keyframeValues.erase(_keyframeValues.begin() + idx);
		_interpolationValid = false;
	}

	template<typename T>
	void PointAnimatedValue<T>::SetKeyframeValue(size_t idx, const T& value) {
		ART_ASSERT(idx < _keyframeValues.size());
		_keyframeValues[idx] = value;
		_interpolationValid = false;
	}

	template<typename T>
	void PointAnimatedValue<T>::GetKeyframeValue(size_t idx, T& value) const {
		ART_ASSERT(idx < _keyframeValues.size());
		value = _keyframeValues[idx];
	}

	template<typename T>
	bool PointAnimatedValue<T>::EvaluateAt(float time) {
		_interpolationValid = false;
		if (_keyframeValues.empty()) return false;

		size_t prevIdx, nextIdx;
		bool hasPrevIdx = GetLastKeyframe(time, prevIdx);
		bool hasNextIdx = GetNextKeyframe(time, nextIdx);
		ART_ASSERT(hasPrevIdx || hasNextIdx); // one of them had to be found because the keyframes are not empty

		if (!hasPrevIdx) prevIdx = nextIdx;

		// the interpolation weight depends on the time parameter of the surrounding keyframes
		const float tPrev = _keyframeTimes[prevIdx];

		_lastInterpolatedValue = _keyframeValues[prevIdx];

		_interpolationValid = true;
		return true;
	}

	template<typename T>
	template<class TWriter>
	void PointAnimatedValue<T>::SerializeJSON(TWriter& writer) const {

		writer.StartObject();

		writer.String("keyframes");
		writer.StartArray();

		for (size_t iKeyframe = 0; iKeyframe < _keyframeValues.size(); iKeyframe++) {
			writer.StartObject();

			writer.String("time");
			writer.Double(_keyframeTimes[iKeyframe]);

			writer.String("value");

			JSONSerializableValue<T> serVal(_keyframeValues[iKeyframe]);
			serVal.SerializeJSON(writer);

			writer.EndObject();
		}

		writer.EndArray();

		writer.EndObject();

	}

	template<typename T>
	void PointAnimatedValue<T>::DeserializeJSON(rapidjson::Value& value) {

		ClearKeyframes();

		if (!value.IsObject()) {
			throw SerializationException("JSON value not recognized as object");
			return;
		}

		for (auto it = value.MemberBegin(); it != value.MemberEnd(); it++) {
			const char* name = it->name.GetString();

			if (STRMATCH(name, "keyframes")) {
				if (!it->value.IsArray())
					throw SerializationException("'keyframes' : expected ARRAY value");

				auto& keyframeArray = it->value;
				// Deserialize each keyframe
				for (rapidjson::SizeType i = 0; i < keyframeArray.Size(); i++) {
					if (!keyframeArray[i].IsObject())
						throw SerializationException("'keyframes' array must contain JSON OBJECTS");
					bool foundTime = false;
					bool foundValue = false;

					for (auto it = keyframeArray[i].MemberBegin(); it != keyframeArray[i].MemberEnd(); it++) {
						const char* name = it->name.GetString();

						if (STRMATCH(name, "time")) {
							foundTime = true;
							if (!it->value.IsDouble())
								throw SerializationException("'time' : expected FLOAT value");
							_keyframeTimes.push_back((float)it->value.GetDouble());
						}
						else if (STRMATCH(name, "value")) {
							foundValue = true;
							JSONSerializableValue<T> serVal;
							serVal.DeserializeJSON(it->value);
							_keyframeValues.push_back(serVal.Value);
						}
					}
					if (!foundTime || !foundValue) {
						ClearKeyframes();
						throw SerializationException("All keyframes must contain a 'time' and 'value' field");
					}
				}
			}
		}
	}

}


