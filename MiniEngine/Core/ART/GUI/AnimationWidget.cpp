
#include "pch.h"

#include "AnimationWidget.h"

#include "..\CommonDefs.h"

#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"

#include "..\..\GameInput.h"

#include "GUICore.h"
#include "GUIUtil.h"

using namespace ART;

// NOTE: while this widget is implemented as a class, due to the stateless immediate manner of ImGUI
// we currently store all states in a static variable
// This means that only one instance of thie widget can work properly at once

#ifdef ART_ENABLE_GUI

namespace ARTGUI {

	AnimationWidget::AnimationWidget(const AnimationController_ptr& controller) {
		_controller = controller;
		_showHelp = true;
		_timeElapsed = 0;
		_lastAnimStart = 0;
		_lastAnimEnd = 0;

		_trackNames.resize(controller->GetNumAnimationTargets());
		_trackNameChrs.resize(_trackNames.size());
		for (size_t i = 0; i < _trackNames.size(); i++) {
			controller->GetAnimationTargetName(i, _trackNames[i]);
			_trackNameChrs[i] = _trackNames[i].c_str();
		}
	}

	void AnimationWidget::Init() {

	}

	void AnimationWidget::Update(float elapsedTime) {

		if(ARTGUI::IsVisible())
			_timeElapsed += elapsedTime;

		if (_timeElapsed > 3.0f)
			_showHelp = false;

		_controller->Update(0);

	}

