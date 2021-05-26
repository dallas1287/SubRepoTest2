#include <emscripten.h>
#include <emscripten/bind.h>
#include <inttypes.h>
#include "DecoderLib/decoder.h"

#include <string>
#include <vector>

using namespace emscripten;

typedef struct Response
{
  std::string format;
  int duration;
  int streams;
} Response;

Response run(std::string filename)
{
  // Open the file and read header.
  std::string name = GetCodecName();

  // Initialize response struct with format data.
  Response r = {
      .format = name,
      .duration = 22,
      .streams = 33,
  };

  return r;
}

EMSCRIPTEN_BINDINGS(structs)
{
  emscripten::value_object<Response>("Response")
      .field("format", &Response::format)
      .field("duration", &Response::duration)
      .field("streams", &Response::streams);
  function("run", &run);
}
