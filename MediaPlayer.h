#include <emscripten.h>
#include <emscripten/bind.h>
#include <inttypes.h>

using namespace emscripten;

typedef struct Response
{
  std::string format;
  int duration;
  int framePts;
  bool isOpen;
} Response;

class MediaPlayer
{
public:
    MediaPlayer();
    ~MediaPlayer();
    
    Response OpenFile(std::string filename);
    Response Play();
    void Pause();
    void FrameStep();
    void RevFrameStep();
    void CountStep();
    void RevCountStep();
};