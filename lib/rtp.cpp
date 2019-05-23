#include "rtp.h"
#include "adts.h"
#include "bitfields.h"
#include "defines.h"
#include "encode.h"
#include "h264.h"
#include "mpeg.h"
#include "sdp.h"
#include "timing.h"
#include <arpa/inet.h>

namespace RTP{
  double Packet::startRTCP = 0;
  unsigned int MAX_SEND = 1500 - 28;

  unsigned int Packet::getHsize() const{
    unsigned int r = 12 + 4 * getContribCount();
    if (getExtension()){
      r += (1+Bit::btohs(data+r+2)) * 4;
    }
    return r;
  }

  unsigned int Packet::getPayloadSize() const{
    return maxDataLen - getHsize() - (getPadding() ? data[maxDataLen-1] : 0);
  }

  char *Packet::getPayload() const{return data + getHsize();}

  unsigned int Packet::getVersion() const{return (data[0] >> 6) & 0x3;}

  unsigned int Packet::getPadding() const{return (data[0] >> 5) & 0x1;}

  unsigned int Packet::getExtension() const{return (data[0] >> 4) & 0x1;}

  unsigned int Packet::getContribCount() const{return (data[0]) & 0xE;}

  unsigned int Packet::getMarker() const{return (data[1] >> 7) & 0x1;}

  unsigned int Packet::getPayloadType() const{return (data[1]) & 0x7F;}

  unsigned int Packet::getSequence() const{return (((((unsigned int)data[2]) << 8) + data[3]));}

  uint32_t Packet::getTimeStamp() const{return Bit::btohl(data + 4);}

  unsigned int Packet::getSSRC() const{return ntohl(*((unsigned int *)(data + 8)));}

  char *Packet::getData(){return data + 8 + 4 * getContribCount() + getExtension();}

  void Packet::setTimestamp(uint32_t t){Bit::htobl(data+4, t);}

  void Packet::setSequence(unsigned int seq){*((short *)(data + 2)) = htons(seq);}

  void Packet::setSSRC(unsigned long ssrc){*((int *)(data + 8)) = htonl(ssrc);}

  void Packet::increaseSequence(){*((short *)(data + 2)) = htons(getSequence() + 1);}

  void Packet::sendH264(void *socket, void callBack(void *, char *, unsigned int, unsigned int),
                        const char *payload, unsigned int payloadlen, unsigned int channel, bool lastOfAccesUnit){
    if ((payload[0] & 0x1F) == 12){return;}
    /// \todo This function probably belongs in DMS somewhere.
    if (payloadlen + getHsize() + 2 <= maxDataLen){
      if (lastOfAccesUnit){
        data[1] |= 0x80; // setting the RTP marker bit to 1
      }
      uint8_t nal_type = (payload[0] & 0x1F);
      if (nal_type < 1 || nal_type > 5){
        data[1] &= ~0x80; // but not for non-vlc types
      }
      memcpy(data + getHsize(), payload, payloadlen);
      callBack(socket, data, getHsize() + payloadlen, channel);
      sentPackets++;
      sentBytes += payloadlen + getHsize();
      increaseSequence();
    }else{
      data[1] &= 0x7F; // setting the RTP marker bit to 0
      unsigned int sent = 0;
      unsigned int sending =
          maxDataLen - getHsize() - 2; // packages are of size MAX_SEND, except for the final one
      char initByte = (payload[0] & 0xE0) | 0x1C;
      char serByte = payload[0] & 0x1F; // ser is now 000
      data[getHsize()] = initByte;
      while (sent < payloadlen){
        if (sent == 0){
          serByte |= 0x80; // set first bit to 1
        }else{
          serByte &= 0x7F; // set first bit to 0
        }
        if (sent + sending >= payloadlen){
          // last package
          serByte |= 0x40;
          sending = payloadlen - sent;
          if (lastOfAccesUnit){
            data[1] |= 0x80; // setting the RTP marker bit to 1
          }
        }
        data[getHsize() + 1] = serByte;
        memcpy(data + getHsize() + 2, payload + 1 + sent, sending);
        callBack(socket, data, getHsize() + 2 + sending, channel);
        sentPackets++;
        sentBytes += sending + getHsize() + 2;
        sent += sending;
        increaseSequence();
      }
    }
  }

  void Packet::sendVP8(void *socket, void callBack(void *, char *, unsigned int, unsigned int),
                       const char *payload, unsigned int payloadlen, unsigned int channel){

    bool isKeyframe = ((payload[0] & 0x01) == 0) ? true : false;
    bool isStartOfPartition = true;
    size_t chunkSize = MAX_SEND;
    size_t bytesWritten = 0;
    uint32_t headerSize = getHsize();

    while (payloadlen > 0){
      chunkSize = std::min<size_t>(1200, payloadlen);
      payloadlen -= chunkSize;

      data[1] =
          (0 != payloadlen) ? (data[1] & 0x7F) : (data[1] | 0x80); // marker bit, 1 for last chunk.
      data[headerSize] = 0x00;                                     // reset
      data[headerSize] |=
          (isStartOfPartition) ? 0x10 : 0x00; // first chunk is always start of a partition.
      data[headerSize] |=
          (isKeyframe)
              ? 0x00
              : 0x20; // non-reference frame. 0 = frame is needed, 1 = frame can be disgarded.

      memcpy(data + headerSize + 1, payload + bytesWritten, chunkSize);
      callBack(socket, data, headerSize + 1 + chunkSize, channel);
      increaseSequence();
      // INFO_MSG("chunk: %zu, sequence: %u", chunkSize, getSequence());

      isStartOfPartition = false;
      bytesWritten += chunkSize;
      sentBytes += headerSize + 1 + chunkSize;
      sentPackets++;
    }
    // WARN_MSG("KEYFRAME: %c", (isKeyframe) ? 'y' : 'n');
  }

