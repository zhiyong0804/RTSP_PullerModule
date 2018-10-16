#include "API_PullerModule.h"
#include "RTPPacket.h"
#include <stdio.h>

//#include "AudioProcessor.h"
static int PullerCallbackFunc(CBDataType dataType, void* data, void* obj);

int main(int argc, char* argv[])
{
	char url[1024] = {0};
	
	if (argc < 3)
	{
		printf("./PullerModuleTest <host> <session> \n");
		return 0;
	}

	sprintf(url, "rtsp://%s/%s.sdp", argv[1], argv[2]);
	printf("%s\n", url);
	RTSP_Puller_Handler handler = RTSP_Puller_Create();
	RTSP_Puller_SetCallback(handler, PullerCallbackFunc, NULL);
	RTSP_Puller_StartStream(handler, url, RTP_OVER_TCP, "4", "admin", 0, 1);
//	CAudioProcessor::instance()->start();

	getchar();
	
//	CAudioProcessor::instance()->stop();
//	CAudioProcessor::instance()->destroyProcessor();
	RTSP_Puller_CloseStream(handler);
	RTSP_Puller_Release(handler);

	return 0;
}

int PullerCallbackFunc(CBDataType dataType, void* data, void* obj)
{
	if (dataType == CB_PULLER_STATE)
	{
		PullerState* pullerState = (PullerState*) data;
		printf("resultCode:%d, resultString:%s\n", pullerState->resultCode, pullerState->resultString);
	}

	if (dataType == CB_MEDIA_ATTR)
	{
		MediaAttr* mediaAttr = (MediaAttr*) data;
		printf("audioCodec:%u,audioSamplerate:%u,audioChannel:%u\n",\
				mediaAttr->audioCodec, mediaAttr->audioSamplerate, mediaAttr->audioChannel);
	}
	
	if (dataType == CB_RTP_DATA)
	{
		RTPData* rtpData = (RTPData*) data;

		RTPPacket rtpPkt(rtpData->dataBuf, rtpData->bufLen);
		if (rtpPkt.HeaderIsValid())	
		{
			int size = 0;
			char* buf = rtpPkt.GetBody(size);
//			CAudioProcessor::instance()->deliverAudioData((uint8_t*)buf+4, size-4);
			printf("recevie a valid rtp packet: payload size[%d] \n", size);
		}	
		else 
			printf("recevie an invalid packet \n");
	}

    if (dataType == CB_CONNECTION_BROKEN)
    {
        printf("net connection broken. \n ");    
    }
    
    return 0;
}
