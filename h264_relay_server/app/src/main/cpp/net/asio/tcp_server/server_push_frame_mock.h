#if !defined __SERVER_PUSH_FRAME_MOCK_H__
#define __SERVER_PUSH_FRAME_MOCK_H__

#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <fstream>

#if defined(WIN32)
#include "media/capture/win32/screen.h"
#include "media/yuv/win32/ks_codec_format_convert.h"
#include "third_party/x264/ks_x264bridge.h"
#endif
#include "base/debug-msg-c.h"
#include "base/loop.h"
#include "base/time-common.h"

#include "live_stream_server-c.h"

namespace net {

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

class ServerPushFrameFileMock
{
public:
	ServerPushFrameFileMock()
     :	test_file_fd_(-1),
        buff_(NULL),
        buff_len_(0),
		has_sps_gop_(false)
    {
    }

    virtual ~ServerPushFrameFileMock() {
        if (buff_) {
            delete []buff_;
            buff_ = NULL;
        }
    }

	static ServerPushFrameFileMock& Instance() {
		static ServerPushFrameFileMock instance;
		return instance;
	}

    void InitStream() {
        if (buff_) {
            buff_ptr_ = buff_;
            buff_left_len_ = buff_len_;
        }
        else {
            std::ifstream file;
#if defined(WIN32)
            file.open("test.h264", std::ios::in | std::ios::binary);
#endif
            file.seekg(0, std::ios::end);
            buff_len_ = file.tellg();
            if (buff_len_ < 0) {
                DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "??? (%u) ServerPushFrameFileMock::InitStream: error, buff_len_ = %d. \n", __LINE__,
                    buff_len_);
            }

            file.seekg(0, std::ios::beg);

            buff_ = new unsigned char[buff_len_];
            file.read((char *)buff_, buff_len_);
            
            buff_ptr_ = buff_;
            buff_left_len_ = buff_len_;
        }
    }

