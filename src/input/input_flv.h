#include "input.h"
#include <mist/urireader.h>
#include <mist/dtsc.h>
#include <mist/flv_tag.h>

namespace Mist{
  class InputFLV : public Input, public Util::DataCallback{
  public:
    InputFLV(Util::Config *cfg);
    ~InputFLV();
    bool needsLock();
    bool needHeader(){return (config->getBool("realtime") || needsLock()) && !readExistingHeader();}
    virtual void dataCallback(const char *ptr, size_t size);
    virtual size_t getDataCallbackPos() const;
    bool openStreamSource(){return true;}
    virtual bool publishesTracks(){return false;}

  protected:
    // Private Functions
    bool checkArguments();
    bool preRun();
    bool readHeader();
    void getNext(size_t idx = INVALID_TRACK_ID);
    void seek(uint64_t seekTime, size_t idx = INVALID_TRACK_ID);
    bool keepRunning(bool updateActCtr = true);
    FLV::Tag tmpTag;
    uint64_t lastModTime;

    HTTP::URIReader inFile;
    Util::ResizeablePointer readBuffer;
    uint64_t readBufferOffset;
    uint64_t readPos;
    bool slideWindowTo(size_t seekPos, size_t seekLen = 0);

    virtual size_t streamByteCount(){
      return totalBytes;
    }; // For live streams: to update the stats with correct values.
    size_t totalBytes;
  };
}// namespace Mist

typedef Mist::InputFLV mistIn;
