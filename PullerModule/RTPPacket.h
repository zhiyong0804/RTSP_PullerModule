/*====================================================================
 * 
 * @file		RTPPacket.h
 * @author		lizhiyong0804319@gmail.com
 * @date		Wed Aug 26 01:42:59 2015
 * 
 * Copyright (c) 2015 lizhiyong0804319@gmail.com
 * 
 * @brief		modified by Zhiyong Li
 *				Some useful things for parsing RTPPackets.
 *  
 * 
 *===================================================================*/

#ifndef _RTPPACKET_H_
#define _RTPPACKET_H_

#ifndef __MacOSX__ 
#include "utils/cpu_arch.h"
#else
typedef unsigned char byte_t;
#endif

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

class RTPPacket
{
    public:
        enum {
           RTP_VERSION = 2,

           RTCP_SR   = 200,
           RTCP_RR   = 201,
           RTCP_SDES = 202,
           RTCP_BYE  = 203,
           RTCP_APP  = 204
        };

        /*
         * RTP data header
         */
#pragma pack(1)
		struct RTPHeader {
 #if 0
           //rtp header
           unsigned int version:2;          /* protocol version */
           unsigned int p:1;                /* padding flag */
           unsigned int x:1;                /* header extension flag */
           unsigned int cc:4;               /* CSRC count */
           unsigned int m:1;                /* marker bit */
           unsigned int pt:7;               /* payload type */
 #endif
           uint16_t rtpheader;
		   uint16_t seq;				        /* sequence number */
           uint32_t ts;                       /* timestamp */
           uint32_t ssrc;                     /* synchronization source */
           //uint32_t csrc[1];                /* optional CSRC list */
        };
#pragma pack()

        RTPPacket(char *inPacket = NULL, uint32_t inLen = 0)
        :   fPacket(reinterpret_cast<RTPHeader *>(inPacket)), fLen(inLen), fAlloc(false)
        {}

		RTPPacket(RTPPacket* pkt, bool bAlloc = false)
		:	fAlloc(bAlloc)
		{
			fLen = pkt->fLen;
			if (fAlloc)
			{
				fPacket = reinterpret_cast<RTPHeader *>(new byte_t[fLen]);
				memcpy(fPacket, pkt->fPacket, fLen);
			}
			else
			{
				fPacket = pkt->fPacket;
			}
		}

		~RTPPacket() 
		{
			if (fAlloc)
				delete[] fPacket;
		}

		byte_t		GetPayloadType() const									{ return ntohs(fPacket->rtpheader) & 0x007F; }
		byte_t		GetCSRCCount() const									{ return (ntohs(fPacket->rtpheader) & 0x0F00 ) >> 8; }

        //The following get functions will convert from network byte order to host byte order.
        //Conversely the set functions will convert from host byte order to network byte order.
        uint16_t		GetSeqNum() const                                       { return ntohs(fPacket->seq); }
        void		SetSeqNum(uint16_t seqNum)                                { fPacket->seq = htons(seqNum); }

        uint32_t		GetTimeStamp() const                                    { return ntohl(fPacket->ts); }
        void		SetTimeStamp(uint32_t timeStamp)                          { fPacket->ts = htonl(timeStamp); }

        uint32_t		GetSSRC() const                                         { return ntohl(fPacket->ssrc); }
        void		SetSSRC(uint32_t SSRC)                                    { fPacket->ssrc = htonl(SSRC); }
		
		//Includes the variable CSRC portion
		uint32_t		GetHeaderLen() const									{ return sizeof(RTPHeader) + GetCSRCCount() * 4; }
		
		char*	GetBody(int &len) const											{ len = fLen - GetHeaderLen(); return reinterpret_cast<char *>(fPacket) + GetHeaderLen(); }

        //Returns true if the header is not bad; do some very basic checking
        bool  HeaderIsValid() const
        {
			assert(sizeof(RTPHeader) == 12);
            if (fLen < sizeof(RTPHeader))
                return false; 
            if ( ( ntohs(fPacket->rtpheader) >> 14)  != RTP_VERSION )
                return false;
			if (GetHeaderLen() > fLen)
				return false;
            return true;
        }

        RTPHeader * fPacket;
        uint32_t      fLen;				//total length of the packet, including the header
	private:		
		bool fAlloc;
};

#endif
