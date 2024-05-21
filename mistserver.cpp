#include "../src/input/input_hls.h"
#include "../src/input/input_dtsc.h"
#include "../src/input/input_mp3.h"
#include "../src/input/input_flv.h"
#include "../src/input/input_ogg.h"
#include "../src/input/input_buffer.h"
#include "../src/input/input_h264.h"
#include "../src/input/input_ebml.h"
#include "../src/input/input_ismv.h"
#include "../src/input/input_mp4.h"
#include "../src/input/input_ts.h"
#include "../src/input/input_folder.h"
#include "../src/input/input_playlist.h"
#include "../src/input/input_balancer.h"
#include "../src/input/input_rtsp.h"
#include "../src/input/input_subrip.h"
#include "../src/input/input_sdp.h"
#include "../src/input/input_aac.h"
#include "../src/input/input_flac.h"
#include "../src/output/output_rtmp.h"
#include "../src/output/output_dtsc.h"
#include "../src/output/output_ogg.h"
#include "../src/output/output_flv.h"
#include "../src/output/output_http_minimalserver.h"
#include "../src/output/output_mp4.h"
#include "../src/output/output_aac.h"
#include "../src/output/output_flac.h"
#include "../src/output/output_mp3.h"
#include "../src/output/output_h264.h"
#include "../src/output/output_hds.h"
#include "../src/output/output_json.h"
#include "../src/output/output_ts.h"
#include "../src/output/output_httpts.h"
#include "../src/output/output_hls.h"
#include "../src/output/output_cmaf.h"
#include "../src/output/output_ebml.h"
#include "../src/output/output_rtsp.h"
#include "../src/output/output_wav.h"
#include "../src/output/output_http_internal.h"
#include "../src/output/output_jsonline.h"
#include "../src/output/output_https.h"
#include <mist/config.h>
#include <mist/defines.h>
#include <mist/socket.h>
#include <mist/util.h>
#include <mist/stream.h>
#include "src/session.h"
#include "go-code/go-code.h"
#include "src/controller/controller.h"
#include "src/output/mist_out.cpp"
#include "src/input/mist_in.cpp"
#ifdef ONE_LIBRARY
extern "C" { int mistServerMain(int argc, char *argv[]){
#else
int main(int argc, char *argv[]){
#endif
  AquareumMain();
  return 0;
  if (argc < 2) {
  }
  // Create a new argv array without argv[1]
  int new_argc = argc - 1;
  char** new_argv = new char*[new_argc];
  for (int i = 0, j = 0; i < argc; ++i) {
      if (i != 1) {
          new_argv[j++] = argv[i];
      }
  }
  if (strcmp(argv[1], "MistController") == 0) {
    return ControllerMain(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInHLS") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputHLS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInDTSC") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputDTSC>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInMP3") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputMP3>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInFLV") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputFLV>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInOGG") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputOGG>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInBuffer") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputBuffer>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInH264") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputH264>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInEBML") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputEBML>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInISMV") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputISMV>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInMP4") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputMP4>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInTS") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputTS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInFolder") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputFolder>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInPlaylist") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputPlaylist>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInBalancer") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputBalancer>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInRTSP") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputRTSP>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInSubRip") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputSubRip>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInSDP") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputSDP>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInAAC") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputAAC>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistInFLAC") == 0) {
    program_invocation_short_name = argv[1];    return InputMain<Mist::InputFLAC>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutRTMP") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutRTMP>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutDTSC") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutDTSC>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutOGG") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutOGG>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutFLV") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutFLV>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutHTTPMinimalServer") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutHTTPMinimalServer>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutMP4") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutMP4>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutAAC") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutAAC>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutFLAC") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutFLAC>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutMP3") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutMP3>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutH264") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutH264>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutHDS") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutHDS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutJSON") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutJSON>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutTS") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutTS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutHTTPTS") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutHTTPTS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutHLS") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutHLS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutCMAF") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutCMAF>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutEBML") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutEBML>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutRTSP") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutRTSP>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutWAV") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutWAV>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutHTTP") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutHTTP>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutJSONLine") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutJSONLine>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistOutHTTPS") == 0) {
    program_invocation_short_name = argv[1];    return OutputMain<Mist::OutHTTPS>(new_argc, new_argv);
  }
  else if (strcmp(argv[1], "MistSession") == 0) {
    return SessionMain(new_argc, new_argv);
  }
  else {
    program_invocation_short_name = (char *)"MistController";
    return ControllerMain(argc, argv);
  }
  INFO_MSG("binary not found: %s", argv[1]);
  return 202;
}
#ifdef ONE_LIBRARY
}
#endif