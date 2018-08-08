#pragma once

#include "../CommonDefs.h"

#include <vector>
#include <map>
#include <string>

namespace ART {


	class PerfCounterReport {
	public:

		PerfCounterReport(const char* reportFileName = "");

		void SetReportFileName(const char* path);

		void BeginCapture();

		void BeginFrame();
		void StoreCounterValue(const char* name, float value);
		void EndFrame();

		void EndCaptureAndSaveReport();

	protected:
		std::map<std::string, uint32_t>		_counterSlots;

		std::vector<std::vector<float>>		_frameCounterValues;

		std::string							_reportFileName;

		bool								_isCapturing;
		bool								_isInsideFrame;

		size_t								_numCounters;
	};

}
