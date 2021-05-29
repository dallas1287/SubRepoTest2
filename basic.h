#include <string>

typedef struct Response
{
  std::string format;
  int duration;
  int streams;
} Response;

class Decoder
{
public:
    Decoder();
    ~Decoder();

    Response run(std::string filename);
};