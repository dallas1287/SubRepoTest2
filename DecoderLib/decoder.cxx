#include "decoder.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
}

std::string GetCodecName()
{
    std::string version(avcodec_get_name(AV_CODEC_ID_H264));
    return version;
}