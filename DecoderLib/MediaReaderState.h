#pragma once
#if defined(_WIN32) || defined(_WIN64)
	#ifdef MEDIACONVERTER_EXPORTS
	#define MEDIACONVERTER_API __declspec(dllexport)
	#else
	#define MEDIACONVERTER_API __declspec(dllimport)
	#endif
#else
	#define MEDIACONVERTER_API 
#endif
#include <algorithm>

//ffmpeg includes
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/md5.h>
#include <libavutil/audio_fifo.h>
}

enum class MEDIACONVERTER_API ErrorCode : int
{
	FFMPEG_ERROR = -3,
	AGAIN = -2,
	FILE_EOF = -1,
	SUCCESS,
	NO_FMT_CTX,
	FMT_UNOPENED,
	NO_CODEC,
	CODEC_UNOPENED,
	NO_STREAMS,
	NO_VID_STREAM,
	NO_AUDIO_STREAM,
	NO_CODEC_CTX,
	CODEC_CTX_UNINIT,
	NO_SWR_CTX,
	NO_SWR_CONVERT,
	NO_FRAME,
	NO_PACKET,
	PKT_NOT_DECODED,
	PKT_NOT_RECEIVED,
	NO_SCALER,
	SEEK_FAILED,
	NO_DATA_AVAIL,
	REPEATING_FRAME,
	NO_AUDIO_DEVICES,
	NO_OUTPUT_FILE,
	NO_FILTER_GRAPH,
	NO_FILTER,
	NO_BUFFER_INSTANCE,
	FILTER_NOT_INIT,
	NO_VOLUME_FILTER,
	NO_VOLUME_INSTANCE,
	VOLUME_FILTER_NOT_INIT,
	NO_AFORMAT_FILTER,
	NO_AFORMAT_INSTANCE,
	AFORMAT_FILTER_NOT_INIT,
	NO_BUFFERSINK_FILTER,
	NO_BUFFERSINK_INSTANCE,
	BUFFERSINK_FILTER_NOT_INIT,
	FILTERS_NOT_CONNECTED,
	FILTER_GRAPH_NOT_CONFIG,
	BAD_WRITE,
	BAD_SCALE,
	ENCODER_UNINIT,
	NO_TRAILER
};

struct MEDIACONVERTER_API VideoFrameData
{
public:
	VideoFrameData();
	VideoFrameData(const VideoFrameData& other);
	~VideoFrameData();
	VideoFrameData& operator=(const VideoFrameData& other);
	bool operator==(const VideoFrameData& other);

	bool FillDataFromFrame(AVFrame* frame);
	bool FillDataFromPacket(AVPacket* packet);
	int FrameNumber() const { return frame_number; }
	int64_t PktPts() const { return pkt_pts; }
	int64_t FramePts() const { return frame_pts; }
	int64_t PktDts() const { return pkt_dts; }
	int64_t FramePktDts() const { return frame_pkt_dts; }
	int KeyFrame() const { return key_frame; }
	size_t PktSize() const { return pkt_size; }

	AVFrame* cloneFrame = nullptr;
private:
	int frame_number = -1;
	int64_t pkt_pts = -1;
	int64_t frame_pts = -1;
	int64_t pkt_dts = -1;
	int64_t frame_pkt_dts = -1; //frame copies pkt_dts when it grabs frame data
	int key_frame = -1;
	size_t pkt_size = -1;
};

struct MEDIACONVERTER_API AudioFrameData
{
	AudioFrameData();
	AudioFrameData(const AudioFrameData& other);
	~AudioFrameData();
	AudioFrameData& operator=(const AudioFrameData& other);
	bool operator==(const AudioFrameData& other);

	bool FillDataFromFrame(AVFrame* frame);
	bool UpdateBitRate(AVCodecContext* ctx);
	int BufferSize(AVSampleFormat fmt) const;

