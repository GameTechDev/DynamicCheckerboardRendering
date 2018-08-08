#pragma once

#include "../CommonDefs.h"
#include "Math/Vector.h"

#include <string>

struct SerializationException {
	std::string		Message;

	SerializationException(const char* message) : Message(message) {
	}
};

// The keyframe value must support serialization
// For C++ primitve types we provide this base class that takes care of it
template<typename T>
struct JSONSerializableValue {

	// Serialization methods are template-specialized, sincw we don't know the JSON
	// layout of T. Therefore the default methods should always fail

	JSONSerializableValue() {}
	JSONSerializableValue(const T& val) : Value(val) {}

	template<class TWriter>
	void SerializeJSON(TWriter& writer) const {
		ART_ASSERT(false);
	}

	void DeserializeJSON(rapidjson::Value& jsonValue) {
		ART_ASSERT(false);
	}

	T Value;
};

// The following are template-specializations of SerializableValue for basic types that we want to JSON serialize with the wrapper class

template<>
template<class TWriter>
void JSONSerializableValue<uint32_t>::SerializeJSON(TWriter& writer) const {
	writer.Uint(Value);
}

template<>
void JSONSerializableValue<uint32_t>::DeserializeJSON(rapidjson::Value& jsonValue) {
	if (!jsonValue.IsUint())
		throw SerializationException("expected UINT value");
	Value = jsonValue.GetUint();
}


template<>
template<class TWriter>
void JSONSerializableValue<float>::SerializeJSON(TWriter& writer) const {
	writer.Double(Value);
}

template<>
void JSONSerializableValue<float>::DeserializeJSON(rapidjson::Value& jsonValue) {
	if (!jsonValue.IsDouble())
		throw SerializationException("expected FLOAT value");
	Value = (float) jsonValue.GetDouble();
}

template<>
template<class TWriter>
void JSONSerializableValue<bool>::SerializeJSON(TWriter& writer) const {
	writer.Bool(Value);
}

template<>
void JSONSerializableValue<bool>::DeserializeJSON(rapidjson::Value& jsonValue) {
	if (!jsonValue.IsBool())
		throw SerializationException("expected BOOL value");
	Value = (float)jsonValue.GetBool();
}

template<>
template<class TWriter>
void JSONSerializableValue<std::string>::SerializeJSON(TWriter& writer) const {
	writer.String(Value.c_str());
}

template<>
void JSONSerializableValue<std::string>::DeserializeJSON(rapidjson::Value& jsonValue) {
	if (!jsonValue.IsString())
		throw SerializationException("expected STRING value");
	Value = jsonValue.GetString();
}

template<>
template<class TWriter>
void JSONSerializableValue<Math::Vector3>::SerializeJSON(TWriter& writer) const {
	writer.StartArray();
		writer.Double(Value.GetX());
		writer.Double(Value.GetY());
		writer.Double(Value.GetZ());
	writer.EndArray();
}

template<>
void JSONSerializableValue<Math::Vector3>::DeserializeJSON(rapidjson::Value& jsonValue) {
	if (!jsonValue.IsArray() || jsonValue.Size() != 3)
		throw SerializationException("Vector3: expected Array of 3 floats");

	for (int i = 0; i < 3; i++) {
		if (!jsonValue[i].IsDouble())
			throw SerializationException("Vector3: all array members must be float values");
		switch (i) {
		case 0:
			Value.SetX((float) jsonValue[i].GetDouble());
			break;
		case 1:
			Value.SetY((float) jsonValue[i].GetDouble());
			break;
		case 2:
			Value.SetZ((float) jsonValue[i].GetDouble());
			break;
		}
	}
}
