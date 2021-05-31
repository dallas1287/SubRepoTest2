#include <emscripten.h>
#include <emscripten/bind.h>
#include <inttypes.h>
#include <vector>

using namespace emscripten;

typedef struct Response
{
  std::string format;
  int duration;
  int framePts;
  bool isOpen;
} Response;

typedef struct BufferResponse
{
  long framePts;
  int width;
  int height;
  std::vector<unsigned char> buffer;
} BufferResponse;

class MediaPlayer
{
public:
    MediaPlayer();
    ~MediaPlayer();
    
    Response OpenFile(std::string filename);
    BufferResponse RetrieveFrame();
    Response SeekTo(long pts);
};