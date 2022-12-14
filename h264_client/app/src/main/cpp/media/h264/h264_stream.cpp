#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "media/h264/bs.h"
#include "media/h264/h264_stream.h"
#include "media/h264/h264_sei.h"
#include "base/debug-msg-c.h"

#define log2(x) ( (1/log((float)2)) * log( ((float)x) ) )

int is_slice_type(int slice_type, int cmp_type)
{
    if (slice_type >= 5) { slice_type -= 5; }
    if (cmp_type >= 5) { cmp_type -= 5; }
    if (slice_type == cmp_type) { return 1; }
    else { return 0; }
}

/***************************** reading ******************************/

/**
 Create a new H264 stream object.  Allocates all structures contained within it.
 @return    the stream object
 */
h264_stream_t* h264_new()
{
    h264_stream_t* h = (h264_stream_t*)malloc(sizeof(h264_stream_t));
    h->nal = (nal_t*)malloc(sizeof(nal_t)); memset(h->nal, 0, sizeof(nal_t));
    h->sps = (sps_t*)malloc(sizeof(sps_t)); memset(h->sps, 0, sizeof(sps_t));
    h->pps = (pps_t*)malloc(sizeof(pps_t)); memset(h->pps, 0, sizeof(pps_t));
    h->aud = (aud_t*)malloc(sizeof(aud_t)); memset(h->aud, 0, sizeof(aud_t));
    h->sh = (slice_header_t*)malloc(sizeof(slice_header_t)); memset(h->sh, 0, sizeof(slice_header_t));
    return h;
}


/**
 Free an existing H264 stream object.  Frees all contained structures.
 @param[in,out] h   the stream object
 */
void h264_free(h264_stream_t* h)
{
    free(h->nal);
    free(h->sps);
    free(h->pps);
    free(h->aud);
    free(h->sh);
    free(h);
}

/**
 Find the beginning and end of a NAL (Network Abstraction Layer) unit in a byte buffer containing H264 bitstream data.
 @param[in]   buf        the buffer
 @param[in]   size       the size of the buffer
 @param[out]  nal_start  the beginning offset of the nal
 @param[out]  nal_end    the end offset of the nal
 @return                 the length of the nal, or 0 if did not find start of nal, or -1 if did not find end of nal
 */
// DEPRECATED - this will be replaced by a similar function with a slightly different API
int find_nal_unit(uint8_t* buf, int size, int* nal_start, int* nal_end)
{
    int i;
    
	if (size <= 0)
		return 0;

    *nal_start = 0;
    *nal_end = 0;
    
    i = 0;
    while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) && 
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01) 
        )
    {
        i++; // skip leading zero
        if (i+4 >= size) { return 0; } // did not find nal start
    }

    if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
    {
        i++;
    }

    if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) { /* error, should never happen */ return 0; }
    i+= 3;
    *nal_start = i;
    
    while (   //( next_bits( 24 ) != 0x000000 && next_bits( 24 ) != 0x000001 )
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) && 
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) 
        )
    {
        i++;
        // FIXME the next line fails when reading a nal that ends exactly at the end of the data
        if (i+3 >= size) { *nal_end = size; break;} // did not find nal end, stream ended first
    }
    
	if (0 == *nal_end)
		*nal_end = i;
    return (*nal_end - *nal_start);
}


int _find_nal_unit(uint8_t *buf, int size, int *nal_start, int *offset)
{
	int i;

	if (size <= 0)
		return 0;

	if (nal_start) *nal_start = 0;
	if (offset) *offset = 0;

	i = 0;
	while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
		(buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) && 
		(buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01) 
		)
	{
		i++; // skip leading zero
		if (i+4 >= size) { return 0; } // did not find nal start
	}

	*offset = i;

	if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
	{
		i++;

		if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) {
			/* error, should never happen */ 
			return 0; 
		}
	}

	i+= 3;
	*nal_start = i;

	return 1;
}


int more_rbsp_data(h264_stream_t* h, bs_t* b) { return !bs_eof(b); }

uint32_t next_bits(bs_t* b, int n) { return 0; } // FIXME UNIMPLEMENTED

/**
   Convert RBSP data to NAL data (Annex B format).
   The size of nal_data must be 4/3 * the size of the rbsp data (rounded up) to guarantee the output will fit.
   If that is not true, output may be truncated.  If that is true, there is no possible error during this conversion.
   @param[in] rbsp_buf   the rbsp data
   @param[in] rbsp_size  pointer to the size of the rbsp data
   @param[in,out] nal_buf   allocated memory in which to put the nal data
   @param[in,out] nal_size  as input, pointer to the maximum size of the nal data; as output, filled in with the size actually written
 */
// 7.3.1 NAL unit syntax
// 7.4.1.1 Encapsulation of an SODB within an RBSP
void rbsp_to_nal(uint8_t* rbsp_buf, int* rbsp_size, uint8_t* nal_buf, int* nal_size)
{
    int i, j;

    i = 1; // NOTE omits first byte which contains nal_ref_idc and nal_unit_type, already written
    j = 0;
    while( i < *nal_size && j < *rbsp_size )
    {
        if( i + 3 < *nal_size && j + 2 < *rbsp_size &&
            rbsp_buf[j] == 0x00 && rbsp_buf[j+1] == 0x00 && 
            ( rbsp_buf[j+2] == 0x01 || rbsp_buf[j+2] == 0x02 || rbsp_buf[j+2] == 0x03 ) ) // next_bits( 24 ) == 0x000001 or 0x000002 or 0x000003
        {
            nal_buf[ i   ] = rbsp_buf[ j   ];
            nal_buf[ i+1 ] = rbsp_buf[ j+1 ];
            nal_buf[ i+2 ] = 0x03;  // emulation_prevention_three_byte equal to 0x03
            nal_buf[ i+3 ] = rbsp_buf[ j+2 ];
            i += 4; j += 3;
        }
        else if ( j == *rbsp_size - 1 && 
                  rbsp_buf[ j ] == 0x00 )
        {
            nal_buf[ i ] = 0x03; // emulation_prevention_three_byte equal to 0x03 in trailing position
            i += 1;
        }
        else
        {
            nal_buf[ i ] = rbsp_buf[ j ];
            i += 1; j+= 1;
        }
    }
    *nal_size = i;
}

/**
   Convert NAL data (Annex B format) to RBSP data.
   The size of rbsp_data must be the same as size of the nal data to guarantee the output will fit.
   If that is not true, output may be truncated.  If that is true, there is no possible error during this conversion.
   @param[in] nal_buf   the nal data
   @param[in] nal_size  pointer to the size of the nal data
   @param[in,out] rbsp_buf   allocated memory in which to put the rbsp data
   @param[in,out] rbsp_size  as input, pointer to the maximum size of the rbsp data; as output, filled in with the size actually written
 */
// 7.3.1 NAL unit syntax
// 7.4.1.1 Encapsulation of an SODB within an RBSP
void nal_to_rbsp(uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size)
{
    // FIXME don't like using *nal_size etc syntax
    int i, j;

    i = 1; // NOTE omits first byte of NAL which contains nal_ref_idc and nal_unit_type, this is NOT part of the RBSP
    j = 0;
    while( i < *nal_size )
    {
        if( i + 2 < *nal_size && 
            nal_buf[i] == 0x00 && nal_buf[i+1] == 0x00 && nal_buf[i+2] == 0x03 ) // next_bits( 24 ) == 0x000003
        {
            rbsp_buf[ j   ] = nal_buf[ i   ];
            rbsp_buf[ j+1 ] = nal_buf[ i+1 ];
            // buf[ i+2 ] == 0x03  // skip emulation_prevention_three_byte equal to 0x03 // this is guaranteed from the above condition
            i += 3; j += 2;
        }
        else if (i + 2 < *nal_size && 
            nal_buf[i] == 0x00 && nal_buf[i+1] == 0x00 && nal_buf[i+2] == 0x01 ) // next_bits( 24 ) == 0x000001 // start of next nal, we're done
        {
            break;
        }
        else
        {
            rbsp_buf[ j ] = nal_buf[ i ];
            i += 1; j += 1;
        }
    }
    *nal_size = i;
    *rbsp_size = j;
}

/**
 Read a NAL unit from a byte buffer.
 The buffer must start exactly at the beginning of the nal (after the start prefix).
 The NAL is read into h->nal and into other fields within h depending on its type (check h->nal->nal_unit_type after reading).
 @param[in,out] h          the stream object
 @param[in]     buf        the buffer
 @param[in]     size       the size of the buffer
 @return                   the length of data actually read
 */
//7.3.1 NAL unit syntax
int read_nal_unit(h264_stream_t* h, uint8_t* buf, int size)
{
    nal_t* nal = h->nal;

    bs_t* b = bs_new(buf, size);

    nal->forbidden_zero_bit = bs_read_f(b,1);
    nal->nal_ref_idc = bs_read_u(b,2);
    nal->nal_unit_type = bs_read_u(b,5);

    bs_free(b);

    uint8_t* rbsp_buf = (uint8_t*)malloc(size);
    int rbsp_size = 0;
    int nal_size = size;

    nal_to_rbsp(buf, &nal_size, rbsp_buf, &rbsp_size);

    b = bs_new(rbsp_buf, rbsp_size);
    
    if( nal->nal_unit_type == 0) { }                                 //  0    Unspecified
    else if( nal->nal_unit_type == 1) { read_slice_layer_rbsp(h, b); }       //  1    Coded slice of a non-IDR picture
    else if( nal->nal_unit_type == 2) {  }                           //  2    Coded slice data partition A
    else if( nal->nal_unit_type == 3) {  }                           //  3    Coded slice data partition B
    else if( nal->nal_unit_type == 4) {  }                           //  4    Coded slice data partition C
    else if( nal->nal_unit_type == 5) { read_slice_layer_rbsp(h, b); }       //  5    Coded slice of an IDR picture
    else if( nal->nal_unit_type == 6) { read_sei_rbsp(h, b); }         //  6    Supplemental enhancement information (SEI)
    else if( nal->nal_unit_type == 7) { read_seq_parameter_set_rbsp(h, b); } //  7    Sequence parameter set
    else if( nal->nal_unit_type == 8) { read_pic_parameter_set_rbsp(h, b); } //  8    Picture parameter set
    else if( nal->nal_unit_type == 9) { read_access_unit_delimiter_rbsp(h, b); } //  9    Access unit delimiter
    else if( nal->nal_unit_type == 10) { read_end_of_seq_rbsp(h, b); }       // 10    End of sequence       
    else if( nal->nal_unit_type == 11) { read_end_of_stream_rbsp(h, b); }    // 11    End of stream
    else if( nal->nal_unit_type == 12) { /* read_filler_data_rbsp(h, b); */ }      // 12    Filler data
    else if( nal->nal_unit_type == 13) { /* seq_parameter_set_extension_rbsp( ) */ } // 13    Sequence parameter set extension
                                                                     //14..18 Reserved
    else if( nal->nal_unit_type == 19) { read_slice_layer_rbsp(h, b); }      // 19    Coded slice of an auxiliary coded picture without partitioning
                                                                      //20..23 Reserved
                                                                     //24..31 Unspecified

    bs_free(b); // TODO check for eof/read-beyond-end

    free(rbsp_buf);

    return nal_size;
}