  void Packet::sendH265(void *socket, void callBack(void *, char *, unsigned int, unsigned int),
                        const char *payload, unsigned int payloadlen, unsigned int channel){
    /// \todo This function probably belongs in DMS somewhere.
    if (payloadlen + getHsize() + 3 <= maxDataLen){
      data[1] |= 0x80; // setting the RTP marker bit to 1
      memcpy(data + getHsize(), payload, payloadlen);
      callBack(socket, data, getHsize() + payloadlen, channel);
      sentPackets++;
      sentBytes += payloadlen + getHsize();
      increaseSequence();
    }else{
      data[1] &= 0x7F; // setting the RTP marker bit to 0
      unsigned int sent = 0;
      unsigned int sending =
          maxDataLen - getHsize() - 3; // packages are of size MAX_SEND, except for the final one
      char initByteA = (payload[0] & 0x81) | 0x62;
      char initByteB = payload[1];
      char serByte = (payload[0] & 0x7E) >> 1; // SE is now 00
      data[getHsize()] = initByteA;
      data[getHsize() + 1] = initByteB;
      while (sent < payloadlen){
        if (sent == 0){
          serByte |= 0x80; // set first bit to 1
        }else{
          serByte &= 0x7F; // set first bit to 0
        }
        if (sent + sending >= payloadlen){
          // last package
          serByte |= 0x40;
          sending = payloadlen - sent;
          data[1] |= 0x80; // setting the RTP marker bit to 1
        }
        data[getHsize() + 2] = serByte;
        memcpy(data + getHsize() + 3, payload + 2 + sent, sending);
        callBack(socket, data, getHsize() + 3 + sending, channel);
        sentPackets++;
        sentBytes += sending + getHsize() + 3;
        sent += sending;
        increaseSequence();
      }
    }
  }

  void Packet::sendMPEG2(void *socket, void callBack(void *, char *, unsigned int, unsigned int),
                         const char *payload, unsigned int payloadlen, unsigned int channel){
    /// \todo This function probably belongs in DMS somewhere.
    if (payloadlen + getHsize() + 4 <= maxDataLen){
      data[1] |= 0x80; // setting the RTP marker bit to 1
      Mpeg::MPEG2Info mInfo = Mpeg::parseMPEG2Headers(payload, payloadlen);
      MPEGVideoHeader mHead(data + getHsize());
      mHead.clear();
      mHead.setTempRef(mInfo.tempSeq);
      mHead.setPictureType(mInfo.frameType);
      if (mInfo.isHeader){mHead.setSequence();}
      mHead.setBegin();
      mHead.setEnd();
      memcpy(data + getHsize() + 4, payload, payloadlen);
      callBack(socket, data, getHsize() + payloadlen + 4, channel);
      sentPackets++;
      sentBytes += payloadlen + getHsize() + 4;
      increaseSequence();
    }else{
      data[1] &= 0x7F; // setting the RTP marker bit to 0
      unsigned int sent = 0;
      unsigned int sending =
          maxDataLen - getHsize() - 4; // packages are of size MAX_SEND, except for the final one
      Mpeg::MPEG2Info mInfo;
      MPEGVideoHeader mHead(data + getHsize());
      while (sent < payloadlen){
        mHead.clear();
        if (sent + sending >= payloadlen){
          mHead.setEnd();
          sending = payloadlen - sent;
          data[1] |= 0x80; // setting the RTP marker bit to 1
        }
        Mpeg::parseMPEG2Headers(payload, sent + sending, mInfo);
        mHead.setTempRef(mInfo.tempSeq);
        mHead.setPictureType(mInfo.frameType);
        if (sent == 0){
          if (mInfo.isHeader){mHead.setSequence();}
          mHead.setBegin();
        }
        memcpy(data + getHsize() + 4, payload + sent, sending);
        callBack(socket, data, getHsize() + 4 + sending, channel);
        sentPackets++;
        sentBytes += sending + getHsize() + 4;
        sent += sending;
        increaseSequence();
      }
    }
  }

  void Packet::sendData(void *socket, void callBack(void *, char *, unsigned int, unsigned int),
                        const char *payload, unsigned int payloadlen, unsigned int channel,
                        std::string codec){
    if (codec == "H264"){
      unsigned long sent = 0;
      while (sent < payloadlen){
        unsigned long nalSize = ntohl(*((unsigned long *)(payload + sent)));
        sendH264(socket, callBack, payload + sent + 4, nalSize, channel, (sent + nalSize + 4) >= payloadlen ? true : false);
        sent += nalSize + 4;
      }
      return;
    }
    if (codec == "VP8"){
      sendVP8(socket, callBack, payload, payloadlen, channel);
      return;
    }
    if (codec == "HEVC"){
      unsigned long sent = 0;
      while (sent < payloadlen){
        unsigned long nalSize = ntohl(*((unsigned long *)(payload + sent)));
        sendH265(socket, callBack, payload + sent + 4, nalSize, channel);
        sent += nalSize + 4;
      }
      return;
    }
    if (codec == "MPEG2"){
      sendMPEG2(socket, callBack, payload, payloadlen, channel);
      return;
    }
    /// \todo This function probably belongs in DMS somewhere.
    data[1] |= 0x80; // setting the RTP marker bit to 1
    long offsetLen = 0;
    if (codec == "AAC"){
      *((long *)(data + getHsize())) = htonl(((payloadlen << 3) & 0x0010fff8) | 0x00100000);
      offsetLen = 4;
    }else if (codec == "MP3" || codec == "MP2"){
      // See RFC 2250, "MPEG Audio-specific header"
      *((long *)(data + getHsize())) = 0; // this is MBZ and Frag_Offset, which are always 0
      if (payload[0] != 0xFF){FAIL_MSG("MP2/MP3 data does not start with header?");}
      offsetLen = 4;
    }else if (codec == "AC3"){
      *((short *)(data + getHsize())) = htons(0x0001); // this is 6 bits MBZ, 2 bits FT = 0 = full
                                                       // frames and 8 bits saying we send 1 frame
      offsetLen = 2;
    }
    if (maxDataLen < getHsize() + offsetLen + payloadlen){
      if (!managed){
        FAIL_MSG("RTP data too big for packet, not sending!");
        return;
      }
      uint32_t newMaxLen = getHsize() + offsetLen + payloadlen;
      char *newData = new char[newMaxLen];
      if (newData){
        memcpy(newData, data, maxDataLen);
        delete[] data;
        data = newData;
        maxDataLen = newMaxLen;
      }
    }
    memcpy(data + getHsize() + offsetLen, payload, payloadlen);
    callBack(socket, data, getHsize() + offsetLen + payloadlen, channel);
    sentPackets++;
    sentBytes += payloadlen + offsetLen + getHsize();
    increaseSequence();
  }

