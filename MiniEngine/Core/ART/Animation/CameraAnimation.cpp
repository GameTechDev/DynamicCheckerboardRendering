
#include "pch.h"

#include "CameraAnimation.h"

using namespace Math;

///////////////////////////////////////////////////////////////////////////////
// CameraFrame

namespace ART {

	CameraFrame operator + (const CameraFrame& a, const CameraFrame& b) {
		CameraFrame res(a);
		res += b;
		return res;
	}

	CameraFrame operator * (const CameraFrame& a, float f) {
		CameraFrame res(a);
		res *= f;
		return res;
	}

	CameraFrame operator * (float f, const CameraFrame& a) {
		CameraFrame res(a);
		res *= f;
		return res;
	}

}

