// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the MEDIACONVERTER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// MEDIACONVERTER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#if defined(_WIN32) || defined(_WIN64)
	#ifdef MEDIACONVERTER_EXPORTS
	#define MEDIACONVERTER_API __declspec(dllexport)
	#else
	#define MEDIACONVERTER_API __declspec(dllimport)
	#endif
#else
	#define MEDIACONVERTER_API 
#endif

#include "MediaReaderState.h"
#include <vector>
#include <memory>
#include <string>

// This class is exported from the dll
class MEDIACONVERTER_API CMediaConverter 
{
	typedef std::vector<uint8_t> VideoBuffer;
	typedef std::vector<uint8_t> AudioBuffer;

public:
	CMediaConverter();
	~CMediaConverter();

	ErrorCode openMediaReader(const char* filename);
	ErrorCode openMediaReader(MediaReaderState* state, const char* filename);

	ErrorCode readVideoFrame(MediaReaderState* state, VideoBuffer& buffer);
	ErrorCode readVideoFrame(VideoBuffer& buffer);

	ErrorCode fullyProcessAudioFrame(MediaReaderState* state, AudioBuffer& audioBuffer);
	ErrorCode fullyProcessAudioFrame(AudioBuffer& audioBuffer);

	ErrorCode readAudioFrame(MediaReaderState* state);
	ErrorCode readAudioFrame();

	ErrorCode readFrame();
	ErrorCode readFrame(MediaReaderState* state);

	ErrorCode outputToBuffer(VideoBuffer& buffer);
	ErrorCode outputToBuffer(MediaReaderState*, VideoBuffer& buffer);

	ErrorCode outputToAudioBuffer(AudioBuffer& audioBuffer, bool freeFrame = true);
	ErrorCode outputToAudioBuffer(MediaReaderState*, AudioBuffer& audioBuffer, bool freeFrame = true);

	ErrorCode freeAVFrame();
	ErrorCode freeAVFrame(MediaReaderState* state);

	ErrorCode processVideoIntoFrames(MediaReaderState* state, bool drainBuffer = false);
	ErrorCode processAudioIntoFrames(MediaReaderState* state);

	ErrorCode processVideoPacketsIntoFrames();
	ErrorCode processVideoPacketsIntoFrames(MediaReaderState* state);

	ErrorCode processAudioPacketsIntoFrames();
	ErrorCode processAudioPacketsIntoFrames(MediaReaderState* state);

	ErrorCode trackToFrame(MediaReaderState* state, int64_t targetPts);
	ErrorCode trackToFrame(int64_t targetPts);

	ErrorCode trackToAudioFrame(MediaReaderState* state, int64_t targetPts);
	ErrorCode trackToAudioFrame(int64_t targetPts);

	ErrorCode seekToFrame(MediaReaderState* state, int64_t targetPts);
	ErrorCode seekToFrame(int64_t targetPts);

	ErrorCode seekToAudioFrame(MediaReaderState* state, int64_t targetPts);
	ErrorCode seekToAudioFrame(int64_t targetPts);

	ErrorCode seekToStart(MediaReaderState* state);
	ErrorCode seekToStart();

	ErrorCode seekToAudioStart(MediaReaderState* state);
	ErrorCode seekToAudioStart();

	ErrorCode closeMediaReader(MediaReaderState* state);
	ErrorCode closeMediaReader();

	ErrorCode initAudioFilterGraph(MediaReaderState* state, double volumeOffset, double faderLevel);
	ErrorCode initAudioFilterGraph(double volumeLevel, double faderLevel);

	ErrorCode adjustAudioVolume(MediaReaderState* state);
	ErrorCode adjustAudioVolume();

	ErrorCode mixdownAudio(MediaReaderState* state, const std::vector<AVFrame*>& frames);
	ErrorCode mixdownAudio(const std::vector<AVFrame*>& frames);

