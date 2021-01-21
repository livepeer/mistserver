#include <mist/socket_srt.h>
#include "output_tssrt.h"
#include <mist/defines.h>
#include <mist/http_parser.h>
#include <mist/url.h>
#include <mist/encode.h>
#include <mist/stream.h>
#include <mist/triggers.h>

namespace Mist{
  OutTSSRT::OutTSSRT(Socket::Connection &conn, Socket::SRTConnection & _srtSock) : TSOutput(conn), srtConn(_srtSock){
    // NOTE: conn is useless for SRT, as it uses a different socket type.
    sendRepeatingHeaders = 500; // PAT/PMT every 500ms (DVB spec)
    streamName = config->getString("streamname");
    Util::setStreamName(streamName);
    pushOut = false;
    // Push output configuration
    if (config->getString("target").size()){
      target = HTTP::URL(config->getString("target"));
      if (target.protocol != "srt"){
        FAIL_MSG("Target %s must begin with srt://, aborting", target.getUrl().c_str());
        onFail("Invalid srt target: doesn't start with srt://", true);
        return;
      }
      if (!target.getPort()){
        FAIL_MSG("Target %s must contain a port, aborting", target.getUrl().c_str());
        onFail("Invalid srt target: missing port", true);
        return;
      }
      pushOut = true;
      std::map<std::string, std::string> arguments;
      HTTP::parseVars(target.args, arguments);
      for (std::map<std::string, std::string>::iterator it = arguments.begin(); it != arguments.end(); ++it){
        targetParams[it->first] = it->second;
      }
      size_t connectCnt = 0;
      do{
        srtConn.connect(target.host, target.getPort(), "output", targetParams);
        if (!srtConn){
          Util::sleep(1000);
        }else{
          INFO_MSG("Connect success on attempt %zu", connectCnt+1);
          break;
        }
        ++connectCnt;
      }while (!srtConn && connectCnt < 5);
      wantRequest = false;
      parseData = true;
      initialize();
    }else{
      // Pull output configuration, In this case we have an srt connection in the second constructor parameter.
      // Handle override / append of streamname options
      std::string sName = srtConn.getStreamName();
      if (sName != ""){
        streamName = sName;
        Util::sanitizeName(streamName);
        Util::setStreamName(streamName);
      }

      int64_t accTypes = config->getInteger("acceptable");
      if (accTypes == 0){//Allow both directions
        srtConn.setBlocking(false);
        //Try to read the socket 10 times. If any reads succeed, assume they are pushing in
        size_t retries = 60;
        while (!accTypes && srtConn && retries){
          size_t recvSize = srtConn.Recv();
          if (recvSize){
            accTypes = 2;
            INFO_MSG("Connection put into ingest mode");
            assembler.assemble(tsIn, srtConn.recvbuf, recvSize, true);
          }else{
            Util::sleep(50);
          }
          --retries;
        }
        //If not, assume they are receiving.
        if (!accTypes){
          accTypes = 1;
          INFO_MSG("Connection put into egress mode");
        }
      }
      if (accTypes == 1){// Only allow outgoing
        srtConn.setBlocking(true);
        srtConn.direction = "output";
        parseData = true;
        wantRequest = false;
        initialize();
      }else if (accTypes == 2){//Only allow incoming
        srtConn.setBlocking(false);
        srtConn.direction = "input";
        if (Triggers::shouldTrigger("PUSH_REWRITE")){
          HTTP::URL reqUrl;
          reqUrl.protocol = "srt";
          reqUrl.port = config->getString("port");
          reqUrl.host = config->getString("interface");
          reqUrl.args = "streamid="+Encodings::URL::encode(sName);
          std::string payload = reqUrl.getUrl() + "\n" + getConnectedHost() + "\n" + streamName;
          std::string newStream = streamName;
          Triggers::doTrigger("PUSH_REWRITE", payload, "", false, newStream);
          if (!newStream.size()){
            FAIL_MSG("Push from %s to URL %s rejected - PUSH_REWRITE trigger blanked the URL",
                     getConnectedHost().c_str(), reqUrl.getUrl().c_str());
            Util::logExitReason(
                "Push from %s to URL %s rejected - PUSH_REWRITE trigger blanked the URL",
                getConnectedHost().c_str(), reqUrl.getUrl().c_str());
            onFinish();
            return;
          }else{
            streamName = newStream;
            Util::sanitizeName(streamName);
          }
        }
        myConn.setHost(srtConn.remotehost);
        if (!allowPush("")){
          onFinish();
          srtConn.close();
          return;
        }
        parseData = false;
        wantRequest = true;
      }

    }
    lastTimeStamp = 0;
    timeStampOffset = 0;
  }

