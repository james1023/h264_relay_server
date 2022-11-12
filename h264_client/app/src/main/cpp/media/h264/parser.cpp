
#include "parser.h"

int find_nal_unit(unsigned char *buf, int size, int *nal_start, int *nal_end)
{
    int i;
    // find start
    *nal_start = 0;
    *nal_end = 0;

    i = 0;
    while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
            (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) &&
            (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01)
            )
    {
        i++; // skip leading zero
        if (i+4 >= size) {
            return 0;
        } // did not find nal start
    }

    if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
    {
        i++;
    }

    if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) {
        /* error, should never happen */ return 0;
    }
    i+= 3;
    *nal_start = i;

    while (   //( next_bits( 24 ) != 0x000000 && next_bits( 24 ) != 0x000001 )
            (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) &&
            (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01)
            )
    {
        i++;
        // FIXME the next line fails when reading a nal that ends exactly at the end of the data
        if (i+3 >= size) {
            *nal_end = size;
            return -1;
        } // did not find nal end, stream ended first
    }

    *nal_end = i;
    return (*nal_end - *nal_start);
}

bool check_nal_type(unsigned char *buf, int size, int *nal_type, int *offset, int *start_code)
{
    int i = 0, sc = 0;
  
    while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
           (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) &&
           (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01)
           )
    {
        i++; // skip leading zero
        if (i+4 >= size) {
            return false;
        } // did not find nal start
    }
    
    if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
    {
        i++;
		sc++;
    }
    
    if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) {
        /* error, should never happen */ 
		return false;
    }
    i += 3;
	sc += 3;
    
    if (nal_type) *nal_type = *(buf+i) & 0x1F;
    
    if (offset) *offset = i;
	if (start_code) *start_code = sc;
    
    return true;
}

bool GetH264SPSAndPPSData(unsigned char *input, unsigned int input_len,
                            unsigned char **sps_data, unsigned int *sps_len, 
                            unsigned char **pps_data, unsigned int *pps_len) 
{
    bool res = false;
    unsigned char *input_ptr = input;
    int input_ptr_len = (int)input_len;
    int nal_start = 0;
    int nal_end = 0;
    int offset = 0;
    int nal_type;

    do {
        while (input_ptr_len > 0) {
            if (true == check_nal_type(input_ptr, input_ptr_len, &nal_type, &offset, 0)) {
                if (7 == nal_type || 8 == nal_type) {

					input_ptr = input_ptr + offset - 4;
					input_ptr_len = input_ptr_len - offset + 4;

                    if (find_nal_unit(input_ptr, input_ptr_len, &nal_start, &nal_end) > 0) {
                        nal_type = *(input_ptr+nal_start) & 0x1F;
                        if (7 == nal_type) {
                            if (sps_data) *sps_data = input_ptr;
                            if (sps_len) *sps_len = nal_end;
                            
                            input_ptr += nal_end;
                            input_ptr_len -= nal_end;
                        }
                        else if (8 == nal_type) {
                            if (pps_data) *pps_data = input_ptr;
                            if (pps_len) *pps_len = nal_end;
                            
                            res = true;
                            break;
                        }
                        else {
                            input_ptr += nal_end;
                            input_ptr_len -= nal_end;
                        }
                    }
					else {
						break;
					}
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
    } while(0);
    
    return res;
}
