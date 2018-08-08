#include "pch.h"


#include "SceneAnimation.h"
#include "AnimatedValue.inl"

#include "Utility.h"

#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include "filereadstream.h"
#include "error/en.h"
#include "prettywriter.h"

#include <iostream>
#include <fstream>
#include <sstream>

#define STRMATCH(x, y) (strcmp(x, y) == 0)

using namespace ART;
using namespace Math;

SceneAnimation::SceneAnimation() {
	SetTimeSpan(0, 0);
	SetTime(0);
}

SceneAnimation::SceneAnimation(float startTime, float endTime) {
	SetTimeSpan(startTime, endTime);
	SetTime(startTime);
}

bool SceneAnimation::LoadJson(const char *filename) {
	
	_animVarLUT.clear();
	_floatVars.clear();
	_float3Vars.clear();
	_boolVars.clear();
	
	std::ifstream file;
	file.open(filename, std::ios::binary);
	if (!file) {
		std::cout << "Failed to open animation file: " << filename << std::endl;
		return false;
	}
	std::stringstream ss;
	ss << file.rdbuf();
	file.close();

	rapidjson::Document doc;
	if (doc.Parse(ss.str().c_str()).HasParseError()) {
		std::cout << "Error parsing JSON: document not recognized as object." << std::endl;
		return false;
	}

	for (rapidjson::Value::MemberIterator it = doc.MemberBegin(); it != doc.MemberEnd(); it++) {

		const char* name = it->name.GetString();
		rapidjson::Value& val = it->value;

		try {

			if (STRMATCH(name, "StartTime")) {
				if (!val.IsDouble())
					throw SerializationException("'StartTime' : expected number");
				_startTime = (float) val.GetDouble();
			}
			else if (STRMATCH(name, "EndTime")) {
				if (!val.IsDouble())
					throw SerializationException("'EndTime' : expected number");
				_endTime = (float)val.GetDouble();
			}
			else if (STRMATCH(name, "CameraAnimation")) {
				_camAnimation.DeserializeJSON(val);
			}
			else if (STRMATCH(name, "Subtitles")) {
				_subtitleAnimation.DeserializeJSON(val);
			}
			else if (STRMATCH(name, "AnimatedVars")) {
				parseVarsJson(val);
			}
			else {
				throwUnexpectedKeyException(name);
			}
		}
		catch (SerializationException ex) {
			std::stringstream msg;
			msg << "Error parsing scene animation: " << ex.Message;
			Utility::Print(msg.str().c_str());
			return false;
		}

	}

	SetTime(_startTime);

	return true;
}

void SceneAnimation::parseVarsJson(rapidjson::Value& rootVal) {
	if (!rootVal.IsArray())
		throw SerializationException("AnimatedVars: expected an array");

	for (rapidjson::SizeType iVar = 0; iVar < rootVal.Size(); iVar++) {
		auto& var = rootVal[iVar];
		if (!var.IsObject()) throw SerializationException("AnimatedVars: array must contain objects.");

		std::string varName;
		AnimatedVarDesc varDesc;
		rapidjson::Value* animValue = nullptr; // we don't know its type yet, so we cannot parse it yet

		for (auto iter = var.MemberBegin(); iter != var.MemberEnd(); iter++) {
			const char* name = iter->name.GetString();
			auto& value = iter->value;
			
			if (STRMATCH(name, "Name")) {
				if (!value.IsString()) throw SerializationException("AnimatedVars.Name: expected string");
				varName = value.GetString();
			}
			else if (STRMATCH(name, "Type")) {
				if (!value.IsUint()) throw SerializationException("AnimatedVars.Type: expected uint");
				if (value.GetUint() >= EVarType_Undefined) throw SerializationException("AnimatedVars.Type: invalid code");
				varDesc.Type = (EVarType)value.GetUint();
			}
			else if (STRMATCH(name, "Animation")) {
				animValue = &value;
			}
			else {
				throwUnexpectedKeyException(name);
			}
		}

		if (varName == "" || varDesc.Type == EVarType_Undefined || animValue == nullptr)
			throw SerializationException("AnimatedVars: missing or invalid fields (Name/Type/Animation)");

		if (_animVarLUT.find(varName) != _animVarLUT.end()) {
			std::string errMsg = std::string("Duplicate animated var name ") + varName;
			throw SerializationException(errMsg.c_str());
		}

		// everything looks OK, let's parse the animated value
		switch (varDesc.Type)
		{
		case EVarType_Float:
			{
				AnimatedValue<float> animFloat;
				animFloat.DeserializeJSON(*animValue);

				varDesc.TrackID = (uint32_t) _floatVars.size();
				_floatVars.push_back(animFloat);
			}
			break;
		case EVarType_Float3:
		{
			AnimatedValue<Math::Vector3> animFloat3;
			animFloat3.DeserializeJSON(*animValue);

			varDesc.TrackID = (uint32_t) _float3Vars.size();
			_float3Vars.push_back(animFloat3);
		}
		break;
		case EVarType_Bool:
		{
			AnimatedValue<bool> animBool;
			animBool.DeserializeJSON(*animValue);

			varDesc.TrackID = (uint32_t)_boolVars.size();
			_boolVars.push_back(animBool);
		}
		break;
		default:
			throw SerializationException("Unsupported animated var type");
			break;
		}

		_animVarLUT[varName] = varDesc;
	} // for

}

