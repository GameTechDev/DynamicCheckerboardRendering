#pragma once

#include "AnimatedValue.h"
#include "SerializationUtil.h"

#include "document.h"

#include "VectorMath.h"

namespace ART {

	// Utilites to animate camera keyframes

	///////////////////////////////////////////////////////////////////////////////
	// CameraFrame
	//
	// Currently we animate the three vectors that define the camera transformation together
	// it would be more elegant to use quaternions in the future
	
	struct CameraFrame {
		Math::Vector3		Pos;
		Math::Vector3		Up;
		Math::Vector3		Forward;

		const CameraFrame& operator = (const CameraFrame& other) {
			Pos = other.Pos;
			Up = other.Up;
			Forward = other.Forward;
			return *this;
		}

		const CameraFrame& operator += (const CameraFrame& other) {
			Pos += other.Pos;
			Up += other.Up;
			Forward += other.Forward;
			return *this;
		}

		const CameraFrame& operator *= (float value) {
			Pos = value * Pos;
			Up = value * Up;
			Forward = value * Forward;
			return *this;
		}

	};


	CameraFrame operator + (const CameraFrame& a, const CameraFrame& b);

	CameraFrame operator * (const CameraFrame& a, float f);

	CameraFrame operator * (float f, const CameraFrame& a);

}

// Template specialization to support the serialization of this type in keyframes

template<>
template<class TWriter>
void JSONSerializableValue<ART::CameraFrame>::SerializeJSON(TWriter& writer) const {
	using namespace Math;
	
	JSONSerializableValue<Vector3> jsonPos(Value.Pos);
	JSONSerializableValue<Vector3> jsonUp(Value.Up);
	JSONSerializableValue<Vector3> jsonForward(Value.Forward);

	writer.StartObject();

	writer.String("pos");
	jsonPos.SerializeJSON(writer);
	writer.String("up");
	jsonUp.SerializeJSON(writer);
	writer.String("forward");
	jsonForward.SerializeJSON(writer);

	writer.EndObject();
}

template<>
void JSONSerializableValue<ART::CameraFrame>::DeserializeJSON(rapidjson::Value& jsonValue) {
	if (!jsonValue.IsObject()) {
		throw SerializationException("CameraFrame : expected JSON OBJECT");
	}

	for (auto it = jsonValue.MemberBegin(); it != jsonValue.MemberEnd(); it++) {

		JSONSerializableValue<Math::Vector3> vecVal;
		vecVal.DeserializeJSON(it->value);

		const char* name = it->name.GetString();
		if (STRMATCH(name, "pos")) {
			Value.Pos = vecVal.Value;
		}
		else if (STRMATCH(name, "up")) {
			Value.Up = vecVal.Value;
		}
		else if (STRMATCH(name, "forward")) {
			Value.Forward = vecVal.Value;
		}
	}
}

