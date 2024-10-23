/// \file timing.cpp
/// Utilities for handling time and timestamps.
#define _XOPEN_SOURCE // Ensures strptime works in Cygwin
#include "timing.h"
#include <cstdio>
#include <cstring>
#include <sys/time.h> //for gettimeofday
#include <sys/stat.h>
#include <time.h>     //for time and nanosleep
#include <stdlib.h>

// emulate clock_gettime() for OSX compatibility
#if defined(__APPLE__) || defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
void clock_gettime(int ign, struct timespec *ts){
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), ign, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  ts->tv_sec = mts.tv_sec;
  ts->tv_nsec = mts.tv_nsec;
}
#endif

/// Sleeps for the indicated amount of milliseconds or longer.
/// Will not sleep if ms is negative.
/// Will not sleep for longer than 10 minutes (600000ms).
/// If interrupted by signal, resumes sleep until at least ms milliseconds have passed.
/// Can be slightly off (in positive direction only) depending on OS accuracy.
void Util::wait(int64_t ms){
  if (ms < 0){return;}
  if (ms > 600000){ms = 600000;}
  uint64_t start = getMS();
  uint64_t now = start;
  while (now < start + ms){
    sleep(start + ms - now);
    now = getMS();
  }
}

/// Sleeps for roughly the indicated amount of milliseconds.
/// Will not sleep if ms is negative.
/// Will not sleep for longer than 100 seconds (100000ms).
/// Can be interrupted early by a signal, no guarantee of minimum sleep time.
/// Can be slightly off depending on OS accuracy.
void Util::sleep(int64_t ms){
  if (ms < 0){return;}
  if (ms > 100000){ms = 100000;}
  struct timespec T;
  T.tv_sec = ms / 1000;
  T.tv_nsec = 1000000 * (ms % 1000);
  nanosleep(&T, 0);
}

/// Sleeps for roughly the indicated amount of microseconds.
/// Will not sleep if ms is negative.
/// Will not sleep for longer than 0.1 seconds (100000us).
/// Can be interrupted early by a signal, no guarantee of minimum sleep time.
/// Can be slightly off depending on OS accuracy.
void Util::usleep(int64_t us){
  if (us < 0){return;}
  if (us > 100000){us = 100000;}
  struct timespec T;
  T.tv_sec = 0;
  T.tv_nsec = 1000 * us;
  nanosleep(&T, 0);
}

uint64_t Util::getNTP(){
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return ((uint64_t)(t.tv_sec + 2208988800ull) << 32) + (t.tv_nsec * 4.2949);
}

