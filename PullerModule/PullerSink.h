/**
 * @file PullerSink.h
 * @brief  Puller Sink 
 * @author lizhiyong0804319@gmail.com
 * @version 1.0
 * @date 2015-11-12
 */

#ifndef PULLER_SINK_H
#define PULLER_SINK_H

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "API_PullerModule.h"

class PullerSink: public MediaSink {
public:
  static PullerSink* createNew(UsageEnvironment& env,
			      MediaSubsession& subsession, // identifies the kind of data that's being received
			      char const* streamId = NULL); // identifies the stream itself (optional)

  int setCallbackFunc(PullerCallback cbFunc, void* cbParam);
private:
  PullerSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId);
    // called only by "createNew()"
  virtual ~PullerSink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
				struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
			 struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  MediaSubsession& fSubsession;
  char* fStreamId;
  PullerCallback m_callbackFunc;
  void* m_cbParam;
};


#endif