//7.3.2.1 Sequence parameter set RBSP syntax
int read_seq_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;
	memset(sps, 0, sizeof(sps_t));

    int i;

    sps->profile_idc = bs_read_u8(b);
    sps->constraint_set0_flag = bs_read_u1(b);
    sps->constraint_set1_flag = bs_read_u1(b);
    sps->constraint_set2_flag = bs_read_u1(b);
    sps->constraint_set3_flag = bs_read_u1(b);
    sps->reserved_zero_4bits = bs_read_u(b,4);  /* all 0's */
    sps->level_idc = bs_read_u8(b);
    sps->seq_parameter_set_id = bs_read_ue(b);
    if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 144 )
    {
        sps->chroma_format_idc = bs_read_ue(b);
        if( sps->chroma_format_idc == 3 )
        {
            sps->residual_colour_transform_flag = bs_read_u1(b);
        }
        sps->bit_depth_luma_minus8 = bs_read_ue(b);
        sps->bit_depth_chroma_minus8 = bs_read_ue(b);
        sps->qpprime_y_zero_transform_bypass_flag = bs_read_u1(b);
        sps->seq_scaling_matrix_present_flag = bs_read_u1(b);
        if( sps->seq_scaling_matrix_present_flag )
        {
            for( i = 0; i < 8; i++ )
            {
                sps->seq_scaling_list_present_flag[ i ] = bs_read_u1(b);
                if( sps->seq_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        read_scaling_list( b, sps->ScalingList4x4[ i ], 16,
                                      sps->UseDefaultScalingMatrix4x4Flag[ i ]);
                    }
                    else
                    {
                        read_scaling_list( b, sps->ScalingList8x8[ i - 6 ], 64,
                                      sps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
    }
    sps->log2_max_frame_num_minus4 = bs_read_ue(b);
    sps->pic_order_cnt_type = bs_read_ue(b);
    if( sps->pic_order_cnt_type == 0 )
    {
        sps->log2_max_pic_order_cnt_lsb_minus4 = bs_read_ue(b);
    }
    else if( sps->pic_order_cnt_type == 1 )
    {
        sps->delta_pic_order_always_zero_flag = bs_read_u1(b);
        sps->offset_for_non_ref_pic = bs_read_se(b);
        sps->offset_for_top_to_bottom_field = bs_read_se(b);
        sps->num_ref_frames_in_pic_order_cnt_cycle = bs_read_ue(b);
		if (sps->num_ref_frames_in_pic_order_cnt_cycle > 256) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
			return 0;
		}

        for( i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
        {
            sps->offset_for_ref_frame[ i ] = bs_read_se(b);
        }
    }

    sps->num_ref_frames = bs_read_ue(b);
    sps->gaps_in_frame_num_value_allowed_flag = bs_read_u1(b);
    sps->pic_width_in_mbs_minus1 = bs_read_ue(b);
    sps->pic_height_in_map_units_minus1 = bs_read_ue(b);
    sps->frame_mbs_only_flag = bs_read_u1(b);
    if( !sps->frame_mbs_only_flag )
    {
        sps->mb_adaptive_frame_field_flag = bs_read_u1(b);
    }
    sps->direct_8x8_inference_flag = bs_read_u1(b);
    sps->frame_cropping_flag = bs_read_u1(b);
    if( sps->frame_cropping_flag )
    {
        sps->frame_crop_left_offset = bs_read_ue(b);                                                                    
        sps->frame_crop_right_offset = bs_read_ue(b);
        sps->frame_crop_top_offset = bs_read_ue(b);
        sps->frame_crop_bottom_offset = bs_read_ue(b);
    }

	// james: 120112 add resolution.
	sps->width = (sps->pic_width_in_mbs_minus1+1)*16-sps->frame_crop_right_offset*2-sps->frame_crop_left_offset*2;
	sps->height = (2-sps->frame_mbs_only_flag)*(sps->pic_height_in_map_units_minus1+1)*16-(sps->frame_crop_bottom_offset*2)-(sps->frame_crop_top_offset*2);

    sps->vui_parameters_present_flag = bs_read_u1(b);
    if( sps->vui_parameters_present_flag )
    {
        if (read_vui_parameters(h, b) < 1) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??#%d CRtspStream.h264_stream: error. \n", __LINE__);
			return 1;
		}
    }
    read_rbsp_trailing_bits(h, b);

	return 1;
}


//7.3.2.1.1 Scaling list syntax
void read_scaling_list(bs_t* b, int* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag )
{
    int j;

    int lastScale = 8;
    int nextScale = 8;
    for( j = 0; j < sizeOfScalingList; j++ )
    {
        if( nextScale != 0 )
        {
            int delta_scale = bs_read_se(b);
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j == 0 && nextScale == 0 );
        }
        scalingList[ j ] = ( nextScale == 0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}

//Appendix E.1.1 VUI parameters syntax
int read_vui_parameters(h264_stream_t* h, bs_t* b)
{
	int ret = 1;

    sps_t* sps = h->sps;

    sps->vui.aspect_ratio_info_present_flag = bs_read_u1(b);
    if( sps->vui.aspect_ratio_info_present_flag )
    {
        sps->vui.aspect_ratio_idc = bs_read_u8(b);
        if( sps->vui.aspect_ratio_idc == SAR_Extended )
        {
            sps->vui.sar_width = bs_read_u(b,16);
            sps->vui.sar_height = bs_read_u(b,16);
        }
    }
    sps->vui.overscan_info_present_flag = bs_read_u1(b);
    if( sps->vui.overscan_info_present_flag )
    {
        sps->vui.overscan_appropriate_flag = bs_read_u1(b);
    }
    sps->vui.video_signal_type_present_flag = bs_read_u1(b);
    if( sps->vui.video_signal_type_present_flag )
    {
        sps->vui.video_format = bs_read_u(b,3);
        sps->vui.video_full_range_flag = bs_read_u1(b);
        sps->vui.colour_description_present_flag = bs_read_u1(b);
        if( sps->vui.colour_description_present_flag )
        {
            sps->vui.colour_primaries = bs_read_u8(b);
            sps->vui.transfer_characteristics = bs_read_u8(b);
            sps->vui.matrix_coefficients = bs_read_u8(b);
        }
    }
    sps->vui.chroma_loc_info_present_flag = bs_read_u1(b);
    if( sps->vui.chroma_loc_info_present_flag )
    {
        sps->vui.chroma_sample_loc_type_top_field = bs_read_ue(b);
        sps->vui.chroma_sample_loc_type_bottom_field = bs_read_ue(b);
    }
    sps->vui.timing_info_present_flag = bs_read_u1(b);
    if( sps->vui.timing_info_present_flag )
    {
        sps->vui.num_units_in_tick = bs_read_u(b,32);
        sps->vui.time_scale = bs_read_u(b,32);
        sps->vui.fixed_frame_rate_flag = bs_read_u1(b);
    }
    sps->vui.nal_hrd_parameters_present_flag = bs_read_u1(b);
    if( sps->vui.nal_hrd_parameters_present_flag )
    {
        ret = read_hrd_parameters(h, b);
		if (ret < 1) {
			return ret;
		}
    }
    sps->vui.vcl_hrd_parameters_present_flag = bs_read_u1(b);
    if( sps->vui.vcl_hrd_parameters_present_flag )
    {
		ret = read_hrd_parameters(h, b);
		if (ret < 1) {
			return ret;
		}
    }
    if( sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag )
    {
        sps->vui.low_delay_hrd_flag = bs_read_u1(b);
    }
    sps->vui.pic_struct_present_flag = bs_read_u1(b);
    sps->vui.bitstream_restriction_flag = bs_read_u1(b);
    if( sps->vui.bitstream_restriction_flag )
    {
        sps->vui.motion_vectors_over_pic_boundaries_flag = bs_read_u1(b);
        sps->vui.max_bytes_per_pic_denom = bs_read_ue(b);
        sps->vui.max_bits_per_mb_denom = bs_read_ue(b);
        sps->vui.log2_max_mv_length_horizontal = bs_read_ue(b);
        sps->vui.log2_max_mv_length_vertical = bs_read_ue(b);
        sps->vui.num_reorder_frames = bs_read_ue(b);
        sps->vui.max_dec_frame_buffering = bs_read_ue(b);
    }

	return ret;
}


//Appendix E.1.2 HRD parameters syntax
int read_hrd_parameters(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;
    int SchedSelIdx;

    sps->hrd.cpb_cnt_minus1 = bs_read_ue(b);
    sps->hrd.bit_rate_scale = bs_read_u(b,4);
    sps->hrd.cpb_size_scale = bs_read_u(b,4);

	if (sps->hrd.cpb_cnt_minus1 >= 32) {
		// DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, ("???#%d CRtspStream.h264_stream: error. \n", __LINE__));
		return 0;
	}

    for( SchedSelIdx = 0; SchedSelIdx <= sps->hrd.cpb_cnt_minus1; SchedSelIdx++ )
    {
        sps->hrd.bit_rate_value_minus1[ SchedSelIdx ] = bs_read_ue(b);
        sps->hrd.cpb_size_value_minus1[ SchedSelIdx ] = bs_read_ue(b);
        sps->hrd.cbr_flag[ SchedSelIdx ] = bs_read_u1(b);
    }
    sps->hrd.initial_cpb_removal_delay_length_minus1 = bs_read_u(b,5);
    sps->hrd.cpb_removal_delay_length_minus1 = bs_read_u(b,5);
    sps->hrd.dpb_output_delay_length_minus1 = bs_read_u(b,5);
    sps->hrd.time_offset_length = bs_read_u(b,5);

	return 1;
}


/*
UNIMPLEMENTED
//7.3.2.1.2 Sequence parameter set extension RBSP syntax
int read_seq_parameter_set_extension_rbsp(bs_t* b, sps_ext_t* sps_ext) {
    seq_parameter_set_id = bs_read_ue(b);
    aux_format_idc = bs_read_ue(b);
    if( aux_format_idc != 0 ) {
        bit_depth_aux_minus8 = bs_read_ue(b);
        alpha_incr_flag = bs_read_u1(b);
        alpha_opaque_value = bs_read_u(v);
        alpha_transparent_value = bs_read_u(v);
    }
    additional_extension_flag = bs_read_u1(b);
    read_rbsp_trailing_bits();
}
*/

//7.3.2.2 Picture parameter set RBSP syntax
int read_pic_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    pps_t* pps = h->pps;

    int i;
    int i_group;

    pps->pic_parameter_set_id = bs_read_ue(b);
    pps->seq_parameter_set_id = bs_read_ue(b);
    pps->entropy_coding_mode_flag = bs_read_u1(b);
    pps->pic_order_present_flag = bs_read_u1(b);
    pps->num_slice_groups_minus1 = bs_read_ue(b);
	if (pps->num_slice_groups_minus1 >= 8) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
		return 0;
	}

    if( pps->num_slice_groups_minus1 > 0 )
    {
        pps->slice_group_map_type = bs_read_ue(b);
        if( pps->slice_group_map_type == 0 )
        {
            for( i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++ )
            {
                pps->run_length_minus1[ i_group ] = bs_read_ue(b);
            }
        }
        else if( pps->slice_group_map_type == 2 )
        {
            for( i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++ )
            {
                pps->top_left[ i_group ] = bs_read_ue(b);
                pps->bottom_right[ i_group ] = bs_read_ue(b);
            }
        }
        else if( pps->slice_group_map_type == 3 ||
                 pps->slice_group_map_type == 4 ||
                 pps->slice_group_map_type == 5 )
        {
            pps->slice_group_change_direction_flag = bs_read_u1(b);
            pps->slice_group_change_rate_minus1 = bs_read_ue(b);
        }
        else if( pps->slice_group_map_type == 6 )
        {
            pps->pic_size_in_map_units_minus1 = bs_read_ue(b);
			if (pps->pic_size_in_map_units_minus1 >= 256) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
				return 0;
			}

            for( i = 0; i <= pps->pic_size_in_map_units_minus1; i++ )
            {
                pps->slice_group_id[ i ] = bs_read_u(b, ceil( log2( pps->num_slice_groups_minus1 + 1 ) ) ); // was u(v)
            }
        }
    }
    pps->num_ref_idx_l0_active_minus1 = bs_read_ue(b);
    pps->num_ref_idx_l1_active_minus1 = bs_read_ue(b);
    pps->weighted_pred_flag = bs_read_u1(b);
    pps->weighted_bipred_idc = bs_read_u(b,2);
    pps->pic_init_qp_minus26 = bs_read_se(b);
    pps->pic_init_qs_minus26 = bs_read_se(b);
    pps->chroma_qp_index_offset = bs_read_se(b);
    pps->deblocking_filter_control_present_flag = bs_read_u1(b);
    pps->constrained_intra_pred_flag = bs_read_u1(b);
    pps->redundant_pic_cnt_present_flag = bs_read_u1(b);
    if( more_rbsp_data(h, b) )
    {
        pps->transform_8x8_mode_flag = bs_read_u1(b);
		if (6 + 2* pps->transform_8x8_mode_flag > 8) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
			return 0;
		}

        pps->pic_scaling_matrix_present_flag = bs_read_u1(b);
        if( pps->pic_scaling_matrix_present_flag )
        {
            for( i = 0; i < 6 + 2* pps->transform_8x8_mode_flag; i++ )
            {
                pps->pic_scaling_list_present_flag[ i ] = bs_read_u1(b);
                if( pps->pic_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        read_scaling_list( b, pps->ScalingList4x4[ i ], 16,
                                      pps->UseDefaultScalingMatrix4x4Flag[ i ] );
                    }
                    else
                    {
                        read_scaling_list( b, pps->ScalingList8x8[ i - 6 ], 64,
                                      pps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
        pps->second_chroma_qp_index_offset = bs_read_se(b);
    }
    read_rbsp_trailing_bits(h, b);

	return 1;
}

//7.3.2.3 Supplemental enhancement information RBSP syntax
void read_sei_rbsp(h264_stream_t* h, bs_t* b)
{
    int i;
    for (i = 0; i < h->num_seis; i++)
    {
        sei_free(h->seis[i]);
    }
    
    h->num_seis = 0;
    do {
        h->num_seis++;
        h->seis = (sei_t**)realloc(h->seis, h->num_seis * sizeof(sei_t*));
        h->seis[h->num_seis - 1] = sei_new();
        h->sei = h->seis[h->num_seis - 1];
        read_sei_message(h, b);
    } while( more_rbsp_data(h, b) );
    read_rbsp_trailing_bits(h, b);
}

int _read_ff_coded_number(bs_t* b)
{
    int n1 = 0;
    int n2;
    do 
    {
        n2 = bs_read_u8(b);
        n1 += n2;
    } while (n2 == 0xff);
    return n1;
}

//7.3.2.3.1 Supplemental enhancement information message syntax
void read_sei_message(h264_stream_t* h, bs_t* b)
{
    h->sei->payloadType = _read_ff_coded_number(b);
    h->sei->payloadSize = _read_ff_coded_number(b);
    read_sei_payload( h, b, h->sei->payloadType, h->sei->payloadSize );
}

//7.3.2.4 Access unit delimiter RBSP syntax
void read_access_unit_delimiter_rbsp(h264_stream_t* h, bs_t* b)
{
    h->aud->primary_pic_type = bs_read_u(b,3);
    read_rbsp_trailing_bits(h, b);
}

//7.3.2.5 End of sequence RBSP syntax
void read_end_of_seq_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.6 End of stream RBSP syntax
void read_end_of_stream_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.7 Filler data RBSP syntax
void read_filler_data_rbsp(h264_stream_t* h, bs_t* b)
{
    int ff_byte; //FIXME
    while( next_bits(b, 8) == 0xFF )
    {
        ff_byte = bs_read_f(b,8);  // equal to 0xFF
    }
    read_rbsp_trailing_bits(h, b);
}

//7.3.2.8 Slice layer without partitioning RBSP syntax
void read_slice_layer_rbsp(h264_stream_t* h, bs_t* b)
{
    read_slice_header(h, b);

    // DEBUG
    //printf("slice data: \n");
    //debug_bytes(b->p, b->end - b->p);
    //printf("bits left in front: %d \n", b->bits_left);

    // FIXME should read or skip data
    //slice_data( ); /* all categories of slice_data( ) syntax */  
    //read_rbsp_slice_trailing_bits(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.9.1 Slice data partition A RBSP syntax
slice_data_partition_a_layer_rbsp( ) {
    read_slice_header( );             // only category 2
    slice_id = bs_read_ue(b)
    read_slice_data( );               // only category 2
    read_rbsp_slice_trailing_bits( ); // only category 2
}

//7.3.2.9.2 Slice data partition B RBSP syntax
slice_data_partition_b_layer_rbsp( ) {
    slice_id = bs_read_ue(b);    // only category 3
    if( redundant_pic_cnt_present_flag )
        redundant_pic_cnt = bs_read_ue(b);
    read_slice_data( );               // only category 3
    read_rbsp_slice_trailing_bits( ); // only category 3
}

//7.3.2.9.3 Slice data partition C RBSP syntax
slice_data_partition_c_layer_rbsp( ) {
    slice_id = bs_read_ue(b);    // only category 4
    if( redundant_pic_cnt_present_flag )
        redundant_pic_cnt = bs_read_ue(b);
    read_slice_data( );               // only category 4
    rbsp_slice_trailing_bits( ); // only category 4
}
*/

int
more_rbsp_trailing_data(h264_stream_t* h, bs_t* b) { return !bs_eof(b); }

//7.3.2.10 RBSP slice trailing bits syntax
void read_rbsp_slice_trailing_bits(h264_stream_t* h, bs_t* b)
{
    read_rbsp_trailing_bits(h, b);
    int cabac_zero_word;
    if( h->pps->entropy_coding_mode_flag )
    {
        while( more_rbsp_trailing_data(h, b) )
        {
            cabac_zero_word = bs_read_f(b,16); // equal to 0x0000
        }
    }
}

//7.3.2.11 RBSP trailing bits syntax
void read_rbsp_trailing_bits(h264_stream_t* h, bs_t* b)
{
    int rbsp_stop_one_bit;
    int rbsp_alignment_zero_bit;
    if( !bs_byte_aligned(b) )
    {
        rbsp_stop_one_bit = bs_read_f(b,1); // equal to 1
        while( !bs_byte_aligned(b) )
        {
            rbsp_alignment_zero_bit = bs_read_f(b,1); // equal to 0
        }
    }
}

//7.3.3 Slice header syntax
void read_slice_header(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;
    nal_t* nal = h->nal;

    sh->first_mb_in_slice = bs_read_ue(b);
    sh->slice_type = bs_read_ue(b);
    sh->pic_parameter_set_id = bs_read_ue(b);
    sh->frame_num = bs_read_u(b, sps->log2_max_frame_num_minus4 + 4 ); // was u(v)

	sh->frame_num = sh->frame_num;

    /*if( !sps->frame_mbs_only_flag )
    {
        sh->field_pic_flag = bs_read_u1(b);
        if( sh->field_pic_flag )
        {
            sh->bottom_field_flag = bs_read_u1(b);
        }
    }
    if( nal->nal_unit_type == 5 )
    {
        sh->idr_pic_id = bs_read_ue(b);
    }
    if( sps->pic_order_cnt_type == 0 )
    {
        sh->pic_order_cnt_lsb = bs_read_u(b, sps->log2_max_pic_order_cnt_lsb_minus4 + 4 ); // was u(v)
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            sh->delta_pic_order_cnt_bottom = bs_read_se(b);
        }
    }
    if( sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag )
    {
        sh->delta_pic_order_cnt[ 0 ] = bs_read_se(b);
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            sh->delta_pic_order_cnt[ 1 ] = bs_read_se(b);
        }
    }
    if( pps->redundant_pic_cnt_present_flag )
    {
        sh->redundant_pic_cnt = bs_read_ue(b);
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        sh->direct_spatial_mv_pred_flag = bs_read_u1(b);
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_P ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        sh->num_ref_idx_active_override_flag = bs_read_u1(b);
        if( sh->num_ref_idx_active_override_flag )
        {
            sh->num_ref_idx_l0_active_minus1 = bs_read_ue(b); // FIXME does this modify the pps?
            if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
            {
                sh->num_ref_idx_l1_active_minus1 = bs_read_ue(b);
            }
        }
    }
    read_ref_pic_list_reordering(h, b);
    if( ( pps->weighted_pred_flag && ( is_slice_type( sh->slice_type, SH_SLICE_TYPE_P ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) ) ) ||
        ( pps->weighted_bipred_idc == 1 && is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) ) )
    {
        read_pred_weight_table(h, b);
    }
    if( nal->nal_ref_idc != 0 )
    {
        read_dec_ref_pic_marking(h, b);
    }
    if( pps->entropy_coding_mode_flag && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        sh->cabac_init_idc = bs_read_ue(b);
    }
    sh->slice_qp_delta = bs_read_se(b);
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) )
        {
            sh->sp_for_switch_flag = bs_read_u1(b);
        }
        sh->slice_qs_delta = bs_read_se(b);
    }
    if( pps->deblocking_filter_control_present_flag )
    {
        sh->disable_deblocking_filter_idc = bs_read_ue(b);
        if( sh->disable_deblocking_filter_idc != 1 )
        {
            sh->slice_alpha_c0_offset_div2 = bs_read_se(b);
            sh->slice_beta_offset_div2 = bs_read_se(b);
        }
    }
    if( pps->num_slice_groups_minus1 > 0 &&
        pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5)
    {
        sh->slice_group_change_cycle = 
            bs_read_u(b, ceil( log2( pps->pic_size_in_map_units_minus1 +  
                                     pps->slice_group_change_rate_minus1 + 1 ) ) ); // was u(v) // FIXME add 2?
    }*/
}

//7.3.3.1 Reference picture list reordering syntax
void read_ref_pic_list_reordering(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        sh->rplr.ref_pic_list_reordering_flag_l0 = bs_read_u1(b);
        if( sh->rplr.ref_pic_list_reordering_flag_l0 )
        {
            do
            {
                sh->rplr.reordering_of_pic_nums_idc = bs_read_ue(b);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    sh->rplr.abs_diff_pic_num_minus1 = bs_read_ue(b);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    sh->rplr.long_term_pic_num = bs_read_ue(b);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 && ! bs_eof(b) );
        }
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        sh->rplr.ref_pic_list_reordering_flag_l1 = bs_read_u1(b);
        if( sh->rplr.ref_pic_list_reordering_flag_l1 )
        {
            do 
            {
                sh->rplr.reordering_of_pic_nums_idc = bs_read_ue(b);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    sh->rplr.abs_diff_pic_num_minus1 = bs_read_ue(b);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    sh->rplr.long_term_pic_num = bs_read_ue(b);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 && ! bs_eof(b) );
        }
    }
}

//7.3.3.2 Prediction weight table syntax
int read_pred_weight_table(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;

    int i, j;

    sh->pwt.luma_log2_weight_denom = bs_read_ue(b);
    if( sps->chroma_format_idc != 0 )
    {
        sh->pwt.chroma_log2_weight_denom = bs_read_ue(b);
    }

	if (pps->num_ref_idx_l0_active_minus1 >= 64) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
		return 0;
	}

    for( i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++ )
    {
        sh->pwt.luma_weight_l0_flag = bs_read_u1(b);
        if( sh->pwt.luma_weight_l0_flag )
        {
            sh->pwt.luma_weight_l0[ i ] = bs_read_se(b);
            sh->pwt.luma_offset_l0[ i ] = bs_read_se(b);
        }
        if ( sps->chroma_format_idc != 0 )
        {
            sh->pwt.chroma_weight_l0_flag = bs_read_u1(b);
            if( sh->pwt.chroma_weight_l0_flag )
            {
                for( j =0; j < 2; j++ )
                {
                    sh->pwt.chroma_weight_l0[ i ][ j ] = bs_read_se(b);
                    sh->pwt.chroma_offset_l0[ i ][ j ] = bs_read_se(b);
                }
            }
        }
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
		if (pps->num_ref_idx_l1_active_minus1 >= 64) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
			return 0;
		}

        for( i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++ )
        {
            sh->pwt.luma_weight_l1_flag = bs_read_u1(b);
            if( sh->pwt.luma_weight_l1_flag )
            {
                sh->pwt.luma_weight_l1[ i ] = bs_read_se(b);
                sh->pwt.luma_offset_l1[ i ] = bs_read_se(b);
            }
            if( sps->chroma_format_idc != 0 )
            {
                sh->pwt.chroma_weight_l1_flag = bs_read_u1(b);
                if( sh->pwt.chroma_weight_l1_flag )
                {
                    for( j = 0; j < 2; j++ )
                    {
                        sh->pwt.chroma_weight_l1[ i ][ j ] = bs_read_se(b);
                        sh->pwt.chroma_offset_l1[ i ][ j ] = bs_read_se(b);
                    }
                }
            }
        }
    }

	return 1;
}

//7.3.3.3 Decoded reference picture marking syntax
void read_dec_ref_pic_marking(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( h->nal->nal_unit_type == 5 )
    {
        sh->drpm.no_output_of_prior_pics_flag = bs_read_u1(b);
        sh->drpm.long_term_reference_flag = bs_read_u1(b);
    }
    else
    {
        sh->drpm.adaptive_ref_pic_marking_mode_flag = bs_read_u1(b);
        if( sh->drpm.adaptive_ref_pic_marking_mode_flag )
        {
            do
            {
                sh->drpm.memory_management_control_operation = bs_read_ue(b);
                if( sh->drpm.memory_management_control_operation == 1 ||
                    sh->drpm.memory_management_control_operation == 3 )
                {
                    sh->drpm.difference_of_pic_nums_minus1 = bs_read_ue(b);
                }
                if(sh->drpm.memory_management_control_operation == 2 )
                {
                    sh->drpm.long_term_pic_num = bs_read_ue(b);
                }
                if( sh->drpm.memory_management_control_operation == 3 ||
                    sh->drpm.memory_management_control_operation == 6 )
                {
                    sh->drpm.long_term_frame_idx = bs_read_ue(b);
                }
                if( sh->drpm.memory_management_control_operation == 4 )
                {
                    sh->drpm.max_long_term_frame_idx_plus1 = bs_read_ue(b);
                }
            } while( sh->drpm.memory_management_control_operation != 0 && ! bs_eof(b) );
        }
    }
}

/*
// UNIMPLEMENTED
//7.3.4 Slice data syntax
slice_data( )
{
    if( pps->entropy_coding_mode_flag )
        while( !byte_aligned( ) )
            cabac_alignment_one_bit = bs_read_f(1);
    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    moreDataFlag = 1;
    prevMbSkipped = 0;
    do {
        if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
            if( !pps->entropy_coding_mode_flag ) {
                mb_skip_run = bs_read_ue(b);
                prevMbSkipped = ( mb_skip_run > 0 );
                for( i=0; i<mb_skip_run; i++ )
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                moreDataFlag = more_rbsp_data( );
            } else {
                mb_skip_flag = bs_read_ae(v);
                moreDataFlag = !mb_skip_flag;
            }
        if( moreDataFlag ) {
            if( MbaffFrameFlag && ( CurrMbAddr % 2 == 0 ||
                                    ( CurrMbAddr % 2 == 1 && prevMbSkipped ) ) )
                mb_field_decoding_flag = bs_read_u1(b) | bs_read_ae(v);
            macroblock_layer( );
        }
        if( !pps->entropy_coding_mode_flag )
            moreDataFlag = more_rbsp_data( );
        else {
            if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
                prevMbSkipped = mb_skip_flag;
            if( MbaffFrameFlag && CurrMbAddr % 2 == 0 )
                moreDataFlag = 1;
            else {
                end_of_slice_flag = bs_read_ae(v);
                moreDataFlag = !end_of_slice_flag;
            }
        }
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag );
}
*/


/***************************** writing ******************************/

#define DBG_START \
    bs_t* b2 = (bs_t*)malloc(sizeof(bs_t)); \
    bs_init(b2, b->p, b->end - b->p); \
    h264_stream_t* h2 = (h264_stream_t*)malloc(sizeof(h264_stream_t));\
    h2->sps=h->sps; h2->pps=h->pps; h2->nal=h->nal; h2->sh=h->sh;  \

#define DBG_END \
    free(h2); \
    free(b2); \

#define DBG \
  printf("line %d:",  __LINE__ ); \
  debug_bs(b); \
  b2->p = b2->start; b2->bits_left = 8; \
  /* read_slice_header(h2, b2); */\
  /* if (h2->sh->drpm.adaptive_ref_pic_marking_mode_flag) { printf(" X"); }; */ \
  printf("\n"); \

/**
 Write a NAL unit to a byte buffer.
 The NAL which is written out has a type determined by h->nal and data which comes from other fields within h depending on its type.
 @param[in,out]  h          the stream object
 @param[out]     buf        the buffer
 @param[in]      size       the size of the buffer
 @return                    the length of data actually written
 */
//7.3.1 NAL unit syntax
int write_nal_unit(h264_stream_t* h, uint8_t* buf, int size)
{
    nal_t* nal = h->nal;

    bs_t* b = bs_new(buf, size);

    bs_write_f(b,1, nal->forbidden_zero_bit);
    bs_write_u(b,2, nal->nal_ref_idc);
    bs_write_u(b,5, nal->nal_unit_type);

    bs_free(b);

    uint8_t* rbsp_buf = (uint8_t*)malloc(size*3/4); // NOTE this may have to be slightly smaller (3/4 smaller, worst case) in order to be guaranteed to fit
    int rbsp_size = 0;
    int nal_size = size;

    b = bs_new(rbsp_buf, rbsp_size); // FIXME DEPRECATED reinit of an already inited bs
    
    if( nal->nal_unit_type == 0) { }                                 //  0    Unspecified
    else if( nal->nal_unit_type == 1) { write_slice_layer_rbsp(h, b); }       //  1    Coded slice of a non-IDR picture
    else if( nal->nal_unit_type == 2) {  }                           //  2    Coded slice data partition A
    else if( nal->nal_unit_type == 3) {  }                           //  3    Coded slice data partition B
    else if( nal->nal_unit_type == 4) {  }                           //  4    Coded slice data partition C
    else if( nal->nal_unit_type == 5) { write_slice_layer_rbsp(h, b); }       //  5    Coded slice of an IDR picture
    else if( nal->nal_unit_type == 6) { write_sei_rbsp(h, b); }         //  6    Supplemental enhancement information (SEI)
    else if( nal->nal_unit_type == 7) { write_seq_parameter_set_rbsp(h, b); } //  7    Sequence parameter set
    else if( nal->nal_unit_type == 8) { write_pic_parameter_set_rbsp(h, b); } //  8    Picture parameter set
    else if( nal->nal_unit_type == 9) { write_access_unit_delimiter_rbsp(h, b); } //  9    Access unit delimiter
    else if( nal->nal_unit_type == 10) { write_end_of_seq_rbsp(h, b); }       // 10    End of sequence       
    else if( nal->nal_unit_type == 11) { write_end_of_stream_rbsp(h, b); }    // 11    End of stream
    else if( nal->nal_unit_type == 12) { /* write_filler_data_rbsp(h, b); */ }      // 12    Filler data
    else if( nal->nal_unit_type == 13) { /* seq_parameter_set_extension_rbsp( ) */ } // 13    Sequence parameter set extension
                                                                     //14..18 Reserved
    else if( nal->nal_unit_type == 19) { write_slice_layer_rbsp(h, b); }      // 19    Coded slice of an auxiliary coded picture without partitioning
                                                                      //20..23 Reserved
                                                                     //24..31 Unspecified

    rbsp_size = bs_pos(b); // TODO check for eof/write-beyond-end
    bs_free(b);

    rbsp_to_nal(rbsp_buf, &rbsp_size, buf, &nal_size);

    free(rbsp_buf);

    return nal_size;
}


//7.3.2.1 Sequence parameter set RBSP syntax
void write_seq_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;

    int i;

    bs_write_u8(b, sps->profile_idc);
    bs_write_u1(b, sps->constraint_set0_flag);
    bs_write_u1(b, sps->constraint_set1_flag);
    bs_write_u1(b, sps->constraint_set2_flag);
    bs_write_u1(b, sps->constraint_set3_flag);
    bs_write_u(b,4, sps->reserved_zero_4bits);  /* all 0's */
    bs_write_u8(b, sps->level_idc);
    bs_write_ue(b, sps->seq_parameter_set_id);
    if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 144 )
    {
        bs_write_ue(b, sps->chroma_format_idc);
        if( sps->chroma_format_idc == 3 )
        {
            bs_write_u1(b, sps->residual_colour_transform_flag);
        }
        bs_write_ue(b, sps->bit_depth_luma_minus8);
        bs_write_ue(b, sps->bit_depth_chroma_minus8);
        bs_write_u1(b, sps->qpprime_y_zero_transform_bypass_flag);
        bs_write_u1(b, sps->seq_scaling_matrix_present_flag);
        if( sps->seq_scaling_matrix_present_flag )
        {
            for( i = 0; i < 8; i++ )
            {
                bs_write_u1(b, sps->seq_scaling_list_present_flag[ i ]);
                if( sps->seq_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        write_scaling_list( b, sps->ScalingList4x4[ i ], 16,
                                      sps->UseDefaultScalingMatrix4x4Flag[ i ]);
                    }
                    else
                    {
                        write_scaling_list( b, sps->ScalingList8x8[ i - 6 ], 64,
                                      sps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
    }
    bs_write_ue(b, sps->log2_max_frame_num_minus4);
    bs_write_ue(b, sps->pic_order_cnt_type);
    if( sps->pic_order_cnt_type == 0 )
    {
        bs_write_ue(b, sps->log2_max_pic_order_cnt_lsb_minus4);
    }
    else if( sps->pic_order_cnt_type == 1 )
    {
        bs_write_u1(b, sps->delta_pic_order_always_zero_flag);
        bs_write_se(b, sps->offset_for_non_ref_pic);
        bs_write_se(b, sps->offset_for_top_to_bottom_field);
        bs_write_ue(b, sps->num_ref_frames_in_pic_order_cnt_cycle);
        for( i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
        {
            bs_write_se(b, sps->offset_for_ref_frame[ i ]);
        }
    }
    bs_write_ue(b, sps->num_ref_frames);
    bs_write_u1(b, sps->gaps_in_frame_num_value_allowed_flag);
    bs_write_ue(b, sps->pic_width_in_mbs_minus1);
    bs_write_ue(b, sps->pic_height_in_map_units_minus1);
    bs_write_u1(b, sps->frame_mbs_only_flag);
    if( !sps->frame_mbs_only_flag )
    {
        bs_write_u1(b, sps->mb_adaptive_frame_field_flag);
    }
    bs_write_u1(b, sps->direct_8x8_inference_flag);
    bs_write_u1(b, sps->frame_cropping_flag);
    if( sps->frame_cropping_flag )
    {
        bs_write_ue(b, sps->frame_crop_left_offset);
        bs_write_ue(b, sps->frame_crop_right_offset);
        bs_write_ue(b, sps->frame_crop_top_offset);
        bs_write_ue(b, sps->frame_crop_bottom_offset);
    }
    bs_write_u1(b, sps->vui_parameters_present_flag);
    if( sps->vui_parameters_present_flag )
    {
        write_vui_parameters(h, b);
    }
    write_rbsp_trailing_bits(h, b);
}

//7.3.2.1.1 Scaling list syntax
void write_scaling_list(bs_t* b, int* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag )
{
    int j;

    int lastScale = 8;
    int nextScale = 8;

    for( j = 0; j < sizeOfScalingList; j++ )
    {
        int delta_scale;

        if( nextScale != 0 )
        {
            // FIXME will not write in most compact way - could truncate list if all remaining elements are equal
            nextScale = scalingList[ j ]; 

            if (useDefaultScalingMatrixFlag)
            {
                nextScale = 0;
            }

            delta_scale = (nextScale - lastScale) % 256 ;
            bs_write_se(b, delta_scale);
        }

        lastScale = scalingList[ j ];
    }
}

//Appendix E.1.1 VUI parameters syntax
int write_vui_parameters(h264_stream_t* h, bs_t* b)
{
	int ret = 1;

	sps_t* sps = h->sps;

	bs_write_u1(b, sps->vui.aspect_ratio_info_present_flag);
	if( sps->vui.aspect_ratio_info_present_flag )
	{
		bs_write_u8(b, sps->vui.aspect_ratio_idc);
		if( sps->vui.aspect_ratio_idc == SAR_Extended )
		{
			bs_write_u(b,16, sps->vui.sar_width);
			bs_write_u(b,16, sps->vui.sar_height);
		}
	}
	bs_write_u1(b, sps->vui.overscan_info_present_flag);
	if( sps->vui.overscan_info_present_flag )
	{
		bs_write_u1(b, sps->vui.overscan_appropriate_flag);
	}
	bs_write_u1(b, sps->vui.video_signal_type_present_flag);
	if( sps->vui.video_signal_type_present_flag )
	{
		bs_write_u(b,3, sps->vui.video_format);
		bs_write_u1(b, sps->vui.video_full_range_flag);
		bs_write_u1(b, sps->vui.colour_description_present_flag);
		if( sps->vui.colour_description_present_flag )
		{
			bs_write_u8(b, sps->vui.colour_primaries);
			bs_write_u8(b, sps->vui.transfer_characteristics);
			bs_write_u8(b, sps->vui.matrix_coefficients);
		}
	}
	bs_write_u1(b, sps->vui.chroma_loc_info_present_flag);
	if( sps->vui.chroma_loc_info_present_flag )
	{
		bs_write_ue(b, sps->vui.chroma_sample_loc_type_top_field);
		bs_write_ue(b, sps->vui.chroma_sample_loc_type_bottom_field);
	}
	bs_write_u1(b, sps->vui.timing_info_present_flag);
	if( sps->vui.timing_info_present_flag )
	{
		bs_write_u(b,32, sps->vui.num_units_in_tick);
		bs_write_u(b,32, sps->vui.time_scale);
		bs_write_u1(b, sps->vui.fixed_frame_rate_flag);
	}
	bs_write_u1(b, sps->vui.nal_hrd_parameters_present_flag);
	if( sps->vui.nal_hrd_parameters_present_flag )
	{
		ret = write_hrd_parameters(h, b);
	}
	bs_write_u1(b, sps->vui.vcl_hrd_parameters_present_flag);
	if( sps->vui.vcl_hrd_parameters_present_flag )
	{
		ret = write_hrd_parameters(h, b);
	}
	if( sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag )
	{
		bs_write_u1(b, sps->vui.low_delay_hrd_flag);
	}
	bs_write_u1(b, sps->vui.pic_struct_present_flag);
	bs_write_u1(b, sps->vui.bitstream_restriction_flag);
	if( sps->vui.bitstream_restriction_flag )
	{
		bs_write_u1(b, sps->vui.motion_vectors_over_pic_boundaries_flag);
		bs_write_ue(b, sps->vui.max_bytes_per_pic_denom);
		bs_write_ue(b, sps->vui.max_bits_per_mb_denom);
		bs_write_ue(b, sps->vui.log2_max_mv_length_horizontal);
		bs_write_ue(b, sps->vui.log2_max_mv_length_vertical);
		bs_write_ue(b, sps->vui.num_reorder_frames);
		bs_write_ue(b, sps->vui.max_dec_frame_buffering);
	}
	
    return ret;
}

//Appendix E.1.2 HRD parameters syntax
int write_hrd_parameters(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;
    int SchedSelIdx;

    bs_write_ue(b, sps->hrd.cpb_cnt_minus1);
	if (sps->hrd.cpb_cnt_minus1 >= 32) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
		return 0;
	}

    bs_write_u(b,4, sps->hrd.bit_rate_scale);
    bs_write_u(b,4, sps->hrd.cpb_size_scale);
    for( SchedSelIdx = 0; SchedSelIdx <= sps->hrd.cpb_cnt_minus1; SchedSelIdx++ )
    {
        bs_write_ue(b, sps->hrd.bit_rate_value_minus1[ SchedSelIdx ]);
        bs_write_ue(b, sps->hrd.cpb_size_value_minus1[ SchedSelIdx ]);
        bs_write_u1(b, sps->hrd.cbr_flag[ SchedSelIdx ]);
    }
    bs_write_u(b,5, sps->hrd.initial_cpb_removal_delay_length_minus1);
    bs_write_u(b,5, sps->hrd.cpb_removal_delay_length_minus1);
    bs_write_u(b,5, sps->hrd.dpb_output_delay_length_minus1);
    bs_write_u(b,5, sps->hrd.time_offset_length);

	return 1;
}

/*
UNIMPLEMENTED
//7.3.2.1.2 Sequence parameter set extension RBSP syntax
int write_seq_parameter_set_extension_rbsp(bs_t* b, sps_ext_t* sps_ext) {
    bs_write_ue(b, seq_parameter_set_id);
    bs_write_ue(b, aux_format_idc);
    if( aux_format_idc != 0 ) {
        bs_write_ue(b, bit_depth_aux_minus8);
        bs_write_u1(b, alpha_incr_flag);
        bs_write_u(v, alpha_opaque_value);
        bs_write_u(v, alpha_transparent_value);
    }
    bs_write_u1(b, additional_extension_flag);
    write_rbsp_trailing_bits();
}
*/

//7.3.2.2 Picture parameter set RBSP syntax
int write_pic_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    pps_t* pps = h->pps;

    int i;
    int i_group;

    bs_write_ue(b, pps->pic_parameter_set_id);
    bs_write_ue(b, pps->seq_parameter_set_id);
    bs_write_u1(b, pps->entropy_coding_mode_flag);
    bs_write_u1(b, pps->pic_order_present_flag);
    bs_write_ue(b, pps->num_slice_groups_minus1);
	if (pps->num_slice_groups_minus1 >= 8) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
		return 0;
	}

    if( pps->num_slice_groups_minus1 > 0 )
    {
        bs_write_ue(b, pps->slice_group_map_type);
        if( pps->slice_group_map_type == 0 )
        {
            for( i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++ )
            {
                bs_write_ue(b, pps->run_length_minus1[ i_group ]);
            }
        }
        else if( pps->slice_group_map_type == 2 )
        {
            for( i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++ )
            {
                bs_write_ue(b, pps->top_left[ i_group ]);
                bs_write_ue(b, pps->bottom_right[ i_group ]);
            }
        }
        else if( pps->slice_group_map_type == 3 ||
                 pps->slice_group_map_type == 4 ||
                 pps->slice_group_map_type == 5 )
        {
            bs_write_u1(b, pps->slice_group_change_direction_flag);
            bs_write_ue(b, pps->slice_group_change_rate_minus1);
        }
        else if( pps->slice_group_map_type == 6 )
        {
            bs_write_ue(b, pps->pic_size_in_map_units_minus1);
			if (pps->pic_size_in_map_units_minus1 >= 256) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
				return 0;
			}

            for( i = 0; i <= pps->pic_size_in_map_units_minus1; i++ )
            {
                bs_write_u(b, ceil( log2( pps->num_slice_groups_minus1 + 1 ) ), pps->slice_group_id[ i ] ); // was u(v)
            }
        }
    }
    bs_write_ue(b, pps->num_ref_idx_l0_active_minus1);
    bs_write_ue(b, pps->num_ref_idx_l1_active_minus1);
    bs_write_u1(b, pps->weighted_pred_flag);
    bs_write_u(b,2, pps->weighted_bipred_idc);
    bs_write_se(b, pps->pic_init_qp_minus26);
    bs_write_se(b, pps->pic_init_qs_minus26);
    bs_write_se(b, pps->chroma_qp_index_offset);
    bs_write_u1(b, pps->deblocking_filter_control_present_flag);
    bs_write_u1(b, pps->constrained_intra_pred_flag);
    bs_write_u1(b, pps->redundant_pic_cnt_present_flag);
    if( more_rbsp_data(h, b) )
    {
        bs_write_u1(b, pps->transform_8x8_mode_flag);
		if (6 + 2* pps->transform_8x8_mode_flag > 8) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
			return 0;
		}

        bs_write_u1(b, pps->pic_scaling_matrix_present_flag);
        if( pps->pic_scaling_matrix_present_flag )
        {
            for( i = 0; i < 6 + 2* pps->transform_8x8_mode_flag; i++ )
            {
                bs_write_u1(b, pps->pic_scaling_list_present_flag[ i ]);
                if( pps->pic_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        write_scaling_list( b, pps->ScalingList4x4[ i ], 16,
                                      pps->UseDefaultScalingMatrix4x4Flag[ i ] );
                    }
                    else
                    {
                        write_scaling_list( b, pps->ScalingList8x8[ i - 6 ], 64,
                                      pps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
        bs_write_se(b, pps->second_chroma_qp_index_offset);
    }
    write_rbsp_trailing_bits(h, b);

	return 0;
}

//7.3.2.3 Supplemental enhancement information RBSP syntax
void write_sei_rbsp(h264_stream_t* h, bs_t* b)
{
    int i;
    for (i = 0; i < h->num_seis; i++)
    {
        h->sei = h->seis[i];
        write_sei_message(h, b);
    }
    write_rbsp_trailing_bits(h, b);
}

void _write_ff_coded_number(bs_t* b, int n)
{
    while (1)
    {
        if (n > 0xff)
        {
            bs_write_u8(b, 0xff);
            n -= 0xff;
        }
        else
        {
            bs_write_u8(b, n);
            break;
        }
    }
}

//7.3.2.3.1 Supplemental enhancement information message syntax
void write_sei_message(h264_stream_t* h, bs_t* b)
{
    // FIXME need some way to calculate size, or write message then write size
    _write_ff_coded_number(b, h->sei->payloadType);
    _write_ff_coded_number(b, h->sei->payloadSize);
    write_sei_payload( h, b, h->sei->payloadType, h->sei->payloadSize );
}

//7.3.2.4 Access unit delimiter RBSP syntax
void write_access_unit_delimiter_rbsp(h264_stream_t* h, bs_t* b)
{
    bs_write_u(b,3, h->aud->primary_pic_type);
    write_rbsp_trailing_bits(h, b);
}

//7.3.2.5 End of sequence RBSP syntax
void write_end_of_seq_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.6 End of stream RBSP syntax
void write_end_of_stream_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.7 Filler data RBSP syntax
void write_filler_data_rbsp(h264_stream_t* h, bs_t* b)
{
    int ff_byte; //FIXME
    while( next_bits(b, 8) == 0xFF )
    {
        bs_write_f(b,8, ff_byte);  // equal to 0xFF
    }
    write_rbsp_trailing_bits(h, b);
}

//7.3.2.8 Slice layer without partitioning RBSP syntax
void write_slice_layer_rbsp(h264_stream_t* h, bs_t* b)
{
    write_slice_header(h, b);
    //slice_data( ); /* all categories of slice_data( ) syntax */
    write_rbsp_slice_trailing_bits(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.9.1 Slice data partition A RBSP syntax
slice_data_partition_a_layer_rbsp( )
{
    write_slice_header( );             // only category 2
    bs_write_ue(b, slice_id)
    write_slice_data( );               // only category 2
    write_rbsp_slice_trailing_bits( ); // only category 2
}

//7.3.2.9.2 Slice data partition B RBSP syntax
slice_data_partition_b_layer_rbsp( )
{
    bs_write_ue(b, slice_id);    // only category 3
    if( redundant_pic_cnt_present_flag )
        bs_write_ue(b, redundant_pic_cnt);
    write_slice_data( );               // only category 3
    write_rbsp_slice_trailing_bits( ); // only category 3
}

//7.3.2.9.3 Slice data partition C RBSP syntax
slice_data_partition_c_layer_rbsp( )
{
    bs_write_ue(b, slice_id);    // only category 4
    if( redundant_pic_cnt_present_flag )
        bs_write_ue(b, redundant_pic_cnt);
    write_slice_data( );               // only category 4
    rbsp_slice_trailing_bits( ); // only category 4
}
*/

//7.3.2.10 RBSP slice trailing bits syntax
void write_rbsp_slice_trailing_bits(h264_stream_t* h, bs_t* b)
{
    write_rbsp_trailing_bits(h, b);
    int cabac_zero_word;
    if( h->pps->entropy_coding_mode_flag )
    {
        /*
        // 9.3.4.6 Byte stuffing process (informative)
        // NOTE do not write any cabac_zero_word for now - this appears to be optional
        while( more_rbsp_trailing_data(h, b) )
        {
            bs_write_f(b,16, cabac_zero_word); // equal to 0x0000
        }
        */
    }
}

//7.3.2.11 RBSP trailing bits syntax
void write_rbsp_trailing_bits(h264_stream_t* h, bs_t* b)
{
    int rbsp_stop_one_bit = 1;
    int rbsp_alignment_zero_bit = 0;
    if( !bs_byte_aligned(b) )
    {
        bs_write_f(b,1, rbsp_stop_one_bit); // equal to 1
        while( !bs_byte_aligned(b) )
        {
            bs_write_f(b,1, rbsp_alignment_zero_bit); // equal to 0
        }
    }
}

//7.3.3 Slice header syntax
void write_slice_header(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;
    nal_t* nal = h->nal;

    //DBG_START
    //h2->sh = (slice_header_t*)malloc(sizeof(slice_header_t));

    bs_write_ue(b, sh->first_mb_in_slice);
    bs_write_ue(b, sh->slice_type);
    bs_write_ue(b, sh->pic_parameter_set_id);
    bs_write_u(b, sps->log2_max_frame_num_minus4 + 4, sh->frame_num ); // was u(v)
    if( !sps->frame_mbs_only_flag )
    {
        bs_write_u1(b, sh->field_pic_flag);
        if( sh->field_pic_flag )
        {
            bs_write_u1(b, sh->bottom_field_flag);
        }
    }
    if( nal->nal_unit_type == 5 )
    {
        bs_write_ue(b, sh->idr_pic_id);
    }
    if( sps->pic_order_cnt_type == 0 )
    {
        bs_write_u(b, sps->log2_max_pic_order_cnt_lsb_minus4 + 4, sh->pic_order_cnt_lsb ); // was u(v)
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            bs_write_se(b, sh->delta_pic_order_cnt_bottom);
        }
    }
    if( sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag )
    {
        bs_write_se(b, sh->delta_pic_order_cnt[ 0 ]);
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            bs_write_se(b, sh->delta_pic_order_cnt[ 1 ]);
        }
    }
    if( pps->redundant_pic_cnt_present_flag )
    {
        bs_write_ue(b, sh->redundant_pic_cnt);
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        bs_write_u1(b, sh->direct_spatial_mv_pred_flag);
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_P ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        bs_write_u1(b, sh->num_ref_idx_active_override_flag);
        if( sh->num_ref_idx_active_override_flag )
        {
            bs_write_ue(b, sh->num_ref_idx_l0_active_minus1); // FIXME does this modify the pps?
            if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
            {
                bs_write_ue(b, sh->num_ref_idx_l1_active_minus1);
            }
        }
    }
    write_ref_pic_list_reordering(h, b);
    if( ( pps->weighted_pred_flag && ( is_slice_type( sh->slice_type, SH_SLICE_TYPE_P ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) ) ) ||
        ( pps->weighted_bipred_idc == 1 && is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) ) )
    {
        write_pred_weight_table(h, b);
    }
    if( nal->nal_ref_idc != 0 )
    {
        write_dec_ref_pic_marking(h, b);
    }
    if( pps->entropy_coding_mode_flag && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        bs_write_ue(b, sh->cabac_init_idc);
    }
    bs_write_se(b, sh->slice_qp_delta);
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) )
        {
            bs_write_u1(b, sh->sp_for_switch_flag);
        }
        bs_write_se(b, sh->slice_qs_delta);
    }
    if( pps->deblocking_filter_control_present_flag )
    {
        bs_write_ue(b, sh->disable_deblocking_filter_idc);
        if( sh->disable_deblocking_filter_idc != 1 )
        {
            bs_write_se(b, sh->slice_alpha_c0_offset_div2);
            bs_write_se(b, sh->slice_beta_offset_div2);
        }
    }
    if( pps->num_slice_groups_minus1 > 0 &&
        pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5)
    {
        bs_write_u(b, ceil( log2( pps->pic_size_in_map_units_minus1 +  
                                  pps->slice_group_change_rate_minus1 + 1 ) ),
                   sh->slice_group_change_cycle ); // was u(v) // FIXME add 2?
    }

    //free(h2->sh);
    //DBG_END
}

