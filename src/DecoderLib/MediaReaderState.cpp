#include "pch.h"
#include "framework.h"
#include "MediaReaderState.h"
#include <algorithm>
#include <cmath>

MediaReaderState::MediaReaderState()
{
}

MediaReaderState::MediaReaderState(const MediaReaderState& other) : 
av_format_ctx(other.av_format_ctx), 
video_codec_ctx(other.video_codec_ctx),
av_frame(other.av_frame),
av_packet(other.av_packet),
sws_scaler_ctx(other.sws_scaler_ctx),
video_stream_index(other.video_stream_index),
audio_codec_ctx(other.audio_codec_ctx),
audio_stream_index(other.audio_stream_index)
{
}

MediaReaderState::~MediaReaderState()
{
}

bool MediaReaderState::IsEqual(const MediaReaderState& other)
{
	return av_format_ctx == other.av_format_ctx &&
		video_codec_ctx == other.video_codec_ctx &&
		av_frame == other.av_frame &&
		av_packet == other.av_packet &&
		sws_scaler_ctx == other.sws_scaler_ctx &&
		video_stream_index == other.video_stream_index &&
		audio_codec_ctx == other.audio_codec_ctx &&
		audio_stream_index == other.audio_stream_index;
}

int MediaReaderState::FPS() const
{
	if (!IsRationalValid(VideoAvgFrameRate()))
		return 0;

	//using round here because i have seen avg_frame_rates report strange values that end up @ 30.03 fps
	return (int)std::round(VideoAvgFrameRateDbl());
}

int64_t MediaReaderState::VideoFrameInterval() const
{
	if (!IsRationalValid(VideoTimebase()))
		return 1;
	auto ret = VideoTimebase().den / (double)VideoTimebase().num / VideoAvgFrameRateDbl();
	if (ret == 0)
		return 1;
	return (int64_t)ret;
}

int64_t MediaReaderState::AudioFrameInterval() const
{
	if (!IsRationalValid(AudioTimebase()) || !IsRationalValid(AudioAvgFrameRate()))
		return (std::max)((int64_t)1, audio_frame_interval);
	auto ret = AudioTimebase().den / (double)AudioTimebase().num / AudioAvgFrameRateDbl();
	if (ret == 0)
		return audio_frame_interval;
	return (int64_t)ret;
}

AVCodecContext* MediaReaderState::GetCodecCtxFromPkt()
{
	return GetCodecCtxFromPkt(av_packet);
}

AVCodecContext* MediaReaderState::GetCodecCtxFromPkt(AVPacket* pkt)
{
	if (pkt->stream_index == video_stream_index)
		return video_codec_ctx;
	else if (pkt->stream_index == audio_stream_index)
		return audio_codec_ctx;

	return nullptr;
}

int64_t MediaReaderState::VideoDuration() const
{
	if (!HasVideoStream())
		return 0;
	return av_format_ctx->streams[video_stream_index]->duration;
}

int64_t MediaReaderState::VideoStartTime() const
{
	if (!HasVideoStream())
		return 0;
	return av_format_ctx->streams[video_stream_index]->start_time;
}

AVRational MediaReaderState::VideoAvgFrameRate() const
{
	if (!HasVideoStream())
		return av_make_q(-1, -1); //returns invalid values 

	return av_format_ctx->streams[video_stream_index]->avg_frame_rate;
}

double MediaReaderState::VideoAvgFrameRateDbl() const
{
	if (!HasVideoStream())
		return 0.0;
	return av_q2d(VideoAvgFrameRate());
}

AVRational MediaReaderState::VideoTimebase() const
{
	if (!HasVideoStream())
		return av_make_q(-1, -1); //returns invalid values 

	return av_format_ctx->streams[video_stream_index]->time_base;
}

double MediaReaderState::VideoTimebaseDbl() const
{
	if (!HasVideoStream())
		return 0.0;
	return av_q2d(VideoTimebase());
}

int64_t MediaReaderState::VideoFrameCt() const
{
	if (!HasVideoStream())
		return 0;
	return av_format_ctx->streams[video_stream_index]->nb_frames;
}

int MediaReaderState::VideoWidth() const
{
	if (!HasVideoStream())
		return 0;
	return av_format_ctx->streams[video_stream_index]->codecpar->width;
}

int MediaReaderState::VideoHeight() const
{
	if (!HasVideoStream())
		return 0;
	return av_format_ctx->streams[video_stream_index]->codecpar->height;
}

