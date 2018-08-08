#include "pch.h"

#include "AnimationController.h"

#include "AnimatedValue.inl"

#include "..\..\GameInput.h"

#ifdef ART_ENABLE_GUI
#include "../GUI/GUICore.h"
#endif

using namespace ART;
using namespace Math;

AnimationController::AnimationController() :
	_isPlaying(false),
	_isDirty(false),
	_targetCamera(nullptr)
{

}

void AnimationController::SetSceneAnimation(SceneAnimation_ptr& animation) {
	_sceneAnimation = animation;
	_isDirty = true;
	Stop();

	_animBindings.clear();

	BindingDesc camBinding;
	camBinding.Name = "Camera";
	camBinding.Type = AnimationController::ETargetType_Camera;
	_animBindings.push_back(camBinding);

	SetActiveTargetIndex(0);
}

SceneAnimation_ptr& AnimationController::GetSceneAnimation() {
	return _sceneAnimation;
}

void AnimationController::SetTargetCamera(BaseCamera* targetCamera) {
	_targetCamera = targetCamera;
	_isDirty = true;
}

BaseCamera* AnimationController::GetTargetCamera() {
	return _targetCamera;
}

void AnimationController::AnimateFloatVar(NumVar* engineVar, const char* trackName) {
	ART_ASSERT(_sceneAnimation);
	ART_ASSERT(engineVar && trackName != "");

	auto animationTrack = _sceneAnimation->GetFloatAnimation(trackName);
	if (!animationTrack) {
		_sceneAnimation->CreateAnimation(trackName, SceneAnimation::EVarType_Float);
		animationTrack = _sceneAnimation->GetFloatAnimation(trackName);
	}
	ART_ASSERT(animationTrack);

	FloatBinding binding;
	binding.Var = engineVar;
	binding.Animation = animationTrack;
	_floatAnimBindings[trackName] = binding;

	BindingDesc desc;
	desc.Name = trackName;
	desc.Type = AnimationController::ETargetType_Float;
	_animBindings.push_back(desc);
}

void AnimationController::AnimateBoolVar(BoolVar* engineVar, const char* trackName) {
	ART_ASSERT(_sceneAnimation);
	ART_ASSERT(engineVar && trackName != "");

	auto animationTrack = _sceneAnimation->GetBoolAnimation(trackName);
	if (!animationTrack) {
		_sceneAnimation->CreateAnimation(trackName, SceneAnimation::EVarType_Bool);
		animationTrack = _sceneAnimation->GetBoolAnimation(trackName);
	}
	ART_ASSERT(animationTrack);

	BoolBinding binding;
	binding.Var = engineVar;
	binding.Animation = animationTrack;
	_boolAnimBindings[trackName] = binding;

	BindingDesc desc;
	desc.Name = trackName;
	desc.Type = AnimationController::ETargetType_Bool;
	_animBindings.push_back(desc);
}

bool AnimationController::IsPlaying() const {
	return _isPlaying;
}

void AnimationController::TogglePlaying() {
	if (IsPlaying())
		Pause();
	else
		Play();
}

void AnimationController::Play() {
	if (!_isPlaying) _isDirty = true;
	_isPlaying = true;
}

void AnimationController::Pause() {
	if (_isPlaying) _isDirty = true;
	_isPlaying = false;
}

void AnimationController::Stop() {
	_isDirty = true;

	if (_sceneAnimation) {
		float startTime, endTime;
		_sceneAnimation->GetTimeSpan(startTime, endTime);

		_sceneAnimation->SetTime(startTime);
	}

	applyAnimation();

	_isPlaying = false;
}


void AnimationController::Update(float elapsedTime) {

	if (GameInput::IsFirstPressed(GameInput::kKey_pause))
		TogglePlaying();

	// Detect hotkeys for keyframe manipulation
	if (GameInput::IsPressed(GameInput::kKey_lcontrol)) {

		if (GameInput::IsFirstPressed(GameInput::kKey_comma)) { // This is also the "<" character
																// jump to prev keyframe
			JumpPrevKeyframe();
		}
		else if (GameInput::IsFirstPressed(GameInput::kKey_period)) { // This is also the ">" character
																	  // jump to next keyframe
			JumpNextKeyframe();
		}
		else if (GameInput::IsFirstPressed(GameInput::kKey_insert)) {
			InsertKeyframe();
		}
		else if (GameInput::IsFirstPressed(GameInput::kKey_delete)) {
			DeleteLastKeyframe();
		}

	}

	if (!_sceneAnimation) return;
	
	if (IsPlaying()) {
		float currTime = _sceneAnimation->GetTime();
		currTime += elapsedTime;

		float startTime, endTime;
		_sceneAnimation->GetTimeSpan(startTime, endTime);

		if (endTime > startTime)
			while (currTime > endTime)
				currTime = startTime + (currTime - endTime);

		_sceneAnimation->SetTime(currTime);

		applyAnimation();
	}

}

void AnimationController::JumpNextKeyframe() {
	
	size_t keyIdx;
	if (_activeAnimation->GetNextKeyframe(_sceneAnimation->GetTime(), keyIdx)) {

		float keyTime = _activeAnimation->GetKeyframeTime(keyIdx);

		Play();
		_sceneAnimation->SetTime(keyTime);
		applyAnimation();
		Pause();
	}
}

