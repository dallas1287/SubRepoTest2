#include "decoder.h"
#include "MediaConverter.h"

static CMediaConverter mc;

int OpenFile(std::string filename)
{
    mc.openMediaReader(filename.c_str());
    return mc.MRState().video_stream_index;
}

std::string GetCodecName()
{
    if(mc.MRState().CodecName() == nullptr)
        return std::string("No codec name available");
    
    return std::string(mc.MRState().CodecName());
}

int GetFrameCount()
{
    return mc.MRState().VideoFrameCt();
}