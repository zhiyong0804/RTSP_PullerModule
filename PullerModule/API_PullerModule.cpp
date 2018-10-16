/**
 * @file RTSP_PullerModule.cpp
 * @brief  1.0
 *
 * @author lizhiyong0804319@gmail.com
 * @version 1.0
 * @date 2015-11-11
 */

#include "API_PullerModule.h"
#include "PullerClient.h"

#define RTSP_CLIENT_VERBOSITY_LEVEL 0 // by default, print verbose output from each "RTSPClient"


_API int _APICALL RTSP_Puller_GetErrcode()
{
	return 0;
}

_API RTSP_Puller_Handler _APICALL RTSP_Puller_Create()
{
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
	
	char const*  application = "PullerClient";
	char const*  url  = NULL;
	PullerClient* handler = PullerClient::createNew(*env, url, RTSP_CLIENT_VERBOSITY_LEVEL, application);
	if (handler == NULL) {
		*env << "Failed to create a RTSP client for URL \"" << url << "\": " << env->getResultMsg() << "\n";
	}

	return handler;
}

_API int _APICALL RTSP_Puller_SetCallback(RTSP_Puller_Handler handler, PullerCallback cb, void* p)
{
	PullerClient* puller = (PullerClient*) handler;
	return puller->setCallbackFunc(cb, p);
}

_API int _APICALL RTSP_Puller_StartStream(RTSP_Puller_Handler handler, const char* url, \
		RTP_ConnectType connType, const char* username, const char* password, int reconn, int retRtpPkt)
{
	PullerClient* puller = (PullerClient*) handler;
	return puller->startStream(url, connType, username, password, reconn, retRtpPkt);
}

_API int _APICALL RTSP_Puller_CloseStream(RTSP_Puller_Handler handler)
{
	PullerClient* puller = (PullerClient*) handler;
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
	return puller->closeStream();
}

_API int _APICALL RTSP_Puller_Release(RTSP_Puller_Handler handler)
{
	PullerClient* puller = (PullerClient*) handler;
	if (puller != NULL)
	{
		UsageEnvironment& env = puller->envir(); // alias
		TaskScheduler* scheduler = &env.taskScheduler();
		Medium::close(puller);puller = NULL;
		env.reclaim(); 
		delete scheduler; 
	}
	return 0;
}