int64_t MediaReaderState::AudioDuration() const
{
	if (!HasAudioStream())
		return 0;
	return av_format_ctx->streams[audio_stream_index]->duration;
}

int64_t MediaReaderState::AudioStartTime() const
{
	if (!HasAudioStream())
		return 0;
	return av_format_ctx->streams[audio_stream_index]->start_time;
}

AVRational MediaReaderState::AudioAvgFrameRate() const
{
	if (!HasAudioStream())
		return av_make_q(-1, -1); //returns invalid values 

	return av_format_ctx->streams[audio_stream_index]->avg_frame_rate;
}

double MediaReaderState::AudioAvgFrameRateDbl() const
{
	if (!HasAudioStream())
		return 0.0;
	if (!IsRationalValid(AudioAvgFrameRate()))
		return 0.0;
	return av_q2d(AudioAvgFrameRate());
}

AVRational MediaReaderState::AudioTimebase() const
{
	if (!HasAudioStream())
		return av_make_q(-1, -1); //returns invalid values 
	return av_format_ctx->streams[audio_stream_index]->time_base;
}

double MediaReaderState::AudioTimebaseDbl() const
{
	if (!HasAudioStream())
		return 0.0;
	return av_q2d(AudioTimebase());
}

int MediaReaderState::AudioFrameSize() const
{
	if (!HasAudioStream() || !audio_codec_ctx)
		return 0;

	return audio_codec_ctx->frame_size;
}

const char* MediaReaderState::CodecName()
{
	int streamIndex = HasVideoStream() ? video_stream_index : HasAudioStream() ? audio_stream_index : -1;
	if (streamIndex == -1)
		return nullptr;

	AVCodecParameters* av_codec_params = av_format_ctx->streams[streamIndex]->codecpar;
	if (!av_codec_params)
		return nullptr;
	const AVCodec* av_codec = avcodec_find_decoder(av_codec_params->codec_id);
	if (!av_codec)
		return nullptr;

	return av_codec->long_name;
}

AVSampleFormat MediaReaderState::AudioSampleFormat() const
{
	if (!HasAudioStream() || !audio_codec_ctx)
		return AV_SAMPLE_FMT_NONE;

	return audio_codec_ctx->sample_fmt;
}

void MediaReaderState::SetAudioFrameInterval(int64_t interval)
{
	if (interval < 0)
		return;
	audio_frame_interval = interval;
}

bool MediaReaderState::IsRationalValid(const AVRational& rational) const
{
	//num can be 0 but den can't 
	//generally if one reports -1 then they both will 
	return (rational.den > 0 && rational.num >= 0);
}

VideoFrameData::VideoFrameData()
{
}

VideoFrameData::VideoFrameData(const VideoFrameData& other) 
{ 
	*this = other; 
}

VideoFrameData::~VideoFrameData()
{
}

VideoFrameData& VideoFrameData::operator=(const VideoFrameData& other)
{
	frame_number = other.frame_number;
	pkt_pts = other.pkt_pts;
	frame_pts = other.frame_pts;
	pkt_dts = other.pkt_dts;
	frame_pkt_dts = other.frame_pkt_dts;
	key_frame = other.key_frame;
	pkt_size = other.pkt_size;
	return *this;
}

bool VideoFrameData::operator==(const VideoFrameData& other)
{
	return frame_number == other.frame_number &&
		pkt_pts == other.pkt_pts &&
		frame_pts == other.frame_pts &&
		pkt_dts == other.pkt_dts &&
		frame_pkt_dts == other.frame_pkt_dts &&
		key_frame == other.key_frame &&
		pkt_size == other.pkt_size;
}

bool VideoFrameData::FillDataFromFrame(AVFrame* frame)
{
	if (!frame)
		return false;
	frame_number = frame->coded_picture_number;
	frame_pts = frame->pts;
	frame_pkt_dts = frame->pkt_dts;
	key_frame = frame->key_frame;
	pkt_size = frame->pkt_size;
	if (cloneFrame)
	{
		av_frame_unref(cloneFrame);
		av_frame_free(&cloneFrame);
	}
	cloneFrame = av_frame_alloc();
	cloneFrame = av_frame_clone(frame);
	return true;
}

bool VideoFrameData::FillDataFromPacket(AVPacket* packet)
{
	if (!packet)
		return false;
	pkt_pts = packet->pts;
	pkt_dts = packet->dts;
	return true;
}

AudioFrameData::AudioFrameData()
{
}