//7.3.3.1 Reference picture list reordering syntax
void write_ref_pic_list_reordering(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        bs_write_u1(b, sh->rplr.ref_pic_list_reordering_flag_l0);
        if( sh->rplr.ref_pic_list_reordering_flag_l0 )
        {
            do
            {
                bs_write_ue(b, sh->rplr.reordering_of_pic_nums_idc);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    bs_write_ue(b, sh->rplr.abs_diff_pic_num_minus1);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    bs_write_ue(b, sh->rplr.long_term_pic_num);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 );
        }
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        bs_write_u1(b, sh->rplr.ref_pic_list_reordering_flag_l1);
        if( sh->rplr.ref_pic_list_reordering_flag_l1 )
        {
            do
            {
                bs_write_ue(b, sh->rplr.reordering_of_pic_nums_idc);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    bs_write_ue(b, sh->rplr.abs_diff_pic_num_minus1);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    bs_write_ue(b, sh->rplr.long_term_pic_num);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 );
        }
    }
}

//7.3.3.2 Prediction weight table syntax
int write_pred_weight_table(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;

    int i, j;

    bs_write_ue(b, sh->pwt.luma_log2_weight_denom);
    if( sps->chroma_format_idc != 0 )
    {
        bs_write_ue(b, sh->pwt.chroma_log2_weight_denom);
    }

	if (pps->num_ref_idx_l0_active_minus1 >= 64) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
		return 0;
	}

    for( i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++ )
    {
        bs_write_u1(b, sh->pwt.luma_weight_l0_flag);
        if( sh->pwt.luma_weight_l0_flag )
        {
            bs_write_se(b, sh->pwt.luma_weight_l0[ i ]);
            bs_write_se(b, sh->pwt.luma_offset_l0[ i ]);
        }
        if ( sps->chroma_format_idc != 0 )
        {
            bs_write_u1(b, sh->pwt.chroma_weight_l0_flag);
            if( sh->pwt.chroma_weight_l0_flag )
            {
                for( j =0; j < 2; j++ )
                {
                    bs_write_se(b, sh->pwt.chroma_weight_l0[ i ][ j ]);
                    bs_write_se(b, sh->pwt.chroma_offset_l0[ i ][ j ]);
                }
            }
        }
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
		if (pps->num_ref_idx_l1_active_minus1 >= 64) {
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "???#%d CRtspStream.h264_stream: error. \n", __LINE__);
			return 0;
		}

        for( i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++ )
        {
            bs_write_u1(b, sh->pwt.luma_weight_l1_flag);
            if( sh->pwt.luma_weight_l1_flag )
            {
                bs_write_se(b, sh->pwt.luma_weight_l1[ i ]);
                bs_write_se(b, sh->pwt.luma_offset_l1[ i ]);
            }
            if( sps->chroma_format_idc != 0 )
            {
                bs_write_u1(b, sh->pwt.chroma_weight_l1_flag);
                if( sh->pwt.chroma_weight_l1_flag )
                {
                    for( j = 0; j < 2; j++ )
                    {
                        bs_write_se(b, sh->pwt.chroma_weight_l1[ i ][ j ]);
                        bs_write_se(b, sh->pwt.chroma_offset_l1[ i ][ j ]);
                    }
                }
            }
        }
    }

	return 1;
}