  OutTSSRT::~OutTSSRT(){}

  static void addIntOpt(JSON::Value & pp, const std::string & param, const std::string & name, const std::string & help, size_t def = 0){
    pp[param]["name"] = name;
    pp[param]["help"] = help;
    pp[param]["type"] = "int";
    pp[param]["default"] = def;
  }

  static void addStrOpt(JSON::Value & pp, const std::string & param, const std::string & name, const std::string & help, const std::string & def = ""){
    pp[param]["name"] = name;
    pp[param]["help"] = help;
    pp[param]["type"] = "str";
    pp[param]["default"] = def;
  }

  static void addBoolOpt(JSON::Value & pp, const std::string & param, const std::string & name, const std::string & help, bool def = false){
    pp[param]["name"] = name;
    pp[param]["help"] = help;
    pp[param]["type"] = "select";
    pp[param]["select"][0u][0u] = 0;
    pp[param]["select"][0u][1u] = "False";
    pp[param]["select"][1u][0u] = 1;
    pp[param]["select"][1u][1u] = "True";
    pp[param]["type"] = "select";
    pp[param]["default"] = def?1:0;

  }


  void OutTSSRT::init(Util::Config *cfg){
    Output::init(cfg);
    capa["name"] = "TSSRT";
    capa["friendly"] = "TS over SRT";
    capa["desc"] = "Real time streaming of TS data over SRT";
    capa["deps"] = "";

    capa["optional"]["streamname"]["name"] = "Stream";
    capa["optional"]["streamname"]["help"] = "What streamname to serve if no streamid is given by the other end of the connection";
    capa["optional"]["streamname"]["type"] = "str";
    capa["optional"]["streamname"]["option"] = "--stream";
    capa["optional"]["streamname"]["short"] = "s";
    capa["optional"]["streamname"]["default"] = "";

    capa["optional"]["filelimit"]["name"] = "Open file descriptor limit";
    capa["optional"]["filelimit"]["help"] = "Increase open file descriptor to this value if current system value is lower. A higher value may be needed for handling many concurrent SRT connections.";

    capa["optional"]["filelimit"]["type"] = "int";
    capa["optional"]["filelimit"]["option"] = "--filelimit";
    capa["optional"]["filelimit"]["short"] = "l";
    capa["optional"]["filelimit"]["default"] = "1024";

    capa["optional"]["acceptable"]["name"] = "Acceptable connection types";
    capa["optional"]["acceptable"]["help"] =
        "Whether to allow only incoming pushes (2), only outgoing pulls (1), or both (0, default)";
    capa["optional"]["acceptable"]["option"] = "--acceptable";
    capa["optional"]["acceptable"]["short"] = "T";
    capa["optional"]["acceptable"]["default"] = 0;
    capa["optional"]["acceptable"]["type"] = "select";
    capa["optional"]["acceptable"]["select"][0u][0u] = 0;
    capa["optional"]["acceptable"]["select"][0u][1u] =
        "Allow both incoming and outgoing connections";
    capa["optional"]["acceptable"]["select"][1u][0u] = 1;
    capa["optional"]["acceptable"]["select"][1u][1u] = "Allow only outgoing connections";
    capa["optional"]["acceptable"]["select"][2u][0u] = 2;
    capa["optional"]["acceptable"]["select"][2u][1u] = "Allow only incoming connections";

    capa["codecs"][0u][0u].append("HEVC");
    capa["codecs"][0u][0u].append("H264");
    capa["codecs"][0u][0u].append("MPEG2");
    capa["codecs"][0u][1u].append("AAC");
    capa["codecs"][0u][1u].append("MP3");
    capa["codecs"][0u][1u].append("AC3");
    capa["codecs"][0u][1u].append("MP2");
    capa["codecs"][0u][1u].append("opus");
    cfg->addConnectorOptions(8889, capa);
    config = cfg;
    capa["push_urls"].append("srt://*");
    JSON::Value & pp = capa["push_parameters"];

    pp["mode"]["name"] = "Mode";
    pp["mode"]["help"] = "The connection mode. Can be listener, caller, or rendezvous. By default is listener if the host is missing from the URL, and is caller otherwise.";
    pp["mode"]["type"] = "select";
    pp["mode"]["select"][0u][0u] = "default";
    pp["mode"]["select"][0u][1u] = "Default";
    pp["mode"]["select"][1u][0u] = "listener";
    pp["mode"]["select"][1u][1u] = "Listener";
    pp["mode"]["select"][2u][0u] = "caller";
    pp["mode"]["select"][2u][1u] = "Caller";
    pp["mode"]["select"][3u][0u] = "rendezvous";
    pp["mode"]["select"][3u][1u] = "Rendezvous";
    pp["mode"]["type"] = "select";

    pp["transtype"]["name"] = "Transmission type";
    pp["transtype"]["help"] = "This should be set to live (the default) unless you know what you're doing.";
    pp["transtype"]["type"] = "select";
    pp["transtype"]["select"][0u][0u] = "";
    pp["transtype"]["select"][0u][1u] = "Live";
    pp["transtype"]["select"][1u][0u] = "file";
    pp["transtype"]["select"][1u][1u] = "File";
    pp["transtype"]["type"] = "select";
    
    //addStrOpt(pp, "adapter", "", "");
    //addIntOpt(pp, "timeout", "", "");
    //addIntOpt(pp, "port", "", "");
    addBoolOpt(pp, "tsbpd", "Timestamp-based Packet Delivery mode", "In this mode the packet's time is assigned at the sending time (or allowed to be predefined), transmitted in the packet's header, and then restored on the receiver side so that the time intervals between consecutive packets are preserved when delivering to the application.", true);
    addBoolOpt(pp, "linger", "Linger closed sockets", "Whether to keep closed sockets around for 180 seconds of linger time or not.", true);
    addIntOpt(pp, "maxbw", "Maximum send bandwidth", "Maximum send bandwidth in bytes per second, -1 for infinite, 0 for relative to input bandwidth.", -1);
    addIntOpt(pp, "pbkeylen", "Encryption key length", "May be 0 (auto), 16 (AES-128), 24 (AES-192) or 32 (AES-256).", 0);
    addStrOpt(pp, "passphrase", "Encryption passphrase", "Enables encryption with the given passphrase.");
    addIntOpt(pp, "mss", "Maximum Segment Size", "Maximum size for packets including all headers, in bytes. The default of 1500 is generally the maximum value you can use in most networks.", 1500);
    addIntOpt(pp, "fc", "Flight Flag Size", "Maximum packets that may be 'in flight' without being acknowledged.", 25600);
    addIntOpt(pp, "sndbuf", "Send Buffer Size", "Size of the send buffer, in bytes");
    addIntOpt(pp, "rcvbuf", "Receive Buffer Size", "Size of the receive buffer, in bytes");
    addIntOpt(pp, "ipttl", "TTL", "Time To Live for IPv4 connections or unicast hops for IPv6 connections. Defaults to system default.");
    addIntOpt(pp, "iptos", "Type of Service", "TOS for IPv4 connections or Traffic Class for IPv6 connections. Defaults to system default.");
    addIntOpt(pp, "inputbw", "Input bandwidth", "Estimated bandwidth of data to be sent. Default of 0 means automatic.");
    addIntOpt(pp, "oheadbw", "Recovery Bandwidth Overhead", "Percentage of bandwidth to use for recovery.", 25);
    addIntOpt(pp, "latency", "Latency", "Socket latency, in milliseconds.", 120);
    //addIntOpt(pp, "rcvlatency", "Receive Latency", "Latency in receive mode, in milliseconds", 120);
    //addIntOpt(pp, "peerlatency", "", "");
    addBoolOpt(pp, "tlpktdrop", "Too-late Packet Drop", "Skips packets that cannot (sending) or have not (receiving) been delivered in time", true);
    addIntOpt(pp, "snddropdelay", "Send Drop Delay", "Extra delay before Too-late packet drop on sender side is triggered, in milliseconds.");
    addBoolOpt(pp, "nakreport", "Repeat loss reports", "When enabled, repeats loss reports every time the retransmission timeout has expired.", true);
    addIntOpt(pp, "conntimeo", "Connect timeout", "Milliseconds to wait before timing out a connection attempt for caller and rendezvous modes.", 3000);
    addIntOpt(pp, "lossmaxttl", "Reorder Tolerance", "Maximum amount of packets that may be out of order, or 0 to disable this mechanism.");
    addIntOpt(pp, "minversion", "Minimum SRT version", "Minimum SRT version to require the other side of the connection to support.");
    addStrOpt(pp, "streamid", "Stream ID", "Stream ID to transmit to the other side. MistServer uses this field for the stream name, but the field is entirely free-form and may contain anything.");
    addStrOpt(pp, "congestion", "Congestion controller", "May be set to 'live' or 'file'", "live");
    addBoolOpt(pp, "messageapi", "Message API", "When true, uses the default Message API. When false, uses the Stream API", true);
    //addIntOpt(pp, "kmrefreshrate", "", "");
    //addIntOpt(pp, "kmreannounce", "", "");
    addBoolOpt(pp, "enforcedencryption", "Enforced Encryption", "If enabled, enforces that both sides either set no passphrase, or set the same passphrase. When disabled, falls back to no passphrase if the passphrases do not match.", true);
    addIntOpt(pp, "peeridletimeo", "Peer Idle Timeout", "Time to wait, in milliseconds, before the connection is considered broken if the peer does not respond.", 5000);
    addStrOpt(pp, "packetfilter", "Packet Filter", "Sets the SRT packet filter string, see SRT library documentation for details.");

    JSON::Value opt;
    opt["arg"] = "string";
    opt["default"] = "";
    opt["arg_num"] = 1;
    opt["help"] = "Target srt:// URL to push out towards.";
    cfg->addOption("target", opt);
  }