  void Packet::sendRTCP_SR(long long &connectedAt, void *socket, unsigned int tid,
                           DTSC::Meta &metadata,
                           void callBack(void *, char *, unsigned int, unsigned int)){
    char *rtcpData = (char *)malloc(32);
    if (!rtcpData){
      FAIL_MSG("Could not allocate 32 bytes. Something is seriously messed up.");
      return;
    }
    rtcpData[0] = 0x80;                  // version 2, no padding, zero receiver reports
    rtcpData[1] = 200;                   // sender report
    Bit::htobs(rtcpData + 2, 6);         // 6 4-byte words follow the header
    Bit::htobl(rtcpData + 4, getSSRC()); // set source identifier

    Bit::htobll(rtcpData + 8, Util::getNTP());
    Bit::htobl(rtcpData + 16, getTimeStamp()); // rtpts
    // it should be the time packet was sent maybe, after all?
    //*((int *)(rtcpData+16) ) = htonl(getTimeStamp());//rtpts
    Bit::htobl(rtcpData + 20, sentPackets); // packet
    Bit::htobl(rtcpData + 24, sentBytes);   // octet
    callBack(socket, (char *)rtcpData, 28, 0);
    free(rtcpData);
  }

  void Packet::sendRTCP_RR(long long &connectedAt, SDP::Track &sTrk, unsigned int tid,
                           DTSC::Meta &metadata,
                           void callBack(void *, char *, unsigned int, unsigned int)){
    char *rtcpData = (char *)malloc(32);
    if (!rtcpData){
      FAIL_MSG("Could not allocate 32 bytes. Something is seriously messed up.");
      return;
    }
    if (!(sTrk.sorter.lostCurrent + sTrk.sorter.packCurrent)){sTrk.sorter.packCurrent++;}
    rtcpData[0] = 0x81;                       // version 2, no padding, one receiver report
    rtcpData[1] = 201;                        // receiver report
    Bit::htobs(rtcpData + 2, 7);              // 7 4-byte words follow the header
    Bit::htobl(rtcpData + 4, sTrk.mySSRC);    // set receiver identifier
    Bit::htobl(rtcpData + 8, sTrk.theirSSRC); // set source identifier
    rtcpData[12] =
        (sTrk.sorter.lostCurrent * 255) /
        (sTrk.sorter.lostCurrent + sTrk.sorter.packCurrent); // fraction lost since prev RR
    Bit::htob24(rtcpData + 13, sTrk.sorter.lostTotal);       // cumulative packets lost since start
    Bit::htobl(rtcpData + 16, sTrk.sorter.rtpSeq | (sTrk.sorter.packTotal &
                                                    0xFFFF0000ul)); // highest sequence received
    Bit::htobl(rtcpData + 20, 0); /// \TODO jitter (diff in timestamp vs packet arrival)
    Bit::htobl(rtcpData + 24, 0); /// \TODO last SR (middle 32 bits of last SR or zero)
    Bit::htobl(rtcpData + 28, 0); /// \TODO delay since last SR in 2b seconds + 2b fraction
    callBack(&(sTrk.rtcp), (char *)rtcpData, 32, 0);
    sTrk.sorter.lostCurrent = 0;
    sTrk.sorter.packCurrent = 0;
    free(rtcpData);
  }

  Packet::Packet(){
    managed = false;
    data = 0;
    maxDataLen = 0;
    sentBytes = 0;
    sentPackets = 0;
  }

  Packet::Packet(unsigned int payloadType, unsigned int sequence, unsigned int timestamp,
                 unsigned int ssrc, unsigned int csrcCount){
    managed = true;
    data = new char[12 + 4 * csrcCount + 2 +
                    MAX_SEND]; // headerSize, 2 for FU-A, MAX_SEND for maximum sent size
    if (data){
      maxDataLen = 12 + 4 * csrcCount + 2 + MAX_SEND;
      data[0] = ((2) << 6) | ((0 & 1) << 5) | ((0 & 1) << 4) |
                (csrcCount & 15);   // version, padding, extension, csrc count
      data[1] = payloadType & 0x7F; // marker and payload type
    }else{
      maxDataLen = 0;
    }
    setSequence(sequence - 1); // we automatically increase the sequence each time when p
    setTimestamp(timestamp);
    setSSRC(ssrc);
    sentBytes = 0;
    sentPackets = 0;
  }

  Packet::Packet(const Packet &o){
    managed = true;
    maxDataLen = 0;
    if (o.data && o.maxDataLen){
      data = new char[o.maxDataLen]; // headerSize, 2 for FU-A, MAX_SEND for maximum sent size
      if (data){
        maxDataLen = o.maxDataLen;
        memcpy(data, o.data, o.maxDataLen);
      }
    }else{
      data = new char[14 + MAX_SEND]; // headerSize, 2 for FU-A, MAX_SEND for maximum sent size
      if (data){
        maxDataLen = 14 + MAX_SEND;
        memset(data, 0, maxDataLen);
      }
    }
    sentBytes = o.sentBytes;
    sentPackets = o.sentPackets;

  }

  void Packet::operator=(const Packet &o){
    if (data && managed){delete[] data;}
    managed = true;
    maxDataLen = 0;
    data = 0;

    if (o.data && o.maxDataLen){
      data = new char[o.maxDataLen]; // headerSize, 2 for FU-A, MAX_SEND for maximum sent size
      if (data){
        maxDataLen = o.maxDataLen;
        memcpy(data, o.data, o.maxDataLen);
      }
    }else{
      data = new char[14 + MAX_SEND]; // headerSize, 2 for FU-A, MAX_SEND for maximum sent size
      if (data){
        maxDataLen = 14 + MAX_SEND;
        memset(data, 0, maxDataLen);
      }
    }
    sentBytes = o.sentBytes;
    sentPackets = o.sentPackets;
  }

  Packet::~Packet(){
    if (managed){delete[] data;}
  }
  Packet::Packet(const char *dat, unsigned int len){
    managed = false;
    maxDataLen = len;
    sentBytes = 0;
    sentPackets = 0;
    data = (char *)dat;
  }

  MPEGVideoHeader::MPEGVideoHeader(char *d){data = d;}

  uint16_t MPEGVideoHeader::getTotalLen() const{
    uint16_t ret = 4;
    if (data[0] & 0x08){
      ret += 4;
      if (data[4] & 0x40){ret += data[8];}
    }
    return ret;
  }