AudioFrameData::AudioFrameData(const AudioFrameData& other) 
{ 
	*this = other; 
}

AudioFrameData::~AudioFrameData()
{
}

AudioFrameData& AudioFrameData::operator=(const AudioFrameData& other)
{
	num_channels = other.num_channels;
	sample_rate = other.sample_rate;
	line_size = other.line_size;
	num_samples = other.num_samples;
	audio_pts = other.audio_pts;
	bit_rate = other.bit_rate;
	return *this;
}

bool AudioFrameData::operator==(const AudioFrameData& other)
{
	return num_channels == other.num_channels &&
		sample_rate == other.sample_rate &&
		line_size == other.line_size &&
		num_samples == other.num_samples &&
		audio_pts == other.audio_pts &&
		bit_rate == other.bit_rate;
}

//the data provided by the frame isn't necessarily the same for each frame
//so we selectively update values based on certain needs 
bool AudioFrameData::FillDataFromFrame(AVFrame* frame)
{
	if (!frame)
		return false;

	if (line_size < 0 && frame->linesize[0] > 0)
		line_size = frame->linesize[0];
	if (num_channels < 0 && frame->channels > 0)
		num_channels = frame->channels;
	if (sample_rate < 0 && frame->sample_rate > 0)
		sample_rate = frame->sample_rate;
	if (num_samples < frame->nb_samples)
		num_samples = frame->nb_samples;

	audio_pts = frame->pts;
	audio_dts = frame->pkt_dts;
	best_effort_ts = frame->best_effort_timestamp;
	return true;
}

bool AudioFrameData::UpdateBitRate(AVCodecContext* ctx)
{
	if (!ctx)
		return false;

	if (bit_rate < 0 || ctx->bit_rate > 0)
		bit_rate = ctx->bit_rate;

	return true;
}

int AudioFrameData::BufferSize(AVSampleFormat fmt) const
{
	auto packed = av_get_packed_sample_fmt(fmt);
	int linesize = 0;
	return av_samples_get_buffer_size(&linesize, num_channels, num_samples, packed, 1);
}

VideoEncoderDetails::VideoEncoderDetails(int w, int h, AVRational fr, AVRational tb, AVRational sar, int gs, int mbf, AVPixelFormat pf, int inW, int inH, AVPixelFormat ipf, int br) : EncoderDetails(br),
width(w), height(h), framerate(fr), time_base(tb), sample_aspect_ratio(sar), gop_size(gs), max_b_frames(mbf), pix_fmt(pf), input_width(inW), input_height(inH), input_pix_fmt(ipf)
{
}

VideoEncoder::VideoEncoder() : Encoder()
{
}

VideoEncoder::~VideoEncoder()
{
}

ErrorCode VideoEncoder::InitCodecContext(const VideoEncoderDetails& details)
{
	encodeCodecCtx->bit_rate = details.bit_rate;
	encodeCodecCtx->width = details.width;
	encodeCodecCtx->height = details.height;
	encodeCodecCtx->framerate = details.framerate;
	encodeCodecCtx->time_base = details.time_base;
	encodeCodecCtx->sample_aspect_ratio = details.sample_aspect_ratio;
	encodeCodecCtx->gop_size = details.gop_size;
	encodeCodecCtx->max_b_frames = details.max_b_frames;
	encodeCodecCtx->pix_fmt = details.pix_fmt;
	av_opt_set(encodeCodecCtx->priv_data, "preset", "slow", 0);
	av_opt_set(encodeCodecCtx->priv_data, "crf", "17", AV_OPT_SEARCH_CHILDREN);
	encodeDetails = details;
	return ErrorCode::SUCCESS;
}

ErrorCode VideoEncoder::InitConverter()
{
	// Init Scaler
	encodeScalerCtx = sws_getContext(encodeDetails.input_width, encodeDetails.input_height, encodeDetails.input_pix_fmt,
		encodeCodecCtx->width, encodeCodecCtx->height, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, 0, 0, 0);

	if (!encodeScalerCtx)
		return ErrorCode::NO_SCALER;

	return ErrorCode::SUCCESS;
}

AudioEncoderDetails::AudioEncoderDetails(int c, uint64_t cl, int sr, AVSampleFormat sf, int ic, int isr, AVSampleFormat isf, int br) : EncoderDetails(br),
channels(c), channel_layout(cl), sample_rate(sr), sample_fmt(sf), input_channels(ic), input_sample_rate(isr), input_sample_fmt(isf)
{
}