	int Channels() const { return num_channels; }
	int SampleRate() const { return sample_rate; }
	int LineSize() const { return line_size; }
	int NumSamples() const { return num_samples; }
	int64_t AudioPts() const { return audio_pts; }
	int64_t AudioDts() const { return audio_dts; }
	int64_t BestEffortTs() const { return best_effort_ts; }
	int64_t BitRate() const { return bit_rate; }
	double VolumeOffset() const { return volume_offset; }
	void SetVolumeOffset(double level) { volume_offset = (std::min)(24.0, (std::max)(level, -24.0)); } //volume offset capped +/- 24.0 dB
	double FaderLevel() const {return fader_level; }
	void SetFaderLevel(double level) { fader_level = (std::min)(1.0, (std::max)(level, 0.0)); }
	void SetSampleRate(int rate) { sample_rate = (std::max)(rate, 0); }
	void SetNumSamples(int num) { num_samples = (std::max)(num, 0); }
	void SetChannels(int channels) { num_channels = channels; }

private:
	int num_channels = -1;
	int sample_rate = -1;
	int line_size = -1;
	int num_samples = -1;
	int64_t audio_pts = -1;
	int64_t audio_dts = -1;
	int64_t best_effort_ts = -1;
	int64_t bit_rate = -1;
	double volume_offset = 0.0;
	double fader_level = 1.0;
};

struct MEDIACONVERTER_API EncoderDetails
{
	EncoderDetails() {}
	EncoderDetails(int br) : bit_rate(br) {}
	EncoderDetails(const EncoderDetails& other) { *this = other; }
	EncoderDetails& operator=(const EncoderDetails& other)
	{
		bit_rate = other.bit_rate;
		return *this;
	}
	int bit_rate = 0;
};

struct MEDIACONVERTER_API VideoEncoderDetails : EncoderDetails
{
	VideoEncoderDetails() {}
	VideoEncoderDetails(int w, int h, AVRational fr, AVRational tb, AVRational sar, int gs, int mbf, AVPixelFormat pf, int inW, int inH, AVPixelFormat ipf, int br);
	VideoEncoderDetails(const VideoEncoderDetails& other) { *this = other; }
	VideoEncoderDetails& operator=(const VideoEncoderDetails& other)
	{
		EncoderDetails::operator=(other);
		width = other.width;
		height = other.height;
		framerate = other.framerate;
		time_base = other.time_base;
		sample_aspect_ratio = other.sample_aspect_ratio;
		gop_size = other.gop_size;
		max_b_frames = other.max_b_frames;
		pix_fmt = other.pix_fmt;
		input_width = other.input_width;
		input_height = other.input_height;
		input_pix_fmt = other.input_pix_fmt;
		return *this;
	}
	void SetInputParams(int inW, int inH, AVPixelFormat ipw)
	{
		input_width = inW;
		input_height = inH;
		input_pix_fmt = ipw;
	}

	int width = 1920;
	int height = 1080;
	AVRational framerate = AVRational{ 30, 1 };
	AVRational time_base = av_inv_q(framerate);
	AVRational sample_aspect_ratio = AVRational{ 1, 1 };
	int gop_size = 12;
	int max_b_frames = 1;
	AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
	int input_width = 1920;
	int input_height = 1080;
	AVPixelFormat input_pix_fmt = AV_PIX_FMT_RGBA;
};

struct MEDIACONVERTER_API AudioEncoderDetails : EncoderDetails
{
	AudioEncoderDetails() {}
	AudioEncoderDetails(int c, uint64_t cl, int sr, AVSampleFormat sf, int ic, int isr, AVSampleFormat isf, int br);
	AudioEncoderDetails(const AudioEncoderDetails& other) { *this = other; }
	AudioEncoderDetails& operator=(const AudioEncoderDetails& other)
	{
		EncoderDetails::operator=(other);
		channels = other.channels;
		input_channels = other.input_channels;
		channel_layout = other.channel_layout;
		sample_rate = other.sample_rate;
		input_sample_rate = other.input_sample_rate;
		sample_fmt = other.sample_fmt;
		input_sample_fmt = other.input_sample_fmt;
		return *this;
	}

	void SetInputParams(int ic, int isr, AVSampleFormat isf)
	{
		input_channels = ic;
		input_sample_rate = isr;
		input_sample_fmt = isf;
	}

	int channels = 2;
	uint64_t channel_layout = AV_CH_LAYOUT_STEREO;
	int input_channels = 2;
	int sample_rate = 0;
	int input_sample_rate = 0;
	AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;
	AVSampleFormat input_sample_fmt = AV_SAMPLE_FMT_NONE;
};

