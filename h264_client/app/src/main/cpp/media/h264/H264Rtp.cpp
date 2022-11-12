
#include <string.h>

#include "media/h264/H264Rtp.h"
#include "base/debug-msg-c.h"

/*!
************************************************************************
* \brief
*    mapping rule for ue(v) syntax elements
* \par Input:
*    lenght and info
* \par Output:
*    number in the code table
************************************************************************
*/
void linfo_ue(int len, int info, int *value1, int *dummy)
{
	*value1 = (1 << (len >> 1)) + info - 1;
}

/*!
************************************************************************
* \brief
*  read one exp-golomb VLC symbol
*
* \param buffer
*    containing VLC-coded data bits
* \param totbitoffset
*    bit offset from start of partition
* \param  info
*    returns the value of the symbol
* \param bytecount
*    buffer length
* \return
*    bits read
************************************************************************
*/
int GetVLCSymbol (unsigned char buffer[],int totbitoffset,int *info, int bytecount)
{

	int inf;
	long byteoffset = (totbitoffset >> 3);         // byte from start of buffer
	int  bitoffset  = (7 - (totbitoffset & 0x07)); // bit from start of byte
	int  bitcounter = 1;
	int  len        = 0;
	unsigned char *cur_byte = &(buffer[byteoffset]);
	int  ctr_bit    = ((*cur_byte) >> (bitoffset)) & 0x01;  // control bit for current bit posision

	while (ctr_bit == 0)
	{                 // find leading 1 bit
		len++;
		bitcounter++;
		bitoffset--;
		bitoffset &= 0x07;
		cur_byte  += (bitoffset == 7);
		byteoffset+= (bitoffset == 7);      
		ctr_bit    = ((*cur_byte) >> (bitoffset)) & 0x01;
	}

	if (byteoffset + ((len + 7) >> 3) > bytecount)
		return -1;

	// make infoword
	inf = 0;                          // shortest possible code is 1, then info is always 0    

	while (len--)
	{
		bitoffset --;    
		bitoffset &= 0x07;
		cur_byte  += (bitoffset == 7);
		bitcounter++;
		inf <<= 1;    
		inf |= ((*cur_byte) >> (bitoffset)) & 0x01;
	}

	*info = inf;
	return bitcounter;           // return absolute offset in bit from start of frame
}

/*!
************************************************************************
* \brief
*    read next UVLC codeword from UVLC-partition and
*    map it to the corresponding syntax element
************************************************************************
*/
int readSyntaxElement_VLC(SyntaxElement *sym, Bitstream *currStream)
{
	int frame_bitoffset        = currStream->frame_bitoffset;
	int BitstreamLengthInBytes = currStream->bitstream_length;
	unsigned char *buf                  = currStream->streamBuffer;

	sym->len =  GetVLCSymbol (buf, frame_bitoffset, &(sym->inf), BitstreamLengthInBytes);
	if (sym->len == -1)
		return -1;
	currStream->frame_bitoffset += sym->len;
	sym->mapping(sym->len,sym->inf,&(sym->value1),&(sym->value2));

	return 1;
}

/*!
*************************************************************************************
* \brief
*    ue_v, reads an ue(v) syntax element, the length in bits is stored in
*    the global UsedBits variable
*
* \param tracestring
*    the string for the trace file
*
* \param bitstream
*    the stream to be read from
*
* \return
*    the value of the coded syntax element
*
*************************************************************************************
*/
int ue_v (char *tracestring, Bitstream *bitstream)
{
	SyntaxElement symbol;

	symbol.type = SE_HEADER;
	symbol.mapping = linfo_ue;   // Mapping rule
	readSyntaxElement_VLC (&symbol, bitstream);

	return symbol.value1;
}


