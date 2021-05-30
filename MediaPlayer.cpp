#include "MediaPlayer.h"
#include <vector>
#include <iostream>
#include "DecoderLib/MediaConverter.h"

static CMediaConverter m_mediaConverter;

MediaPlayer::MediaPlayer()
{
}

MediaPlayer::~MediaPlayer()
{
}

Response MediaPlayer::OpenFile(std::string filename)
{ 
  auto openRet = m_mediaConverter.openMediaReader(filename.c_str());
  bool openSuccess = openRet == ErrorCode::SUCCESS;
    // Open the file and read header.
  std::string name(m_mediaConverter.MRState().CodecName());
  int frameCt = m_mediaConverter.MRState().VideoFrameCt();
  bool isOpen = m_mediaConverter.MRState().IsOpened();
  int frameNum = m_mediaConverter.MRState().VideoFramePts();
  // Initialize response struct with format data.
  Response r = {
      .format = name,
      .duration = frameCt,
      .framePts = frameNum,
      .isOpen = isOpen
  };

  return r;
}

Response MediaPlayer::Play()
{
  if(!m_mediaConverter.MRState().IsOpened())
  {
    Response err = {
      .format = "file not opened",
      .duration = 0,
      .framePts = -1,
      .isOpen = false
    };
    return err;
  }  
  auto ret = m_mediaConverter.processVideoPacketsIntoFrames();
  int duration = m_mediaConverter.MRState().VideoDuration();
  int pts = m_mediaConverter.MRState().VideoFramePts();
  Response r = {
    .format = "processing frame",
    .duration = duration,
    .framePts = pts,
    .isOpen = m_mediaConverter.MRState().IsOpened()
  };
  return r;
}

void MediaPlayer::Pause()
{
  std::cout << "Media Player Pause" << std::endl;
}

void MediaPlayer::FrameStep()
{
  std::cout << "Media Player Frame Step" << std::endl;
}

void MediaPlayer::RevFrameStep()
{
  std::cout << "Media Player Reverse Frame Step" << std::endl;
}

void MediaPlayer::CountStep()
{

}

void MediaPlayer::RevCountStep()
{

}

EMSCRIPTEN_BINDINGS(MediaPlayerModule)
{
    emscripten::value_object<Response>("Response")
      .field("format", &Response::format)
      .field("duration", &Response::duration)
      .field("framePts", &Response::framePts)
      .field("isOpen", &Response::isOpen);
      
  class_<MediaPlayer>("MediaPlayer")
    .constructor()
    .function("OpenFile", &MediaPlayer::OpenFile)
    .function("Play", &MediaPlayer::Play)
    .function("Pause", &MediaPlayer::Pause)
    .function("FrameStep", &MediaPlayer::FrameStep)
    .function("RevFrameStep", &MediaPlayer::RevFrameStep);
}
