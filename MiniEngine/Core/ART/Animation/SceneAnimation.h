#pragma once

#include "CameraAnimation.h"
#include "../CommonDefs.h"

#include <string>
#include <vector>
#include <map>

#include "document.h"

namespace ART {

	class SceneAnimation {
	public:

		enum EVarType {
			EVarType_Float = 0,
			EVarType_Float3,
			EVarType_Bool,
			EVarType_Undefined,
			EVarType_Count
		};

		struct AnimatedVarDesc {
			EVarType		Type;
			uint32_t		TrackID;

			AnimatedVarDesc() :
				Type(EVarType_Undefined),
				TrackID(uint32_t(-1)) {}
		};

		SceneAnimation();
		SceneAnimation(float startTime, float endTime);

		bool LoadJson(const char *filename);
		bool SaveJson(const char *filename);

		void SetTimeSpan(float startTime, float endTime);
		void GetTimeSpan(float& startTime, float& endTime);

		// sets the animation's global time and evaluates the animation tracks at this time
		void SetTime(float t);
		float GetTime() const;

		AnimatedValue<CameraFrame>& GetCameraAnimation() {
			return _camAnimation;
		}

		PointAnimatedValue<std::string>& GetSubtitleAnimation() {
			return _subtitleAnimation;
		}

		// returns null if does not exist
		AnimatedValue<float>* GetFloatAnimation(const char* name);
		AnimatedValue<Math::Vector3>* GetFloat3Animation(const char* name);
		AnimatedValue<bool>* GetBoolAnimation(const char* name);

		// creates a new animation track with the given name, but returns false if the name is already used
		bool CreateAnimation(const char* name, EVarType type);

	protected:

		void parseVarsJson(rapidjson::Value& rootVal);

		inline void throwUnexpectedKeyException(const char* key);

		float							_startTime;
		float							_endTime;
		float							_currTime;

		// currently we only animate a camera, but later this class will be extended to support
		// arbitrary animation tracks
		AnimatedValue<CameraFrame>		_camAnimation;
		PointAnimatedValue<std::string>	_subtitleAnimation;

		// additional, arbitrary variables can be animated using typed animated values
		// These will be identified from the engine side using a key-index lookup table
		std::map<std::string, AnimatedVarDesc>	_animVarLUT;

		std::vector<AnimatedValue<float>>			_floatVars;
		std::vector<AnimatedValue<Math::Vector3>>	_float3Vars;
		std::vector<AnimatedValue<bool>>			_boolVars;

	};

	DECLARE_PTR_TYPES(SceneAnimation);

}