int H264VideoRTPSource::processSpecialHeader(unsigned char *&pbBuff, 
											 int BuffSize, 
											 int &nLenOffset,
											 int &fCurrentPacketBeginsFrame,
											 int &fCurrentPacketCompletesFrame,
											 int recv_packet_max_len)
{
	unsigned char *headerStart		= pbBuff;
	unsigned char *p							= pbBuff;
	unsigned packetSize					= BuffSize;
	int fCurPacketNALUnitType;
	fCurrentPacketBeginsFrame			= 1;
	fCurrentPacketCompletesFrame	= 0;
	nLenOffset									= 0;
	int nIsNeedStartCode					= 0;

	// The header has a minimum size of 0, since the NAL header is used
	// as a payload header
	int expectedHeaderSize = 0;
	int sps_offset = 0;

	// Check if the type field is 28 (FU-A) or 29 (FU-B)
	fCurPacketNALUnitType = (headerStart[0]&0x1F);
	switch (fCurPacketNALUnitType) 
	{
	case 24: 
		{	// STAP-A
			expectedHeaderSize = 1; // discard the type byte
			nIsNeedStartCode = 1;
			break;
		}
	case 25: case 26: case 27: 
		{ // STAP-B, MTAP16, or MTAP24
			expectedHeaderSize = 3; // discard the type byte, and the initial DON
			nIsNeedStartCode = 1;
			break;
		}
	case 28: case 29: 
		{	// FU-A or FU-B
			// For these NALUs, the first two bytes are the FU indicator and the FU header.
			// If the start bit is set, we reconstruct the original NAL header:
			unsigned char startBit = headerStart[1]&0x80;
			unsigned char endBit = headerStart[1]&0x40;
			if (startBit) 
			{
				expectedHeaderSize = 1;
				if (packetSize < (unsigned)expectedHeaderSize) return 0;

				headerStart[1] = (headerStart[0]&0xE0)+(headerStart[1]&0x1F); 
				fCurrentPacketBeginsFrame = 1;
				nIsNeedStartCode = 1;
			} 
			else 
			{
				// If the startbit is not set, both the FU indicator and header
				// can be discarded
				expectedHeaderSize = 2;
				if (packetSize < (unsigned)expectedHeaderSize) return 0;

				fCurrentPacketBeginsFrame = 0;
			}
			fCurrentPacketCompletesFrame = (endBit != 0);
			break;
		}
	default: 
		{
			// This packet contains one or more complete, decodable NAL units
			fCurrentPacketBeginsFrame = fCurrentPacketCompletesFrame = 1;
			nIsNeedStartCode = 1;
			break;
		}
	}

	p += expectedHeaderSize;
	BuffSize -= expectedHeaderSize;

	int resultNALUSize;
	switch (fCurPacketNALUnitType) 
	{
	case 24: case 25: 
		{	// STAP-A or STAP-B
			// The first two bytes are NALU size:
			#define MAX_NAL_LEN 512
			unsigned char pbTemp[MAX_NAL_LEN];
			// unsigned char pbPPS[3]; memset(pbPPS, 0, sizeof(pbPPS));
			int nOffset = 0;
			int i;
			for (i=0; i < BuffSize; i++)
			{
				resultNALUSize = (p[i]<<8)|p[i+1];

				// james: 130201 for user data
				if ( resultNALUSize > MAX_NAL_LEN ) {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d H264VideoRTPprocessSpecialHeader, resultNALUSize=%d, error. \n", __LINE__, resultNALUSize);
					return 0;
				}

//			    DEBUG_TRACE(TRUE, (_T("#%d ProcessSpecialHeader: expectedHeaderSize=%d, resultNALUSize=%d, buff[%d]=0x%x, buff[%d+1]=0x%x. \n"), __LINE__, expectedHeaderSize, resultNALUSize, i, pbBuff[i], i, pbBuff[i+1]));
				if (i != 0) {
					unsigned char pbStartCode[] = {0x00, 0x00, 0x00, 0x01};
					memcpy(pbTemp+nOffset, pbStartCode, 4);
					nOffset += 4;
					expectedHeaderSize -= 4;
					sps_offset -= 2;
				}
				else 
					sps_offset += 2;

				if (nOffset+resultNALUSize > 512) {
					DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d H264VideoRTPprocessSpecialHeader, nOffset+resultNALUSize=%d, error. \n", __LINE__, nOffset+resultNALUSize);
					return 0;
				}

				memcpy(pbTemp+nOffset, p+i+2, resultNALUSize); nOffset += resultNALUSize;
				expectedHeaderSize += 2;

				/*if ((*(p+i+2)&0x1F) == 0x08) {
					break;
				}*/

				i += ((resultNALUSize+2)-1);
			}
			
//			DEBUG_TRACE(TRUE, (_T("#%d ProcessSpecialHeader: expectedHeaderSize=%d, resultNALUSize=%d, nOffset=%d. \n"), __LINE__, expectedHeaderSize, resultNALUSize, nOffset));
			if ((p+sps_offset <=(pbBuff-1024)) ||
				((p+sps_offset+nOffset) > (pbBuff+recv_packet_max_len))) 
			{
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d H264VideoRTPSource.processSpecialHeader, sps_offset=%d,  nOffset=%d, error. \n", __LINE__, sps_offset, nOffset);
				return 0;
			}
			memcpy(p+sps_offset, pbTemp, nOffset);

			break;
		}
	case 26: 
		{	// MTAP16
			// The first two bytes are NALU size.  The next three are the DOND and TS offset:
			if (BuffSize < 5) break;
			resultNALUSize = (p[0]<<8)|p[1];
			p += 5;
			expectedHeaderSize += 5;

			break;
		}
	case 27: 
		{	// MTAP24
			// The first two bytes are NALU size.  The next four are the DOND and TS offset:
			if (BuffSize < 6) break;
			resultNALUSize = (p[0]<<8)|p[1];
			p += 6;
			expectedHeaderSize += 6;

			break;
		}
	}


	if (nIsNeedStartCode) {
		unsigned char pbStartCode[] = {0x00, 0x00, 0x00, 0x01};
		sps_offset -= 4;

		if ((p+sps_offset <=(pbBuff-1024)) ||
			((p+sps_offset+4) > (pbBuff+recv_packet_max_len))) 
		{
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d H264VideoRTPSource.processSpecialHeader, sps_offset=%d, error. \n", __LINE__, sps_offset);
			return 0;
		}

		memcpy(p+sps_offset, pbStartCode, 4);
		expectedHeaderSize -= 4;
	} 
	else {
		if ((p+sps_offset <=(pbBuff-1024)) ||
			((p+sps_offset) > (pbBuff+recv_packet_max_len))) 
		{
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d H264VideoRTPSource.processSpecialHeader, sps_offset=%d, error. \n", __LINE__, sps_offset);
			return 0;
		}
	}

	nLenOffset = expectedHeaderSize;
	p += sps_offset;
	pbBuff = p;


	return 1;
}

