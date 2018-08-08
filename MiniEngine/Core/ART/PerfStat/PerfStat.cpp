#include "pch.h"

#include "PerfStat.h"

#include <fstream>
#include <iostream>

using namespace ART;
using namespace std;


PerfCounterReport::PerfCounterReport(const char* reportFileName) :
	_isCapturing(false), _isInsideFrame(false), _numCounters(0)
{
	SetReportFileName(reportFileName);
}

void PerfCounterReport::SetReportFileName(const char* path) {
	_reportFileName = path;
}

void PerfCounterReport::BeginCapture() {
	ART_ASSERT(!_isCapturing && _reportFileName.size() > 0);
	_isCapturing = true;
	_isInsideFrame = false;

	_counterSlots.clear();
	_frameCounterValues.clear();
}

void PerfCounterReport::BeginFrame() {
	ART_ASSERT(_isCapturing && !_isInsideFrame);
	_isInsideFrame = true;

	// add a new record to the counter values
	std::vector<float> frameStats(_numCounters, 0);
	_frameCounterValues.push_back(frameStats);
}

void PerfCounterReport::StoreCounterValue(const char* name, float value) {
	ART_ASSERT(_isInsideFrame);

	auto& currFrameStats = _frameCounterValues[_frameCounterValues.size() - 1];

	// counter exists?
	uint32_t ctrSlot = uint32_t(-1);
	if (_counterSlots.find(name) != _counterSlots.end()) {
		ctrSlot = _counterSlots[name];
	}
	else {
		// add a new counter slot
		_counterSlots[name] = _numCounters;
		ctrSlot = _numCounters;
		
		_numCounters++;
		currFrameStats.resize(_numCounters);
	}

	ART_ASSERT(ctrSlot < currFrameStats.size());
	currFrameStats[ctrSlot] = value;
}

void PerfCounterReport::EndFrame() {
	ART_ASSERT(_isInsideFrame);
	_isInsideFrame = false;
}

void PerfCounterReport::EndCaptureAndSaveReport() {
	ART_ASSERT(_isCapturing && !_isInsideFrame);
	_isCapturing = false;

	ofstream reportFile(_reportFileName.c_str());
	if (!reportFile) {
		cout << "Unable to write perf report file: " << _reportFileName << endl;
		return;
	}

	// right now we just simply dump each counter value in a row, where each column stands for a frame
	// later on we should structure the order of these timers to represent the rendering timeline
	for (auto& ctr : _counterSlots) {
		
		// the first column is the counter name
		reportFile << ctr.first << ", ";

		uint32_t ctrSlot = ctr.second;
		for (auto& frame : _frameCounterValues) {
			if (ctrSlot < frame.size())
				reportFile << frame[ctrSlot] << ", ";
			else
				reportFile << -1 << ",";
		}
		reportFile << endl;
	}

	reportFile.close();

}


