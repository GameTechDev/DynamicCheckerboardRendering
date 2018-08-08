#pragma once

#include "SceneAnimation.h"

#include "Camera.h"
#include "EngineTuning.h"

#include <map>

namespace ART {

	// in its limited form the animation controller targets only a camera
	// Later on we will extend it to custom animation targets
	class AnimationController {
	public:

		AnimationController();

		void SetSceneAnimation(SceneAnimation_ptr& animation);
		SceneAnimation_ptr& GetSceneAnimation();

		void SetTargetCamera(Math::BaseCamera* targetCamera);
		Math::BaseCamera* GetTargetCamera();

		// creates a link between an engine var and a named float animation track
		// if the track does not exist yet, it creates a new one
		void AnimateFloatVar(NumVar* engineVar, const char* trackName);
		void AnimateBoolVar(BoolVar* engineVar, const char* trackName);

		// replay controls
		// TO DO: support off-line animation with fixed frame rate
		bool IsPlaying() const;

		void TogglePlaying();

		bool IsDirty() const {
			return _isDirty;
		}
		void SetDirty(bool dirty) {
			_isDirty = dirty;
		}

		void Play();
		
		// stops playing
		void Pause();
		// stops playing and resets the time
		void Stop();

		// If the animation is playing, updates the state of the animation in real-time
		// At the end of the animation time frame, it loops the animation sequence from the beginning
		// TO DO: make this behavior more flexible
		void Update(float elapsedTime);

		// keyframe manipulation: the controller establishes the connection between a scene object or parameter
		// and the animation data. Therefore only this class is able to capture new keyframes
		// NOTE: later these methods will take effect on the active animation track. Right now we only have a single track for the camera
		
		void JumpPrevKeyframe();

		void JumpNextKeyframe();
		
		void DeleteLastKeyframe();

		void InsertKeyframe();

		// GUI interop
		size_t GetNumAnimationTargets() const;
		void GetAnimationTargetName(size_t index, std::string& name) const;
		
		// The active animation target is the one we capture keyframes from. 
		// We assume that the GUI will also display the keyframes of this target
		void SetActiveTargetIndex(size_t index);
		size_t GetActiveTargetIndex() const;

		AnimatedValueBase* GetActiveAnimationFromScene();

	protected:

		struct FloatBinding {
			NumVar*					Var;
			AnimatedValue<float>*	Animation;
		};

		struct BoolBinding {
			BoolVar*				Var;
			AnimatedValue<bool>*	Animation;
		};

		enum ETargetType {
			ETargetType_Camera = 0,
			ETargetType_Float,
			ETargetType_Bool,
			ETargetType_Count
		};

		struct BindingDesc {
			std::string				Name;
			ETargetType				Type;
		};

		void applyAnimation();

		bool										_isPlaying;
		bool										_isDirty;
		
		SceneAnimation_ptr							_sceneAnimation;

		Math::BaseCamera*							_targetCamera;

		std::vector<BindingDesc>					_animBindings;
		size_t										_activeBindingIdx;
		AnimatedValueBase*							_activeAnimation;

		std::map<std::string, FloatBinding>			_floatAnimBindings;
		std::map<std::string, BoolBinding>			_boolAnimBindings;

	};

	DECLARE_PTR_TYPES(AnimationController);

}