  std::string MPEGVideoHeader::toString() const{
    std::stringstream ret;
    uint32_t firstHead = Bit::btohl(data);
    ret << "TR=" << ((firstHead & 0x3FF0000) >> 16);
    if (firstHead & 0x4000000){ret << " Ext";}
    if (firstHead & 0x2000){ret << " SeqHead";}
    if (firstHead & 0x1000){ret << " SliceBegin";}
    if (firstHead & 0x800){ret << " SliceEnd";}
    ret << " PicType=" << ((firstHead & 0x700) >> 8);
    if (firstHead & 0x80){ret << " FBV";}
    ret << " BFC=" << ((firstHead & 0x70) >> 4);
    if (firstHead & 0x8){ret << " FFV";}
    ret << " FFC=" << (firstHead & 0x7);
    return ret.str();
  }

  void MPEGVideoHeader::clear(){((uint32_t *)data)[0] = 0;}

  void MPEGVideoHeader::setTempRef(uint16_t ref){
    data[0] |= (ref >> 8) & 0x03;
    data[1] = ref & 0xff;
  }

  void MPEGVideoHeader::setPictureType(uint8_t pType){data[2] |= pType & 0x7;}

  void MPEGVideoHeader::setSequence(){data[2] |= 0x20;}
  void MPEGVideoHeader::setBegin(){data[2] |= 0x10;}
  void MPEGVideoHeader::setEnd(){data[2] |= 0x8;}
  
  Sorter::Sorter(uint64_t trackId, void (*cb)(const uint64_t track, const Packet &p)){
    packTrack = trackId;
    rtpSeq = 0;
    lostTotal = 0;
    lostCurrent = 0;
    packTotal = 0;
    packCurrent = 0;
    callback = cb;
  }
  
  bool Sorter::wantSeq(uint16_t seq) const{
    return !rtpSeq || !(seq < rtpSeq || seq > (rtpSeq + 500) || packBuffer.count(seq));
  }

  void Sorter::setCallback(uint64_t track, void (*cb)(const uint64_t track, const Packet &p)){
    callback = cb;
    packTrack = track;
  }

  /// Calls addPacket(pack) with a newly constructed RTP::Packet from the given arguments.
  void Sorter::addPacket(const char *dat, unsigned int len){addPacket(RTP::Packet(dat, len));}

  /// Takes in new RTP packets for a single track.
  /// Automatically sorts them, waiting when packets come in slow or not at all.
  /// Calls the callback with packets in sorted order, whenever it becomes possible to do so.
  void Sorter::addPacket(const Packet &pack){
    if (!rtpSeq){rtpSeq = pack.getSequence();}
    // packet is very early - assume dropped after 30 packets
    while ((int16_t)(rtpSeq - ((uint16_t)pack.getSequence())) < -30){
      WARN_MSG("Giving up on packet %u", rtpSeq);
      ++rtpSeq;
      ++lostTotal;
      ++lostCurrent;
      ++packTotal;
      ++packCurrent;
      // send any buffered packets we may have
      while (packBuffer.count(rtpSeq)){
        outPacket(packTrack, packBuffer[rtpSeq]);
        packBuffer.erase(rtpSeq);
        VERYHIGH_MSG("Sent packet %u, now %llu in buffer", rtpSeq, packBuffer.size());
        ++rtpSeq;
        ++packTotal;
        ++packCurrent;
      }
    }
    // send any buffered packets we may have
    while (packBuffer.count(rtpSeq)){
      outPacket(packTrack, packBuffer[rtpSeq]);
      packBuffer.erase(rtpSeq);
      VERYHIGH_MSG("Sent packet %u, now %llu in buffer", rtpSeq, packBuffer.size());
      ++rtpSeq;
      ++packTotal;
      ++packCurrent;
    }
    // packet is slightly early - buffer it
    if ((int16_t)(rtpSeq - (uint16_t)pack.getSequence()) < 0){
      HIGH_MSG("Buffering early packet #%u->%u", rtpSeq, pack.getSequence());
      packBuffer[pack.getSequence()] = pack;
    }
    // packet is late
    if ((int16_t)(rtpSeq - (uint16_t)pack.getSequence()) > 0){
      // negative difference?
      --lostTotal;
      --lostCurrent;
      ++packTotal;
      ++packCurrent;
      WARN_MSG("Dropped a packet that arrived too late! (%d packets difference)",
               (int16_t)(rtpSeq - (uint16_t)pack.getSequence()));
      return;
    }
    // packet is in order
    if (rtpSeq == pack.getSequence()){
      outPacket(packTrack, pack);
      ++rtpSeq;
      ++packTotal;
      ++packCurrent;
    }
  }

  toDTSC::toDTSC(){
    wrapArounds = 0;
    recentWrap = false;
    cbPack = 0;
    cbInit = 0;
    multiplier = 1.0;
    trackId = 0;
    firstTime = 0;
    packCount = 0;
    lastSeq = 0;
    vp8BufferHasKeyframe = false;
  }

  void toDTSC::setProperties(const uint64_t track, const std::string &c, const std::string &t,
                             const std::string &i, const double m){
    trackId = track;
    codec = c;
    type = t;
    init = i;
    multiplier = m;
    if (codec == "HEVC" && init.size()){
      hevcInfo = h265::initData(init);
      h265::metaInfo MI = hevcInfo.getMeta();
      fps = MI.fps;
    }
    if (codec == "H264" && init.size()){
      MP4::AVCC avccbox;
      avccbox.setPayload(init);
      spsData.assign(avccbox.getSPS(), avccbox.getSPSLen());
      ppsData.assign(avccbox.getPPS(), avccbox.getPPSLen());
      h264::sequenceParameterSet sps(spsData.data(), spsData.size());
      h264::SPSMeta hMeta = sps.getCharacteristics();
      fps = hMeta.fps;
    }
  }

  void toDTSC::setProperties(const DTSC::Track &Trk){
    double m = (double)Trk.rate / 1000.0;
    if (Trk.type == "video" || Trk.codec == "MP2" || Trk.codec == "MP3"){m = 90.0;}
    setProperties(Trk.trackID, Trk.codec, Trk.type, Trk.init, m);
  }

  void toDTSC::setCallbacks(void (*cbP)(const DTSC::Packet &pkt),
                            void (*cbI)(const uint64_t track, const std::string &initData)){
    cbPack = cbP;
    cbInit = cbI;
  }

