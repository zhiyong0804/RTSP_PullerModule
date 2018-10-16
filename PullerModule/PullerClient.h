/**
 * @file PullerClient.h
 * @brief  Puller Client 
 * @author lizhiyong0804319@gmail.com
 * @version 1.0
 * @date 2015-11-12
 */

#ifndef PULLER_CLIENT_H
#define PULLER_CLIENT_H

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "API_PullerModule.h"

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
  StreamClientState();
  void release();
  virtual ~StreamClientState();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

#include <pthread.h>
#include <string>

class PullerClient: public RTSPClient {
public:
  static PullerClient* createNew(UsageEnvironment& env, char const* rtspURL,
				  int verbosityLevel = 0,
				  char const* applicationName = NULL,
				  portNumBits tunnelOverHTTPPortNum = 0);

  int setCallbackFunc(PullerCallback cbFunc, void* cbParam);
  PullerCallback getCallbackFunc() const {return m_callbackFunc;}
  void* getCallbackFuncParam() const {return m_cbParam;}
  Boolean retRtpPkt() const {return m_retRtpPkt;}
  Boolean usingTcpData() const { return m_connType == RTP_OVER_TCP ? true:false; }

  int startStream(const char* url, int connType, const char* username, const char* password, int reconn, Boolean retRtpPkt);
  int closeStream();

  void resetUrl() { setBaseURL(m_url.data()); }
  void parseMediaAttr(char* sdpString) const;

protected:
  PullerClient(UsageEnvironment& env, char const* rtspURL,
		int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
  virtual ~PullerClient();

  static void processAfterDescribe(RTSPClient* rtspClient, int resultCode, char* resultString);
  static void processAfterSetup(RTSPClient* rtspClient, int resultCode, char* resultString);
  static void processAfterPlay(RTSPClient* rtspClient, int resultCode, char* resultString);
  static void setupNextSubsession(RTSPClient* rtspClient);
  
  static void subsessionAfterPlaying(void* clientData);
  static void subsessionByeHandler(void* clientData);
  static void streamTimerHandler(void* clietData);

  static void* entryPoint(void* param);
  virtual void run();

  void teardownStream(int resultCode, char* resultString);
public:
  StreamClientState fScs;

private:
  char m_running;
  pthread_t m_tid;
  PullerCallback m_callbackFunc;
  void* m_cbParam;
  Boolean m_retRtpPkt;
  std::string m_url;
  int m_connType;
};

#endif
