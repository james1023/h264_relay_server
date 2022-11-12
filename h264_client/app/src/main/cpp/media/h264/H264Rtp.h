#ifndef __H264_COMMON_H__
#define __H264_COMMON_H__

#include "media/h264/h264_stream.h"

#define H264_MEDIA_AUDIO									0x00
#define H264_MEDIA_VIDEO									0x01

typedef enum _FRAME_MODE
{
	FRAME_MODE_FRAME,
	FRAME_MODE_FIELD,
	FRAME_MODE_CIF,
} FRAME_MODE;

//! definition of H.264 syntax elements
typedef enum 
{
	SE_HEADER,
	SE_PTYPE,
	SE_MBTYPE,
	SE_REFFRAME,
	SE_INTRAPREDMODE,
	SE_MVD,
	SE_CBP_INTRA,
	SE_LUM_DC_INTRA,
	SE_CHR_DC_INTRA,
	SE_LUM_AC_INTRA,
	SE_CHR_AC_INTRA,
	SE_CBP_INTER,
	SE_LUM_DC_INTER,
	SE_CHR_DC_INTER,
	SE_LUM_AC_INTER,
	SE_CHR_AC_INTER,
	SE_DELTA_QUANT_INTER,
	SE_DELTA_QUANT_INTRA,
	SE_BFRAME,
	SE_EOS,
	SE_MAX_ELEMENTS				//!< number of maximum syntax elements, this MUST be the last one!
} SE_type; // substituting the definitions in element.h

//! Bitstream
typedef struct
{
	// CABAC Decoding
	int           read_len;				//!< actual position in the codebuffer, CABAC only
	int           code_len;				//!< overall codebuffer length, CABAC only
	// UVLC Decoding
	int           frame_bitoffset;    //!< actual position in the codebuffer, bit-oriented, UVLC only
	int           bitstream_length; //!< over codebuffer lnegth, byte oriented, UVLC only
	// ErrorConcealment
	unsigned char *streamBuffer; //!< actual codebuffer for read bytes
	int           ei_flag;					//!< error indication, 0: no error, else unspecified error
} Bitstream;

//! Syntaxelement
typedef struct syntaxelement
{
	int           type;						//!< type of syntax element for data part.
	int           value1;					//!< numerical value of syntax element
	int           value2;					//!< for blocked symbols, e.g. run/level
	int           len;						//!< length of code
	int           inf;						//!< info part of UVLC code
	unsigned int  bitpattern;     //!< UVLC bitpattern
	int           context;				//!< CABAC context
	int           k;							//!< CABAC context for coeff_count,uv

	//! for mapping of UVLC to syntaxElement
	void    (*mapping)(int len, int info, int *value1, int *value2);

} SyntaxElement;

extern int ue_v(char *tracestring, Bitstream *bitstream);

class H264VideoRTPSource
{
public:
	static int processSpecialHeader(
		unsigned char *&pbBuff, 
		int BuffSize, 
		int &nLenOffset,
		int &fCurrentPacketBeginsFrame,
		int &fCurrentPacketCompletesFrame,
		int recv_packet_max_len);
};

class H264RTP
{
public:

	H264RTP();
	virtual ~H264RTP();

	static int GetFrameType(
		const unsigned char *pbBuff,
		int nBuffSize, 
		FRAME_TYPE &eFrameType,
		int *pnGetOffset);
};

#endif //__H264_COMMON_H__