  /// Adds an RTP packet to the converter, outputting DTSC packets and/or updating init data,
  /// as-needed.
  void toDTSC::addRTP(const RTP::Packet &pkt){
    if (codec.empty()){
      MEDIUM_MSG("Unknown codec - ignoring RTP packet.");
      return;
    }
    // First calculate the timestamp of the packet, get the pointer and length to RTP payload.
    // This part isn't codec-specific, so we do it before anything else.
    int64_t pTime = pkt.getTimeStamp();
    if (!firstTime){
      firstTime = pTime + 1;
      INFO_MSG("RTP timestamp rollover expected in " PRETTY_PRINT_TIME, PRETTY_ARG_TIME((0xFFFFFFFFul - firstTime) / multiplier / 1000));
    }else{
      if (prevTime > pTime && pTime < 0x40000000lu && prevTime > 0x80000000lu){
        ++wrapArounds;
        recentWrap = true;
      }
      if (recentWrap){
        if (pTime < 0x80000000lu && pTime > 0x40000000lu){recentWrap = false;}
        if (pTime > 0x80000000lu){pTime -= 0xFFFFFFFFll;}
      }
    }
    prevTime = pkt.getTimeStamp();
    uint64_t msTime = ((uint64_t)pTime - firstTime + 1 + 0xFFFFFFFFull*wrapArounds) / multiplier;
    char *pl = pkt.getPayload();
    uint32_t plSize = pkt.getPayloadSize();
    bool missed = lastSeq != (pkt.getSequence() - 1);
    lastSeq = pkt.getSequence();
    INSANE_MSG("Received RTP packet for track %llu, time %llu -> %llu", trackId, pkt.getTimeStamp(), msTime);
    // From here on, there is codec-specific parsing. We call handler functions for each codec,
    // except for the trivial codecs.
    if (codec == "H264"){
      return handleH264(msTime, pl, plSize, missed, (pkt.getPadding() == 1) ? true : false);
    }
    if (codec == "AAC"){return handleAAC(msTime, pl, plSize);}
    if (codec == "MP2" || codec == "MP3"){return handleMP2(msTime, pl, plSize);}
    if (codec == "HEVC"){return handleHEVC(msTime, pl, plSize, missed);}
    if (codec == "MPEG2"){return handleMPEG2(msTime, pl, plSize);}
    if (codec == "VP8"){
      return handleVP8(msTime, pl, plSize, missed, (pkt.getPadding() == 1) ? true : false);
    }
    // Trivial codecs just fill a packet with raw data and continue. Easy peasy, lemon squeezy.
    if (codec == "ALAW" || codec == "opus" || codec == "PCM" || codec == "ULAW"){
      DTSC::Packet nextPack;
      nextPack.genericFill(msTime, 0, trackId, pl, plSize, 0, false);
      outPacket(nextPack);
      return;
    }
    // If we don't know how to handle this codec in RTP, print an error and ignore the packet.
    FAIL_MSG("Unimplemented RTP reader for codec `%s`! Throwing away packet.", codec.c_str());
  }

  void toDTSC::handleAAC(uint64_t msTime, char *pl, uint32_t plSize){
    // assume AAC packets are single AU units
    /// \todo Support other input than single AU units
    unsigned int headLen =
        (Bit::btohs(pl) >> 3) + 2; // in bits, so /8, plus two for the prepended size
    DTSC::Packet nextPack;
    uint16_t samples = aac::AudSpecConf::samples(init);
    uint32_t sampleOffset = 0;
    uint32_t offset = 0;
    uint32_t auSize = 0;
    for (uint32_t i = 2; i < headLen; i += 2){
      auSize = Bit::btohs(pl + i) >> 3; // only the upper 13 bits
      nextPack.genericFill(msTime + sampleOffset / multiplier, 0, trackId, pl + headLen + offset,
                           std::min(auSize, plSize - headLen - offset), 0, false);
      offset += auSize;
      sampleOffset += samples;
      outPacket(nextPack);
    }
  }

  void toDTSC::handleMP2(uint64_t msTime, char *pl, uint32_t plSize){
    if (plSize < 5){
      WARN_MSG("Empty packet ignored!");
      return;
    }
    DTSC::Packet nextPack;
    nextPack.genericFill(msTime, 0, trackId, pl + 4, plSize - 4, 0, false);
    outPacket(nextPack);
  }

  void toDTSC::handleMPEG2(uint64_t msTime, char *pl, uint32_t plSize){
    if (plSize < 5){
      WARN_MSG("Empty packet ignored!");
      return;
    }
    ///\TODO Merge packets with same timestamp together
    HIGH_MSG("Received MPEG2 packet: %s", RTP::MPEGVideoHeader(pl).toString().c_str());
    DTSC::Packet nextPack;
    nextPack.genericFill(msTime, 0, trackId, pl + 4, plSize - 4, 0, false);
    outPacket(nextPack);
  }

  void toDTSC::handleHEVC(uint64_t msTime, char *pl, uint32_t plSize, bool missed){
    if (plSize < 2){
      WARN_MSG("Empty packet ignored!");
      return;
    }
    uint8_t nalType = (pl[0] & 0x7E) >> 1;
    if (nalType == 48){
      ERROR_MSG("AP not supported yet");
    }else if (nalType == 49){
      DONTEVEN_MSG("H265 Fragmentation Unit");
      // No length yet? Check for start bit. Ignore rest.
      if (!fuaBuffer.size() && (pl[2] & 0x80) == 0){
        HIGH_MSG("Not start of a new FU - throwing away");
        return;
      }
      if (fuaBuffer.size() && ((pl[2] & 0x80) || missed)){
        WARN_MSG("H265 FU packet incompleted: %lu", fuaBuffer.size());
        Bit::htobl(fuaBuffer, fuaBuffer.size() - 4); // size-prepend
        fuaBuffer[4] |= 0x80;                        // set error bit
        handleHEVCSingle(msTime, fuaBuffer, fuaBuffer.size(),
                         h265::isKeyframe(fuaBuffer + 4, fuaBuffer.size() - 4));
        fuaBuffer.size() = 0;
        return;
      }

      unsigned long len = plSize - 3;      // ignore the three FU bytes in front
      if (!fuaBuffer.size()){len += 6;}// six extra bytes for the first packet
      if (!fuaBuffer.allocate(fuaBuffer.size() + len)){return;}
      if (!fuaBuffer.size()){
        memcpy(fuaBuffer + 6, pl + 3, plSize - 3);
        // reconstruct first byte
        fuaBuffer[4] = ((pl[2] & 0x3F) << 1) | (pl[0] & 0x81);
        fuaBuffer[5] = pl[1];
      }else{
        memcpy(fuaBuffer + fuaBuffer.size(), pl + 3, plSize - 3);
      }
      fuaBuffer.size() += len;

      if (pl[2] & 0x40){// last packet
        VERYHIGH_MSG("H265 FU packet type %s (%u) completed: %lu",
                     h265::typeToStr((fuaBuffer[4] & 0x7E) >> 1),
                     (uint8_t)((fuaBuffer[4] & 0x7E) >> 1), fuaBuffer.size());
        Bit::htobl(fuaBuffer, fuaBuffer.size() - 4); // size-prepend
        handleHEVCSingle(msTime, fuaBuffer, fuaBuffer.size(),
                         h265::isKeyframe(fuaBuffer + 4, fuaBuffer.size() - 4));
        fuaBuffer.size() = 0;
      }
    }else if (nalType == 50){
      ERROR_MSG("PACI/TSCI not supported yet");
    }else{
      DONTEVEN_MSG("%s NAL unit (%u)", h265::typeToStr(nalType), nalType);
      if (!packBuffer.allocate(plSize + 4)){return;}
      Bit::htobl(packBuffer, plSize); // size-prepend
      memcpy(packBuffer + 4, pl, plSize);
      handleHEVCSingle(msTime, packBuffer, plSize + 4, h265::isKeyframe(packBuffer + 4, plSize));
    }
  }

