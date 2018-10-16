#include "PullerClient.h"
#include "PullerSink.h"

#include "RTPSource.hh"

#include <string>
using namespace std;

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

// Implementation of the RTSP 'response handlers':

void PullerClient::processAfterDescribe(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((PullerClient*)rtspClient)->fScs; // alias

    if (resultCode != 0) {
#ifdef DEBUG_PRINT
		env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
#endif
		break;
    }

	PullerClient* client = dynamic_cast<PullerClient*>(rtspClient);
	client->parseMediaAttr(resultString);
	client->resetUrl();
    
	char* const sdpDescription = resultString;
#ifdef DEBUG_PRINT
    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";
#endif

	// Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (scs.session == NULL) {
#ifdef DEBUG_PRINT
      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
#endif
      break;
    } else if (!scs.session->hasSubsessions()) {
#ifdef DEBUG_PRINT
      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
#endif
	  break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  ((PullerClient*)rtspClient)->teardownStream(resultCode, resultString);
}

void PullerClient::setupNextSubsession(RTSPClient* rtspClient) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((PullerClient*)rtspClient)->fScs; // alias
  
  scs.subsession = scs.iter->next();
  if (scs.subsession != NULL) {
    if (!scs.subsession->initiate()) {
#ifdef DEBUG_PRINT
      env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
#endif
	  setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    } else {
#ifdef DEBUG_PRINT
      env << *rtspClient << "Initiated the \"" << *scs.subsession
	  << "\" subsession (client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1 << ")\n";
#endif
      // Continue setting up this subsession, by sending a RTSP "SETUP" command:
	  PullerClient* client = dynamic_cast<PullerClient*>(rtspClient);
      rtspClient->sendSetupCommand(*scs.subsession, processAfterSetup, false, client->usingTcpData());//tcp or udp
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL) {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, processAfterPlay, scs.session->absStartTime(), scs.session->absEndTime());
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, processAfterPlay);
  }
}

void PullerClient::processAfterSetup(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((PullerClient*)rtspClient)->fScs; // alias

    if (resultCode != 0) {
#ifdef DEBUG_PRINT
      env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
#endif
	  break;
    }

#ifdef DEBUG_PRINT
    env << *rtspClient << "Set up the \"" << *scs.subsession
	<< "\" subsession (client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1 << ")\n";
#endif
	
    // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
    // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
    // after we've sent a RTSP "PLAY" command.)

    scs.subsession->sink = PullerSink::createNew(env, *scs.subsession, rtspClient->url());
      // perhaps use your own custom "MediaSink" subclass instead
    if (scs.subsession->sink == NULL) {
#ifdef DEBUG_PRINT
      env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
	  << "\" subsession: " << env.getResultMsg() << "\n";
#endif
	  break;
    }
	PullerSink* sink = dynamic_cast<PullerSink*>(scs.subsession->sink);
	PullerClient* client = dynamic_cast<PullerClient*>(rtspClient);
	sink->setCallbackFunc(client->getCallbackFunc(), client->getCallbackFuncParam());

	RTPSource* source = dynamic_cast<RTPSource*>(scs.subsession->readSource());
	source->curPacketMarkerBit(client->retRtpPkt());

#ifdef DEBUG_PRINT
    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
#endif
	scs.subsession->miscPtr = rtspClient; // a hack to let subsession handle functions get the "RTSPClient" from the subsession 
    scs.subsession->sink->startPlaying(*source,
				       subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
    }
  } while (0);

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void PullerClient::processAfterPlay(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((PullerClient*)rtspClient)->fScs; // alias

    if (resultCode != 0) {
#ifdef DEBUG_PRINT
	env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
#endif
		break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    if (scs.duration > 0) {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      scs.duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
      scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }
	
#ifdef DEBUG_PRINT
    env << *rtspClient << "Started playing session";
    if (scs.duration > 0) {
      env << " (for up to " << scs.duration << " seconds)";
    }
    env << "...\n";
#endif

	return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  ((PullerClient*)rtspClient)->teardownStream(resultCode, resultString);
}


// Implementation of the other event handlers:

void PullerClient::subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  ((PullerClient*)rtspClient)->teardownStream(600, (char*)"stream closed.");
}

