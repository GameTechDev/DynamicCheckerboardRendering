
#include "pch.h"

#include "SequencerWidget.h"

#include "..\CommonDefs.h"

#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"

#include "..\..\GameInput.h"

#include "GUICore.h"
#include "GUIUtil.h"

using namespace ART;

#pragma warning(disable: 4996)


// NOTE: while this widget is implemented as a class, due to the stateless immediate manner of ImGUI
// we currently store all states in a static variable
// This means that only one instance of thie widget can work properly at once

#ifdef ART_ENABLE_GUI

namespace ARTGUI {

	SequencerWidget::SequencerWidget(ART::FrameSequencer* sequencer, const ART::AnimationController_ptr& controller):
		_sequencer(sequencer),
		_controller(controller)
	{
		_nextSequence.Name = "MySequence";
		//_nextSequence.FPS = 24.0f;
		//_nextSequence.CaptureFPS = 24.0f;
		_nextSequence.FPS = 60.0f;
		_nextSequence.CaptureFPS = 60.0f;
	}

	void SequencerWidget::Init() {

	}

	void SequencerWidget::Update(float elapsedTime) {
		if (_sequencer->IsCapturing() && _sequencer->GetCurrFrameID() == 0)
			_controller->Play();
	}

	// helper method
	void updateFloatFromCharArray(float& f, char* txt) {
		float tmp = std::atof(txt);
		if (f > 0.0f) {
			f = tmp;
		}
		else
			GUIUtil::CopyFloatToCharArray(f, txt);
	}

	void SequencerWidget::Render() {
		static bool isOpen = true;

		auto& io = ImGui::GetIO();

		float width = 350;
		float height = 150;

		ImGui::SetNextWindowSize(ImVec2(width, height));
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - width - 100, 100));

		ImGui::Begin("Sequencer", &isOpen);

		ImGui::Text("Sequence Name: ");
		ImGui::SameLine();

		ImGui::PushItemWidth(200);

		static char txtSeqName[24] = "MySequence";
		if (ImGui::InputText("##txtSeqName", txtSeqName, 24, ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string name(txtSeqName);
			if (name.size() > 0)
				_nextSequence.Name = name;
			else
				_nextSequence.Name.copy(txtSeqName, _nextSequence.Name.size());
		}
		ImGui::PopItemWidth();

		ImGui::Text("Render Frame Rate: ");
		ImGui::SameLine();

		ImGui::PushItemWidth(200);

		//static char txtSequenceFPS[8] = "24.0";
		//static char txtCaptureFPS[8] = "24.0";
		static char txtSequenceFPS[8] = "60.0";
		static char txtCaptureFPS[8] = "1.0";
		if (ImGui::InputText("##txtSeqFPS", txtSequenceFPS, 8, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
			float f = std::atof(txtSequenceFPS);
			if (f > 0.0f) {
				_nextSequence.FPS = f;
				if (f < _nextSequence.CaptureFPS) {
					_nextSequence.CaptureFPS = f;
					GUIUtil::CopyFloatToCharArray(f, txtCaptureFPS);
				}
			}
			else
				GUIUtil::CopyFloatToCharArray(_nextSequence.FPS, txtSequenceFPS);
		}
		ImGui::PopItemWidth();

		ImGui::Text("Capture Frame Rate: ");
		ImGui::SameLine();

		ImGui::PushItemWidth(200);

		if (ImGui::InputText("##txtCapFPS", txtCaptureFPS, 8, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
			float f = std::atof(txtCaptureFPS);
			if (f > 0.0f) {
				_nextSequence.CaptureFPS = f;
				if (f > _nextSequence.FPS) {
					_nextSequence.FPS = f;
					GUIUtil::CopyFloatToCharArray(f, txtSequenceFPS);
				}
			}
			else
				GUIUtil::CopyFloatToCharArray(_nextSequence.CaptureFPS, txtCaptureFPS);
		}
		ImGui::PopItemWidth();


		ImGui::PushItemWidth(300);
		if (_sequencer->IsCapturing()) {
			if (ImGui::Button("Stop")) {
				endSequence();
			}
		}
		else {
			if (ImGui::Button("Render")) {

				std::string name(txtSeqName);
				if (name.size() > 0) _nextSequence.Name = name;
				updateFloatFromCharArray(_nextSequence.FPS, txtSequenceFPS);
				updateFloatFromCharArray(_nextSequence.CaptureFPS, txtCaptureFPS);
				if (_nextSequence.CaptureFPS > _nextSequence.FPS)
					_nextSequence.CaptureFPS = _nextSequence.FPS;

				beginSequence();
			}
		}

		ImGui::PopItemWidth();

		ImGui::Separator();

		if (_sequencer->IsCapturing()) {

			int frameCnt = _sequencer->GetTotalFrameCnt();
			int currFrameIdx = _sequencer->GetCurrFrameID();

			float progress = currFrameIdx / (float)frameCnt;

			std::stringstream progressStrm;
			progressStrm << "[" << currFrameIdx << "/" << frameCnt << "]";
			ImGui::ProgressBar(progress, ImVec2(-1, 0), progressStrm.str().c_str());
		}

		ImGui::End();

	}

	void SequencerWidget::beginSequence() {

		auto sceneAnimation = _controller->GetSceneAnimation();
		sceneAnimation->GetTimeSpan(_nextSequence.StartTime, _nextSequence.EndTime);

		_controller->Stop();
		_sequencer->BeginCapture(_nextSequence);

	}

	void SequencerWidget::endSequence() {
		_controller->Stop();
		_sequencer->EndCapture();
	}

}

#endif