  void toDTSC::handleHEVCSingle(uint64_t ts, const char *buffer, const uint32_t len, bool isKey){
    MEDIUM_MSG("H265: %llu@%llu, %lub%s", trackId, ts, len, isKey ? " (key)" : "");
    // Ignore zero-length packets (e.g. only contained init data and nothing else)
    if (!len){return;}

    // Header data? Compare to init, set if needed, and throw away
    uint8_t nalType = (buffer[4] & 0x7E) >> 1;
    switch (nalType){
    case 32: // VPS
    case 33: // SPS
    case 34: // PPS
      hevcInfo.addUnit(buffer);
      if (hevcInfo.haveRequired()){
        std::string newInit = hevcInfo.generateHVCC();
        if (newInit != init){
          init = newInit;
          outInit(trackId, init);
          h265::metaInfo MI = hevcInfo.getMeta();
          fps = MI.fps;
        }
      }
      return;
    default: // others, continue parsing
      break;
    }

    uint32_t offset = 0;
    uint64_t newTs = ts;
    if (fps > 1){
      // Assume a steady frame rate, clip the timestamp based on frame number.
      uint64_t frameNo = (ts / (1000.0 / fps)) + 0.5;
      while (frameNo < packCount){packCount--;}
      // More than 32 frames behind? We probably skipped something, somewhere...
      if ((frameNo - packCount) > 32){packCount = frameNo;}
      // After some experimentation, we found that the time offset is the difference between the
      // frame number and the packet counter, times the frame rate in ms
      offset = (frameNo - packCount) * (1000.0 / fps);
      //... and the timestamp is the packet counter times the frame rate in ms.
      newTs = packCount * (1000.0 / fps);
      VERYHIGH_MSG("Packing time %llu = %sframe %llu (%.2f FPS). Expected %llu -> +%llu/%lu", ts,
                   isKey ? "key" : "i", frameNo, fps, packCount, (frameNo - packCount), offset);
    }else{
      // For non-steady frame rate, assume no offsets are used and the timestamp is already correct
      VERYHIGH_MSG("Packing time %llu = %sframe %llu (variable rate)", ts, isKey ? "key" : "i",
                   packCount);
    }
    // Fill the new DTSC packet, buffer it.
    DTSC::Packet nextPack;
    nextPack.genericFill(newTs, offset, trackId, buffer, len, 0, isKey);
    packCount++;
    outPacket(nextPack);
  }

  /// Handles common H264 packets types, but not all.
  /// Generalizes and converts them all to a data format ready for DTSC, then calls handleH264Single
  /// for that data.
  /// Prints a WARN-level message if packet type is unsupported.
  /// \todo Support other H264 packets types?
  void toDTSC::handleH264(uint64_t msTime, char *pl, uint32_t plSize, bool missed,
                          bool hasPadding){
    if (!plSize){
      WARN_MSG("Empty packet ignored!");
      return;
    }

    uint8_t num_padding_bytes = 0;
    if (hasPadding){
      num_padding_bytes = pl[plSize - 1];
      if (num_padding_bytes >= plSize){
        WARN_MSG("Only padding data (%u / %u).", num_padding_bytes, plSize);
        return;
      }
    }

    if ((pl[0] & 0x1F) == 0){
      WARN_MSG("H264 packet type null ignored");
      return;
    }
    if ((pl[0] & 0x1F) < 24){
      DONTEVEN_MSG("H264 single packet, type %u", (unsigned int)(pl[0] & 0x1F));
      if (!packBuffer.allocate(plSize + 4)){return;}
      Bit::htobl(packBuffer, plSize); // size-prepend
      memcpy(packBuffer + 4, pl, plSize);
      handleH264Single(msTime, packBuffer, plSize + 4, h264::isKeyframe(packBuffer + 4, plSize));
      return;
    }
    if ((pl[0] & 0x1F) == 24){
      DONTEVEN_MSG("H264 STAP-A packet");
      unsigned int pos = 1;
      while (pos + 1 < plSize){
        unsigned int pLen = Bit::btohs(pl + pos);
        INSANE_MSG("Packet of %ub and type %u", pLen, (unsigned int)(pl[pos + 2] & 0x1F));
        if (packBuffer.allocate(4 + pLen)){
          Bit::htobl(packBuffer, pLen); // size-prepend
          memcpy(packBuffer + 4, pl + pos + 2, pLen);
          handleH264Single(msTime, packBuffer, pLen + 4, h264::isKeyframe(pl + pos + 2, pLen));
        }
        pos += 2 + pLen;
      }
      return;
    }
    if ((pl[0] & 0x1F) == 28){
      DONTEVEN_MSG("H264 FU-A packet");
      // No length yet? Check for start bit. Ignore rest.
      if (!fuaBuffer.size() && (pl[1] & 0x80) == 0){
        HIGH_MSG("Not start of a new FU-A - throwing away");
        return;
      }
      if (fuaBuffer.size() && ((pl[1] & 0x80) || missed)){
        WARN_MSG("Ending unfinished FU-A");
        INSANE_MSG("H264 FU-A packet incompleted: %lu", fuaBuffer.size());
        fuaBuffer.size() = 0;
        return;
      }

      unsigned long len = plSize - 2;      // ignore the two FU-A bytes in front
      if (!fuaBuffer.size()){len += 5;}// five extra bytes for the first packet
      if (!fuaBuffer.allocate(fuaBuffer.size() + len)){return;}
      if (!fuaBuffer.size()){
        memcpy(fuaBuffer + 4, pl + 1, plSize - 1);
        // reconstruct first byte
        fuaBuffer[4] = (fuaBuffer[4] & 0x1F) | (pl[0] & 0xE0);
      }else{
        memcpy(fuaBuffer + fuaBuffer.size(), pl + 2, plSize - 2);
      }
      fuaBuffer.size() += len;

      if (pl[1] & 0x40){// last packet
        INSANE_MSG("H264 FU-A packet type %u completed: %lu", (unsigned int)(fuaBuffer[4] & 0x1F),
                   fuaBuffer.size());
        uint8_t nalType = (fuaBuffer[4] & 0x1F);
        if (nalType == 7 || nalType == 8){
          // attempt to detect multiple H264 packets, even though specs disallow it
          handleH264Multi(msTime, fuaBuffer, fuaBuffer.size());
        }else{
          Bit::htobl(fuaBuffer, fuaBuffer.size() - 4); // size-prepend
          handleH264Single(msTime, fuaBuffer, fuaBuffer.size(),
                           h264::isKeyframe(fuaBuffer + 4, fuaBuffer.size() - 4));
        }
        fuaBuffer.size() = 0;
      }
      return;
    }
    WARN_MSG("H264 packet type %u unsupported", (unsigned int)(pl[0] & 0x1F));
  }

