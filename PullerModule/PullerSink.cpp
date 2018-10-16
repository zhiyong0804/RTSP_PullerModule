/**
 * @file PullerSink.cpp
 * @brief  1.0
 *		implementation of PullerSink
 * @author lizhiyong0804319@gmail.com
 * @version 1.0
 * @date 2015-11-12
 */

#include "PullerSink.h"
#include "API_PullerTypes.h"

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000

PullerSink* PullerSink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId) {
  return new PullerSink(env, subsession, streamId);
}

PullerSink::PullerSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env),
    fSubsession(subsession), m_callbackFunc(NULL) {
  fStreamId = strDup(streamId);
  fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

PullerSink::~PullerSink() {
  delete[] fReceiveBuffer;
  delete[] fStreamId;
}

int PullerSink::setCallbackFunc(PullerCallback cbFunc, void* cbParam)
{
	m_callbackFunc = cbFunc;
	m_cbParam = cbParam;
	return 0;
}

void PullerSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
  PullerSink* sink = (PullerSink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void PullerSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
  // We've just received a frame of data.  (Optionally) print out information about it:
#ifdef DEBUG_PRINT
  if (fStreamId != NULL) envir() << "Stream \"" << fStreamId << "\"; ";
  envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
  if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
  char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
  sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
  envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
  if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
    envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
  }
#ifdef DEBUG_PRINT_NPT
  envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
#endif
  envir() << "\n";
#endif

  if (frameSize != 0)
  {
    RTPData rtpData;
    rtpData.dataBuf = (char*)fReceiveBuffer;
    rtpData.bufLen = frameSize;

    m_callbackFunc(CB_RTP_DATA, &rtpData, m_cbParam); 
    // Then continue, to request the next frame of data:
    continuePlaying();  
  }
  else
  {
    m_callbackFunc(CB_CONNECTION_BROKEN, 0, m_cbParam); 
  }
}

Boolean PullerSink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}
