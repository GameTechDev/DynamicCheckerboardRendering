#include "pch.h"

#include "FrameSequencer.h"

#include "PixelBuffer.h"
#include "GraphicsCore.h"
#include "EngineProfiling.h"

#include "document.h"
#include "writer.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

using namespace ART;
using namespace std::experimental;
using namespace std::chrono;

#pragma warning(disable: 4996)

void FrameSequencer::BeginCapture(const FrameSequence& sequence) {
	ART_ASSERT(!_isCapturing && sequence.EndTime > sequence.StartTime && sequence.CaptureFPS <= sequence.FPS && sequence.CaptureFPS > 0);
	_isCapturing = true;

	_currSequence = sequence;
	_frameCtr = -60; // we skip the first frames to let TAA + mblur "settle" when an animation is reset
	_captureFrameCtr = -1;
	_numFrames = (int)((sequence.EndTime - sequence.StartTime) * sequence.CaptureFPS);
	_spf = 1.0f / sequence.FPS;
	_captureSPF = 1.0f / sequence.CaptureFPS;

	_timeSinceLastCapture = 0; // we want to capture the first frame
	_currTime = sequence.StartTime;
	
	filesystem::path capturePath = _captureRootFolder / filesystem::path(_currSequence.Name);
	if (!exists(capturePath))
		filesystem::create_directory(capturePath);

	_ctrReport.SetReportFileName((capturePath / filesystem::path("perfreport.csv")).generic_string().c_str());
	_ctrReport.BeginCapture();
}

void FrameSequencer::EndCapture() {
	_isCapturing = false;
	_ctrReport.EndCaptureAndSaveReport();
}

void FrameSequencer::CaptureOne(const char*) {

	_isCapturingSingleFrame = true;
}

void FrameSequencer::FinishFrame() {
	if (_isCapturing) {
		captureIfNeeded();
		Graphics::SetOfflineTimestep(GetSecondsPerFrame());
	}
	else {
		Graphics::SetOfflineTimestep(0);

		if (_isCapturingSingleFrame) {
			_isCapturingSingleFrame = false;

			std::time_t now = system_clock::to_time_t(system_clock::now());
			char buf[255];
			std::strftime(buf, sizeof(buf), "Capture-%Y-%m-%d-%H-%M-%S", std::localtime(&now));
			
			filesystem::path capturePath = _captureRootFolder / filesystem::path("Screenshots");
			if (!exists(capturePath))
				filesystem::create_directory(capturePath);

			CaptureToFile((capturePath / filesystem::path(buf)).generic_string(), true);
		}
	}
}

bool FrameSequencer::CaptureToFile(const std::string& path, bool dumpProfilerData) {
	
	if (!_srcBuffer)
		return false;

	const std::string imgPath = path + std::string(".tga");
	const std::string profilePath = path + std::string("_profile.json");

	std::wstring& wpath = std::wstring(imgPath.begin(), imgPath.end());
	_srcBuffer->ExportToTGA(wpath);

	if(dumpProfilerData)
		writeProfilerJson(profilePath);

	return true;
}

bool FrameSequencer::captureIfNeeded() {
	if (!_isCapturing) return false;

	_ctrReport.BeginFrame();
	EngineProfiling::FillPerfDataForLastFrame(_ctrReport);
	_ctrReport.EndFrame();

	_frameCtr++;

	if (_frameCtr <= 0) return true; // skipped frames

	if (_currTime > _currSequence.EndTime || _captureFrameCtr >= _frameCtr) {
		EndCapture();
		return true;
	}

	_timeSinceLastCapture += _spf;
	_currTime += _spf;
	if (_timeSinceLastCapture < _captureSPF)
		return true;

	_timeSinceLastCapture -= _captureSPF;
	_captureFrameCtr++;

	std::string zeroPrefix;

	if (_captureFrameCtr < 9)
		zeroPrefix = "000";
	else if (_captureFrameCtr < 99)
		zeroPrefix = "00";
	else if (_captureFrameCtr < 999)
		zeroPrefix = "0";

	
	std::stringstream strm;
	strm << "frame" << zeroPrefix << _captureFrameCtr + 1;

	filesystem::path capturePath = _captureRootFolder / filesystem::path(_currSequence.Name);

	return true;//CaptureToFile((capturePath / filesystem::path(strm.str())).generic_string(), false); // in frame sequence mode we capture an entire report, not a single frame dump
}

void FrameSequencer::writeProfilerJson(const std::string& path) {
	EngineProfiling::WriteLastFrameToJson(path);
}