void SceneAnimation::throwUnexpectedKeyException(const char* name) {
	std::string errMsg = std::string("Unexpected key: ") + name;
	throw SerializationException(errMsg.c_str());
}

bool SceneAnimation::SaveJson(const char *filename) {

	rapidjson::StringBuffer buf;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);

	writer.StartObject();
	
	writer.Key("StartTime");
	writer.Double(_startTime);
	writer.Key("EndTime");
	writer.Double(_endTime);

	writer.Key("CameraAnimation");
	_camAnimation.SerializeJSON(writer);

	writer.Key("Subtitles");
	_subtitleAnimation.SerializeJSON(writer);
	
	writer.Key("AnimatedVars");
	writer.StartArray();
	for (auto& lutEntry : _animVarLUT) {
		AnimatedVarDesc& varDesc = lutEntry.second;
		
		writer.StartObject();
			writer.Key("Name");
			writer.String(lutEntry.first.c_str());

			writer.Key("Type");
			writer.Uint((uint32_t)varDesc.Type);

			writer.Key("Animation");
			switch (varDesc.Type)
			{
			case EVarType_Float:
				_floatVars[varDesc.TrackID].SerializeJSON(writer);
				break;
			case EVarType_Float3:
				_float3Vars[varDesc.TrackID].SerializeJSON(writer);
				break;
			case EVarType_Bool:
				_boolVars[varDesc.TrackID].SerializeJSON(writer);
				break;
			default:
				ART_ASSERT(false); // should never happed
				throw SerializationException("Failed to serialize animated var: unsupported type");
				break;
			}

		writer.EndObject();
	}
	writer.EndArray();

	writer.EndObject();


	std::ofstream of(filename);
	if (!of) {
		std::cerr << "Unable to write file: " << filename << std::endl;
		return false;
	}

	of << buf.GetString();
	of.close();
	return true;
}

void SceneAnimation::SetTimeSpan(float startTime, float endTime) {
	_startTime = startTime;
	_endTime = endTime;
}

void SceneAnimation::GetTimeSpan(float& startTime, float& endTime) {
	startTime = _startTime;
	endTime = _endTime;
}

void SceneAnimation::SetTime(float t) {
	_currTime = t;

	if (_currTime > _endTime)
		_currTime = _endTime;
	else if (_currTime < _startTime)
		_currTime = _startTime;

	_camAnimation.EvaluateAt(_currTime);
	_subtitleAnimation.EvaluateAt(_currTime);

	for (auto& animVar : _floatVars)
		animVar.EvaluateAt(_currTime);
	for (auto& animVar : _float3Vars)
		animVar.EvaluateAt(_currTime);
	for (auto& animVar : _boolVars)
		animVar.EvaluateAt(_currTime);
}

float SceneAnimation::GetTime() const {
	return _currTime;
}

AnimatedValue<float>* SceneAnimation::GetFloatAnimation(const char* name) {
	auto it = _animVarLUT.find(name);
	if (it == _animVarLUT.end() || it->second.Type != EVarType_Float)
		return nullptr;

	return &_floatVars[it->second.TrackID];
}

AnimatedValue<Math::Vector3>* SceneAnimation::GetFloat3Animation(const char* name) {
	auto it = _animVarLUT.find(name);
	if (it == _animVarLUT.end() || it->second.Type != EVarType_Float3)
		return nullptr;

	return &_float3Vars[it->second.TrackID];
}

AnimatedValue<bool>* SceneAnimation::GetBoolAnimation(const char* name) {
	auto it = _animVarLUT.find(name);
	if (it == _animVarLUT.end() || it->second.Type != EVarType_Bool)
		return nullptr;

	return &_boolVars[it->second.TrackID];
}

bool SceneAnimation::CreateAnimation(const char* name, EVarType type) {
	if (_animVarLUT.find(name) != _animVarLUT.end())
		return false;

	AnimatedVarDesc varDesc;
	varDesc.Type = type;

	switch (type) {
	case EVarType_Float:
		varDesc.TrackID = (uint32_t) _floatVars.size();
		_floatVars.push_back(AnimatedValue<float>());
		break;
	case EVarType_Float3:
		varDesc.TrackID = (uint32_t) _float3Vars.size();
		_float3Vars.push_back(AnimatedValue<Math::Vector3>());
		break;
	case EVarType_Bool: {
		varDesc.TrackID = (uint32_t) _boolVars.size();
		AnimatedValue<bool> anim;
		anim.SetInterpolationMode(INTERPOLATION_MODE_CONSTANT);
		_boolVars.push_back(anim);
		}
		break;
	default:
		ART_ASSERT(false);
		return false;
	}

	_animVarLUT[name] = varDesc;

	return true;
}