    void GetFrame(unsigned char **out, unsigned int *out_len) {

		unsigned char *frame_ptr = buff_ptr_;
		int frame_len = 0;
		int nal_start = 0;
		int nal_end = 0;

		bool get_frame = false;

		while (1) {
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ServerPushFileMock::GetFrame: buff_ptr_=>0x%.02X%.02X%.02X%.02X%.02X, gop_ct_=%d. \n", __LINE__,
			//            *(buff_ptr_), *(buff_ptr_ + 1), *(buff_ptr_ + 2), *(buff_ptr_ + 3), *(buff_ptr_ + 4), gop_ct_);

			if (find_nal_unit(buff_ptr_, buff_left_len_, &nal_start, &nal_end) > 0) {

				unsigned char nal_type = *(buff_ptr_ + nal_start) & 0x1F;
				if (1 == nal_type) {
					gop_ct_++;

					//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ServerPushFileMock::GetFrame: get p-frame, 0x%.02X%.02X%.02X%.02X%.02X, gop_ct_=%d. \n", __LINE__,
					//	*(buff_ptr_), *(buff_ptr_ + 1), *(buff_ptr_ + 2), *(buff_ptr_ + 3), *(buff_ptr_ + 4), gop_ct_);

					frame_ptr = buff_ptr_;
					frame_len += nal_end;

					buff_ptr_ += nal_end;
					buff_left_len_ -= nal_end;

					get_frame = true;
					break;
				}
				else if (7 == nal_type) {
					has_sps_gop_ = true;
					gop_ct_ = 1;

					//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "@@@ (%u) ServerPushFileMock::GetFrame: get i-frame, 0x%.02X%.02X%.02X%.02X%.02X, gop_ct_=%d \n", __LINE__,
					//	*(buff_ptr_), *(buff_ptr_ + 1), *(buff_ptr_ + 2), *(buff_ptr_ + 3), *(buff_ptr_ + 4), gop_ct_);

					frame_ptr = buff_ptr_;
					frame_len += nal_end;

					buff_ptr_ += nal_end;
					buff_left_len_ -= nal_end;
				}
				else if (8 == nal_type) {

					//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "@@@ (%u) ServerPushFileMock::GetFrame: get i-frame, 0x%.02X%.02X%.02X%.02X%.02X, gop_ct_=%d \n", __LINE__,
					//	*(buff_ptr_), *(buff_ptr_ + 1), *(buff_ptr_ + 2), *(buff_ptr_ + 3), *(buff_ptr_ + 4), gop_ct_);

					// 20181016 james: add other frame.
					frame_len += nal_end;

					buff_ptr_ += nal_end;
					buff_left_len_ -= nal_end;
				}
				else if (5 == nal_type) {

					//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "@@@ (%u) ServerPushFileMock::GetFrame: get i-frame, 0x%.02X%.02X%.02X%.02X%.02X, gop_ct_=%d \n", __LINE__,
					//	*(buff_ptr_), *(buff_ptr_ + 1), *(buff_ptr_ + 2), *(buff_ptr_ + 3), *(buff_ptr_ + 4), gop_ct_);

					if (false == has_sps_gop_) {
						frame_ptr = buff_ptr_;
						frame_len = nal_end;
					}
					else {
						frame_len += nal_end;
						has_sps_gop_ = false;
					}

					buff_ptr_ += nal_end;
					buff_left_len_ -= nal_end;

					get_frame = true;
					break;
				}
				else {
					// 20181016 james: add other frame.
					frame_len += nal_end;

					buff_ptr_ += nal_end;
					buff_left_len_ -= nal_end;
				}
			}
			else {
				frame_ptr = buff_;
				frame_len = 0;

				buff_ptr_ = buff_;
				buff_left_len_ = buff_len_;
			}
		}

		if (true == get_frame) {
			//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ServerPushFileMock::GetFrame: get frame, 0x%.02X%.02X%.02X%.02X%.02X, gop_ct_=%d. \n", __LINE__,
			//	*(frame_ptr), *(frame_ptr + 1), *(frame_ptr + 2), *(frame_ptr + 3), *(frame_ptr + 4), gop_ct_);

			*out = frame_ptr;
			*out_len = frame_len;

			// 20180727 james: test
#if 0
			static std::ofstream out_strm;
			if (!out_strm.is_open())
				out_strm.open("test_out.h264", std::ios::binary);

			if (out_strm)
				out_strm.write(reinterpret_cast<char *>(frame_ptr), frame_len);
#endif
		}
    }

    void OnCall(const void *user_arg)
    {
        unsigned char *frm_ptr = NULL;
        unsigned int frm_len = 0;
        GetFrame(&frm_ptr, &frm_len);
   
        if (frm_len > 0) {
			PushFrame(MEDIA_FRAME_FMT_H264, frm_ptr, frm_len);
        }

        return;
    }

    void UnitTest() {
        if (NULL == loop_) {
            loop_.reset(new base::Loop);
			// 20180426 james: test
#if 0
            loop_->set_timeval(1);
#else
			loop_->set_timeval(33);
#endif
        }

        InitStream();
		loop_->Start(std::bind(&ServerPushFrameFileMock::OnCall, this, std::placeholders::_1), NULL);
        
        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) ServerPushFrameFileMock::UnitTest: out. \n", __LINE__);
    }

    void set_test_file(const int fd) {
        test_file_fd_ = fd;
    }

    void set_buff(const unsigned char *buff, int buff_len) {
        if (buff_) {
            delete []buff_;
            buff_ = NULL;
        }

        buff_ = new unsigned char[buff_len];
        memcpy(buff_, buff, buff_len);
        buff_len_ = buff_len;
    }

    int test_file_fd_;
    unsigned char *buff_;
    int buff_len_;

    unsigned char *buff_ptr_;
    unsigned int buff_left_len_;
    int gop_ct_;

	bool has_sps_gop_;

    std::shared_ptr<base::Loop> loop_;

protected:

};

} // namespace net

#endif // __SERVER_PUSH_FRAME_MOCK_H__
