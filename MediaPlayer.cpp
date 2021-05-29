#include "MediaPlayer.h"

#include <string>
#include <vector>

MediaPlayer::MediaPlayer()
{
}

MediaPlayer::~MediaPlayer()
{
}

int MediaPlayer::OpenFile(std::string filename)
{ 
  m_mediaConverter.openMediaReader(filename.c_str());
  return m_mediaConverter.MRState().IsOpened() ? m_mediaConverter.MRState().video_stream_index : -1; 
}

std::string MediaPlayer::GetCodecName()
{
  if(!m_mediaConverter.MRState().IsOpened())
    return std::string("not opened");

  return std::string(m_mediaConverter.MRState().CodecName());
}

int MediaPlayer::GetVideoFrameCount()
{
  if(!m_mediaConverter.MRState().IsOpened())
    return -1;

  return m_mediaConverter.MRState().VideoFrameCt();
}

void MediaPlayer::Play()
{

}

void MediaPlayer::Pause()
{

}

void MediaPlayer::FrameStep()
{

}

void MediaPlayer::RevFrameStep()
{

}

void MediaPlayer::CountStep()
{

}

void MediaPlayer::RevCountStep()
{

}

Response MediaPlayer::run(std::string filename)
{
  // Open the file and read header.
  int streamIndex = OpenFile(filename);
  std::string name = GetCodecName();
  int frameCt = GetVideoFrameCount();
  // Initialize response struct with format data.
  Response r = {
      .format = name,
      .duration = frameCt,
      .streams = streamIndex
  };

  return r;
}

EMSCRIPTEN_BINDINGS(MediaPlayerModule)
{
    emscripten::value_object<Response>("Response")
      .field("format", &Response::format)
      .field("duration", &Response::duration)
      .field("streams", &Response::streams);
      
  class_<MediaPlayer>("MediaPlayer")
    .constructor()
    .function("run", &MediaPlayer::run);
}