/// Gets the current time in milliseconds.
uint64_t Util::getMS(){
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

uint64_t Util::bootSecs(){
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec;
}

uint64_t Util::bootMS(){
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return ((uint64_t)t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}

uint64_t Util::unixMS(){
  struct timeval t;
  gettimeofday(&t, 0);
  return ((uint64_t)t.tv_sec) * 1000 + t.tv_usec / 1000;
}

/// Gets the current time in microseconds.
uint64_t Util::getMicros(){
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

/// Gets the time difference in microseconds.
uint64_t Util::getMicros(uint64_t previous){
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000 - previous;
}

/// Gets the amount of seconds since 01/01/1970.
uint64_t Util::epoch(){
  return time(0);
}

uint64_t Util::ISO8601toUnixmillis(const std::string &ts){
  // Format examples:
  //  2019-12-05T09:41:16.765000+00:00
  //  2019-12-05T09:41:16.765Z
  uint64_t unxTime = 0;
  const size_t T = ts.find('T');
  if (T == std::string::npos){
    //ERROR_MSG("Timestamp is date-only (no time marker): %s", ts.c_str());
    return 0;
  }
  const size_t Z = ts.find_first_of("Z+-", T);
  const std::string date = ts.substr(0, T);
  const std::string time = ts.substr(T + 1, Z - T - 1);
  const std::string zone = ts.substr(Z);
  unsigned long year, month, day;
  if (sscanf(date.c_str(), "%lu-%lu-%lu", &year, &month, &day) != 3){
    //ERROR_MSG("Could not parse date: %s", date.c_str());
    return 0;
  }
  unsigned int hour, minute;
  double seconds;
  if (sscanf(time.c_str(), "%u:%d:%lf", &hour, &minute, &seconds) != 3){
    //ERROR_MSG("Could not parse time: %s", time.c_str());
    return 0;
  }
  // Fill the tm struct with the values we just read.
  // We're ignoring time zone for now, and forcing seconds to zero since we add them in later with more precision
  struct tm tParts;
  tParts.tm_sec = 0;
  tParts.tm_min = minute;
  tParts.tm_hour = hour;
  tParts.tm_mon = month - 1;
  tParts.tm_year = year - 1900;
  tParts.tm_mday = day;
  tParts.tm_isdst = 0;
  // convert to unix time, in seconds
  unxTime = timegm(&tParts);
  // convert to milliseconds
  unxTime *= 1000;
  // finally add the seconds (and milliseconds)
  unxTime += (seconds * 1000);

  // Now, adjust for time zone if needed
  if (zone.size() && zone[0] != 'Z'){
    bool sign = (zone[0] == '+');
    {
      unsigned long hrs, mins;
      if (sscanf(zone.c_str() + 1, "%lu:%lu", &hrs, &mins) == 2){
        if (sign){
          unxTime += mins * 60000 + hrs * 3600000;
        }else{
          unxTime -= mins * 60000 + hrs * 3600000;
        }
      }else if (sscanf(zone.c_str() + 1, "%lu", &hrs) == 1){
        if (hrs > 100){
          if (sign){
            unxTime += (hrs % 100) * 60000 + ((uint64_t)(hrs / 100)) * 3600000;
          }else{
            unxTime -= (hrs % 100) * 60000 + ((uint64_t)(hrs / 100)) * 3600000;
          }
        }else{
          if (sign){
            unxTime += hrs * 3600000;
          }else{
            unxTime -= hrs * 3600000;
          }
        }
      }else{
        //WARN_MSG("Could not parse time zone '%s'; assuming UTC", zone.c_str());
      }
    }
  }
  //DONTEVEN_MSG("Time '%s' = %" PRIu64, ts.c_str(), unxTime);
  return unxTime;
}


std::string Util::getUTCString(uint64_t epoch){
  if (!epoch){epoch = time(0);}
  time_t rawtime = epoch;
  struct tm *ptm;
  ptm = gmtime(&rawtime);
  char result[20];
  snprintf(result, 20, "%.4u-%.2u-%.2uT%.2u:%.2u:%.2u", (ptm->tm_year + 1900)%10000, (ptm->tm_mon + 1)%100, ptm->tm_mday%100, ptm->tm_hour%100, ptm->tm_min%100, ptm->tm_sec%100);
  return std::string(result);
}

std::string Util::getUTCStringMillis(uint64_t epoch_millis){
  if (!epoch_millis){epoch_millis = unixMS();}
  time_t rawtime = epoch_millis/1000;
  struct tm *ptm;
  ptm = gmtime(&rawtime);
  char result[25];
  snprintf(result, 25, "%.4u-%.2u-%.2uT%.2u:%.2u:%.2u.%.3uZ", (ptm->tm_year + 1900)%10000, (ptm->tm_mon + 1)%100, ptm->tm_mday%100, ptm->tm_hour%100, ptm->tm_min%100, ptm->tm_sec%100, (unsigned int)(epoch_millis%1000));
  return std::string(result);
}

// Returns the epoch of a UTC string in the format of %Y-%m-%dT%H:%M:%S%z
uint64_t Util::getMSFromUTCString(std::string UTCString){
  if (UTCString.size() < 24){return 0;}
  // Strip milliseconds
  std::string millis = UTCString.substr(UTCString.rfind('.') + 1, 3);
  UTCString = UTCString.substr(0, UTCString.rfind('.')) + "Z";
  struct tm ptm;
  memset(&ptm, 0, sizeof(struct tm));
  strptime(UTCString.c_str(), "%Y-%m-%dT%H:%M:%S%z", &ptm);
  time_t ts = mktime(&ptm);
  return ts * 1000 + atoll(millis.c_str());
}

// Converts epoch_millis into UTC time and returns the diff with UTCString in seconds
uint64_t Util::getUTCTimeDiff(std::string UTCString, uint64_t epochMillis){
  if (!epochMillis){return 0;}
  if (UTCString.size() < 24){return 0;}
  // Convert epoch to UTC time
  time_t epochSeconds = epochMillis / 1000;
  struct tm *ptmEpoch;
  ptmEpoch = gmtime(&epochSeconds);
  uint64_t epochTime = mktime(ptmEpoch);
  // Parse UTC string and strip the milliseconds
  UTCString = UTCString.substr(0, UTCString.rfind('.')) + "Z";
  struct tm ptmUTC;
  memset(&ptmUTC, 0, sizeof(struct tm));
  strptime(UTCString.c_str(), "%Y-%m-%dT%H:%M:%S%z", &ptmUTC);
  time_t UTCTime = mktime(&ptmUTC);
  return epochTime - UTCTime;
}

std::string Util::getDateString(uint64_t epoch){
  char buffer[80];
  time_t rawtime = epoch;
  if (!epoch) {
    time(&rawtime);
  }
  struct tm * timeinfo;
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %z", timeinfo);
  return std::string(buffer);
}

/// Gets unix time of last file modification, or 0 if this information is not available for any reason
uint64_t Util::getFileUnixTime(const std::string & filename){
  struct stat fInfo;
  if (stat(filename.c_str(), &fInfo) == 0){
    return fInfo.st_mtime;
  }
  return 0;
}

