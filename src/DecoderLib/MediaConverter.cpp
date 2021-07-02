// MediaConverter.cpp : Defines the exported functions for the DLL.
//
#include "pch.h"
#include "framework.h"
#include "MediaConverter.h"
#include <thread>
#include <string>
#include <cmath>
#include <math.h>
#include <fstream>

#define LOG 0

const int AudioMixerStreamIndex = 99;

// This is the constructor of a class that has been exported.
CMediaConverter::CMediaConverter()
{
}

CMediaConverter::~CMediaConverter()
{
}

ErrorCode CMediaConverter::openMediaReader(const char* filename)
{
    return openMediaReader(&m_mrState, filename);
}

ErrorCode CMediaConverter::openMediaReader(MediaReaderState* state, const char* filename)
{
#if LOG
    av_log_set_callback(&CMediaConverter::avlog_cb);
#endif
    state->av_format_ctx = avformat_alloc_context();
    if (!state->av_format_ctx)
        return ErrorCode::NO_FMT_CTX;

    if (avformat_open_input(&state->av_format_ctx, filename, NULL, NULL)) //returns 0 on success
        return ErrorCode::FMT_UNOPENED;

    if (state->av_format_ctx->nb_streams < 1)
        return ErrorCode::NO_STREAMS;

    state->av_format_ctx->seek2any = 1;

    state->video_stream_index = -1;
    state->audio_stream_index = -1;
    //AVCodec* av_codec = nullptr;
    AVCodecParameters* av_codec_params = nullptr;

    for (unsigned int i = 0; i < state->av_format_ctx->nb_streams; ++i)
    {
        auto stream = state->av_format_ctx->streams[i];
        av_codec_params = state->av_format_ctx->streams[i]->codecpar;

        const AVCodec* av_codec = avcodec_find_decoder(av_codec_params->codec_id);
        if (!av_codec)
            return ErrorCode::NO_CODEC;

        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            state->video_stream_index = i;

            state->video_codec_ctx = avcodec_alloc_context3(av_codec);
            if (!state->video_codec_ctx)
                return ErrorCode::NO_CODEC_CTX;

            //state->video_codec_ctx->thread_count = std::thread::hardware_concurrency();

            if (avcodec_parameters_to_context(state->video_codec_ctx, av_codec_params) < 0)
                return ErrorCode::CODEC_CTX_UNINIT;

            if (avcodec_open2(state->video_codec_ctx, av_codec, NULL) < 0)
                return ErrorCode::CODEC_UNOPENED;
        }
        else if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            state->audio_stream_index = i;
            state->audio_codec_ctx = avcodec_alloc_context3(av_codec);
            if (!state->audio_codec_ctx)
                return ErrorCode::NO_CODEC_CTX;

            state->audio_codec_ctx->thread_count = std::thread::hardware_concurrency();

            if (avcodec_parameters_to_context(state->audio_codec_ctx, av_codec_params) < 0)
                return ErrorCode::CODEC_CTX_UNINIT;

            if (avcodec_open2(state->audio_codec_ctx, av_codec, NULL) < 0)
                return ErrorCode::CODEC_UNOPENED;

            if (state->audio_codec_ctx->channel_layout == 0)
                state->audio_codec_ctx->channel_layout = AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT;
        }
    }

    state->av_packet = av_packet_alloc();
    if (!state->av_packet)
        return ErrorCode::NO_PACKET;

    state->SetIsOpened();
    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::readVideoFrame(VideoBuffer& buffer)
{
    return readVideoFrame(&m_mrState, buffer);
}

ErrorCode CMediaConverter::readVideoFrame(MediaReaderState* state, VideoBuffer& buffer)
{
    ErrorCode response = processVideoPacketsIntoFrames(state);

    if (response == ErrorCode::FILE_EOF)
    {
        avcodec_flush_buffers(state->video_codec_ctx);
        return response;
    }

    return outputToBuffer(state, buffer);
}

ErrorCode CMediaConverter::readFrameToPtr(unsigned char** dataPtr)
{
    return readFrameToPtr(&m_mrState, dataPtr);
}

ErrorCode CMediaConverter::readFrameToPtr(MediaReaderState* state, unsigned char** dataPtr)
{
    ErrorCode response = processVideoPacketsIntoFrames(state);

    if (response == ErrorCode::FILE_EOF)
    {
        avcodec_flush_buffers(state->video_codec_ctx);
        return response;
    }

    return outputToBuffer(state, dataPtr);
}

ErrorCode CMediaConverter::fullyProcessAudioFrame(AudioBuffer& audioBuffer)
{
    return fullyProcessAudioFrame(&m_mrState, audioBuffer);
}

ErrorCode CMediaConverter::fullyProcessAudioFrame(MediaReaderState* state, AudioBuffer& audioBuffer)
{
    ErrorCode response = readAudioFrame(state);
    if(response == ErrorCode::SUCCESS)
        return (ErrorCode)outputToAudioBuffer(state, audioBuffer);

    return response;
}

ErrorCode CMediaConverter::readAudioFrame()
{
    return readAudioFrame(&m_mrState);
}

ErrorCode CMediaConverter::readAudioFrame(MediaReaderState* state)
{
    ErrorCode response = processAudioPacketsIntoFrames(state);

    if (response == ErrorCode::FILE_EOF)
    {
        avcodec_flush_buffers(state->audio_codec_ctx);
        return response;
    }

    if (response == ErrorCode::SUCCESS)
        return adjustAudioVolume();

    return response;
}

/*
* this returns int because the av_read_frame supercedes any other errors but those errors are sent back 
* and can be handled if returned. av_read_frame returns negative for errors and 0 for success.
* any errors for ErrorCode class are all positive
*/
ErrorCode CMediaConverter::processVideoPacketsIntoFrames()
{
    return processVideoPacketsIntoFrames(&m_mrState);
}

ErrorCode CMediaConverter::processVideoPacketsIntoFrames(MediaReaderState* state)
{
    //inital frame read, gives it an initial state to branch from
    ErrorCode response = readFrame(state);

    if (response == ErrorCode::FILE_EOF)
        bool stop = true;

    if (response == ErrorCode::SUCCESS || response == ErrorCode::FILE_EOF)
        return processVideoIntoFrames(state, response == ErrorCode::FILE_EOF);

    return response;
}

ErrorCode CMediaConverter::processAudioPacketsIntoFrames()
{
    return processAudioPacketsIntoFrames(&m_mrState);
}

ErrorCode CMediaConverter::processAudioPacketsIntoFrames(MediaReaderState* state)
{
    ErrorCode response = readFrame(state);

    if (response == ErrorCode::SUCCESS)
    {
        return processAudioIntoFrames(state);
    }

    return response;
}

ErrorCode CMediaConverter::processVideoIntoFrames(MediaReaderState* state, bool drainBuffer)
{
    if (!state->video_codec_ctx)
        return ErrorCode::NO_CODEC_CTX;

    if (!state->av_frame)
        state->av_frame = av_frame_alloc();

    if (!state->av_frame)
        return ErrorCode::NO_FRAME;

    int response = 0;
    //send back and receive decoded frames, until frames can't be read
    do {
        if (state->av_packet->stream_index != state->video_stream_index)
            continue;

        response = avcodec_send_packet(state->video_codec_ctx, drainBuffer ? nullptr : state->av_packet);
        if (response < 0 && response != AVERROR_EOF)
            return ErrorCode::PKT_NOT_DECODED;

        response = avcodec_receive_frame(state->video_codec_ctx, state->av_frame);
        if (response == AVERROR(EAGAIN))
            continue;
        else if (response == AVERROR_EOF)
            return ErrorCode::FILE_EOF;
        else if (response < 0)
            return ErrorCode::PKT_NOT_RECEIVED;

        av_packet_unref(state->av_packet);
        break;
    } while (readFrame(state) == ErrorCode::SUCCESS);

    //retrieve stats
    if (response == 0)
    {
        state->videoFrameData.FillDataFromFrame(state->av_frame);
        return ErrorCode::SUCCESS;
    }
    switch (response)
    {
    case AVERROR(EAGAIN):
        return ErrorCode::AGAIN;
    case AVERROR_EOF:
        return ErrorCode::FILE_EOF;
    default:
        return ErrorCode::FFMPEG_ERROR;
    }
}

