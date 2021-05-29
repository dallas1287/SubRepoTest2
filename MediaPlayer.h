#include <emscripten.h>
#include <emscripten/bind.h>
#include <inttypes.h>
#include "DecoderLib/MediaConverter.h"

using namespace emscripten;

typedef struct Response
{
  std::string format;
  int duration;
  int streams;
} Response;

class MediaPlayer
{
public:
    MediaPlayer();
    ~MediaPlayer();
    int OpenFile(std::string filename);
    std::string GetCodecName();
    int GetVideoFrameCount();
    void Play();
    void Pause();
    void FrameStep();
    void RevFrameStep();
    void CountStep();
    void RevCountStep();
    Response run(std::string filename);

private:
  CMediaConverter m_mediaConverter;
};