AudioEncoder::AudioEncoder() : Encoder()
{
}

AudioEncoder::~AudioEncoder()
{
}

ErrorCode AudioEncoder::InitCodecContext(const AudioEncoderDetails& details)
{
	encodeCodecCtx->channels = details.channels;
	encodeCodecCtx->channel_layout = details.channel_layout;
	encodeCodecCtx->sample_rate = details.sample_rate;
	encodeCodecCtx->sample_fmt = VerifySampleFormat(details.sample_fmt) ? details.sample_fmt : encodeCodec->sample_fmts[0];
	encodeCodecCtx->bit_rate = details.bit_rate;
	encodeCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL; //for AAC codec
	encodeDetails = details;
	return ErrorCode::SUCCESS;
}

bool AudioEncoder::VerifySampleFormat(AVSampleFormat fmt) const
{
	const enum AVSampleFormat* sFmt = encodeCodec->sample_fmts;
	while (*sFmt != AV_SAMPLE_FMT_NONE)
	{
		if (*sFmt == fmt)
			return true;
		sFmt++;
	}
	return false;
}

ErrorCode AudioEncoder::InitConverter()
{
	//Init Resampler
	encodeResamplerCtx = swr_alloc_set_opts(nullptr, av_get_default_channel_layout(encodeCodecCtx->channels),
		encodeCodecCtx->sample_fmt, encodeCodecCtx->sample_rate, av_get_default_channel_layout(encodeDetails.input_channels),
		encodeDetails.input_sample_fmt, encodeDetails.input_sample_rate, 0, nullptr);
	if (!encodeResamplerCtx)
		return ErrorCode::NO_SCALER;

	if (swr_init(encodeResamplerCtx) < 0)
		return ErrorCode::NO_SCALER;

	//init fifo
	encodeAudioFifo = av_audio_fifo_alloc(encodeCodecCtx->sample_fmt, encodeCodecCtx->channels, 1);
	if (!encodeAudioFifo)
		return ErrorCode::NO_AUDIO_STREAM;

	return ErrorCode::SUCCESS;
}

MultistreamEncoder::MultistreamEncoder()
{
}

MultistreamEncoder::~MultistreamEncoder()
{
}

ErrorCode MultistreamEncoder::Init(const char* outfile, AVCodecContext* videoCtx, AVCodecContext* audioCtx)
{
	if (!videoCtx || !audioCtx)
		return ErrorCode::NO_CODEC_CTX;

	encodeCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encodeCodec)
		return ErrorCode::NO_CODEC;

	encodeCodecCtx = avcodec_alloc_context3(encodeCodec);
	if (!encodeCodecCtx)
		return ErrorCode::NO_CODEC_CTX;

	audioEncodeCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!audioEncodeCodec)
		return ErrorCode::NO_CODEC;

	audioEncodeCodecCtx = avcodec_alloc_context3(audioEncodeCodec);
	if (!audioEncodeCodecCtx)
		return ErrorCode::NO_CODEC_CTX;

	//format context
	avformat_alloc_output_context2(&encodeFmtCtx, nullptr, nullptr, outfile);
	if (!encodeFmtCtx)
		return ErrorCode::NO_CODEC_CTX;

	//create stream
	stream = avformat_new_stream(encodeFmtCtx, encodeCodec);
	if (!stream)
		return ErrorCode::NO_STREAMS;

	audioStream = avformat_new_stream(encodeFmtCtx, audioEncodeCodec);
	if (!audioStream)
		return ErrorCode::NO_STREAMS;

	//encodeCodecCtx->bit_rate = 400000;
	//encodeCodecCtx->width = 1920;
	//encodeCodecCtx->height = 1080;
	//encodeCodecCtx->framerate = { 30, 1 };
	//encodeCodecCtx->time_base = { 1, 30 };
	//encodeCodecCtx->sample_aspect_ratio = { 1, 1 };
	//encodeCodecCtx->gop_size = 12;
	//encodeCodecCtx->max_b_frames = 1;
	//encodeCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	encodeCodecCtx->bit_rate = videoCtx->bit_rate;
	encodeCodecCtx->width = videoCtx->width;
	encodeCodecCtx->height = videoCtx->height;
	encodeCodecCtx->framerate = { 30, 1 };
	encodeCodecCtx->time_base = { 1, 30 };
	encodeCodecCtx->sample_aspect_ratio = { 1, 1 };
	encodeCodecCtx->gop_size = videoCtx->gop_size;
	encodeCodecCtx->max_b_frames = videoCtx->max_b_frames;
	encodeCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	av_opt_set(encodeCodecCtx->priv_data, "preset", "slow", 0);
	av_opt_set(encodeCodecCtx->priv_data, "crf", "17", AV_OPT_SEARCH_CHILDREN);

	//audioEncodeCodecCtx->channels = 2;
	//audioEncodeCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	//audioEncodeCodecCtx->sample_rate = 44100;
	//audioEncodeCodecCtx->sample_fmt = audioEncodeCodec->sample_fmts[0];
	//audioEncodeCodecCtx->bit_rate = 96000;
	audioEncodeCodecCtx->channels = audioCtx->channels;
	audioEncodeCodecCtx->channel_layout = audioCtx->channel_layout;
	audioEncodeCodecCtx->sample_rate = audioCtx->sample_rate;
	audioEncodeCodecCtx->sample_fmt = audioCtx->sample_fmt;
	audioEncodeCodecCtx->bit_rate = audioCtx->bit_rate;
	audioEncodeCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL; //for AAC codec

	// Some container formats (like MP4) require global headers to be present.
	// Mark the encoder so that it behaves accordingly. 
	if (encodeFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
		encodeCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(encodeCodecCtx, encodeCodec, nullptr) < 0)
		return ErrorCode::CODEC_UNOPENED;

	if (avcodec_parameters_from_context(stream->codecpar, encodeCodecCtx) < 0)
		return ErrorCode::NO_CODEC_CTX;

	if (avcodec_open2(audioEncodeCodecCtx, audioEncodeCodec, nullptr) < 0)
		return ErrorCode::CODEC_UNOPENED;

	if (avcodec_parameters_from_context(audioStream->codecpar, audioEncodeCodecCtx) < 0)
		return ErrorCode::NO_CODEC_CTX;

	if (!(encodeFmtCtx->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&encodeFmtCtx->pb, outfile, AVIO_FLAG_WRITE) < 0)
			return ErrorCode::NO_OUTPUT_FILE;
	}

	if (avformat_write_header(encodeFmtCtx, nullptr) < 0)
		return ErrorCode::NO_DATA_AVAIL;

	nextEncodePts = 0;

	return ErrorCode::SUCCESS;
}