ErrorCode CMediaConverter::processAudioIntoFrames(MediaReaderState* state)
{
    if (!state->audio_codec_ctx)
        return ErrorCode::NO_CODEC_CTX;

    if (!state->av_frame)
        state->av_frame = av_frame_alloc();

    if (!state->av_frame)
        return ErrorCode::NO_FRAME;

    int response = 0;
    //send back and receive decoded frames, until frames can't be read
    do {
        if (state->av_packet->stream_index != state->audio_stream_index)
            continue;

        response = avcodec_send_packet(state->audio_codec_ctx, state->av_packet);

        if (response < 0)
            return ErrorCode::PKT_NOT_DECODED;

        response = avcodec_receive_frame(state->audio_codec_ctx, state->av_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
            continue;
        else if (response < 0)
            return ErrorCode::PKT_NOT_RECEIVED;

        av_packet_unref(state->av_packet);
        break;
    } while (readFrame(state) == ErrorCode::SUCCESS);

    if (response == 0)
    {
        //update frame data as necessary
        state->audioFrameData.FillDataFromFrame(state->av_frame);
        state->audioFrameData.UpdateBitRate(state->audio_codec_ctx);
        return ErrorCode::SUCCESS;
    }

    switch (response)
    {
    case AVERROR(EAGAIN):
        return ErrorCode::AGAIN;
    case AVERROR_EOF:
        return ErrorCode::FILE_EOF;
    default:
        return ErrorCode::FFMPEG_ERROR;
    }
}

ErrorCode CMediaConverter::readFrame()
{
    return readFrame(&m_mrState);
}

ErrorCode CMediaConverter::readFrame(MediaReaderState* state)
{
    if (!state->av_format_ctx)
        return ErrorCode::NO_FMT_CTX;
    int ret = av_read_frame(state->av_format_ctx, state->av_packet);
    //retrieve stats
    if (ret == 0)
    {
        state->videoFrameData.FillDataFromPacket(state->av_packet);
        return ErrorCode::SUCCESS;
    }
    switch (ret)
    {
    case AVERROR(EAGAIN):
        return ErrorCode::AGAIN;
    case AVERROR_EOF:
        return ErrorCode::FILE_EOF;
    default:
        return ErrorCode::FFMPEG_ERROR;
    }
}

ErrorCode CMediaConverter::outputToBuffer(VideoBuffer& buffer)
{
    return outputToBuffer(&m_mrState, buffer);
}

ErrorCode CMediaConverter::outputToBuffer(MediaReaderState* state, VideoBuffer& buffer)
{
    if (!state->video_codec_ctx)
        return ErrorCode::NO_CODEC_CTX;
    //setup scaler
    if (!state->sws_scaler_ctx)
    {
        state->sws_scaler_ctx = sws_getContext(state->VideoWidth(), state->VideoHeight(), state->video_codec_ctx->pix_fmt, //input
            state->VideoWidth(), state->VideoHeight(), AV_PIX_FMT_RGBA, //output
            SWS_BILINEAR, NULL, NULL, NULL); //options
    }
    if (!state->sws_scaler_ctx)
        return ErrorCode::NO_SCALER;
    uint64_t w = state->av_frame->width;
    uint64_t h = state->av_frame->height;
    uint64_t size = w * h * 4;

    if (size == 0)
        return ErrorCode::NO_DATA_AVAIL;

    buffer.resize(size);
    if (buffer.empty())
        return ErrorCode::NO_DATA_AVAIL;

    //using 4 here because RGB0 designates 4 channels of values
    unsigned char* dest[4] = { &buffer[0], NULL, NULL, NULL };
    int dest_linesize[4] = { state->VideoWidth() * 4, 0, 0, 0 };

    int ret = sws_scale(state->sws_scaler_ctx, state->av_frame->data, state->av_frame->linesize, 0, state->VideoHeight(), dest, dest_linesize);

    av_frame_unref(state->av_frame);

    return ret > 0 ? ErrorCode::SUCCESS : ErrorCode::BAD_SCALE;
}

ErrorCode CMediaConverter::outputToBuffer(unsigned char** dataPtr)
{
    return outputToBuffer(&m_mrState, dataPtr);
}

ErrorCode CMediaConverter::outputToBuffer(MediaReaderState* state, unsigned char** dataPtr)
{
    if (!state->video_codec_ctx)
        return ErrorCode::NO_CODEC_CTX;
    //setup scaler
    if (!state->sws_scaler_ctx)
    {
        state->sws_scaler_ctx = sws_getContext(state->VideoWidth(), state->VideoHeight(), state->video_codec_ctx->pix_fmt, //input
            state->VideoWidth(), state->VideoHeight(), AV_PIX_FMT_RGBA, //output
            SWS_BILINEAR, NULL, NULL, NULL); //options
    }
    if (!state->sws_scaler_ctx)
        return ErrorCode::NO_SCALER;
    uint64_t w = state->av_frame->width;
    uint64_t h = state->av_frame->height;
    uint64_t size = w * h * 4;

    if (size == 0)
        return ErrorCode::NO_DATA_AVAIL;

    *dataPtr = new unsigned char[size]; 

    //using 4 here because RGB0 designates 4 channels of values
    unsigned char* dest[4] = { *dataPtr, NULL, NULL, NULL };
    int dest_linesize[4] = { state->VideoWidth() * 4, 0, 0, 0 };

    int ret = sws_scale(state->sws_scaler_ctx, state->av_frame->data, state->av_frame->linesize, 0, state->VideoHeight(), dest, dest_linesize);

    av_frame_unref(state->av_frame);

    return ret > 0 ? ErrorCode::SUCCESS : ErrorCode::BAD_SCALE;
}

ErrorCode CMediaConverter::outputToAudioBuffer(AudioBuffer& audioBuffer, bool freeFrame)
{
    return outputToAudioBuffer(&m_mrState, audioBuffer, freeFrame);
}

ErrorCode CMediaConverter::outputToAudioBuffer(MediaReaderState* state, AudioBuffer& audioBuffer, bool freeFrame)
{
    if (audioBuffer.size() != state->AudioBufferSize() && state->AudioBufferSize() > 0)
        audioBuffer.resize(state->AudioBufferSize());
    else
        return ErrorCode::NO_DATA_AVAIL;

    auto ptr = &audioBuffer[0];
    
    //don't let it reallocate here it will cause a memory leak--if you need to set new opts you must free it first
    if (!state->swr_ctx) 
    {
        state->swr_ctx = swr_alloc_set_opts(nullptr, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, state->SampleRate(),
            state->audio_codec_ctx->channel_layout, state->AudioSampleFormat(), state->SampleRate(), 0, nullptr);
    }
    if (!state->swr_ctx)
        return ErrorCode::NO_SWR_CTX;

    swr_init(state->swr_ctx);

    int got_samples = swr_convert(state->swr_ctx, &ptr, state->NumSamples(), (const uint8_t**)state->av_frame->extended_data, state->av_frame->nb_samples);

    if(got_samples < 0)
        return ErrorCode::NO_SWR_CONVERT;

    while (got_samples > 0)
    {
        got_samples = swr_convert(state->swr_ctx, &ptr, state->NumSamples(), nullptr, 0);
        if (got_samples < 0)
            return ErrorCode::NO_SWR_CONVERT;
    }

    return freeFrame ? freeAVFrame(state) : ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::freeAVFrame()
{
    return freeAVFrame(&m_mrState);
}

ErrorCode CMediaConverter::freeAVFrame(MediaReaderState* state)
{
    if (!state->av_frame)
        return ErrorCode::NO_FRAME;
    av_frame_unref(state->av_frame);
    av_frame_free(&state->av_frame);
    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::trackToFrame(int64_t targetPts)
{
    return trackToFrame(&m_mrState, targetPts);
}

ErrorCode CMediaConverter::trackToFrame(MediaReaderState* state, int64_t targetPts)
{
    seekToFrame(state, targetPts);
    auto ret = processVideoPacketsIntoFrames(state);

    int64_t interval = state->VideoFrameInterval() * state->FPS(); // interval starts at 1 second previous
    int64_t previous = state->VideoFramePts();
    if (ret != ErrorCode::SUCCESS)
        seekToFrame(state, targetPts - interval);
    while (!WithinTolerance(targetPts, state->VideoFramePts(), state->VideoFrameInterval() - 10))
    {
        if (state->VideoFramePts() < targetPts)
        {
            processVideoPacketsIntoFrames(state);
        }
        else
        {
            seekToFrame(state, state->VideoFramePts() - interval);
            auto ret = processVideoPacketsIntoFrames(state);
            if (ret != ErrorCode::SUCCESS)
                return ret;
            interval *= 2;
        }

        if (previous == state->VideoFramePts())
            return ErrorCode::SUCCESS;
        previous = state->VideoFramePts();
    }

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::trackToAudioFrame(int64_t targetPts)
{
    return trackToAudioFrame(&m_mrState, targetPts);
}

ErrorCode CMediaConverter::trackToAudioFrame(MediaReaderState* state, int64_t targetPts)
{
    if (!state->audio_codec_ctx)
        return ErrorCode::NO_CODEC_CTX;
    if (state->audio_stream_index < 0)
        return ErrorCode::NO_STREAMS;

    seekToAudioFrame(state, targetPts);
    auto ret = processAudioPacketsIntoFrames(state);
    if (ret != ErrorCode::SUCCESS)
        return ret;
    int64_t interval = state->AudioFrameInterval() * state->FPS(); // interval starts at 1 second previous
    int64_t previous = state->AudioPts();
    while (!WithinTolerance(targetPts, state->AudioPts(), state->AudioFrameInterval() - 10))
    {
        if (state->AudioPts() < targetPts)
        {
            processAudioPacketsIntoFrames(state);
        }
        else
        {
            seekToAudioFrame(state, state->AudioPts() - interval);
            auto ret = processAudioPacketsIntoFrames(state);
            if (ret != ErrorCode::SUCCESS)
                return ret;
        }

        if (previous == state->AudioPts())
            return ErrorCode::SUCCESS;
        previous = state->AudioPts();
    }

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::seekToFrame(int64_t targetPts)
{
    return seekToFrame(&m_mrState, targetPts);
}

ErrorCode CMediaConverter::seekToFrame(MediaReaderState* state, int64_t targetPts)
{
    if (av_seek_frame(state->av_format_ctx, state->video_stream_index, targetPts, AVSEEK_FLAG_BACKWARD) >= 0)
    {
        avcodec_flush_buffers(state->video_codec_ctx);
        return ErrorCode::SUCCESS;
    }
    return ErrorCode::SEEK_FAILED;
}

ErrorCode CMediaConverter::seekToAudioFrame(int64_t targetPts)
{
    return seekToAudioFrame(&m_mrState, targetPts);
}

ErrorCode CMediaConverter::seekToAudioFrame(MediaReaderState* state, int64_t targetPts)
{
    if (av_seek_frame(state->av_format_ctx, state->audio_stream_index, targetPts, AVSEEK_FLAG_BACKWARD) >= 0)
    {
        avcodec_flush_buffers(state->audio_codec_ctx);
        return ErrorCode::SUCCESS;
    }
    return ErrorCode::SEEK_FAILED;
}

ErrorCode CMediaConverter::seekToStart()
{
    return seekToStart(&m_mrState);
}

ErrorCode CMediaConverter::seekToStart(MediaReaderState* state)
{
    if (av_seek_frame(state->av_format_ctx, state->video_stream_index, 0, 0) >= 0)
    {
        avcodec_flush_buffers(state->video_codec_ctx);
        return ErrorCode::SUCCESS;
    }
    return ErrorCode::SEEK_FAILED;
}

ErrorCode CMediaConverter::seekToAudioStart()
{
    return seekToAudioStart(&m_mrState);
}

ErrorCode CMediaConverter::seekToAudioStart(MediaReaderState* state)
{
    if (av_seek_frame(state->av_format_ctx, state->audio_stream_index, 0, 0) >= 0)
    {
        avcodec_flush_buffers(state->audio_codec_ctx);
        return ErrorCode::SUCCESS;
    }
    return ErrorCode::SEEK_FAILED;
}

ErrorCode CMediaConverter::closeMediaReader()
{
    return closeMediaReader(&m_mrState);
}

ErrorCode CMediaConverter::closeMediaReader(MediaReaderState* state)
{
    if (state->IsOpened())
    {
        sws_freeContext(state->sws_scaler_ctx);
        swr_free(&state->swr_ctx);
        avformat_close_input(&state->av_format_ctx);
        avformat_free_context(state->av_format_ctx);
        avcodec_free_context(&state->video_codec_ctx);
        avcodec_free_context(&state->audio_codec_ctx);
        av_frame_free(&state->av_frame);
        av_packet_free(&state->av_packet);
        state->SetIsOpened(false);
    }
    return ErrorCode::SUCCESS;
}

bool CMediaConverter::WithinTolerance(int64_t referencePts, int64_t targetPts, int64_t tolerance)
{
    return std::abs(targetPts - referencePts) < tolerance;
}

bool CMediaConverter::ValidateFrames(const std::vector<AVFrame*>& frames)
{
    if (frames.empty())
        return false;

    for (auto f : frames)
        if (!f) return false;

    return true;
}

std::string CMediaConverter::CreateMixString(int numTracks, double volumeOffset)
{
    std::string srcBufferStrs;
    std::string mixInputs;
    for (int i = 0; i < numTracks; ++i)
    {
        std::string input("[in" + std::to_string(i) + "]");
        srcBufferStrs += std::string("abuffer = channel_layout=stereo : sample_fmt = fltp : sample_rate = 44100 " + input + ";");
        mixInputs += input;
    }
    std::string mixStr("amix=inputs=" + std::to_string(numTracks) + ",volume=" + std::to_string(numTracks));
    std::string closeInputsStr("[out];");

    std::string sinkBufferStr("[out] abuffersink");

    return srcBufferStrs + mixInputs + mixStr + "," + CreateVolumeDbString(volumeOffset) + "," + CreateStatsString() + closeInputsStr + sinkBufferStr;
}

std::string CMediaConverter::CreateStatsString()
{
    return std::string("astats=metadata=1:reset=1,ametadata=print:key=lavfi.astats.Overall.RMS_level:key=lavfi.astats.Overall.DC_offset:key=lavfi.astats.Overall.Peak_level:key=lavfi.astats.1.RMS_level:key=lavfi.astats.1.Peak_level:key=lavfi.astats.2.RMS_level:key=lavfi.astats.2.Peak_level");
}

std::string CMediaConverter::CreateVolumeDbString(double volumeOffset)
{
    return std::string("volume=" + std::to_string(volumeOffset) + "dB");
}

std::string CMediaConverter::CreateVolumeString(double volume)
{
    return std::string("volume=" + std::to_string(volume));
}

bool CMediaConverter::InitAudioMixerContext()
{
    return InitAudioMixerContext(&m_mrState);
}

bool CMediaConverter::InitAudioMixerContext(MediaReaderState* state)
{
    if (state->audio_codec_ctx && state->audio_codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        return true;
    if (state->audio_codec_ctx)
        avcodec_free_context(&state->audio_codec_ctx);

    const AVCodec* av_codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
    if (!av_codec)
        return false;
    state->audio_codec_ctx = avcodec_alloc_context3(av_codec);
    state->audio_codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    state->audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    state->audio_stream_index = AudioMixerStreamIndex;

    return true;
}

ErrorCode CMediaConverter::initAudioFilterGraph(double volumeOffset, double faderLevel)
{
    return initAudioFilterGraph(&m_mrState, volumeOffset, faderLevel);
}

ErrorCode CMediaConverter::initAudioFilterGraph(MediaReaderState* state, double volumeOffset, double faderLevel)
{
    /* Create a new filtergraph, which will contain all the filters. */
    state->av_filter_graph = avfilter_graph_alloc();
    if (!state->av_filter_graph)
        return ErrorCode::NO_FILTER_GRAPH;

    AVFilterInOut* outputs = nullptr;
    AVFilterInOut* inputs = nullptr;

    std::string fmt(av_get_sample_fmt_name(state->AudioSampleFormat()));
    std::string filters("abuffer = channel_layout=stereo : sample_fmt = " + fmt + ": sample_rate = " + std::to_string(state->SampleRate()) + "[in0]; [in0]" + CreateVolumeDbString(volumeOffset) + "," + CreateVolumeString(faderLevel) + "," + CreateStatsString() + "[out]; [out] abuffersink");

    int err = avfilter_graph_parse2(state->av_filter_graph, filters.c_str(), &inputs, &outputs);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    /* Configure the graph. */
    err = avfilter_graph_config(state->av_filter_graph, nullptr);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::adjustAudioVolume()
{
    return adjustAudioVolume(&m_mrState);
}

ErrorCode CMediaConverter::adjustAudioVolume(MediaReaderState* state)
{
    ErrorCode errCode = ErrorCode::SUCCESS;

    errCode = initAudioFilterGraph(state->AudioVolumeOffset(), state->AudioFaderLevel());
    if (errCode != ErrorCode::SUCCESS)
        return errCode;

    /* Send the frame to the input of the filtergraph. */
    int err = av_buffersrc_add_frame(state->av_filter_graph->filters[0], state->av_frame);
    if (err < 0)
    {
        av_frame_unref(state->av_frame);
        return ErrorCode::NO_FRAME;
    }
    /* Get all the filtered output that is available. */
    AVFilterContext* bufferSink = state->av_filter_graph->filters[state->av_filter_graph->nb_filters - 1];
    if(!bufferSink || strcmp(bufferSink->filter->name, "abuffersink"))
        return ErrorCode::NO_BUFFERSINK_FILTER;
    err = av_buffersink_get_frame(bufferSink, state->av_frame);

    if (err == AVERROR(EAGAIN))
        return ErrorCode::AGAIN;
    else if (err == AVERROR_EOF)
        return ErrorCode::FILE_EOF;
    else if (err < 0)
        return ErrorCode::NO_FRAME;

    avfilter_graph_free(&state->av_filter_graph);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::mixdownAudio(const std::vector<AVFrame*>& frames)
{
    return mixdownAudio(&m_mrState, frames);
}

ErrorCode CMediaConverter::mixdownAudio(MediaReaderState* state, const std::vector<AVFrame*>& frames)
{
    InitAudioMixerContext();
    return mixAudioTracks(state, frames);
}

ErrorCode CMediaConverter::readMixedAudio(const std::vector<AVFrame*>& frames, AudioBuffer& buffer)
{
    return readMixedAudio(&m_mrState, frames, buffer);
}

ErrorCode CMediaConverter::readMixedAudio(MediaReaderState* state, const std::vector<AVFrame*>& frames, AudioBuffer& buffer)
{
    ErrorCode errCode = mixdownAudio(state, frames);

    if(errCode == ErrorCode::SUCCESS)
    {
        errCode = (ErrorCode)outputToAudioBuffer(buffer, false);
    }
    return errCode;
}

ErrorCode CMediaConverter::initAudioMixFilterGraph(int numTracks)
{
    return initAudioMixFilterGraph(&m_mrState, numTracks);
}

ErrorCode CMediaConverter::initAudioMixFilterGraph(MediaReaderState* state, int numTracks)
{
    ///* Create a new filtergraph, which will contain all the filters. */
    if(!state->av_filter_graph)
        state->av_filter_graph = avfilter_graph_alloc();
    if (!state->av_filter_graph)
        return ErrorCode::NO_FILTER_GRAPH;

    AVFilterInOut* outputs = nullptr;
    AVFilterInOut* inputs = nullptr;

    std::string filters = CreateMixString(numTracks, state->AudioVolumeOffset());

    int err = avfilter_graph_parse2(state->av_filter_graph, filters.c_str(), &inputs, &outputs);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    err = avfilter_graph_config(state->av_filter_graph, nullptr);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    char* str = avfilter_graph_dump(state->av_filter_graph, nullptr);

    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::mixAudioTracks(const std::vector<AVFrame*>& frames)
{
    return mixAudioTracks(&m_mrState, frames);
}

ErrorCode CMediaConverter::mixAudioTracks(MediaReaderState* state, const std::vector<AVFrame*>& frames)
{
    ErrorCode errCode = ErrorCode::SUCCESS;

    if (!ValidateFrames(frames))
        return ErrorCode::NO_FRAME;

    errCode = initAudioMixFilterGraph(state, (int)frames.size());
    if (errCode != ErrorCode::SUCCESS)
        return errCode;

    if (!state->av_filter_graph || state->av_filter_graph->nb_filters != frames.size() + 6) //filters should include mix filter, pre-amp volume filter, master volume offset filter, stats filter, metadata filter and buffer sink filter + all the source filters for each frame 
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    /* Send the frame to the input of the filtergraph. */
    for (int i = 0; i < frames.size(); ++i)
    {
        AVFrame* f = frames.at(i);
        AVFilterContext* srcCtx = state->av_filter_graph->filters[i];
        if (!srcCtx)
        {
            for (auto fr : frames)
                av_frame_unref(fr);
            return ErrorCode::NO_BUFFER_INSTANCE;
        }
        int err = av_buffersrc_add_frame(srcCtx, f);
        if (err < 0)
        {
            for (auto fr : frames)
                av_frame_unref(fr);
            return ErrorCode::NO_FRAME;
        }
    }

    AVFilterContext* bufferSink = state->av_filter_graph->filters[state->av_filter_graph->nb_filters - 1];
    if (!bufferSink || strcmp(bufferSink->filter->name, "abuffersink"))
    {
        for (auto f : frames)
            av_frame_unref(f);
        return ErrorCode::NO_BUFFERSINK_INSTANCE;
    }
    /* Get all the filtered output that is available. */
    if (!state->av_frame)
        state->av_frame = av_frame_alloc();
    int err = av_buffersink_get_frame(bufferSink, state->av_frame);

    if (err == AVERROR(EAGAIN))
        return ErrorCode::AGAIN;
    else if (err == AVERROR_EOF)
        return ErrorCode::FILE_EOF;
    else if (err < 0)
        return ErrorCode::NO_FRAME;

    state->audioFrameData.FillDataFromFrame(state->av_frame);

    avfilter_graph_free(&state->av_filter_graph);
    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::initFormatFilterGraph()
{
    return initFormatFilterGraph(&m_mrState);
}

ErrorCode CMediaConverter::initFormatFilterGraph(MediaReaderState* state)
{
    ///* Create a new filtergraph, which will contain all the filters. */
    state->av_filter_graph = avfilter_graph_alloc();
    if (!state->av_filter_graph)
        return ErrorCode::NO_FILTER_GRAPH;

    AVFilterInOut* outputs = nullptr;
    AVFilterInOut* inputs = nullptr;

    std::string srcBufferStrs = std::string("abuffer = channel_layout=stereo : sample_fmt = fltp : sample_rate = 44100 [in0];");
    std::string formatStr("[in0]aformat=sample_fmts=s16:channel_layouts=stereo:sample_rates=44100[out];");
    std::string sinkBufferStr("[out] abuffersink");
    std::string filters = srcBufferStrs + formatStr + sinkBufferStr;
    int err = avfilter_graph_parse2(state->av_filter_graph, filters.c_str(), &inputs, &outputs);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    err = avfilter_graph_config(state->av_filter_graph, nullptr);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    char* str = avfilter_graph_dump(state->av_filter_graph, nullptr);

    std::string stdStr(str);

    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::reformatAudio()
{
    return reformatAudio(&m_mrState);
}

ErrorCode CMediaConverter::reformatAudio(MediaReaderState* state)
{
    ErrorCode errCode = ErrorCode::SUCCESS;

    errCode = initFormatFilterGraph();
    if (errCode != ErrorCode::SUCCESS)
        return errCode;

    /* Send the frame to the input of the filtergraph. */
    int err = av_buffersrc_add_frame(state->av_filter_graph->filters[0], state->av_frame);
    if (err < 0)
    {
        av_frame_unref(state->av_frame);
        return ErrorCode::NO_FRAME;
    }

    /* Get all the filtered output that is available. */
    err = av_buffersink_get_frame(state->av_filter_graph->filters[2], state->av_frame);

    if (err == AVERROR(EAGAIN))
        return ErrorCode::AGAIN;
    else if (err == AVERROR_EOF)
        return ErrorCode::FILE_EOF;
    else if (err < 0)
        return ErrorCode::NO_FRAME;

    avfilter_graph_free(&state->av_filter_graph);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::initWaveformImageFilterGraph(int width, int height, std::string colors)
{
    return initWaveformImageFilterGraph(&m_mrState, width, height, colors);
}

ErrorCode CMediaConverter::initWaveformImageFilterGraph(MediaReaderState* state, int width, int height, std::string colors)
{
    state->av_filter_graph = avfilter_graph_alloc();
    if (!state->av_filter_graph)
        return ErrorCode::NO_FILTER_GRAPH;

    auto waveFilter = avfilter_get_by_name("showwavespic");

    AVFilterInOut* outputs = nullptr;
    AVFilterInOut* inputs = nullptr;

    std::string srcBufferStr("abuffer = channel_layout=stereo : sample_fmt = " +  std::string(av_get_sample_fmt_name(state->audio_codec_ctx->sample_fmt)) + ": sample_rate = " + std::to_string(state->audio_codec_ctx->sample_rate) + "[in0];");
    std::string filterStr("[in0]showwavespic=s=" + std::to_string(width) + "x" + std::to_string(height) + ":split_channels=1" + (!colors.empty() ? ":colors=" + colors : "") + "[out];");
    std::string sinkBufferStr("[out] buffersink");
    std::string filters = srcBufferStr + filterStr + sinkBufferStr;
    int err = avfilter_graph_parse2(state->av_filter_graph, filters.c_str(), &inputs, &outputs);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    err = avfilter_graph_config(state->av_filter_graph, nullptr);
    if (err < 0)
        return ErrorCode::FILTER_GRAPH_NOT_CONFIG;

    char* str = avfilter_graph_dump(state->av_filter_graph, nullptr);

    std::string stdStr(str);

    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::generateWaveformImage()
{
    return generateWaveformImage(&m_mrState);
}

ErrorCode CMediaConverter::generateWaveformImage(MediaReaderState* state)
{
    if (!state->av_filter_graph)
        return ErrorCode::NO_FILTER_GRAPH;

    /* Send the frame to the input of the filtergraph. */
    int err = av_buffersrc_add_frame(state->av_filter_graph->filters[0], state->av_frame);
    if (err < 0)
    {
        av_frame_unref(state->av_frame);
        return ErrorCode::NO_FRAME;
    }

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::getWaveformImage(VideoBuffer& buffer)
{
    return getWaveformImage(&m_mrState, buffer);
}

ErrorCode CMediaConverter::getWaveformImage(MediaReaderState* state, VideoBuffer& buffer)
{
    if (!state->visualizer_frame)
        state->visualizer_frame = av_frame_alloc();

    int err = av_buffersrc_add_frame(state->av_filter_graph->filters[0], state->av_frame);
    if (err < 0)
    {
        av_frame_unref(state->av_frame);
        return ErrorCode::NO_FRAME;
    }
    /* Get all the filtered output that is available. */
    err = av_buffersink_get_frame(state->av_filter_graph->filters[2], state->visualizer_frame);

    avfilter_graph_free(&state->av_filter_graph);

    if (err == AVERROR(EAGAIN))
        return ErrorCode::AGAIN;
    else if (err == AVERROR_EOF)
        return ErrorCode::FILE_EOF;
    else if (err < 0)
        return ErrorCode::NO_FRAME;

    int w = state->visualizer_frame->width;
    int h = state->visualizer_frame->height;
    //setup scaler
    SwsContext* sws = sws_getContext(w, h, AV_PIX_FMT_RGBA, //input
            w, h, AV_PIX_FMT_RGBA, //output
            SWS_BILINEAR, NULL, NULL, NULL); //options
    if (!sws)
        return ErrorCode::NO_SCALER;

    uint64_t size = w * h * 4;

    if (size == 0)
        return ErrorCode::NO_FRAME;

    buffer.resize(size);

    //using 4 here because RGB0 designates 4 channels of values
    unsigned char* dest[4] = { &buffer[0], NULL, NULL, NULL };
    int dest_linesize[4] = { w * 4, 0, 0, 0 };

    sws_scale(sws, state->visualizer_frame->data, state->visualizer_frame->linesize, 0, h, dest, dest_linesize);

    av_frame_unref(state->visualizer_frame);
    av_frame_free(&state->visualizer_frame);

    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::encodeMedia(const char* inFile, const char* outFile)
{
    return encodeMedia(inFile, outFile, &m_mrState);
}

ErrorCode CMediaConverter::encodeMedia(const char* inFile, const char* outFile, MediaReaderState* state)
{
    if (avformat_open_input(&state->av_format_ctx, inFile, nullptr, nullptr) < 0)
        return ErrorCode::FMT_UNOPENED;

    if (avformat_find_stream_info(state->av_format_ctx, nullptr) < 0)
        return ErrorCode::NO_STREAMS;

    AVFormatContext* out_ctx = nullptr;
    avformat_alloc_output_context2(&out_ctx, nullptr, nullptr, outFile);
    if (!out_ctx)
        return ErrorCode::NO_CODEC_CTX;

    int num_streams = state->av_format_ctx->nb_streams;
    if (num_streams <= 0)
        return ErrorCode::NO_STREAMS;

    int* streams_list = nullptr;
    streams_list = (int*)av_mallocz_array(num_streams, sizeof(*streams_list));
    if (!streams_list)
        return ErrorCode::NO_STREAMS;

    int stream_index = 0;
    for (unsigned int i = 0; i < state->av_format_ctx->nb_streams; ++i)
    {
        AVStream* out_stream = nullptr;
        AVStream* in_stream = state->av_format_ctx->streams[i];
        AVCodecParameters* in_codecpar = in_stream->codecpar;

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
        {
            streams_list[i] = -1;
            continue;
        }

        streams_list[i] = stream_index++;
        out_stream = avformat_new_stream(out_ctx, nullptr);
        if (!out_stream)
            return ErrorCode::NO_CODEC_CTX;

        if (avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0)
            return ErrorCode::NO_CODEC_CTX;
    }

    av_dump_format(out_ctx, 0, outFile, 1);

    if (!(out_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&out_ctx->pb, outFile, AVIO_FLAG_WRITE) < 0)
            return ErrorCode::NO_OUTPUT_FILE;
    }

    int output = avformat_write_header(out_ctx, nullptr);
    if (output < 0)
        return ErrorCode::NO_OUTPUT_FILE;

    AVPacket pkt;
    while (1) //this will end up linking up with the start and stop frames of the files
    {
        AVStream* in_stream = nullptr;
        AVStream* out_stream = nullptr;
        if (av_read_frame(state->av_format_ctx, &pkt) < 0)
            break;

        in_stream = state->av_format_ctx->streams[pkt.stream_index];
        if (pkt.stream_index > num_streams || streams_list[pkt.stream_index] < 0)
        {
            av_packet_unref(&pkt);
            continue;
        }

        pkt.stream_index = streams_list[pkt.stream_index];
        out_stream = out_ctx->streams[pkt.stream_index];
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        av_interleaved_write_frame(out_ctx, &pkt);

        av_packet_unref(&pkt);
    }

    av_write_trailer(out_ctx);

    avformat_close_input(&state->av_format_ctx);
    if (out_ctx && !(out_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&out_ctx->pb);

    avformat_free_context(out_ctx);
    av_freep(&streams_list);
    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::transcodeAudio(const char* inFile, const char* outFile)
{
    return transcodeAudio(inFile, outFile, &m_mrState);
}

ErrorCode CMediaConverter::transcodeAudio(const char* inFile, const char* outFile, MediaReaderState* state)
{
    /***************************************
    *   Open Input
    ***************************************/
    //open input
    if (avformat_open_input(&state->av_format_ctx, inFile, nullptr, nullptr) < 0)
        return ErrorCode::FMT_UNOPENED;

    if (avformat_find_stream_info(state->av_format_ctx, nullptr) < 0)
        return ErrorCode::NO_STREAMS;

    const AVCodec* in_codec = avcodec_find_decoder(state->av_format_ctx->streams[0]->codecpar->codec_id);
    if (!in_codec)
        return ErrorCode::NO_CODEC;

    state->audio_codec_ctx = avcodec_alloc_context3(in_codec);
    if (!state->audio_codec_ctx)
        return ErrorCode::NO_CODEC_CTX;

    if (avcodec_parameters_to_context(state->audio_codec_ctx, state->av_format_ctx->streams[0]->codecpar) < 0)
        return ErrorCode::NO_CODEC_CTX;

    if (avcodec_open2(state->audio_codec_ctx, in_codec, nullptr) < 0)
        return ErrorCode::CODEC_UNOPENED;

    /************************************
    * Open Output
    ************************************/
    initAudioEncoder(outFile);

    /************************************
    *Loop through input samples and write
    ************************************/
    state->av_packet = av_packet_alloc();
    state->audio_stream_index = 0;
    auto ret = ErrorCode::SUCCESS;
    while (ret != ErrorCode::FILE_EOF)
    {
        ret = encodeAudioFrame(state);
    }
    state->audioEncoder.CompleteEncoding();
    return ErrorCode::SUCCESS;
}

void CMediaConverter::avlog_cb(void* data, int level, const char* szFmt, va_list varg)
{
    char line[256];
    int prefix = 0;
    av_log_format_line(data, level, szFmt, varg, line, 256, &prefix);
    std::ofstream outfile("log.txt", std::ios_base::app);
    outfile << line << std::endl;
    outfile.close();
}

ErrorCode CMediaConverter::initEncoders(const char* outfile)
{
    return initEncoders(outfile, &m_mrState);
}

ErrorCode CMediaConverter::initEncoders(const char* outfile, MediaReaderState* state)
{
    ErrorCode vRet = state->videoEncoder.Init(VideoEncoderDetails(), outfile);
    ErrorCode aRet = state->audioEncoder.Init(AudioEncoderDetails(), outfile);
    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::initVideoEncoder(const char* outfile)
{
    return initVideoEncoder(outfile, &m_mrState);
}

ErrorCode CMediaConverter::initVideoEncoder(const char* outfile, MediaReaderState* state)
{
    VideoEncoderDetails vDetails(1920, 1080, { 30, 1 }, { 1, 90000 }, { 1, 1 }, 15, 0, AV_PIX_FMT_YUV420P, 1920, 1080, AV_PIX_FMT_RGBA, 400000);
    return state->videoEncoder.Init(vDetails, outfile);
}

ErrorCode CMediaConverter::convertVideoBuffer(std::vector<uint8_t> vec , int w, int h)
{
    return convertVideoBuffer(&m_mrState, vec, w, h);
}

ErrorCode CMediaConverter::convertVideoBuffer(MediaReaderState* state, std::vector<uint8_t> vec, int w, int h)
{
    if (vec.empty())
        return ErrorCode::NO_DATA_AVAIL;

    if (!state->videoEncoder.encodeFrame)
    {
        state->videoEncoder.encodeFrame = av_frame_alloc();
        state->videoEncoder.encodeFrame->format = AV_PIX_FMT_YUV420P;
        state->videoEncoder.encodeFrame->width = w;
        state->videoEncoder.encodeFrame->height = h;
        if (av_frame_get_buffer(state->videoEncoder.encodeFrame, 0) < 0)
            return ErrorCode::NO_FRAME;
    }

    if (!state->videoEncoder.encodeScalerCtx)
        return ErrorCode::NO_SCALER;

    uint8_t* inData[4] = { &vec[0], nullptr, nullptr, nullptr };
    int inLinesize[4] = { 4 * w, 0, 0, 0 };
   
    int ret = sws_scale(state->videoEncoder.encodeScalerCtx, inData, inLinesize, 0, h, state->videoEncoder.encodeFrame->data, state->videoEncoder.encodeFrame->linesize);
    state->videoEncoder.encodeFrame->pts = state->videoEncoder.nextEncodePts;
    state->videoEncoder.nextEncodePts += 3000;
    return ret > 0 ? ErrorCode::SUCCESS : ErrorCode::BAD_SCALE;
}

ErrorCode CMediaConverter::encodeVideoFrame(bool drainBuffer)
{
    return encodeVideoFrame(&m_mrState, drainBuffer);
}

ErrorCode CMediaConverter::encodeVideoFrame(MediaReaderState* state, bool drainBuffer)
{
    return state->videoEncoder.EncodeFrame(0, drainBuffer);
}

ErrorCode CMediaConverter::initAudioEncoder(const char* outfile)
{
    return initAudioEncoder(outfile, &m_mrState);
}

ErrorCode CMediaConverter::initAudioEncoder(const char* outfile, MediaReaderState* state)
{
    AudioEncoderDetails aDetails(2, AV_CH_LAYOUT_STEREO, 44100, AV_SAMPLE_FMT_NONE, 2, 44100, AV_SAMPLE_FMT_FLTP, 96000);
    return state->audioEncoder.Init(aDetails, outfile);
}

ErrorCode CMediaConverter::encodeAudioFrame()
{
    return encodeAudioFrame(&m_mrState);
}

ErrorCode CMediaConverter::encodeAudioFrame(MediaReaderState* state)
{
    ErrorCode ret = fillAudioFifo(state);
    if(ret == ErrorCode::SUCCESS)
        return drainAudioFifo(state, ret == ErrorCode::FILE_EOF);
    return ret;
}

ErrorCode CMediaConverter::fillAudioFifo()
{
    return fillAudioFifo(&m_mrState);
}

ErrorCode CMediaConverter::fillAudioFifo(MediaReaderState* state)
{
    while (av_audio_fifo_size(state->audioEncoder.encodeAudioFifo) < state->audioEncoder.encodeCodecCtx->frame_size)
    {
        /*********************************
        * Read, Decode, Convert and Store
        *********************************/
        uint8_t** converted_input_samples = nullptr;
        {
            //allocate memory for samples
            if (!(converted_input_samples = (uint8_t**)calloc(state->audioEncoder.encodeCodecCtx->channels, sizeof(*converted_input_samples))))
                return ErrorCode::BAD_WRITE;
            int err = av_samples_alloc(converted_input_samples, nullptr, state->audioEncoder.encodeCodecCtx->channels, state->av_frame->nb_samples, state->audioEncoder.encodeCodecCtx->sample_fmt, 0);
            if (err < 0)
                return ErrorCode::BAD_WRITE;

            //convert samples
            if (swr_convert(state->audioEncoder.encodeResamplerCtx, converted_input_samples, state->av_frame->nb_samples, (const uint8_t**)state->av_frame->extended_data, state->av_frame->nb_samples) < 0)
                return ErrorCode::NO_SWR_CONVERT;

            //fill the fifo buffer
            if (av_audio_fifo_realloc(state->audioEncoder.encodeAudioFifo, av_audio_fifo_size(state->audioEncoder.encodeAudioFifo) + state->av_frame->nb_samples) < 0)
                return ErrorCode::BAD_WRITE;

            if (av_audio_fifo_write(state->audioEncoder.encodeAudioFifo, (void**)converted_input_samples, state->av_frame->nb_samples) < state->av_frame->nb_samples)
                return ErrorCode::BAD_WRITE;
        }
        av_freep(&converted_input_samples[0]);
        if(converted_input_samples)
            free(converted_input_samples);
    }
    return ErrorCode::SUCCESS;
}

ErrorCode CMediaConverter::drainAudioFifo(bool needsFlush)
{
    return drainAudioFifo(&m_mrState, needsFlush);
}

ErrorCode CMediaConverter::drainAudioFifo(MediaReaderState* state, bool needsFlush)
{
    while (av_audio_fifo_size(state->audioEncoder.encodeAudioFifo) >= state->audioEncoder.encodeCodecCtx->frame_size || (needsFlush && av_audio_fifo_size(state->audioEncoder.encodeAudioFifo) > 0))
    {
        int framesize = FFMIN(av_audio_fifo_size(state->audioEncoder.encodeAudioFifo), state->audioEncoder.encodeCodecCtx->frame_size);
        bool data_written = false;

        //Init output frame
        state->audioEncoder.encodeFrame = av_frame_alloc();
        if (!state->audioEncoder.encodeFrame)
            return ErrorCode::NO_FRAME;
        state->audioEncoder.encodeFrame->nb_samples = framesize;
        state->audioEncoder.encodeFrame->channel_layout = state->audioEncoder.encodeCodecCtx->channel_layout;
        state->audioEncoder.encodeFrame->format = state->audioEncoder.encodeCodecCtx->sample_fmt;
        state->audioEncoder.encodeFrame->sample_rate = state->audioEncoder.encodeCodecCtx->sample_rate;
        if (av_frame_get_buffer(state->audioEncoder.encodeFrame, 0) < 0)
            return ErrorCode::NO_FRAME;

        //read the fifo into the frame
        if (av_audio_fifo_read(state->audioEncoder.encodeAudioFifo, (void**)state->audioEncoder.encodeFrame->data, framesize) < framesize)
            return ErrorCode::NO_FRAME;

        //init the encoded packet
        if (!state->audioEncoder.encodePkt)
            state->audioEncoder.encodePkt = av_packet_alloc();
        //av_init_packet(state->audioEncoder.encodePkt);
        state->audioEncoder.encodePkt->data = nullptr;
        state->audioEncoder.encodePkt->size = 0;
        state->audioEncoder.encodeFrame->pts = state->audioEncoder.nextEncodePts;
        state->audioEncoder.nextEncodePts += state->audioEncoder.encodeFrame->nb_samples;

        state->audioEncoder.EncodeFrame(0);

        av_frame_unref(state->audioEncoder.encodeFrame);
        av_frame_free(&state->audioEncoder.encodeFrame);
        av_packet_unref(state->audioEncoder.encodePkt);
        av_packet_free(&state->audioEncoder.encodePkt);

        if (needsFlush) //flush out and encode any samples still remaining
        {
            ErrorCode ret = state->audioEncoder.EncodeFrame(0, nullptr);
            while (ret == ErrorCode::SUCCESS)
                ret = state->audioEncoder.EncodeFrame(0, nullptr);
        }
    }
    return ErrorCode::SUCCESS;
}

// ErrorCode CMediaConverter::encodeAudioToneMP2()
// {
//     return encodeAudioToneMP2(&m_mrState);
// }

// ErrorCode CMediaConverter::encodeAudioToneMP2(MediaReaderState* state)
// {
//     const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
//     if (!codec)
//         return ErrorCode::NO_CODEC;

//     state->audioEncoder.encodeCodecCtx = avcodec_alloc_context3(codec);
//     if (!state->audioEncoder.encodeCodecCtx)
//         return ErrorCode::NO_CODEC_CTX;

//     state->audioEncoder.encodeCodecCtx->bit_rate = 160000;
//     state->audioEncoder.encodeCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
//     state->audioEncoder.encodeCodecCtx->sample_rate = 44100;
//     state->audioEncoder.encodeCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
//     state->audioEncoder.encodeCodecCtx->channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

//     if (avcodec_open2(state->audioEncoder.encodeCodecCtx, codec, nullptr) < 0)
//         return ErrorCode::CODEC_UNOPENED;

//     const char* filename = "tone.mp2";
//     FILE* f = nullptr;
//     fopen_s(&f, filename, "wb");
//     if (!f)
//         return ErrorCode::NO_OUTPUT_FILE;

//     state->audioEncoder.encodePkt = av_packet_alloc();
//     if (!state->audioEncoder.encodePkt)
//         return ErrorCode::NO_PACKET;
    
//     if(!state->av_frame)
//         state->av_frame = av_frame_alloc();
//     if (!state->av_frame)
//         return ErrorCode::NO_FRAME;

//     state->av_frame->nb_samples = state->audioEncoder.encodeCodecCtx->frame_size;
//     state->av_frame->format = state->audioEncoder.encodeCodecCtx->sample_fmt;
//     state->av_frame->channel_layout = state->audioEncoder.encodeCodecCtx->channel_layout;

//     if (av_frame_get_buffer(state->av_frame, 0) < 0)
//         return ErrorCode::NO_FRAME;

//     auto encode = [](AVCodecContext* ctx, AVFrame* frame, AVPacket* pkt, FILE* output)
//     {
//         int ret = avcodec_send_frame(ctx, frame);
//         if (ret < 0)
//             return ErrorCode::NO_FRAME;

//         while (ret >= 0)
//         {
//             ret = avcodec_receive_packet(ctx, pkt);
//             if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
//                 return ErrorCode::AGAIN;
//             else if (ret < 0)
//                 return ErrorCode::FFMPEG_ERROR;

//             fwrite(pkt->data, 1, pkt->size, output);
//             av_packet_unref(pkt);
//         }
//         return ErrorCode::SUCCESS;
//     };

//     /* encode a single tone sound */
//     double t = 0.0;
//     double tincr = 2 * M_PI * 440.0 / state->audioEncoder.encodeCodecCtx->sample_rate;
//     int highLevel = 0;
//     for (int i = 0; i < 400; i++) 
//     {
//         /* make sure the frame is writable -- makes a copy if the encoder
//          * kept a reference internally */
//         if (av_frame_make_writable(state->av_frame) < 0)
//             return ErrorCode::NO_FRAME;

//         uint16_t* samples = (uint16_t*)state->av_frame->data[0];
//         for (int j = 0; j < state->audioEncoder.encodeCodecCtx->frame_size; j++) 
//         {
//             samples[2 * j] = (highLevel < 5) ? (int)(sin(t) * 10000) : 0;
//             for (int k = 1; k < state->audioEncoder.encodeCodecCtx->channels; k++)
//             {
//                 samples[2 * j + k] = samples[2 * j];
//             }
//             t += tincr;
//         }
//         ++highLevel;
//         if (highLevel >= 20)
//             highLevel = 0;
//         encode(state->audioEncoder.encodeCodecCtx, state->av_frame, state->audioEncoder.encodePkt, f);
//     }
//     /* flush the encoder */
//     encode(state->audioEncoder.encodeCodecCtx, nullptr, state->audioEncoder.encodePkt, f);
//     fclose(f);
//     av_frame_free(&state->av_frame);
//     av_packet_free(&state->audioEncoder.encodePkt);
//     avcodec_free_context(&state->audioEncoder.encodeCodecCtx);

//     return ErrorCode::SUCCESS;
// }

// ErrorCode CMediaConverter::createAudioToneFrame(float hertz, std::vector<AVFrame*>& toneFrames)
// {
//     return createAudioToneFrame(&m_mrState, hertz, toneFrames);
// }

// ErrorCode CMediaConverter::createAudioToneFrame(MediaReaderState* state, float hertz, std::vector<AVFrame*>& toneFrames)
// {
//     const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
//     if (!codec)
//         return ErrorCode::NO_CODEC;

//     state->audioEncoder.encodeCodecCtx = avcodec_alloc_context3(codec);
//     if (!state->audioEncoder.encodeCodecCtx)
//         return ErrorCode::NO_CODEC_CTX;

//     state->audioEncoder.encodeCodecCtx->bit_rate = 160000;
//     state->audioEncoder.encodeCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
//     state->audioEncoder.encodeCodecCtx->sample_rate = 44100;
//     state->audioEncoder.encodeCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
//     state->audioEncoder.encodeCodecCtx->channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

//     if (avcodec_open2(state->audioEncoder.encodeCodecCtx, codec, nullptr) < 0)
//         return ErrorCode::CODEC_UNOPENED;

//     const char* filename = "tone.mp2";
//     FILE* f = nullptr;
//     fopen_s(&f, filename, "wb");
//     if (!f)
//         return ErrorCode::NO_OUTPUT_FILE;

//     state->audioEncoder.encodePkt = av_packet_alloc();
//     if (!state->audioEncoder.encodePkt)
//         return ErrorCode::NO_PACKET;

//     if (!state->av_frame)
//         state->av_frame = av_frame_alloc();
//     if (!state->av_frame)
//         return ErrorCode::NO_FRAME;

//     state->av_frame->nb_samples = state->audioEncoder.encodeCodecCtx->frame_size;
//     state->av_frame->format = state->audioEncoder.encodeCodecCtx->sample_fmt;
//     state->av_frame->channel_layout = state->audioEncoder.encodeCodecCtx->channel_layout;

//     if (av_frame_get_buffer(state->av_frame, 0) < 0)
//         return ErrorCode::NO_FRAME;

//     auto encode = [](AVCodecContext* ctx, AVFrame* frame, AVPacket* pkt, FILE* output)
//     {
//         int ret = avcodec_send_frame(ctx, frame);
//         if (ret < 0)
//             return ErrorCode::NO_FRAME;

//         while (ret >= 0)
//         {
//             ret = avcodec_receive_packet(ctx, pkt);
//             if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
//                 return ErrorCode::AGAIN;
//             else if (ret < 0)
//                 return ErrorCode::FFMPEG_ERROR;

//             fwrite(pkt->data, 1, pkt->size, output);
//             av_packet_unref(pkt);
//         }
//         return ErrorCode::SUCCESS;
//     };

//     /* encode a single tone sound */
//     double t = 0;
//     double tincr = 2 * M_PI * hertz / state->audioEncoder.encodeCodecCtx->sample_rate;

//     for (int i = 0; i < 5; i++)
//     {
//         /* make sure the frame is writable -- makes a copy if the encoder
//          * kept a reference internally */
//         if (av_frame_make_writable(state->av_frame) < 0)
//             return ErrorCode::NO_FRAME;

//         uint16_t* samples = (uint16_t*)state->av_frame->data[0];
//         for (int j = 0; j < state->audioEncoder.encodeCodecCtx->frame_size; j++)
//         {
//             samples[2 * j] = (int)(sin(t) * 10000);
//             for (int k = 1; k < state->audioEncoder.encodeCodecCtx->channels; k++)
//             {
//                 samples[2 * j + k] = samples[2 * j];
//             }
//             t += tincr;
//         }
//         encode(state->audioEncoder.encodeCodecCtx, state->av_frame, state->audioEncoder.encodePkt, f);
//     }
//     /* flush the encoder */
//     encode(state->audioEncoder.encodeCodecCtx, nullptr, state->audioEncoder.encodePkt, f);
//     fclose(f);
//     av_frame_free(&state->av_frame);
//     av_packet_free(&state->audioEncoder.encodePkt);
//     avcodec_free_context(&state->audioEncoder.encodeCodecCtx);

//     auto ret = openMediaReader("tone.mp2");
//     if (ret != ErrorCode::SUCCESS)
//         return ret;
//     while(readAudioFrame() == ErrorCode::SUCCESS)
//         toneFrames.push_back(av_frame_clone(m_mrState.av_frame));

//     closeMediaReader();
//     std::remove("tone.mp2");
//     return ErrorCode::SUCCESS;
// }