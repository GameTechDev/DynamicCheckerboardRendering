#pragma once

#include "../CommonDefs.h"

#include "../PerfStat/PerfStat.h"

#include <string>
#include <filesystem>

class PixelBuffer;

namespace ART {

	struct FrameSequence {
		std::string Name;
		float		StartTime;
		float		EndTime;
		float		FPS;
		float		CaptureFPS;		// we may want to save memory and bandwidth by dropping frames. CaptureFPS <= FPS
	};
	
	class FrameSequencer {
	public:
		FrameSequencer():
			_srcBuffer(nullptr),
			_isCapturing(false),
			_isCapturingSingleFrame(false)
		{}

		// Set the GPU buffer to capture
		void SetSrcBuffer(PixelBuffer* buffer) {
			_srcBuffer = buffer;
		}
		PixelBuffer* GetSrcBuffer() {
			return _srcBuffer;
		}

		void SetDstRootFolder(const std::string& path) {
			_captureRootFolder = path;
		}

		void GetDstRootFolder(std::string& path) const {
			path = _captureRootFolder.generic_string();
		}

		void FinishFrame(); // should be called once the status of the captured buffers is finalized

		bool CaptureToFile(const std::string& path, bool dumpProfilerData);

		void BeginCapture(const FrameSequence& sequence); // TO DO: define a custom folder for the frames

		// captures ther next frame only
		// the file name, unless otherwise specified, will be based on the current time
		void CaptureOne(const char* filename = nullptr);

		void EndCapture();

		bool IsCapturing() const {
			return _isCapturing;
		}

		bool IsCapturingSingleFrame() const {
			return _isCapturingSingleFrame;
		}
		
		// returns the timestep between frames in seconds (the inverse of FPS)
		// Only has a meaning while capturing a sequence (between BeginCapture() and EndCapture())
		float GetSecondsPerFrame() const {
			return _spf;
		}

		int GetCurrFrameID() const {
			return _captureFrameCtr;
		}

		int GetTotalFrameCnt() const {
			return _numFrames;
		}

	protected:

		// checks if the elapsed time reached the desired capture rate and performs the
		// capture if needed
		bool captureIfNeeded();

		// makes the engine profiler dump its last timings into a Json file
		void writeProfilerJson(const std::string& path);

		PixelBuffer*		_srcBuffer;

		bool				_isCapturing;
		bool				_isCapturingSingleFrame;

		FrameSequence		_currSequence;
		PerfCounterReport	_ctrReport;

		int					_frameCtr;
		int					_captureFrameCtr;
		int					_numFrames;
		float				_spf; // seconds per frame
		float				_captureSPF;

		float				_timeSinceLastCapture;
		float				_currTime;

		std::experimental::filesystem::path	_captureRootFolder;
	};

}
