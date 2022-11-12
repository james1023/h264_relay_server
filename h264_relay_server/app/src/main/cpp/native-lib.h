
#ifndef __NATIVE_LIB_H__
#define __NATIVE_LIB_H__

#include <jni.h>

class H264RelayServerJni
{
public:
    enum STREAM_SERVER_LIVE_STATUS {
        STREAM_SERVER_LIVE_STATUS_CONNECT_STARTED = 0,				// stream has started.
        STREAM_SERVER_LIVE_STATUS_CONNECT_DESTROYED,				// stream has been destroyed.
    };

public:
    static H264RelayServerJni& Instance() {
        static H264RelayServerJni instance;
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

    jmethodID &on_status_mid() {
        return MID_ServerJni_OnStatus_;
    }
    void set_on_status_mid(jmethodID in) {
        MID_ServerJni_OnStatus_ = in;
    }

    std::mutex &jobj_mutex() {
        return jobj_mutex_;
    }

    int gop_no_;

private:
    H264RelayServerJni():
            jvm_(NULL),
            jcls_(NULL),
            jobj_(NULL),
            MID_ServerJni_OnStatus_(NULL),
            gop_no_(0)
    {

    }

    virtual ~H264RelayServerJni() {
        if (jcls_) {
            jenv_->DeleteGlobalRef(jcls_);
            jcls_ = NULL;
        }

        if (jobj_) {
            jenv_->DeleteGlobalRef(jobj_);
            jobj_ = NULL;
        }
    }

    H264RelayServerJni(const H264RelayServerJni&) = delete;
    H264RelayServerJni & operator=(const H264RelayServerJni&) = delete;

    JavaVM *jvm_;
    JNIEnv *jenv_;
    jclass jcls_;
    jobject jobj_;

    jmethodID MID_ServerJni_OnStatus_;

    std::mutex jobj_mutex_;
};

#endif // __NATIVE_LIB_H__