template<typename T>
class MEDIACONVERTER_API Encoder
{
public:
	Encoder() {}
	virtual ~Encoder() {}
	ErrorCode Init(const T& details, const char* outfile)
	{
		encodeCodec = avcodec_find_encoder(GetCodecType());
		if (!encodeCodec)
			return ErrorCode::NO_CODEC;

		encodeCodecCtx = avcodec_alloc_context3(encodeCodec);
		if (!encodeCodecCtx)
			return ErrorCode::NO_CODEC_CTX;

		//format context
		avformat_alloc_output_context2(&encodeFmtCtx, nullptr, nullptr, outfile);
		if (!encodeFmtCtx)
			return ErrorCode::NO_CODEC_CTX;

		//create stream
		stream = avformat_new_stream(encodeFmtCtx, encodeCodec);
		if (!stream)
			return ErrorCode::NO_STREAMS;

		ErrorCode ret = InitCodecContext(details);
		if (ret != ErrorCode::SUCCESS)
			return ret;

		// Some container formats (like MP4) require global headers to be present.
		// Mark the encoder so that it behaves accordingly. 
		if (encodeFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
			encodeCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		if (avcodec_open2(encodeCodecCtx, encodeCodec, nullptr) < 0)
			return ErrorCode::CODEC_UNOPENED;

		if (avcodec_parameters_from_context(stream->codecpar, encodeCodecCtx) < 0)
			return ErrorCode::NO_CODEC_CTX;
		stream->avg_frame_rate = { 30, 1 };
		stream->time_base = { 1, 90000 };

		if (!(encodeFmtCtx->oformat->flags & AVFMT_NOFILE))
		{
			if (avio_open(&encodeFmtCtx->pb, outfile, AVIO_FLAG_WRITE) < 0)
				return ErrorCode::NO_OUTPUT_FILE;
		}

		ret = InitConverter();
		if (ret != ErrorCode::SUCCESS)
			return ret;

		if (avformat_write_header(encodeFmtCtx, nullptr) < 0)
			return ErrorCode::NO_DATA_AVAIL;

		nextEncodePts = 0;

		return ErrorCode::SUCCESS;
	}

	virtual ErrorCode EncodeFrame(int streamIndex, AVFrame* inputFrame)
	{
		if (!encodeCodecCtx || !encodeFmtCtx)
			return ErrorCode::ENCODER_UNINIT;

		if (!encodePkt)
			encodePkt = av_packet_alloc();
		encodePkt->data = nullptr;
		encodePkt->size = 0;
		av_init_packet(encodePkt);
		//encode frame
		int ret = avcodec_send_frame(encodeCodecCtx, inputFrame);
		if (ret < 0)
			return ErrorCode::NO_FRAME;

		while (ret >= 0)
		{
			ret = avcodec_receive_packet(encodeCodecCtx, encodePkt);

			if (ret == AVERROR(EAGAIN))
				return ErrorCode::NO_FRAME;
			if (ret == AVERROR_EOF)
				return ErrorCode::FILE_EOF;

			encodePkt->stream_index = streamIndex;
			av_packet_rescale_ts(encodePkt, encodeCodecCtx->time_base, encodeFmtCtx->streams[streamIndex]->time_base);

			av_interleaved_write_frame(encodeFmtCtx, encodePkt);
			av_packet_unref(encodePkt);
		}
		return ret == 0 ? ErrorCode::SUCCESS : ErrorCode::BAD_WRITE;
	}

	virtual ErrorCode EncodeFrame(int streamIndex, bool drainBuffer = false)
	{
		return EncodeFrame(streamIndex, drainBuffer ? nullptr : encodeFrame);
	}

	virtual void SetDetails(const T& details) { encodeDetails = details; }
	bool IsValid() const { return encodeCodecCtx && encodeFmtCtx && encodeFrame; }

	virtual AVCodecID GetCodecType() = 0;
	virtual ErrorCode InitCodecContext(const T& details) = 0;
	virtual ErrorCode InitConverter() = 0;
	virtual ErrorCode CompleteEncoding()
	{
		if (!encodeFmtCtx)
			return ErrorCode::NO_TRAILER;
		av_write_trailer(encodeFmtCtx);
		Cleanup();
		return ErrorCode::SUCCESS;
	}
	virtual void Cleanup()
	{
		if (encodePkt)
		{
			av_packet_unref(encodePkt);
			av_packet_free(&encodePkt);
		}
		if (encodeFrame)
		{
			av_frame_unref(encodeFrame);
			av_frame_free(&encodeFrame);
		}
		if (encodeCodecCtx)
			avcodec_free_context(&encodeCodecCtx);
		if (encodeFmtCtx)
		{
			avio_closep(&encodeFmtCtx->pb);
			avformat_free_context(encodeFmtCtx);
		}
	}

	const AVCodec* encodeCodec = nullptr;
	const AVCodec* audioEncodeCodec = nullptr;
	AVCodecContext* encodeCodecCtx = nullptr;
	AVCodecContext* audioEncodeCodecCtx = nullptr;
	AVFormatContext* encodeFmtCtx = nullptr;
	AVPacket* encodePkt = nullptr;
	AVFrame* encodeFrame = nullptr;
	AVStream* stream = nullptr;
	AVStream* audioStream = nullptr;
	int64_t nextEncodePts = 0;
	T encodeDetails;
};

class MEDIACONVERTER_API VideoEncoder : public Encoder<VideoEncoderDetails>
{
public:
	VideoEncoder();
	virtual ~VideoEncoder();
	virtual AVCodecID GetCodecType() override { return AV_CODEC_ID_H264; }
	virtual ErrorCode InitCodecContext(const VideoEncoderDetails& details) override;
	virtual ErrorCode InitConverter() override;
	virtual void Cleanup() override
	{
		Encoder::Cleanup();
		if (encodeScalerCtx)
			sws_freeContext(encodeScalerCtx);
	}

