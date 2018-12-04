#include <mist/defines.h>
#include <mist/stream.h>
#include <mist/http_parser.h>
#include <mist/encode.h>
#include "input_balancer.h"

namespace Mist {
  inputBalancer::inputBalancer(Util::Config * cfg) : Input(cfg) {
    capa["name"] = "Balancer";
    capa["desc"] = "The load balancer input restarts itself as the input a load balancer tells it it should be. The syntax is in the form 'balance:http://HOST:PORT[?fallback=FALLBACK]', where HOST and PORT are the host and port of the load balancer and the FALLBACK is the full source URL that should be used if the load balancer cannot be reached.";
    capa["source_match"] = "balance:*";
    capa["priority"] = 9ll;
    capa["morphic"] = 1ll;
  }

  int inputBalancer::boot(int argc, char * argv[]){
    if (!config->parseArgs(argc, argv)){return 1;}
    if (config->getBool("json")){return Input::boot(argc, argv);}
    
    streamName = config->getString("streamname");
    
    std::string blncr = config->getString("input");
    if (blncr.substr(0, 8) != "balance:"){
      FAIL_MSG("Input must start with \"balance:\"");
      return 1;
    }

    HTTP::URL url(blncr.substr(8));
    if (url.protocol != "http"){
      FAIL_MSG("Load balancer protocol %s is not supported", url.protocol.c_str());
      return 1;
    }

    std::string source; //empty by default

    //Parse fallback from URL arguments, if possible.
    if (url.args.size()){
      std::map<std::string, std::string> args;
      HTTP::parseVars(url.args, args);
      if (args.count("fallback")){source = args.at("fallback");}
    }


    Socket::Connection balConn(url.host, url.getPort(), true);
    if (!balConn){
      WARN_MSG("Failed to reach %s on port %lu", url.host.c_str(), url.getPort());
    }else{
      HTTP::Parser http;
      http.url = "/" + url.path;
      http.SetVar("source", streamName);
      if (source.size()){
        http.SetVar("fallback", source);
      }
      http.method = "GET";
      http.SetHeader("Host", url.host);
      http.SetHeader("X-MistServer", PACKAGE_VERSION);
      balConn.SendNow(http.BuildRequest());
      http.Clean();

      unsigned int startTime = Util::epoch();
      while ((Util::epoch() - startTime < 10) && (balConn || balConn.Received().size())){
        if (balConn.spool() || balConn.Received().size()){
          if (http.Read(balConn.Received().get())){
            HTTP::URL newUrl(http.body);
            if (Socket::isLocalhost(newUrl.host)){
              WARN_MSG("Load balancer returned a local address - ignoring");
              startTime = 0;
              break;//break out of while loop, ignore return value - it's local.
            }
            source = http.body;
            startTime = 0;//note success
            break;//break out of while loop
          }
        }
      }
      if (startTime){
        FAIL_MSG("Timeout while trying to contact load balancer at %s!", blncr.c_str()+8);
      }
      balConn.close();
    }
    
    if (!source.size()){
      FAIL_MSG("Could not determine source to use for %s", streamName.c_str());
      return 1;
    }

    //Attempt to boot the source we got
    Util::startInput(streamName, source, false, getenv("MISTPROVIDER"));
    return 1;
  }

}
