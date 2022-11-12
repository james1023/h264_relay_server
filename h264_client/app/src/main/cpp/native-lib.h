
#ifndef __NATIVE_LIB_H__
#define __NATIVE_LIB_H__

#include <jni.h>

#include <mutex>
#include "base/debug-msg-c.h"

class H264ClientJni
{
private:
    H264ClientJni():
            jvm_(NULL),
            jcls_(NULL),
            jobj_(NULL),
            MID_H264ClientJni_OnFrame_(NULL),
            gop_no_(0)
    {

    }

    virtual ~H264ClientJni() {
        if (jcls_) {
            jenv_->DeleteGlobalRef(jcls_);
            jcls_ = NULL;
        }

        if (jobj_) {
            jenv_->DeleteGlobalRef(jobj_);
            jobj_ = NULL;
        }

        if (frame_info_cls_) {
            jenv_->DeleteGlobalRef(frame_info_cls_);
            frame_info_cls_ = NULL;
        }
    }

    H264ClientJni(const H264ClientJni&) = delete;
    H264ClientJni & operator=(const H264ClientJni&) = delete;

public:
    static H264ClientJni& Instance() {
        static H264ClientJni instance;
        return instance;
    }

    void InitJobj(JNIEnv* env, jclass clazz);

    JavaVM *jvm() const {
        return jvm_;
    }
    void set_jvm(JavaVM *in) {
        jvm_ = in;
    }

    JNIEnv *jenv() const {
        return jenv_;
    }
    void set_jenv(JNIEnv *in) {
        jenv_ = in;
    }

    jclass &jcls() {
        return jcls_;
    }
    void set_jcls(jclass in) {
        jcls_ = in;
    }

    jobject &jobj() {
        return jobj_;
    }
    void set_jobj(jobject in) {
        jobj_ = in;
    }

    jmethodID &on_frame_mid() {
        return MID_H264ClientJni_OnFrame_;
    }
    void set_on_frame_mid(jmethodID in) {
        MID_H264ClientJni_OnFrame_ = in;
    }

    jclass &frame_info_cls() {
        return frame_info_cls_;
    }
    void set_frame_info_cls(jclass in) {
        frame_info_cls_ = in;
    }

    jfieldID &frame_info_fmt_fid() {
        return FID_FrameInfo_fmt_;
    }
    void set_frame_info_fmt_fid(jfieldID in) {
        FID_FrameInfo_fmt_ = in;
    }

    jfieldID &frame_info_frame_width_fid() {
        return FID_FrameInfo_frame_width_;
    }
    void set_frame_info_frame_width_fid(jfieldID in) {
        FID_FrameInfo_frame_width_ = in;
    }

    jfieldID &frame_info_frame_height_fid() {
        return FID_FrameInfo_frame_height_;
    }
    void set_frame_info_frame_height_fid(jfieldID in) {
        FID_FrameInfo_frame_height_ = in;
    }

    jfieldID &frame_info_is_i_frame_fid() {
        return FID_FrameInfo_is_i_frame_;
    }
    void set_frame_info_is_i_frame_fid(jfieldID in) {
        FID_FrameInfo_is_i_frame_ = in;
    }

    jfieldID &frame_info_debug_frame_no_fid() {
        return FID_FrameInfo_debug_frame_no_;
    }
    void set_frame_info_debug_frame_no_fid(jfieldID in) {
        FID_FrameInfo_debug_frame_no_ = in;
    }

    jmethodID &on_status_mid() {
        return MID_H264ClientJni_OnStatus_;
    }
    void set_on_status_mid(jmethodID in) {
        MID_H264ClientJni_OnStatus_ = in;
    }

    std::mutex &jobj_mutex() {
        return jobj_mutex_;
    }

    int gop_no_;

private:
    JavaVM *jvm_;
    JNIEnv *jenv_;
    jclass jcls_;
    jobject jobj_;

    jmethodID MID_H264ClientJni_OnFrame_;
    jmethodID MID_H264ClientJni_OnStatus_;

    jclass frame_info_cls_;
    jfieldID FID_FrameInfo_fmt_;
    jfieldID FID_FrameInfo_frame_width_;
    jfieldID FID_FrameInfo_frame_height_;
    jfieldID FID_FrameInfo_is_i_frame_;
    jfieldID FID_FrameInfo_debug_frame_no_;

    std::mutex jobj_mutex_;
};

#endif // __NATIVE_LIB_H__