  // Buffers TS packets and sends after 7 are buffered.
  void OutTSSRT::sendTS(const char *tsData, size_t len){
    packetBuffer.append(tsData, len);
    if (packetBuffer.size() >= 1316){//7 whole TS packets
      if (!srtConn){
        if (config->getString("target").size()){
          INFO_MSG("Reconnecting...");
          srtConn.connect(target.host, target.getPort(), "output", targetParams);
          if (!srtConn){Util::sleep(500);}
        }else{
          Util::logExitReason("SRT connection closed");
          myConn.close();
          parseData = false;
          return;
        }
      }
      if (srtConn){
        srtConn.SendNow(packetBuffer, packetBuffer.size());
        if (!srtConn){
          if (!config->getString("target").size()){
            Util::logExitReason("SRT connection closed");
            myConn.close();
            parseData = false;
          }
        }
      }
      packetBuffer.assign(0,0);
    }
  }

  void OutTSSRT::requestHandler(){
    size_t recvSize = srtConn.Recv();
    if (!recvSize){
      if (!srtConn){
        myConn.close();
        wantRequest = false;
      }else{
        Util::sleep(50);
      }
      return;
    }
    lastRecv = Util::bootSecs();
    if (!assembler.assemble(tsIn, srtConn.recvbuf, recvSize, true)){return;}
    while (tsIn.hasPacket()){
      tsIn.getEarliestPacket(thisPacket);
      if (!thisPacket){
        INFO_MSG("Could not get TS packet");
        myConn.close();
        wantRequest = false;
        return;
      }

      tsIn.initializeMetadata(meta);
      size_t thisIdx = M.trackIDToIndex(thisPacket.getTrackId(), getpid());
      if (thisIdx == INVALID_TRACK_ID){return;}
      if (!userSelect.count(thisIdx)){
        userSelect[thisIdx].reload(streamName, thisIdx, COMM_STATUS_SOURCE | COMM_STATUS_DONOTTRACK);
      }

      uint64_t adjustTime = thisPacket.getTime() + timeStampOffset;
      if (lastTimeStamp || timeStampOffset){
        if (lastTimeStamp + 5000 < adjustTime || lastTimeStamp > adjustTime + 5000){
          INFO_MSG("Timestamp jump " PRETTY_PRINT_MSTIME " -> " PRETTY_PRINT_MSTIME ", compensating.",
                   PRETTY_ARG_MSTIME(lastTimeStamp), PRETTY_ARG_MSTIME(adjustTime));
          timeStampOffset += (lastTimeStamp - adjustTime);
          adjustTime = thisPacket.getTime() + timeStampOffset;
        }
      }
      lastTimeStamp = adjustTime;
      thisPacket.setTime(adjustTime);
      bufferLivePacket(thisPacket);
    }
  }

  void OutTSSRT::connStats(uint64_t now, Comms::Statistics &statComm){
    if (!srtConn){return;}
    statComm.setUp(srtConn.dataUp());
    statComm.setDown(srtConn.dataDown());
    statComm.setTime(now - srtConn.connTime());
    statComm.setPacketCount(srtConn.packetCount());
    statComm.setPacketLostCount(srtConn.packetLostCount());
    statComm.setPacketRetransmitCount(srtConn.packetRetransmitCount());
  }

}// namespace Mist