	ErrorCode readMixedAudio(MediaReaderState* state, const std::vector<AVFrame*>& frames, AudioBuffer& buffer);
	ErrorCode readMixedAudio(const std::vector<AVFrame*>& frames, AudioBuffer& buffer);

	ErrorCode initAudioMixFilterGraph(MediaReaderState* state, int numTracks);
	ErrorCode initAudioMixFilterGraph(int numTracks);

	ErrorCode mixAudioTracks(MediaReaderState* state, const std::vector<AVFrame*>& frames);
	ErrorCode mixAudioTracks(const std::vector<AVFrame*>& frames);

	ErrorCode initFormatFilterGraph(MediaReaderState* state);
	ErrorCode initFormatFilterGraph();

	ErrorCode reformatAudio(MediaReaderState* state);
	ErrorCode reformatAudio();

	ErrorCode initWaveformImageFilterGraph(MediaReaderState* state, int width, int height, std::string colors = std::string());
	ErrorCode initWaveformImageFilterGraph(int width, int height, std::string colors = std::string());

	ErrorCode generateWaveformImage(MediaReaderState* state);
	ErrorCode generateWaveformImage();

	ErrorCode getWaveformImage(MediaReaderState* state, VideoBuffer& buffer);
	ErrorCode getWaveformImage(VideoBuffer& buffer);

	ErrorCode encodeMedia(const char* inFile, const char* outFile);
	ErrorCode encodeMedia(const char* inFile, const char* outFile, MediaReaderState* state);

	ErrorCode transcodeAudio(const char* inFile, const char* outFile);
	ErrorCode transcodeAudio(const char* inFile, const char* outFile, MediaReaderState* state);

	ErrorCode initEncoders(const char* outfile);
	ErrorCode initEncoders(const char* outfile, MediaReaderState* state);

	ErrorCode initVideoEncoder(const char* outfile);
	ErrorCode initVideoEncoder(const char* outfile, MediaReaderState* state);

	ErrorCode convertVideoBuffer(std::vector<uint8_t> vec, int w, int h);
	ErrorCode convertVideoBuffer(MediaReaderState* state, std::vector<uint8_t> vec, int w, int h);

	ErrorCode encodeVideoFrame(bool drainBuffer = false);
	ErrorCode encodeVideoFrame(MediaReaderState* state, bool drainBuffer = false);

	ErrorCode initAudioEncoder(const char* outfile);
	ErrorCode initAudioEncoder(const char* outfile, MediaReaderState* state);

	ErrorCode encodeAudioFrame();
	ErrorCode encodeAudioFrame(MediaReaderState* state);

	ErrorCode fillAudioFifo();
	ErrorCode fillAudioFifo(MediaReaderState* state);

	ErrorCode drainAudioFifo(bool needsFlush);
	ErrorCode drainAudioFifo(MediaReaderState* state, bool needsFlush);

	//ErrorCode encodeAudioToneMP2();
	//ErrorCode encodeAudioToneMP2(MediaReaderState* state);

	//ErrorCode createAudioToneFrame(float hertz, std::vector<AVFrame*>& toneFrames);
	//ErrorCode createAudioToneFrame(MediaReaderState* state, float hertz, std::vector<AVFrame*>& toneFrames);

	static void avlog_cb(void*, int level, const char* szFmt, va_list varg);
	const MediaReaderState& MRState() const { return m_mrState; }
	MediaReaderState& MRState() { return m_mrState; }

private:
	bool WithinTolerance(int64_t referencePts, int64_t targetPts, int64_t tolerance);
	bool ValidateFrames(const std::vector<AVFrame*>& frames);
	std::string CreateMixString(int numTracks, double volumeOffset);
	std::string CreateStatsString();
	std::string CreateVolumeDbString(double dbOffset);
	std::string CreateVolumeString(double volume);
	bool InitAudioMixerContext();
	bool InitAudioMixerContext(MediaReaderState* state);
	MediaReaderState m_mrState;
};