//7.3.3.3 Decoded reference picture marking syntax
void write_dec_ref_pic_marking(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( h->nal->nal_unit_type == 5 )
    {
        bs_write_u1(b, sh->drpm.no_output_of_prior_pics_flag);
        bs_write_u1(b, sh->drpm.long_term_reference_flag);
    }
    else
    {
        bs_write_u1(b, sh->drpm.adaptive_ref_pic_marking_mode_flag);
        if( sh->drpm.adaptive_ref_pic_marking_mode_flag )
        {
            do
            {
                bs_write_ue(b, sh->drpm.memory_management_control_operation);
                if( sh->drpm.memory_management_control_operation == 1 ||
                    sh->drpm.memory_management_control_operation == 3 )
                {
                    bs_write_ue(b, sh->drpm.difference_of_pic_nums_minus1);
                }
                if(sh->drpm.memory_management_control_operation == 2 )
                {
                    bs_write_ue(b, sh->drpm.long_term_pic_num);
                }
                if( sh->drpm.memory_management_control_operation == 3 ||
                    sh->drpm.memory_management_control_operation == 6 )
                {
                    bs_write_ue(b, sh->drpm.long_term_frame_idx);
                }
                if( sh->drpm.memory_management_control_operation == 4 )
                {
                    bs_write_ue(b, sh->drpm.max_long_term_frame_idx_plus1);
                }
            } while( sh->drpm.memory_management_control_operation != 0 );
        }
    }
}

