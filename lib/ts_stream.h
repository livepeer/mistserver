#include "adts.h"
#include "h265.h"
#include "ts_packet.h"
#include "tinythread.h"
#include <deque>
#include <map>
#include <set>

#include "shared_memory.h"
#define TS_PTS_ROLLOVER 95443718 

namespace TS{
  enum codecType{H264 = 0x1B, AAC = 0x0F, AC3 = 0x81, MP3 = 0x03, H265 = 0x24, ID3 = 0x15, MPEG2 = 0x02, MP2 = 0x03};

  class ADTSRemainder{
  private:
    char *data;
      uint64_t max;
      uint64_t now;
      uint64_t len;
    uint64_t bpos;

  public:
      void setRemainder(const aac::adts & p, const void * source, uint32_t avail, uint64_t bPos);

    ADTSRemainder();
    ~ADTSRemainder();
      uint64_t getLength();
    uint64_t getBpos();
      uint64_t getTodo();
    char *getData();

    void append(const char *p, uint32_t pLen);
    bool isComplete();
    void clear();
  };

  class Stream{
  public:
    Stream(bool _threaded = false);
    ~Stream();
      void add(char * newPack, uint64_t bytePos = 0);
      void add(Packet & newPack, uint64_t bytePos = 0);
      void parse(Packet & newPack, uint64_t bytePos);
      void parse(char * newPack, uint64_t bytePos);
      void parse(size_t tid);
      void parseNal(size_t tid, const char * pesPayload, const char * packetPtr, bool & isKeyFrame);
    bool hasPacketOnEachTrack() const;
      bool hasPacket(size_t tid) const;
    bool hasPacket() const;
    void getPacket(size_t tid, DTSC::Packet &pack);
    uint32_t getEarliestPID();
    void getEarliestPacket(DTSC::Packet &pack);
    void initializeMetadata(DTSC::Meta &meta, size_t tid = 0, size_t mappingId = 0);
    void partialClear();
    void clear();
    void finish();
      void eraseTrack(size_t tid);
      bool isDataTrack(size_t tid) const;
      void parseBitstream(size_t tid, const char * pesPayload, uint64_t realPayloadSize,
                        uint64_t timeStamp, int64_t timeOffset, uint64_t bPos, bool alignment);
      std::set<size_t> getActiveTracks();

      void setLastms(size_t tid, uint64_t timestamp);
  private:
      uint64_t lastPAT;
    ProgramAssociationTable associationTable;
      std::map<size_t, ADTSRemainder> remainders;

      std::map<size_t, uint64_t> lastPMT;
      std::map<size_t, ProgramMappingTable> mappingTable;

      std::map<size_t, std::deque<Packet> > pesStreams;
      std::map<size_t, std::deque<uint64_t> > pesPositions;
      std::map<size_t, std::deque<DTSC::Packet> > outPackets;
      std::map<size_t, DTSC::Packet> buildPacket;
      std::map<size_t, uint32_t> pidToCodec;
      std::map<size_t, aac::adts > adtsInfo;
      std::map<size_t, std::string > spsInfo;
      std::map<size_t, std::string > ppsInfo;
      std::map<size_t, h265::initData > hevcInfo;
      std::map<size_t, std::string> metaInit;
      std::map<size_t, std::string> descriptors;
      std::map<size_t, uint32_t> seenUnitStart;
      std::map<size_t, std::string> mpeg2SeqHdr;
      std::map<size_t, std::string> mpeg2SeqExt;
      std::map<size_t, std::string> mp2Hdr;

      std::map<size_t, size_t> rolloverCount;
      std::map<size_t, unsigned long long> lastms;

    mutable tthread::recursive_mutex tMutex;

    bool threaded;

      std::set<size_t> pmtTracks;

      void parsePES(size_t tid, bool finished = false);
  };
}
