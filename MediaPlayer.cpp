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

BufferResponse MediaPlayer::RetrieveFrame()
{
  BufferResponse response;
  m_mediaConverter.readVideoFrame(response.buffer);
  response.framePts = m_mediaConverter.MRState().VideoFramePts();
  response.width = m_mediaConverter.MRState().VideoWidth();
  response.height = m_mediaConverter.MRState().VideoHeight();
  return response;
}

Response MediaPlayer::SeekTo(long pts)
{
  std::cout << "Seeking to: " << std::to_string(pts) << std::endl;
}

EMSCRIPTEN_BINDINGS(MediaPlayerModule)
{
    emscripten::value_object<Response>("Response")
      .field("format", &Response::format)
      .field("duration", &Response::duration)
      .field("framePts", &Response::framePts)
      .field("isOpen", &Response::isOpen);
    
    emscripten::value_object<BufferResponse>("BufferResponse")
      .field("framePts", &BufferResponse::framePts)
      .field("width", &BufferResponse::width)
      .field("height", &BufferResponse::height)
      .field("buffer", &BufferResponse::buffer);
      
    class_<MediaPlayer>("MediaPlayer")
    .constructor()
    .function("OpenFile", &MediaPlayer::OpenFile)
    .function("RetrieveFrame", &MediaPlayer::RetrieveFrame)
    .function("SeekTo", &MediaPlayer::SeekTo);

    register_vector<unsigned char>("VideoBuffer");
}