/*
// UNIMPLEMENTED
//7.3.4 Slice data syntax
slice_data( )
{
    if( pps->entropy_coding_mode_flag )
        while( !byte_aligned( ) )
            bs_write_f(1, cabac_alignment_one_bit);
    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    moreDataFlag = 1;
    prevMbSkipped = 0;
    do {
        if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ssh->lice_type != SH_SLICE_TYPE_SI )
            if( !pps->entropy_coding_mode_flag ) {
                bs_write_ue(b, mb_skip_run);
                prevMbSkipped = ( mb_skip_run > 0 );
                for( i=0; i<mb_skip_run; i++ )
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                moreDataFlag = more_rbsp_data( );
            } else {
                bs_write_ae(v, mb_skip_flag);
                moreDataFlag = !mb_skip_flag;
            }
        if( moreDataFlag ) {
            if( MbaffFrameFlag && ( CurrMbAddr % 2 == 0 ||
                                    ( CurrMbAddr % 2 == 1 && prevMbSkipped ) ) )
                bs_write_u1(b) | bs_write_ae(v, mb_field_decoding_flag);
            macroblock_layer( );
        }
        if( !pps->entropy_coding_mode_flag )
            moreDataFlag = more_rbsp_data( );
        else {
            if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
                prevMbSkipped = mb_skip_flag;
            if( MbaffFrameFlag && CurrMbAddr % 2 == 0 )
                moreDataFlag = 1;
            else {
                bs_write_ae(v, end_of_slice_flag);
                moreDataFlag = !end_of_slice_flag;
            }
        }
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag );
}
*/




