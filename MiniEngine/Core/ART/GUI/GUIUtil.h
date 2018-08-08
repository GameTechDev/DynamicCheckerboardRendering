#pragma once

#pragma warning(disable: 4996)

#include <sstream>

namespace ART {

	struct GUIUtil {
		// helper function
		static void CopyFloatToCharArray(float f, char* buffer) {
			std::stringstream strm;
			strm << f;

			strm.str().copy(buffer, strm.str().length());
			buffer[strm.str().length()] = '\0';
		}
	};

}

#pragma warning(default: 4996)