  void toDTSC::handleH264Single(uint64_t ts, const char *buffer, const uint32_t len, bool isKey){
    MEDIUM_MSG("H264: %llu@%llu, %lub%s", trackId, ts, len, isKey ? " (key)" : "");
    // Ignore zero-length packets (e.g. only contained init data and nothing else)
    if (!len){return;}

    // Header data? Compare to init, set if needed, and throw away
    uint8_t nalType = (buffer[4] & 0x1F);
    if (nalType == 9 && len < 20){return;}// ignore delimiter-only packets
    switch (nalType){
    case 6: // SEI
      return;
    case 7: // SPS
      if (spsData.size() != len - 4 || memcmp(buffer + 4, spsData.data(), len - 4) != 0){
        HIGH_MSG("Updated SPS from RTP data");
        spsData.assign(buffer + 4, len - 4);
        h264::sequenceParameterSet sps(spsData.data(), spsData.size());
        h264::SPSMeta hMeta = sps.getCharacteristics();
        fps = hMeta.fps;

        MP4::AVCC avccBox;
        avccBox.setVersion(1);
        avccBox.setProfile(spsData[1]);
        avccBox.setCompatibleProfiles(spsData[2]);
        avccBox.setLevel(spsData[3]);
        avccBox.setSPSCount(1);
        avccBox.setSPS(spsData);
        avccBox.setPPSCount(1);
        avccBox.setPPS(ppsData);
        std::string newInit = std::string(avccBox.payload(), avccBox.payloadSize());
        if (newInit != init){
          init = newInit;
          outInit(trackId, init);
        }
      }
      return;
    case 8: // PPS
      if (ppsData.size() != len - 4 || memcmp(buffer + 4, ppsData.data(), len - 4) != 0){
        HIGH_MSG("Updated PPS from RTP data");
        ppsData.assign(buffer + 4, len - 4);
        MP4::AVCC avccBox;
        avccBox.setVersion(1);
        avccBox.setProfile(spsData[1]);
        avccBox.setCompatibleProfiles(spsData[2]);
        avccBox.setLevel(spsData[3]);
        avccBox.setSPSCount(1);
        avccBox.setSPS(spsData);
        avccBox.setPPSCount(1);
        avccBox.setPPS(ppsData);
        std::string newInit = std::string(avccBox.payload(), avccBox.payloadSize());
        if (newInit != init){
          init = newInit;
          outInit(trackId, init);
        }
      }
      return;
    case 5:{
      // @todo add check if ppsData and spsData are not empty?
      static Util::ResizeablePointer tmp;
      tmp.assign(0, 0);

      char sizeBuffer[4];
      Bit::htobl(sizeBuffer, spsData.size());
      tmp.append(sizeBuffer, 4);
      tmp.append(spsData.data(), spsData.size());

      Bit::htobl(sizeBuffer, ppsData.size());
      tmp.append(sizeBuffer, 4);
      tmp.append(ppsData.data(), ppsData.size());
      tmp.append(buffer, len);

      uint32_t offset = 0;
      uint64_t newTs = ts;

      if (fps > 1){
        // Assume a steady frame rate, clip the timestamp based on frame number.
        uint64_t frameNo = (ts / (1000.0 / fps)) + 0.5;
        while (frameNo < packCount){packCount--;}
        // More than 32 frames behind? We probably skipped something, somewhere...
        if ((frameNo - packCount) > 32){packCount = frameNo;}
        // After some experimentation, we found that the time offset is the difference between the
        // frame number and the packet counter, times the frame rate in ms
        offset = (frameNo - packCount) * (1000.0 / fps);
        //... and the timestamp is the packet counter times the frame rate in ms.
        newTs = packCount * (1000.0 / fps);
        VERYHIGH_MSG("Packing time %llu = %sframe %llu (%.2f FPS). Expected %llu -> +%llu/%lu", ts,
                     isKey ? "key" : "i", frameNo, fps, packCount, (frameNo - packCount), offset);
      }else{
        // For non-steady frame rate, assume no offsets are used and the timestamp is already
        // correct
        VERYHIGH_MSG("Packing time %llu = %sframe %llu (variable rate)", ts, isKey ? "key" : "i",
                     packCount);
      }
      // Fill the new DTSC packet, buffer it.
      DTSC::Packet nextPack;
      nextPack.genericFill(newTs, offset, trackId, tmp, tmp.size(), 0, isKey);
      packCount++;
      outPacket(nextPack);
      return;
    }
    default: // others, continue parsing
      break;
    }

    uint32_t offset = 0;
    uint64_t newTs = ts;
    if (fps > 1){
      // Assume a steady frame rate, clip the timestamp based on frame number.
      uint64_t frameNo = (ts / (1000.0 / fps)) + 0.5;
      while (frameNo < packCount){packCount--;}
      // More than 32 frames behind? We probably skipped something, somewhere...
      if ((frameNo - packCount) > 32){packCount = frameNo;}
      // After some experimentation, we found that the time offset is the difference between the
      // frame number and the packet counter, times the frame rate in ms
      offset = (frameNo - packCount) * (1000.0 / fps);
      //... and the timestamp is the packet counter times the frame rate in ms.
      newTs = packCount * (1000.0 / fps);
      VERYHIGH_MSG("Packing time %llu = %sframe %llu (%.2f FPS). Expected %llu -> +%llu/%lu", ts,
                   isKey ? "key" : "i", frameNo, fps, packCount, (frameNo - packCount), offset);
    }else{
      // For non-steady frame rate, assume no offsets are used and the timestamp is already correct
      VERYHIGH_MSG("Packing time %llu = %sframe %llu (variable rate)", ts, isKey ? "key" : "i",
                   packCount);
    }
    // Fill the new DTSC packet, buffer it.
    DTSC::Packet nextPack;
    nextPack.genericFill(newTs, offset, trackId, buffer, len, 0, isKey);
    packCount++;
    outPacket(nextPack);
  }