ErrorCode MultistreamEncoder::EncodeFrame(int streamIndex, AVFrame* inputFrame)
{
	if (!encodeCodecCtx  || !audioEncodeCodecCtx || !encodeFmtCtx)
		return ErrorCode::ENCODER_UNINIT;

	if (!encodePkt)
		encodePkt = av_packet_alloc();

	//encode frame
	int ret = avcodec_send_frame(streamIndex == 0 ? encodeCodecCtx : audioEncodeCodecCtx, inputFrame);
	if (ret < 0)
		return ErrorCode::NO_FRAME;

	while (ret >= 0)
	{
		ret = avcodec_receive_packet(streamIndex == 0 ? encodeCodecCtx : audioEncodeCodecCtx, encodePkt);

		if (ret == AVERROR(EAGAIN))
			return ErrorCode::NO_FRAME;
		if (ret == AVERROR_EOF)
			return ErrorCode::FILE_EOF;
		encodePkt->stream_index = streamIndex;
		//av_packet_rescale_ts(encodePkt, streamIndex == 0 ? encodeCodecCtx->time_base : audioEncodeCodecCtx->time_base, encodeFmtCtx->streams[streamIndex]->time_base);
		av_interleaved_write_frame(encodeFmtCtx, encodePkt);
		av_packet_unref(encodePkt);
	}
	return ret == 0 ? ErrorCode::SUCCESS : ErrorCode::BAD_WRITE;
}

ErrorCode MultistreamEncoder::CompleteEncoding()
{
	if (!encodeFmtCtx)
		return ErrorCode::NO_TRAILER;
	av_write_trailer(encodeFmtCtx);
	Cleanup();
	return ErrorCode::SUCCESS;
}

void MultistreamEncoder::Cleanup()
{
	if (encodePkt)
	{
		av_packet_unref(encodePkt);
		av_packet_free(&encodePkt);
	}
	if (encodeCodecCtx)
		avcodec_free_context(&encodeCodecCtx);
	if (audioEncodeCodecCtx)
		avcodec_free_context(&audioEncodeCodecCtx);
	if (encodeFmtCtx)
	{
		avio_closep(&encodeFmtCtx->pb);
		avformat_free_context(encodeFmtCtx);
	}
}