H264RTP::H264RTP()
{
};

H264RTP::~H264RTP()
{
}

int H264RTP::GetFrameType(const unsigned char *pbBuff,
							 int nBuffSize, 
							 FRAME_TYPE &eFrameType,
							 int *pnGetOffset)
{
	int i;
	int nIsGetUserData	= 0;
	char *pchPtr1			= (char *)pbBuff;
	int nPtr1Size			= nBuffSize;
	int nTemp					= 0;
	eFrameType				= OTHER_FRAME;
	int nGetOffset			= 0;
	int nSingal				= 0;

	for (i=0; i < nBuffSize; i++) {
		if (i+4 >= nBuffSize) break;
		if ((pchPtr1[i] != 0 || pchPtr1[i+1] != 0 || pchPtr1[i+2] != 0x01) && 
			(pchPtr1[i] != 0 || pchPtr1[i+1] != 0 || pchPtr1[i+2] != 0 || pchPtr1[i+3] != 0x01))
			continue;

		nIsGetUserData	= 1;

		if (pchPtr1[i] == 0 && pchPtr1[i+1] == 0 && pchPtr1[i+2] == 0x01) {
			nSingal = i+3;
		}
		else if (pchPtr1[i] == 0 && pchPtr1[i+1] == 0 && pchPtr1[i+2] == 0 && pchPtr1[i+3] == 0x01) {
			nSingal = i+4;
		}

		if (0x01 == (pchPtr1[nSingal]&0x1F)) {
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("#%d CRtspStream.GetFrameType: 0x01, i=%d. \n", __LINE__, i));

			eFrameType = H264_P_FRAME;
			nGetOffset = i;
			break;
		}
		else if (0x05 == (pchPtr1[nSingal]&0x1F)) {
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("#%d CRtspStream.GetFrameType: 0x05, i=%d. \n", __LINE__, i));

			if (H264_SPS_FRAME == eFrameType) {
				eFrameType = H264_SI_FRAME;
			}
			else {
				eFrameType = H264_I_FRAME;
				nGetOffset = i;
			}
			break;
		}
		else if (0x07 == (pchPtr1[nSingal]&0x1F)) {
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("#%d CRtspStream.GetFrameType: 0x07, i=%d. \n", __LINE__, i));

			eFrameType = H264_SPS_FRAME;
			nGetOffset = i;
		}
		else if (0x08 == (pchPtr1[nSingal]&0x1F)) {
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("#%d CRtspStream.GetFrameType: 0x08, i=%d. \n", __LINE__, i));
			if (OTHER_FRAME == eFrameType)
				eFrameType = H264_PPS_FRAME;
		}

		i += 3;
	}

	if (pnGetOffset)
		*pnGetOffset = nGetOffset;

	return nIsGetUserData;
}