/***************************** debug ******************************/

void debug_sps(sps_t* sps)
{/*
    printf("======= SPS =======\n");
    printf(" profile_idc : %d \n", sps->profile_idc );
    printf(" constraint_set0_flag : %d \n", sps->constraint_set0_flag );
    printf(" constraint_set1_flag : %d \n", sps->constraint_set1_flag );
    printf(" constraint_set2_flag : %d \n", sps->constraint_set2_flag );
    printf(" constraint_set3_flag : %d \n", sps->constraint_set3_flag );
    printf(" reserved_zero_4bits : %d \n", sps->reserved_zero_4bits );
    printf(" level_idc : %d \n", sps->level_idc );
    printf(" seq_parameter_set_id : %d \n", sps->seq_parameter_set_id );
    printf(" chroma_format_idc : %d \n", sps->chroma_format_idc );
    printf(" residual_colour_transform_flag : %d \n", sps->residual_colour_transform_flag );
    printf(" bit_depth_luma_minus8 : %d \n", sps->bit_depth_luma_minus8 );
    printf(" bit_depth_chroma_minus8 : %d \n", sps->bit_depth_chroma_minus8 );
    printf(" qpprime_y_zero_transform_bypass_flag : %d \n", sps->qpprime_y_zero_transform_bypass_flag );
    printf(" seq_scaling_matrix_present_flag : %d \n", sps->seq_scaling_matrix_present_flag );
    //  int seq_scaling_list_present_flag[8];
    //  void* ScalingList4x4[6];
    //  int UseDefaultScalingMatrix4x4Flag[6];
    //  void* ScalingList8x8[2];
    //  int UseDefaultScalingMatrix8x8Flag[2];
    printf(" log2_max_frame_num_minus4 : %d \n", sps->log2_max_frame_num_minus4 );
    printf(" pic_order_cnt_type : %d \n", sps->pic_order_cnt_type );
      printf("   log2_max_pic_order_cnt_lsb_minus4 : %d \n", sps->log2_max_pic_order_cnt_lsb_minus4 );
      printf("   delta_pic_order_always_zero_flag : %d \n", sps->delta_pic_order_always_zero_flag );
      printf("   offset_for_non_ref_pic : %d \n", sps->offset_for_non_ref_pic );
      printf("   offset_for_top_to_bottom_field : %d \n", sps->offset_for_top_to_bottom_field );
      printf("   num_ref_frames_in_pic_order_cnt_cycle : %d \n", sps->num_ref_frames_in_pic_order_cnt_cycle );
    //  int offset_for_ref_frame[256];
    printf(" num_ref_frames : %d \n", sps->num_ref_frames );
    printf(" gaps_in_frame_num_value_allowed_flag : %d \n", sps->gaps_in_frame_num_value_allowed_flag );
    printf(" pic_width_in_mbs_minus1 : %d \n", sps->pic_width_in_mbs_minus1 );
    printf(" pic_height_in_map_units_minus1 : %d \n", sps->pic_height_in_map_units_minus1 );
    printf(" frame_mbs_only_flag : %d \n", sps->frame_mbs_only_flag );
    printf(" mb_adaptive_frame_field_flag : %d \n", sps->mb_adaptive_frame_field_flag );
    printf(" direct_8x8_inference_flag : %d \n", sps->direct_8x8_inference_flag );
    printf(" frame_cropping_flag : %d \n", sps->frame_cropping_flag );
      printf("   frame_crop_left_offset : %d \n", sps->frame_crop_left_offset );
      printf("   frame_crop_right_offset : %d \n", sps->frame_crop_right_offset );
      printf("   frame_crop_top_offset : %d \n", sps->frame_crop_top_offset );
      printf("   frame_crop_bottom_offset : %d \n", sps->frame_crop_bottom_offset );
    printf(" vui_parameters_present_flag : %d \n", sps->vui_parameters_present_flag );

    printf("=== VUI ===\n");
    printf(" aspect_ratio_info_present_flag : %d \n", sps->vui.aspect_ratio_info_present_flag );
      printf("   aspect_ratio_idc : %d \n", sps->vui.aspect_ratio_idc );
        printf("     sar_width : %d \n", sps->vui.sar_width );
        printf("     sar_height : %d \n", sps->vui.sar_height );
    printf(" overscan_info_present_flag : %d \n", sps->vui.overscan_info_present_flag );
      printf("   overscan_appropriate_flag : %d \n", sps->vui.overscan_appropriate_flag );
    printf(" video_signal_type_present_flag : %d \n", sps->vui.video_signal_type_present_flag );
      printf("   video_format : %d \n", sps->vui.video_format );
      printf("   video_full_range_flag : %d \n", sps->vui.video_full_range_flag );
      printf("   colour_description_present_flag : %d \n", sps->vui.colour_description_present_flag );
        printf("     colour_primaries : %d \n", sps->vui.colour_primaries );
        printf("   transfer_characteristics : %d \n", sps->vui.transfer_characteristics );
        printf("   matrix_coefficients : %d \n", sps->vui.matrix_coefficients );
    printf(" chroma_loc_info_present_flag : %d \n", sps->vui.chroma_loc_info_present_flag );
      printf("   chroma_sample_loc_type_top_field : %d \n", sps->vui.chroma_sample_loc_type_top_field );
      printf("   chroma_sample_loc_type_bottom_field : %d \n", sps->vui.chroma_sample_loc_type_bottom_field );
    printf(" timing_info_present_flag : %d \n", sps->vui.timing_info_present_flag );
      printf("   num_units_in_tick : %d \n", sps->vui.num_units_in_tick );
      printf("   time_scale : %d \n", sps->vui.time_scale );
      printf("   fixed_frame_rate_flag : %d \n", sps->vui.fixed_frame_rate_flag );
    printf(" nal_hrd_parameters_present_flag : %d \n", sps->vui.nal_hrd_parameters_present_flag );
    printf(" vcl_hrd_parameters_present_flag : %d \n", sps->vui.vcl_hrd_parameters_present_flag );
      printf("   low_delay_hrd_flag : %d \n", sps->vui.low_delay_hrd_flag );
    printf(" pic_struct_present_flag : %d \n", sps->vui.pic_struct_present_flag );
    printf(" bitstream_restriction_flag : %d \n", sps->vui.bitstream_restriction_flag );
      printf("   motion_vectors_over_pic_boundaries_flag : %d \n", sps->vui.motion_vectors_over_pic_boundaries_flag );
      printf("   max_bytes_per_pic_denom : %d \n", sps->vui.max_bytes_per_pic_denom );
      printf("   max_bits_per_mb_denom : %d \n", sps->vui.max_bits_per_mb_denom );
      printf("   log2_max_mv_length_horizontal : %d \n", sps->vui.log2_max_mv_length_horizontal );
      printf("   log2_max_mv_length_vertical : %d \n", sps->vui.log2_max_mv_length_vertical );
      printf("   num_reorder_frames : %d \n", sps->vui.num_reorder_frames );
      printf("   max_dec_frame_buffering : %d \n", sps->vui.max_dec_frame_buffering );

    printf("=== HRD ===\n");
    printf(" cpb_cnt_minus1 : %d \n", sps->hrd.cpb_cnt_minus1 );
    printf(" bit_rate_scale : %d \n", sps->hrd.bit_rate_scale );
    printf(" cpb_size_scale : %d \n", sps->hrd.cpb_size_scale );
    //  printf("bit_rate_value_minus1[32] : %d \n", sps->hrd.bit_rate_value_minus1[32] ); // up to cpb_cnt_minus1, which is <= 31
    //  printf("cpb_size_value_minus1[32] : %d \n", sps->hrd.cpb_size_value_minus1[32] );
    //  printf("cbr_flag[32] : %d \n", sps->hrd.cbr_flag[32] );
    printf(" initial_cpb_removal_delay_length_minus1 : %d \n", sps->hrd.initial_cpb_removal_delay_length_minus1 );
    printf(" cpb_removal_delay_length_minus1 : %d \n", sps->hrd.cpb_removal_delay_length_minus1 );
    printf(" dpb_output_delay_length_minus1 : %d \n", sps->hrd.dpb_output_delay_length_minus1 );
    printf(" time_offset_length : %d \n", sps->hrd.time_offset_length );*/
}