void AnimationController::JumpPrevKeyframe() {

	size_t keyIdx;
	if (_activeAnimation->GetLastKeyframe(_sceneAnimation->GetTime() - 0.0001f, keyIdx)) {

		float keyTime = _activeAnimation->GetKeyframeTime(keyIdx);

		Play();
		_sceneAnimation->SetTime(keyTime);
		applyAnimation();
		Pause();
	}
}

void AnimationController::DeleteLastKeyframe() {

	size_t keyIdx;
	if (_activeAnimation->GetLastKeyframe(_sceneAnimation->GetTime(), keyIdx)) {
		_activeAnimation->DeleteKeyframe(keyIdx);
	}

	applyAnimation();
}

void AnimationController::InsertKeyframe() {
	if (!_targetCamera) return;

	ART_ASSERT(_activeBindingIdx < _animBindings.size());
	BindingDesc& desc = _animBindings[_activeBindingIdx];
	switch (desc.Type) {
	case AnimationController::ETargetType_Camera:
	{
		auto& camAnimation = _sceneAnimation->GetCameraAnimation();

		// capture camera frame
		CameraFrame camFrame;
		camFrame.Pos = _targetCamera->GetPosition();
		camFrame.Forward = _targetCamera->GetForwardVec();
		camFrame.Up = _targetCamera->GetUpVec();

		size_t keyIdx = camAnimation.InsertKeyframe(_sceneAnimation->GetTime());
		camAnimation.SetKeyframeValue(keyIdx, camFrame);
	}
		break;
	case AnimationController::ETargetType_Float:
	{
		auto& it = _floatAnimBindings.find(desc.Name);
		ART_ASSERT(it != _floatAnimBindings.end());

		FloatBinding& binding = it->second;

		float value = *binding.Var;
		size_t keyIdx = binding.Animation->InsertKeyframe(_sceneAnimation->GetTime());
		binding.Animation->SetKeyframeValue(keyIdx, value);
	}
		break;
	case AnimationController::ETargetType_Bool:
	{
		auto& it = _boolAnimBindings.find(desc.Name);
		ART_ASSERT(it != _boolAnimBindings.end());

		BoolBinding& binding = it->second;

		bool value = *binding.Var;
		size_t keyIdx = binding.Animation->InsertKeyframe(_sceneAnimation->GetTime());
		binding.Animation->SetKeyframeValue(keyIdx, value);
	}
	break;
	default:
		ART_ASSERT(false);
		break;
	}
	

	applyAnimation();
}

void AnimationController::applyAnimation() {

	if (!_sceneAnimation)
		return;

	SetDirty(true);

	// update animation targets
	if (_targetCamera) {
		auto& camAnimation = _sceneAnimation->GetCameraAnimation();

		CameraFrame camFrame;
		if (camAnimation.GetLastInterpolatedValue(camFrame)) {

			_targetCamera->SetPosition(camFrame.Pos);
			_targetCamera->SetLookDirection(camFrame.Forward, camFrame.Up);
		}
		
		// TMP: we expect the application to do this every frame anyway
		// turns out that multiple calls to the Update() method will make the camera
		// "forget" the correct reprojection matrix.

		//_targetCamera->Update();
	}

	// animate all linked engine vars
	for (auto it = _floatAnimBindings.begin(); it != _floatAnimBindings.end(); it++) {
		FloatBinding& binding = it->second;

		float interpolatedValue;
		if (binding.Animation->GetLastInterpolatedValue(interpolatedValue))
			*binding.Var = interpolatedValue;
	}

	for (auto it = _boolAnimBindings.begin(); it != _boolAnimBindings.end(); it++) {
		BoolBinding& binding = it->second;

		bool interpolatedValue;
		if (binding.Animation->GetLastInterpolatedValue(interpolatedValue))
			*binding.Var = interpolatedValue;
	}

#ifdef ART_ENABLE_GUI
	std::string subtitle;
	_sceneAnimation->GetSubtitleAnimation().GetLastInterpolatedValue(subtitle);
	ARTGUI::SetText(subtitle);
#endif
}

size_t AnimationController::GetNumAnimationTargets() const {
	return _animBindings.size();
}

void AnimationController::GetAnimationTargetName(size_t index, std::string& name) const {
	ART_ASSERT(index < _animBindings.size());
	name = _animBindings[index].Name;
}

void AnimationController::SetActiveTargetIndex(size_t index) {
	ART_ASSERT(index < _animBindings.size());
	_activeBindingIdx = index;
	_activeAnimation = GetActiveAnimationFromScene();
}

size_t AnimationController::GetActiveTargetIndex() const {
	return _activeBindingIdx;
}

AnimatedValueBase* AnimationController::GetActiveAnimationFromScene() {
	
	ART_ASSERT(_activeBindingIdx < _animBindings.size());
	BindingDesc& desc = _animBindings[_activeBindingIdx];
	switch (desc.Type) {
	case AnimationController::ETargetType_Camera:
	{
		auto& camAnimation = _sceneAnimation->GetCameraAnimation();
		return &camAnimation;
	}
	break;
	case AnimationController::ETargetType_Float:
	{
		auto& it = _floatAnimBindings.find(desc.Name);
		ART_ASSERT(it != _floatAnimBindings.end());

		FloatBinding& binding = it->second;
		return binding.Animation;
	}
	break;
	case AnimationController::ETargetType_Bool:
	{
		auto& it = _boolAnimBindings.find(desc.Name);
		ART_ASSERT(it != _boolAnimBindings.end());

		BoolBinding& binding = it->second;
		return binding.Animation;
	}
	break;
	default:
		ART_ASSERT(false);
		break;
	}
	return nullptr;
}
