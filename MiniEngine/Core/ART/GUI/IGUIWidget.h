#pragma once

#pragma once

#include "..\CommonDefs.h"

#ifdef ART_ENABLE_GUI

namespace ARTGUI {
	
	struct IGUIWidget {
		virtual ~IGUIWidget() {};

		virtual void Init() = 0;

		virtual void Update(float elapsedTime) = 0;

		// called by the GUI framework, this makes the widget to render
		// later on we can extend this by defining the parent frame, right now this is very minimalistic
		virtual void Render() = 0;
	};

	DECLARE_PTR_TYPES(IGUIWidget);

}

#endif