	SwsContext* encodeScalerCtx = nullptr;
};

class MEDIACONVERTER_API AudioEncoder : public Encoder<AudioEncoderDetails>
{
public:
	AudioEncoder();
	virtual ~AudioEncoder();
	virtual AVCodecID GetCodecType() override { return AV_CODEC_ID_AAC; }
	virtual ErrorCode InitCodecContext(const AudioEncoderDetails& details) override;
	virtual ErrorCode InitConverter() override;
	virtual void Cleanup() override
	{
		Encoder::Cleanup();
		if (encodeAudioFifo)
			av_audio_fifo_free(encodeAudioFifo);
		if (encodeResamplerCtx)
			swr_free(&encodeResamplerCtx);
	}

	SwrContext* encodeResamplerCtx = nullptr;
	AVAudioFifo* encodeAudioFifo = nullptr;

private:
	bool VerifySampleFormat(AVSampleFormat fmt) const;
};

class MEDIACONVERTER_API MultistreamEncoder
{
public:
	MultistreamEncoder();
	virtual ~MultistreamEncoder();
	ErrorCode Init(const char* outfile, AVCodecContext* videoCtx, AVCodecContext* audioCtx);
	ErrorCode EncodeFrame(int streamIndex, AVFrame* inputFrame);
	ErrorCode CompleteEncoding();
	void Cleanup();

	const AVCodec* encodeCodec = nullptr;
	const AVCodec* audioEncodeCodec = nullptr;
	AVCodecContext* encodeCodecCtx = nullptr;
	AVCodecContext* audioEncodeCodecCtx = nullptr;
	AVFormatContext* encodeFmtCtx = nullptr;
	AVPacket* encodePkt = nullptr;
	AVStream* stream = nullptr;
	AVStream* audioStream = nullptr;
	int64_t nextEncodePts = 0;
};

class MEDIACONVERTER_API MediaReaderState
{
public:
	MediaReaderState();
	MediaReaderState(const MediaReaderState& other);
	~MediaReaderState();

	bool IsEqual(const MediaReaderState& other);
	int FPS() const;
	int64_t VideoFrameInterval() const;
	int64_t AudioFrameInterval() const;
	AVCodecContext* GetCodecCtxFromPkt();
	AVCodecContext* GetCodecCtxFromPkt(AVPacket* pkt);

	bool HasVideoStream() const { return video_stream_index >= 0; }
	bool HasAudioStream() const { return audio_stream_index >= 0; }

