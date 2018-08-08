#pragma once

#include "..\CommonDefs.h"
#include "..\Animation\AnimationController.h"

#include "IGUIWidget.h"

#ifdef ART_ENABLE_GUI

namespace ARTGUI {


	class AnimationWidget;
	DECLARE_PTR_TYPES(AnimationWidget);


	class AnimationWidget : public IGUIWidget {
	public:

		AnimationWidget(const ART::AnimationController_ptr& controller);

		virtual ~AnimationWidget() {}

		virtual void Init() override;
		
		virtual void Update(float elapsedTime) override;
		
		virtual void Render() override;

		static AnimationWidget_ptr Create(const ART::AnimationController_ptr& controller) {
			return std::make_shared<AnimationWidget>(controller);
		}

	protected:

		void showHelpOverlay();

		ART::AnimationController_ptr			_controller;

		bool											_showHelp;
		float											_timeElapsed; // just to time the help screen

		float											_lastAnimStart;
		float											_lastAnimEnd;

		std::vector<std::string>						_trackNames;
		std::vector<const char*>						_trackNameChrs;

	};

}

#endif