void debug_pps(pps_t* pps)
{/*
    printf("======= PPS =======\n");
    printf(" pic_parameter_set_id : %d \n", pps->pic_parameter_set_id );
    printf(" seq_parameter_set_id : %d \n", pps->seq_parameter_set_id );
    printf(" entropy_coding_mode_flag : %d \n", pps->entropy_coding_mode_flag );
    printf(" pic_order_present_flag : %d \n", pps->pic_order_present_flag );
    printf(" num_slice_groups_minus1 : %d \n", pps->num_slice_groups_minus1 );
    printf(" slice_group_map_type : %d \n", pps->slice_group_map_type );
    //  int run_length_minus1[8]; // up to num_slice_groups_minus1, which is <= 7 in Baseline and Extended, 0 otheriwse
    //  int top_left[8];
    //  int bottom_right[8];
    //  int slice_group_change_direction_flag;
    //  int slice_group_change_rate_minus1;
    //  int pic_size_in_map_units_minus1;
    //  int slice_group_id[256]; // FIXME what size?
    printf(" num_ref_idx_l0_active_minus1 : %d \n", pps->num_ref_idx_l0_active_minus1 );
    printf(" num_ref_idx_l1_active_minus1 : %d \n", pps->num_ref_idx_l1_active_minus1 );
    printf(" weighted_pred_flag : %d \n", pps->weighted_pred_flag );
    printf(" weighted_bipred_idc : %d \n", pps->weighted_bipred_idc );
    printf(" pic_init_qp_minus26 : %d \n", pps->pic_init_qp_minus26 );
    printf(" pic_init_qs_minus26 : %d \n", pps->pic_init_qs_minus26 );
    printf(" chroma_qp_index_offset : %d \n", pps->chroma_qp_index_offset );
    printf(" deblocking_filter_control_present_flag : %d \n", pps->deblocking_filter_control_present_flag );
    printf(" constrained_intra_pred_flag : %d \n", pps->constrained_intra_pred_flag );
    printf(" redundant_pic_cnt_present_flag : %d \n", pps->redundant_pic_cnt_present_flag );
    printf(" transform_8x8_mode_flag : %d \n", pps->transform_8x8_mode_flag );
    printf(" pic_scaling_matrix_present_flag : %d \n", pps->pic_scaling_matrix_present_flag );
    //  int pic_scaling_list_present_flag[8];
    //  void* ScalingList4x4[6];
    //  int UseDefaultScalingMatrix4x4Flag[6];
    //  void* ScalingList8x8[2];
    //  int UseDefaultScalingMatrix8x8Flag[2];
    printf(" second_chroma_qp_index_offset : %d \n", pps->second_chroma_qp_index_offset );*/
}

void debug_slice_header(slice_header_t* sh)
{/*
    printf("======= Slice Header =======\n");
    printf(" first_mb_in_slice : %d \n", sh->first_mb_in_slice );
    const char* slice_type_name;
    switch(sh->slice_type)
    {
    case SH_SLICE_TYPE_P :       slice_type_name = "P slice"; break;
    case SH_SLICE_TYPE_B :       slice_type_name = "B slice"; break;
    case SH_SLICE_TYPE_I :       slice_type_name = "I slice"; break;
    case SH_SLICE_TYPE_SP :      slice_type_name = "SP slice"; break;
    case SH_SLICE_TYPE_SI :      slice_type_name = "SI slice"; break;
    case SH_SLICE_TYPE_P_ONLY :  slice_type_name = "P slice only"; break;
    case SH_SLICE_TYPE_B_ONLY :  slice_type_name = "B slice only"; break;
    case SH_SLICE_TYPE_I_ONLY :  slice_type_name = "I slice only"; break;
    case SH_SLICE_TYPE_SP_ONLY : slice_type_name = "SP slice only"; break;
    case SH_SLICE_TYPE_SI_ONLY : slice_type_name = "SI slice only"; break;
    default :                    slice_type_name = "Unknown"; break;
    }
    printf(" slice_type : %d ( %s ) \n", sh->slice_type, slice_type_name );

    printf(" pic_parameter_set_id : %d \n", sh->pic_parameter_set_id );
    printf(" frame_num : %d \n", sh->frame_num );
    printf(" field_pic_flag : %d \n", sh->field_pic_flag );
      printf(" bottom_field_flag : %d \n", sh->bottom_field_flag );
    printf(" idr_pic_id : %d \n", sh->idr_pic_id );
    printf(" pic_order_cnt_lsb : %d \n", sh->pic_order_cnt_lsb );
    printf(" delta_pic_order_cnt_bottom : %d \n", sh->delta_pic_order_cnt_bottom );
    // int delta_pic_order_cnt[ 2 ];
    printf(" redundant_pic_cnt : %d \n", sh->redundant_pic_cnt );
    printf(" direct_spatial_mv_pred_flag : %d \n", sh->direct_spatial_mv_pred_flag );
    printf(" num_ref_idx_active_override_flag : %d \n", sh->num_ref_idx_active_override_flag );
    printf(" num_ref_idx_l0_active_minus1 : %d \n", sh->num_ref_idx_l0_active_minus1 );
    printf(" num_ref_idx_l1_active_minus1 : %d \n", sh->num_ref_idx_l1_active_minus1 );
    printf(" cabac_init_idc : %d \n", sh->cabac_init_idc );
    printf(" slice_qp_delta : %d \n", sh->slice_qp_delta );
    printf(" sp_for_switch_flag : %d \n", sh->sp_for_switch_flag );
    printf(" slice_qs_delta : %d \n", sh->slice_qs_delta );
    printf(" disable_deblocking_filter_idc : %d \n", sh->disable_deblocking_filter_idc );
    printf(" slice_alpha_c0_offset_div2 : %d \n", sh->slice_alpha_c0_offset_div2 );
    printf(" slice_beta_offset_div2 : %d \n", sh->slice_beta_offset_div2 );
    printf(" slice_group_change_cycle : %d \n", sh->slice_group_change_cycle );

    printf("=== Prediction Weight Table ===\n");
        printf(" luma_log2_weight_denom : %d \n", sh->pwt.luma_log2_weight_denom );
        printf(" chroma_log2_weight_denom : %d \n", sh->pwt.chroma_log2_weight_denom );
        printf(" luma_weight_l0_flag : %d \n", sh->pwt.luma_weight_l0_flag );
        // int luma_weight_l0[64];
        // int luma_offset_l0[64];
        printf(" chroma_weight_l0_flag : %d \n", sh->pwt.chroma_weight_l0_flag );
        // int chroma_weight_l0[64][2];
        // int chroma_offset_l0[64][2];
        printf(" luma_weight_l1_flag : %d \n", sh->pwt.luma_weight_l1_flag );
        // int luma_weight_l1[64];
        // int luma_offset_l1[64];
        printf(" chroma_weight_l1_flag : %d \n", sh->pwt.chroma_weight_l1_flag );
        // int chroma_weight_l1[64][2];
        // int chroma_offset_l1[64][2];

    printf("=== Ref Pic List Reordering ===\n");
        printf(" ref_pic_list_reordering_flag_l0 : %d \n", sh->rplr.ref_pic_list_reordering_flag_l0 );
        printf(" ref_pic_list_reordering_flag_l1 : %d \n", sh->rplr.ref_pic_list_reordering_flag_l1 );
        // int reordering_of_pic_nums_idc;
        // int abs_diff_pic_num_minus1;
        // int long_term_pic_num;

    printf("=== Decoded Ref Pic Marking ===\n");
        printf(" no_output_of_prior_pics_flag : %d \n", sh->drpm.no_output_of_prior_pics_flag );
        printf(" long_term_reference_flag : %d \n", sh->drpm.long_term_reference_flag );
        printf(" adaptive_ref_pic_marking_mode_flag : %d \n", sh->drpm.adaptive_ref_pic_marking_mode_flag );
        // int memory_management_control_operation;
        // int difference_of_pic_nums_minus1;
        // int long_term_pic_num;
        // int long_term_frame_idx;
        // int max_long_term_frame_idx_plus1;
		*/
}

void debug_aud(aud_t* aud)
{/*
    printf("======= Access Unit Delimiter =======\n");
    const char* primary_pic_type_name;
    switch (aud->primary_pic_type)
    {
    case AUD_PRIMARY_PIC_TYPE_I :       primary_pic_type_name = "I"; break;
    case AUD_PRIMARY_PIC_TYPE_IP :      primary_pic_type_name = "I, P"; break;
    case AUD_PRIMARY_PIC_TYPE_IPB :     primary_pic_type_name = "I, P, B"; break;
    case AUD_PRIMARY_PIC_TYPE_SI :      primary_pic_type_name = "SI"; break;
    case AUD_PRIMARY_PIC_TYPE_SISP :    primary_pic_type_name = "SI, SP"; break;
    case AUD_PRIMARY_PIC_TYPE_ISI :     primary_pic_type_name = "I, SI"; break;
    case AUD_PRIMARY_PIC_TYPE_ISIPSP :  primary_pic_type_name = "I, SI, P, SP"; break;
    case AUD_PRIMARY_PIC_TYPE_ISIPSPB : primary_pic_type_name = "I, SI, P, SP, B"; break;
    default : primary_pic_type_name = "Unknown"; break;
    }
    printf(" primary_pic_type : %d ( %s ) \n", aud->primary_pic_type, primary_pic_type_name );*/
}