	//AudioFrameData accessors - these change per frame
	int SampleRate() const { return audioFrameData.SampleRate(); }
	void SetSampleRate(int rate) { audioFrameData.SetSampleRate(rate); }
	int AudioBufferSize() const { return audioFrameData.BufferSize(AudioSampleFormat()); }
	int Channels() const { return audioFrameData.Channels(); }
	void SetChannels(int channels) { audioFrameData.SetChannels(channels); }
	int NumSamples() const { return audioFrameData.NumSamples(); }
	void SetNumSamples(int num) { audioFrameData.SetNumSamples(num); }
	int LineSize() const { return audioFrameData.LineSize(); }
	int BytesPerSample() const { return av_get_bytes_per_sample(AudioSampleFormat()); }
	int64_t AudioPts() const { return audioFrameData.AudioPts(); }
	int64_t AudioDts() const { return audioFrameData.AudioDts(); }
	int64_t BestEffortTs() const { return audioFrameData.BestEffortTs(); }
	int64_t BitRate() const { return audioFrameData.BitRate(); }
	double AudioTotalSeconds() const { return AudioDuration() / (double)AudioFrameInterval(); }
	double AudioFaderLevel() const { return audioFrameData.FaderLevel(); }
	void SetAudioFaderLevel(double level) { audioFrameData.SetFaderLevel(level); }
	double AudioVolumeOffset() const { return audioFrameData.VolumeOffset(); }
	void SetAudioVolumeOffset(double level) { audioFrameData.SetVolumeOffset(level); }

	//VideoFrameData accessors - these change per frame 
	int VideoFrameNumber() const { return videoFrameData.FrameNumber(); }
	int64_t PktPts() { return videoFrameData.PktPts(); }
	int64_t VideoFramePts() const { return videoFrameData.FramePts(); }
	int64_t PktDts() const { return videoFrameData.PktDts(); }
	int64_t FramePktDts() const { return videoFrameData.FramePktDts(); }
	int KeyFrame() const { return videoFrameData.KeyFrame(); }
	size_t VideoPktSize() const { return videoFrameData.PktSize(); }
	double VideoTotalSeconds() const { return VideoDuration() / (double)VideoFrameInterval(); }

	//File data accessors - these are constant and read when the file is opened
	int64_t VideoDuration() const;
	int64_t VideoStartTime() const;
	AVRational VideoAvgFrameRate() const;
	double VideoAvgFrameRateDbl() const;
	AVRational VideoTimebase() const;
	double VideoTimebaseDbl() const;
	int64_t VideoFrameCt() const;
	int VideoWidth() const;
	int VideoHeight() const;

	int64_t AudioDuration() const;
	int64_t AudioStartTime() const;
	AVRational AudioAvgFrameRate() const;
	double AudioAvgFrameRateDbl() const;
	AVRational AudioTimebase() const;
	double AudioTimebaseDbl() const;
	int AudioFrameSize() const;

	const char* CodecName();
	AVSampleFormat AudioSampleFormat() const;
	void SetAudioFrameInterval(int64_t interval);
	bool IsRationalValid(const AVRational& rational) const;

	bool IsOpened() const { return is_opened; }
	void SetIsOpened(bool opened = true) { is_opened = opened; }

	bool is_opened = false;

	AVFrame* av_frame = nullptr;
	AVPacket* av_packet = nullptr;

	AVFormatContext* av_format_ctx = nullptr;
	AVCodecContext* video_codec_ctx = nullptr;
	SwsContext* sws_scaler_ctx = nullptr;
	int video_stream_index = -1;

	VideoFrameData videoFrameData;

	//Audio details
	AVCodecContext* audio_codec_ctx = nullptr;
	SwrContext* swr_ctx = nullptr;
	int audio_stream_index = -1;

	AVFilterGraph* av_filter_graph = nullptr;

	AudioFrameData audioFrameData;
	int64_t audio_frame_interval = 0; //this is calculated manually from the buffer since it isn't known prior through ffmpeg

	//audio visualizer frame
	AVFrame* visualizer_frame = nullptr;

	//Encoder items
	VideoEncoder videoEncoder;
	AudioEncoder audioEncoder;
	MultistreamEncoder multistreamEncoder;
};