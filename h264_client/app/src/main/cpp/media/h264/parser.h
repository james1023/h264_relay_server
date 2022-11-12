#pragma once

extern int find_nal_unit(unsigned char *buf, int size, int *nal_start, int *nal_end);

extern bool check_nal_type(unsigned char *buf, int size, int *nal_type, int *offset, int *start_code);

extern bool GetH264SPSAndPPSData(unsigned char *input, unsigned int input_len, 
                            unsigned char **sps_data, unsigned int *sps_len, 
                            unsigned char **pps_data, unsigned int *pps_len);
