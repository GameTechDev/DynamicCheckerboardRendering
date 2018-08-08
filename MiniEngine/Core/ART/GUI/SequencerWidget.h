#pragma once

#include "..\CommonDefs.h"
#include "..\Sequencer\FrameSequencer.h"
#include "..\Animation\AnimationController.h"

#include "IGUIWidget.h"

#ifdef ART_ENABLE_GUI

namespace ARTGUI {

	class SequencerWidget;
	DECLARE_PTR_TYPES(SequencerWidget);

	class SequencerWidget : public IGUIWidget {
	public:

		SequencerWidget(ART::FrameSequencer* sequencer, const ART::AnimationController_ptr& controller);

		virtual ~SequencerWidget() {}

		virtual void Init() override;

		virtual void Update(float elapsedTime) override;

		virtual void Render() override;

		static SequencerWidget_ptr Create(ART::FrameSequencer* sequencer, const ART::AnimationController_ptr& controller) {
			return std::make_shared<SequencerWidget>(sequencer, controller);
		}

	protected:

		void beginSequence();
		void endSequence();

		ART::FrameSequencer*				_sequencer;
		ART::AnimationController_ptr		_controller;

		ART::FrameSequence					_nextSequence;

	};

}

#endif // ART_ENABLE_GUI