	void AnimationWidget::Render() {

		static bool isOpen = true;
		static float time = 0;

		auto& io = ImGui::GetIO();

		float width = io.DisplaySize.x - 200;
		float height = 110;

		ImGui::SetNextWindowSize(ImVec2(width, height));
		ImGui::SetNextWindowPos(ImVec2(100, io.DisplaySize.y - height - 90));

		ImGui::Begin("Animation", &isOpen);

		ImGui::BeginChild("AnimButtons", ImVec2(200, 100), true);

		ImGui::PushItemWidth(300);
		
		static int selectedTrackIdx = 0;
		ImGui::Combo("##cmbActiveTarget", &selectedTrackIdx, &_trackNameChrs[0], (int) _trackNameChrs.size());

		if (selectedTrackIdx != _controller->GetActiveTargetIndex())
			_controller->SetActiveTargetIndex(selectedTrackIdx);

		ImGui::PopItemWidth();
		
		ImGui::PushItemWidth(100);
		
		if (_controller->IsPlaying()) {
			if (ImGui::Button("Pause"))
				_controller->Pause();
		}
		else if (ImGui::Button("Play"))
			_controller->Play();

		ImGui::SameLine();
		if (ImGui::Button("Key->>"))
			_controller->JumpNextKeyframe();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Jump to next keyframe");

		ImGui::SameLine();
		if (ImGui::Button("+ Key"))
			_controller->InsertKeyframe();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Insert keyframe at current time");

		if (ImGui::Button("Stop"))
			_controller->Stop();

		ImGui::SameLine();
		if (ImGui::Button("<<-Key"))
			_controller->JumpPrevKeyframe();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Jump to previous keyframe");

		ImGui::SameLine();
		if (ImGui::Button("- Key"))
			_controller->DeleteLastKeyframe();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Delete last keyframe");

		ImGui::PopItemWidth();

		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("AnimTimeLine", ImVec2(ImGui::GetWindowContentRegionWidth() - 200, 80), true);

		auto sceneAnimation = _controller->GetSceneAnimation();
		float start, end;
		sceneAnimation->GetTimeSpan(start, end);
		time = sceneAnimation->GetTime();

		static char txtInputStart[16] = "0.0";
		static char txtInputEnd[16] = "0.0";
		if (_lastAnimStart != start || _lastAnimEnd != end) {
			GUIUtil::CopyFloatToCharArray(start, txtInputStart);
			GUIUtil::CopyFloatToCharArray(end, txtInputEnd);
			_lastAnimStart = start;
			_lastAnimEnd = end;
		}

		const float timeLineWidth = ImGui::GetWindowContentRegionWidth();
		
		ImGui::PushItemWidth(60);
		if (ImGui::InputText("##start", txtInputStart, 16, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
			float f = std::atof(txtInputStart);
			if (f < _lastAnimEnd)
				_lastAnimStart = f;
			else
				GUIUtil::CopyFloatToCharArray(_lastAnimStart, txtInputStart);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Animation start time");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushItemWidth(timeLineWidth - 120);
		if (ImGui::SliderFloat("##sldTime", &time, start, end)) {
			// if the value changed we pause the animation and let the slider control the time
			_controller->Play();
			sceneAnimation->SetTime(time);
			_controller->Update(0);
			_controller->Pause();
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();

		ImGui::PushItemWidth(60);
		if (ImGui::InputText("##end", txtInputEnd, 16, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
			float f = std::atof(txtInputEnd);
			if (f > _lastAnimStart)
				_lastAnimEnd = f;
			else
				GUIUtil::CopyFloatToCharArray(_lastAnimEnd, txtInputEnd);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Animation end time");
		ImGui::PopItemWidth();

		ImGui::Separator();

		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			auto animation = _controller->GetActiveAnimationFromScene();
			ART_ASSERT(animation);

			// use custom drawing to render keyframes
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const ImU32 backColor = ImColor(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
			const ImU32 keyColor = ImColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			const ImU32 timeColor = ImColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			static float sz = 12.0f;

			draw_list->AddRectFilled(ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + timeLineWidth, p.y + 6.0f + sz), backColor);

			for (size_t iKeyframe = 0; iKeyframe < animation->GetKeyframeCount(); iKeyframe++) {
				
				float keyTime = animation->GetKeyframeTime(iKeyframe);

				float relOffset = (keyTime - start) / (end - start);
				
				float x = p.x + relOffset * timeLineWidth, y = p.y + 4.0f;

				//draw_list->AddCircleFilled(ImVec2(x, y + sz*0.5f), sz*0.5f, keyColor);;
				draw_list->AddRectFilled(ImVec2(x-1, y), ImVec2(x + 1, y + sz), keyColor);

			}

			// render current time
			float relOffset = (time - start) / (end - start);
			float x = p.x + relOffset * timeLineWidth, y = p.y + 4.0f;

			//draw_list->AddCircleFilled(ImVec2(x, y + sz*0.5f), sz*0.5f, keyColor);;
			draw_list->AddRectFilled(ImVec2(p.x, p.y + 0.5 * sz + 2), ImVec2(x, p.y + 0.5 * sz + 6), timeColor);
			draw_list->AddRectFilled(ImVec2(x - 1, y), ImVec2(x + 1, y + sz), timeColor);

		}

		ImGui::EndChild();

		ImGui::End();
		if (_lastAnimStart != start || _lastAnimEnd != end) {
			if (time < _lastAnimStart)
				time = _lastAnimStart;
			if (time > _lastAnimEnd)
				time = _lastAnimEnd;

			sceneAnimation->SetTimeSpan(_lastAnimStart, _lastAnimEnd);
			sceneAnimation->SetTime(time);
		}


		if(_showHelp)
			showHelpOverlay();
	}

	void AnimationWidget::showHelpOverlay() {
		ImGui::SetNextWindowPos(ImVec2(50, 50));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));

		ImGui::Begin("Animation Keyboard Shortcuts", NULL, 
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

		ImGui::Text("Animation Controls");
		ImGui::Separator();
		ImGui::Text("PAUSE       toggle playing");
		ImGui::Text("LCTRL + >   jump to next keyframe");
		ImGui::Text("LCTRL + <   jump to prev keyframe");
		ImGui::Separator();
		ImGui::Text("LCTRL + INS insert new keyframe at current time");
		ImGui::Text("LCTRL + DEL insert new keyframe at current time");
		ImGui::Separator();
		ImGui::Text("LCTRL + S	 save animation");
		ImGui::Text("LCTRL + R   reload animation");

		ImGui::End();
		ImGui::PopStyleColor();
	}

}

#endif