  /// Handles a single H264 packet, checking if others are appended at the end in Annex B format.
  /// If so, splits them up and calls handleH264Single for each. If not, calls it only once for the
  /// whole payload.
  void toDTSC::handleH264Multi(uint64_t ts, char *buffer, const uint32_t len){
    uint32_t lastStart = 0;
    for (uint32_t i = 0; i < len - 4; ++i){
      // search for start code
      if (buffer[i] == 0 && buffer[i + 1] == 0 && buffer[i + 2] == 0 && buffer[i + 3] == 1){
        // if found, handle a packet from the last start code up to this start code
        Bit::htobl(buffer + lastStart, (i - lastStart - 1) - 4); // size-prepend
        handleH264Single(ts, buffer + lastStart, (i - lastStart - 1),
                         h264::isKeyframe(buffer + lastStart + 4, i - lastStart - 5));
        lastStart = i;
      }
    }
    // Last packet (might be first, if no start codes found)
    Bit::htobl(buffer + lastStart, (len - lastStart) - 4); // size-prepend
    handleH264Single(ts, buffer + lastStart, (len - lastStart),
                     h264::isKeyframe(buffer + lastStart + 4, len - lastStart - 4));
  }

  void toDTSC::handleVP8(uint64_t msTime, const char *buffer, const uint32_t len, bool missed,
                         bool hasPadding){

    // 1 byte is required but we assume that there some payload
    // data too :P
    if (len < 3){
      FAIL_MSG("Received a VP8 RTP packet with invalid size.");
      return;
    }

    // it may happen that we receive a packet with only padding
    // data. (against the spec I think) Although `drno` from
    // Mozilla told me these are probing packets and should be
    // ignored.
    uint8_t num_padding_bytes = 0;
    if (hasPadding){
      num_padding_bytes = buffer[len - 1];
      if (num_padding_bytes >= len){
        WARN_MSG("Only padding data (%u/%u)", num_padding_bytes, len);
        return;
      }
    }

    // parse the vp8 payload descriptor, https://tools.ietf.org/html/rfc7741#section-4.2
    uint8_t extended_control_bits = (buffer[0] & 0x80) >> 7;
    uint8_t start_of_partition = (buffer[0] & 0x10) >> 4;
    uint8_t partition_index = (buffer[0] & 0x07);

    uint32_t vp8_header_size = 1;
    vp8_header_size += extended_control_bits;

    if (extended_control_bits == 1){

      uint8_t pictureid_present = (buffer[1] & 0x80) >> 7;
      uint8_t tl0picidx_present = (buffer[1] & 0x40) >> 6;
      uint8_t tid_present = (buffer[1] & 0x20) >> 5;
      uint8_t keyidx_present = (buffer[1] & 0x10) >> 4;

      uint8_t has_extended_pictureid = 0;
      if (pictureid_present == 1){has_extended_pictureid = (buffer[2] & 0x80) > 7;}

      vp8_header_size += pictureid_present;
      vp8_header_size += tl0picidx_present;
      vp8_header_size += ((tid_present == 1 || keyidx_present == 1)) ? 1 : 0;
      vp8_header_size += has_extended_pictureid;
    }

    if (vp8_header_size > len){
      FAIL_MSG("The vp8 header size exceeds the RTP packet size. Invalid size.");
      return;
    }

    const char *vp8_payload_buffer = buffer + vp8_header_size;
    uint32_t vp8_payload_size = len - vp8_header_size;
    bool start_of_frame = (start_of_partition == 1) && (partition_index == 0);

    if (hasPadding){
      if (num_padding_bytes > vp8_payload_size){
        FAIL_MSG("More padding bytes than payload bytes. Invalid.");
        return;
      }

      vp8_payload_size -= num_padding_bytes;
      if (vp8_payload_size == 0){
        WARN_MSG("No payload data at all, only required VP8 header.");
        return;
      }
    }

    // when we have data in our buffer and the current packet is
    // for a new frame started or we missed some data
    // (e.g. only received the first partition of a frame) we will
    // flush a new DTSC packet.
    if (vp8FrameBuffer.size()){
      //new frame and nothing missed? Send.
      if (start_of_frame && !missed){
        DTSC::Packet nextPack;
        nextPack.genericFill(msTime, 0, trackId, vp8FrameBuffer, vp8FrameBuffer.size(), 0,
                             vp8BufferHasKeyframe);
        packCount++;
        outPacket(nextPack);
      }
      //Wipe the buffer clean if missed packets or we just sent data out.
      if (start_of_frame || missed){
        vp8FrameBuffer.assign(0, 0);
        vp8BufferHasKeyframe = false;
      }
    }

    // copy the data into the buffer. assign() will write the
    // buffer from the start, append() appends the data to the
    // end of the previous buffer.
    if (vp8FrameBuffer.size() == 0){
      if (!start_of_frame){
        FAIL_MSG("Skipping packet; not start of partition (%u).", partition_index);
        return;
      }
      if (!vp8FrameBuffer.assign(vp8_payload_buffer, vp8_payload_size)){
        FAIL_MSG("Failed to assign vp8 buffer data.");
      }
    }else{
      vp8FrameBuffer.append(vp8_payload_buffer, vp8_payload_size);
    }

    bool is_keyframe = (vp8_payload_buffer[0] & 0x01) == 0;
    if (start_of_frame && is_keyframe){vp8BufferHasKeyframe = true;}
  }
}// namespace RTP