void PullerClient::subsessionByeHandler(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

#ifdef DEBUG_PRINT
  env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";
#endif

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void PullerClient::streamTimerHandler(void* clientData) {
  PullerClient* puller = (PullerClient*)clientData;

  StreamClientState& scs = puller->fScs; // alias
  scs.streamTimerTask = NULL;

  // Shut down the stream:
  ((PullerClient*)puller)->teardownStream(0, NULL);
}

// Implementation of "PullerClient":

PullerClient* PullerClient::createNew(UsageEnvironment& env, char const* rtspURL,
					int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new PullerClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

PullerClient::PullerClient(UsageEnvironment& env, char const* rtspURL,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum), m_running(0), m_tid(0), m_callbackFunc(NULL), m_cbParam(NULL), m_retRtpPkt(false), m_url(""), m_connType(RTP_OVER_TCP) {
}

PullerClient::~PullerClient() {
}

int PullerClient::setCallbackFunc(PullerCallback cbFunc, void* cbParam)
{
	m_callbackFunc = cbFunc;
	m_cbParam = cbParam;
	return 0;
}

int PullerClient::startStream(const char* url, int connType, const char* username, const char* password, int reconn, Boolean retRtpPkt) 
{
  m_running = 0x00;
  m_retRtpPkt = retRtpPkt;
  m_url = url;
  m_connType = connType;
  Authenticator auth(username, password);

  char authUrl[1024] = {0};
  ::sprintf(authUrl, "%s&token=%s", url, username);
  setBaseURL(authUrl);

  sendDescribeCommand(processAfterDescribe, &auth); 

  return pthread_create(&m_tid, NULL, entryPoint, this);
}

int PullerClient::closeStream() {
	
	teardownStream(0, NULL);

	m_running = 0xFF;
	pthread_join(m_tid, NULL);

	return 0;
}

void PullerClient::teardownStream(int resultCode, char* resultString)
{
	// First, check whether any subsessions have still to be closed:
	if (fScs.session != NULL) { 
	    Boolean someSubsessionsWereActive = False;
	    MediaSubsessionIterator iter(*fScs.session);
	    MediaSubsession* subsession;
	
	    while ((subsession = iter.next()) != NULL) {
	      if (subsession->sink != NULL) {
			Medium::close(subsession->sink);
			subsession->sink = NULL;

			if (subsession->rtcpInstance() != NULL) {
			  subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
			}

			someSubsessionsWereActive = True;
	      }
	    }

	    if (someSubsessionsWereActive) {
	      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
	      // Don't bother handling the response to the "TEARDOWN".
	      sendTeardownCommand(*fScs.session, NULL);
	    }
		
	 }
		
	if (m_callbackFunc != NULL && resultCode != 0)
	{
		PullerState pullerState;
		pullerState.resultCode = resultCode;
		pullerState.resultString = resultString;
		m_callbackFunc(CB_PULLER_STATE, &pullerState, m_cbParam);
	}
	
}

void* PullerClient::entryPoint(void* param){	PullerClient* thd = (PullerClient *)param;	thd->run();}

void PullerClient::run()
{
	envir().taskScheduler().doEventLoop(&m_running);
}

void PullerClient::parseMediaAttr(char* sdpString) const
{
	//ex: a=rtpmap:14 MPA/44100/2
	string sdpstr (sdpString);

	size_t pos = sdpstr.find("a=rtpmap:");

	if (pos != string::npos)
	{
		string rtpmap = sdpstr.substr(pos+9);
		char substr[32] = {0};
		
		MediaAttr mediaAttr;
		::sscanf(rtpmap.c_str(), "%u %[^/]/%d/%d\r\n", &mediaAttr.audioCodec,\
				&substr[0], &mediaAttr.audioSamplerate, &mediaAttr.audioChannel);
		
		if (m_callbackFunc != NULL)
			m_callbackFunc(CB_MEDIA_ATTR, &mediaAttr, m_cbParam);
	}
}

// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

void StreamClientState::release()
{
	if (iter != NULL) {
		delete iter;
		iter = NULL;
	}
	
	if (session != NULL) {
		// We also need to delete "session", and unschedule "streamTimerTask" (if set)
	    UsageEnvironment& env = session->envir(); // alias

	    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
	    Medium::close(session);
	}

	subsession = NULL;
}

StreamClientState::~StreamClientState() {
	release();
}