void debug_seis(sei_t** seis, int num_seis)
{/*
    printf("======= SEI =======\n");
    const char* sei_type_name;
    int i;
    for (i = 0; i < num_seis; i++)
    {
        sei_t* s = seis[i];
        switch(s->payloadType)
        {
        case SEI_TYPE_BUFFERING_PERIOD :          sei_type_name = "Buffering period"; break;
        case SEI_TYPE_PIC_TIMING :                sei_type_name = "Pic timing"; break;
        case SEI_TYPE_PAN_SCAN_RECT :             sei_type_name = "Pan scan rect"; break;
        case SEI_TYPE_FILLER_PAYLOAD :            sei_type_name = "Filler payload"; break;
        case SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35 : sei_type_name = "User data registered ITU-T T35"; break;
        case SEI_TYPE_USER_DATA_UNREGISTERED :    sei_type_name = "User data unregistered"; break;
        case SEI_TYPE_RECOVERY_POINT :            sei_type_name = "Recovery point"; break;
        case SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION : sei_type_name = "Dec ref pic marking repetition"; break;
        case SEI_TYPE_SPARE_PIC :                 sei_type_name = "Spare pic"; break;
        case SEI_TYPE_SCENE_INFO :                sei_type_name = "Scene info"; break;
        case SEI_TYPE_SUB_SEQ_INFO :              sei_type_name = "Sub seq info"; break;
        case SEI_TYPE_SUB_SEQ_LAYER_CHARACTERISTICS : sei_type_name = "Sub seq layer characteristics"; break;
        case SEI_TYPE_SUB_SEQ_CHARACTERISTICS :   sei_type_name = "Sub seq characteristics"; break;
        case SEI_TYPE_FULL_FRAME_FREEZE :         sei_type_name = "Full frame freeze"; break;
        case SEI_TYPE_FULL_FRAME_FREEZE_RELEASE : sei_type_name = "Full frame freeze release"; break;
        case SEI_TYPE_FULL_FRAME_SNAPSHOT :       sei_type_name = "Full frame snapshot"; break;
        case SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_START : sei_type_name = "Progressive refinement segment start"; break;
        case SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_END : sei_type_name = "Progressive refinement segment end"; break;
        case SEI_TYPE_MOTION_CONSTRAINED_SLICE_GROUP_SET : sei_type_name = "Motion constrained slice group set"; break;
        case SEI_TYPE_FILM_GRAIN_CHARACTERISTICS : sei_type_name = "Film grain characteristics"; break;
        case SEI_TYPE_DEBLOCKING_FILTER_DISPLAY_PREFERENCE : sei_type_name = "Deblocking filter display preference"; break;
        case SEI_TYPE_STEREO_VIDEO_INFO :         sei_type_name = "Stereo video info"; break;
        default: sei_type_name = "Unknown"; break;
        }
        printf("=== %s ===\n", sei_type_name);
        printf(" payloadType : %d \n", s->payloadType );
        printf(" payloadSize : %d \n", s->payloadSize );
        
        printf(" payload : " );
        debug_bytes(s->payload, s->payloadSize);
    }*/
}

/**
 Print the contents of a NAL unit to standard output.
 The NAL which is printed out has a type determined by nal and data which comes from other fields within h depending on its type.
 @param[in]      h          the stream object
 @param[in]      nal        the nal unit
 */
void debug_nal(h264_stream_t* h, nal_t* nal)
{/*
    printf("==================== NAL ====================\n");
    printf(" forbidden_zero_bit : %d \n", nal->forbidden_zero_bit );
    printf(" nal_ref_idc : %d \n", nal->nal_ref_idc );
    // TODO make into subroutine
    const char* nal_unit_type_name;
    switch (nal->nal_unit_type)
    {
    case  NAL_UNIT_TYPE_UNSPECIFIED :                   nal_unit_type_name = "Unspecified"; break;
    case  NAL_UNIT_TYPE_CODED_SLICE_NON_IDR :           nal_unit_type_name = "Coded slice of a non-IDR picture"; break;
    case  NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A :  nal_unit_type_name = "Coded slice data partition A"; break;
    case  NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B :  nal_unit_type_name = "Coded slice data partition B"; break;
    case  NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C :  nal_unit_type_name = "Coded slice data partition C"; break;
    case  NAL_UNIT_TYPE_CODED_SLICE_IDR :               nal_unit_type_name = "Coded slice of an IDR picture"; break;
    case  NAL_UNIT_TYPE_SEI :                           nal_unit_type_name = "Supplemental enhancement information (SEI)"; break;
    case  NAL_UNIT_TYPE_SPS :                           nal_unit_type_name = "Sequence parameter set"; break;
    case  NAL_UNIT_TYPE_PPS :                           nal_unit_type_name = "Picture parameter set"; break;
    case  NAL_UNIT_TYPE_AUD :                           nal_unit_type_name = "Access unit delimiter"; break;
    case  NAL_UNIT_TYPE_END_OF_SEQUENCE :               nal_unit_type_name = "End of sequence"; break;
    case  NAL_UNIT_TYPE_END_OF_STREAM :                 nal_unit_type_name = "End of stream"; break;
    case  NAL_UNIT_TYPE_FILLER :                        nal_unit_type_name = "Filler data"; break;
    case  NAL_UNIT_TYPE_SPS_EXT :                       nal_unit_type_name = "Sequence parameter set extension"; break;
        // 14..18    // Reserved
    case  NAL_UNIT_TYPE_CODED_SLICE_AUX :               nal_unit_type_name = "Coded slice of an auxiliary coded picture without partitioning"; break;
        // 20..23    // Reserved
        // 24..31    // Unspecified
    default :                                           nal_unit_type_name = "Unknown"; break;
    }
    printf(" nal_unit_type : %d ( %s ) \n", nal->nal_unit_type, nal_unit_type_name );

    if( nal->nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_NON_IDR) { debug_slice_header(h->sh); }
    else if( nal->nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_IDR) { debug_slice_header(h->sh); }
    else if( nal->nal_unit_type == NAL_UNIT_TYPE_SPS) { debug_sps(h->sps); }
    else if( nal->nal_unit_type == NAL_UNIT_TYPE_PPS) { debug_pps(h->pps); }
    else if( nal->nal_unit_type == NAL_UNIT_TYPE_AUD) { debug_aud(h->aud); }
    else if( nal->nal_unit_type == NAL_UNIT_TYPE_SEI) { debug_seis(h->seis, h->num_seis); }*/
}

void debug_bytes(uint8_t* buf, int len)
{/*
    int i;
    for (i = 0; i < len; i++)
    {
        printf("%02X ", buf[i]);
        if ((i+1) % 16 == 0) { printf ("\n"); }
    }
    printf("\n");*/
}

void debug_bs(bs_t* b)
{/*
    bs_t* b2 = (bs_t*)malloc(sizeof(bs_t));
    bs_init(b2, b->start, b->end - b->start);

    while (b2->p < b->p || 
           (b2->p == b->p && b2->bits_left > b->bits_left))
    {
        printf("%d", bs_read_u1(b2));
        if (b2->bits_left == 8) { printf(" "); }
    }

    free(b2);*/
}

H264Tool::H264Tool():
m_nUid(0),
m_pStream(NULL)
{
	m_pStream = h264_new();
}

H264Tool::~H264Tool()
{
	h264_free(m_pStream);
}

int H264Tool::getH264Info(const unsigned char *pbBitstream, unsigned int dwLen, h264_info_t *h264_info) 
{
	int ret = 0;

	int rsz = 0;
	int sz = 0;
	int64_t off = 0;
	uint8_t *p = (uint8_t *)pbBitstream;
	int nal_start, offset;
	bs_t b;

	h264_info->frame_type = OTHER_FRAME;
	sz = dwLen;

	while (_find_nal_unit(p, sz, &nal_start, &offset) > 0)
	{
        if (nal_start > 64)
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264Tool::getH264Info: nal_start=%d, error. \n", 	__LINE__, nal_start);

		ret = 1;

		p += nal_start;
		sz -= nal_start;
	
		uint8_t *buf = p;
		int size = sz;
		nal_t *nal = m_pStream->nal;
		bs_init(&b, buf, sz);

		nal->forbidden_zero_bit = bs_read_f(&b,1);
		nal->nal_ref_idc = bs_read_u(&b,2);
		nal->nal_unit_type = bs_read_u(&b,5);

		bs_init(&b, p+1, sz-1);

		/*
		uint8_t* rbsp_buf = (uint8_t*)malloc(size);
		int rbsp_size = 0;
		int nal_size = size;

		nal_to_rbsp(buf, &nal_size, rbsp_buf, &rbsp_size);
		b = bs_new(rbsp_buf, rbsp_size);
		*/

		if ( nal->nal_unit_type == 0) {}									 //  0    Unspecified
		else if ( nal->nal_unit_type == 1) {
			read_slice_layer_rbsp(m_pStream, &b); 

			h264_info->frame_no = m_pStream->sh->frame_num;
			h264_info->frame_type = H264_P_FRAME;
			h264_info->offset = offset;
			break;
		}       //  1    Coded slice of a non-IDR picture
		else if ( nal->nal_unit_type == 2) {  }                           //  2    Coded slice data partition A
		else if ( nal->nal_unit_type == 3) {  }                           //  3    Coded slice data partition B
		else if ( nal->nal_unit_type == 4) {  }                           //  4    Coded slice data partition C
		else if ( nal->nal_unit_type == 5) {
			read_slice_layer_rbsp(m_pStream, &b); 

			h264_info->frame_no = m_pStream->sh->frame_num;
			if (H264_SPS_FRAME == h264_info->frame_type) {
				h264_info->frame_type = H264_SI_FRAME;
			}
			else {
				h264_info->frame_type = H264_I_FRAME;
				h264_info->offset = offset;
			}
			break;
		}       //  5    Coded slice of an IDR picture
		else if ( nal->nal_unit_type == 6) { /*read_sei_rbsp(m_pStream, b);*/ }         //  6    Supplemental enhancement information (SEI)
		else if ( nal->nal_unit_type == 7) {

			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("############################################################. \n", 	__LINE__));
			/*DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("#%d H264Tool::getH264Info: uid=%d, dwLen=%lu, pbBitstream=", 	__LINE__, m_nUid, dwLen));
			for (int i=0; i < dwLen; i++)
				printf("0x%.2x, ", (unsigned int)*(pbBitstream+i));
			printf("\n");*/
			ret = read_seq_parameter_set_rbsp(m_pStream, &b); 
			if (ret < 1) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) H264Tool::getH264Info: uid=%d, error. \n", 	__LINE__, m_nUid);
				break;
			}
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("#%d H264Tool::getH264Info: uid=%d, width=%d, height=%d, vui_parameters_present_flag=%d, hrd.cpb_cnt_minus1=%d. \n", 	__LINE__, m_nUid, m_pStream->sps->width, m_pStream->sps->height, m_pStream->sps->vui_parameters_present_flag, m_pStream->sps->hrd.cpb_cnt_minus1));
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("############################################################. \n", 	__LINE__));

			h264_info->frame_type = H264_SPS_FRAME;
			h264_info->offset = offset;

			if (m_pStream->sps) {
				h264_info->width = m_pStream->sps->width;
				h264_info->height = m_pStream->sps->height;
			}
			else {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) H264Tool::getH264Info: uid=%d, error. \n", 	__LINE__, m_nUid);
			}
		} //  7    Sequence parameter set
		else if ( nal->nal_unit_type == 8) {
			//read_pic_parameter_set_rbsp(m_pStream, b); 

			if (OTHER_FRAME == h264_info->frame_type)
				h264_info->frame_type = H264_PPS_FRAME;
		} //  8    Picture parameter set
		else if ( nal->nal_unit_type == 9) { /*read_access_unit_delimiter_rbsp(m_pStream, &b);*/ } //  9    Access unit delimiter
		else if ( nal->nal_unit_type == 10) { /*read_end_of_seq_rbsp(hm_pStream, &b);*/ }       // 10    End of sequence
		else if ( nal->nal_unit_type == 11) { /*read_end_of_stream_rbsp(m_pStream, &b);*/ }    // 11    End of stream
		else if ( nal->nal_unit_type == 12) { /* read_filler_data_rbsp(m_pStream, &b); */ }      // 12    Filler data
		else if ( nal->nal_unit_type == 13) { /* seq_parameter_set_extension_rbsp( ) */ } // 13    Sequence parameter set extension
		//14..18 Reserved
		else if ( nal->nal_unit_type == 19) { /*read_slice_layer_rbsp(m_pStream, &b);*/ }      // 19    Coded slice of an auxiliary coded picture without partitioning
		//20..23 Reserved
		//24..31 Unspecified
		else {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) H264Tool::getH264Info: uid=%d, error. \n", __LINE__, m_nUid);
			ret = 0;
			break;
		}
	}

	return ret;
}