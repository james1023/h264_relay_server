
#include "native-lib.h"

#include <jni.h>
#include <string>

#include "base/debug-msg-c.h"
#include "net/check_stream.h"
#include "net/asio/tcp_client/stream-client-i.h"

void OnFrame(unsigned int sid, STREAM_CLIENT_CB_PROPERTIES *client_cb_props, const void *usr_arg)
{
    std::unique_lock<std::mutex> lock(H264ClientJni::Instance().jobj_mutex());

    JNIEnv *jnienv = NULL;
    bool ret = true;
    jint jres = -1;

    JavaVM *jvm = H264ClientJni::Instance().jvm();
    jclass jcls = H264ClientJni::Instance().jcls();

    do {
        jres = jvm->AttachCurrentThread(&jnienv, NULL);
        if (jres < 0) {
            ret = false;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) OnFrame: AttachCurrentThread, error%d. \n", __LINE__, jres);
            break;
        }

        //DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) OnFrame: e=%u, dwBitstreamLength=%d. \n", __LINE__, client_cb_props->e, client_cb_props->u.video.dwBitstreamLength);

        if (client_cb_props->e == client_cb_props->CLIENT_CB_MEDIA_TYPE_VIDEO) {
            jobject bufout = jnienv->NewDirectByteBuffer((unsigned char *)(client_cb_props->u.video.pBitstreamData), client_cb_props->u.video.dwBitstreamLength);

            jobject frame_info_obj = jnienv->AllocObject(H264ClientJni::Instance().frame_info_cls());

            jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_fmt_fid(), client_cb_props->u.video.fmt);
            jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_frame_width_fid(), client_cb_props->u.video.wImageWidth);
            jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_frame_height_fid(), client_cb_props->u.video.wImageHeight);

            if (1 == client_cb_props->u.video.bIFrame) {
                jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_is_i_frame_fid(), 1);
                H264ClientJni::Instance().gop_no_ = 1;
            }
            else {
                jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_is_i_frame_fid(), 0);
                H264ClientJni::Instance().gop_no_++;
            }

            jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_debug_frame_no_fid(), H264ClientJni::Instance().gop_no_);

            jnienv->CallStaticVoidMethod(jcls, H264ClientJni::Instance().on_frame_mid(), sid, bufout, client_cb_props->u.video.dwBitstreamLength, frame_info_obj);

            jnienv->DeleteLocalRef(bufout);
        }
        else {
            jobject bufout = jnienv->NewDirectByteBuffer((unsigned char *)(client_cb_props->u.audio.pBitstreamData), client_cb_props->u.audio.dwBitstreamLength);

            jobject frame_info_obj = jnienv->AllocObject(H264ClientJni::Instance().frame_info_cls());

            jnienv->SetIntField(frame_info_obj, H264ClientJni::Instance().frame_info_fmt_fid(), client_cb_props->u.audio.fmt);

            jnienv->CallStaticVoidMethod(jcls, H264ClientJni::Instance().on_frame_mid(), sid, bufout, client_cb_props->u.audio.dwBitstreamLength, frame_info_obj);

            jnienv->DeleteLocalRef(bufout);
        }

        jres = -1;
        jres = jvm->DetachCurrentThread();
        if (jres < 0) {
            ret = false;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) OnFrame: AttachCurrentThread, error%d. \n", __LINE__, jres);
            break;
        }

    } while(0);

    return;
}

void OnStatus(unsigned int sid, STREAM_CLIENT_LIVE_STATUS status, const void *usr_arg)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) OnStatus: sid=%u, status=%d. \n", __LINE__, sid, status);

    // james: test
    //return;

    std::unique_lock<std::mutex> lock(H264ClientJni::Instance().jobj_mutex());

    JNIEnv *jnienv = NULL;
    bool ret = true;
    jint jres = -1;

    JavaVM *jvm = H264ClientJni::Instance().jvm();
    // jobject jobj = RtspClientJni::Instance().jobj();
    jclass jcls = H264ClientJni::Instance().jcls();

    do {
        jres = jvm->AttachCurrentThread(&jnienv, NULL);
        if (jres < 0) {
            ret = false;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) OnFrame: AttachCurrentThread, error%d. \n", __LINE__, jres);
            break;
        }

        jnienv->CallStaticVoidMethod(jcls, H264ClientJni::Instance().on_status_mid(), sid, status);

        jres = -1;
        jres = jvm->DetachCurrentThread();
        if (jres < 0) {
            ret = false;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) OnFrame: AttachCurrentThread, error%d. \n", __LINE__, jres);
            break;
        }

    } while(0);

    return;
}

void H264ClientJni::InitJobj(JNIEnv *env, jclass clazz)
{
    std::unique_lock<std::mutex> lock(H264ClientJni::Instance().jobj_mutex());

    if (NULL == jvm_) {
        if (env->GetJavaVM(&jvm_) < 0) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) H264ClientJni::InitJobj, error. \n", __LINE__);
        }
    }

    if (NULL == jenv_) {
        jenv_ = env;
    }

    if (NULL == jcls_) {
        //jobj_ = env->NewGlobalRef(thiz);
        //jclass cls = env->GetObjectClass(thiz);

        jcls_ = (jclass)env->NewGlobalRef(clazz);

        if (NULL == MID_H264ClientJni_OnFrame_)
            MID_H264ClientJni_OnFrame_ = env->GetStaticMethodID(jcls(), "OnFrame", "(ILjava/nio/ByteBuffer;ILcom/example/h264_client/H264ClientJni$FrameInfo;)V");
        if (NULL == MID_H264ClientJni_OnFrame_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetMethodID failed. \n", __LINE__);
        }

        jclass frame_info_cls = env->FindClass("com/example/h264_client/H264ClientJni$FrameInfo");

        FID_FrameInfo_fmt_ = env->GetFieldID(frame_info_cls, "fmt_", "I");
        if (NULL == FID_FrameInfo_fmt_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetFieldID failed. \n", __LINE__);
        }

        FID_FrameInfo_frame_width_ = env->GetFieldID(frame_info_cls, "frame_width_", "I");
        if (NULL == FID_FrameInfo_frame_width_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetFieldID failed. \n", __LINE__);
        }

        FID_FrameInfo_frame_height_ = env->GetFieldID(frame_info_cls, "frame_height_", "I");
        if (NULL == FID_FrameInfo_frame_height_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetFieldID failed. \n", __LINE__);
        }

        FID_FrameInfo_is_i_frame_ = env->GetFieldID(frame_info_cls, "is_i_frame_", "I");
        if (NULL == FID_FrameInfo_is_i_frame_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetFieldID failed. \n", __LINE__);
        }

        FID_FrameInfo_debug_frame_no_ = env->GetFieldID(frame_info_cls, "debug_frame_no_", "I");
        if (NULL == FID_FrameInfo_debug_frame_no_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetFieldID failed. \n", __LINE__);
        }

        if (NULL == frame_info_cls_) {
            frame_info_cls_ = (jclass)env->NewGlobalRef(frame_info_cls);
        }
        env->DeleteLocalRef(frame_info_cls);

        if (NULL == MID_H264ClientJni_OnStatus_)
            MID_H264ClientJni_OnStatus_ = env->GetStaticMethodID(jcls(), "OnStatus", "(II)V");
        if (NULL == MID_H264ClientJni_OnStatus_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264ClientJni::InitJobj: GetMethodID failed. \n", __LINE__);
        }
    }
}

int Java_com_example_h264_client_H264ClientJni_CreateClient(JNIEnv *env, jclass clazz,
        jstring ip, jint trans_proto, jstring username, jstring password)
{
    H264ClientJni::Instance().InitJobj(env, clazz);

    const char *ip_c = env->GetStringUTFChars(ip, NULL);
    const char *username_c = env->GetStringUTFChars(username, NULL);
    const char *password_c = env->GetStringUTFChars(password, NULL);

    TRANSPORT_PROTOCOL_TYPE trans_proto_e = TRANSPORT_PROTOCOL_NOVO_TCP;

    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO,
                "(%u) Java_com_example_h264_client_H264ClientJni_CreateClient in, env=%#x, clazz=%#x, ip_c=%s. \n",
                __LINE__, env, clazz, ip_c);

    int sid = IStreamClient::CreateClient(ip_c, trans_proto_e, username_c, password_c, OnFrame, OnStatus, NULL);

    env->ReleaseStringUTFChars(ip, ip_c);
    env->ReleaseStringUTFChars(username, username_c);
    env->ReleaseStringUTFChars(password, password_c);

    return sid;
}

void Java_com_example_h264_client_H264ClientJni_StartClient(JNIEnv *env, jclass clazz, jint sid)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO,
                "(%u) Java_com_example_h264_client_H264ClientJni_StartClient in, env=%#x, clazz=%#x. \n",
                __LINE__, env, clazz);

    IStreamClient::StartClient(sid);

    return;
}

void Java_com_example_h264_client_H264ClientJni_DestroyClient(JNIEnv *env, jclass clazz,
                                                                    jint sid)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO,
                "(%u) Java_com_example_h264_client_H264ClientJni_DestroyClient in, env=%#x, clazz=%#x, sid=%d. \n",
                __LINE__, env, clazz, sid);

    IStreamClient::DestroyClient(sid);

    return;
}

void Java_com_example_h264_client_H264ClientJni_DestroyAllClient(JNIEnv *env, jclass clazz)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO,
                "(%u) Java_com_example_h264_client_H264ClientJni_DestroyAllClient in, env=%#x, clazz=%#x. \n",
                __LINE__, env, clazz);

    IStreamClient::DestroyAllClient();

    return;
}

static const char *SCClassPathName = "com/example/h264_client/H264ClientJni";
static JNINativeMethod sc_methods[] = {
        {"CreateClient", 							"(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)I", (void *)Java_com_example_h264_client_H264ClientJni_CreateClient},
        {"StartClient", 							    "(I)V", (void *)Java_com_example_h264_client_H264ClientJni_StartClient},
        {"DestroyClient", 							"(I)V", (void *)Java_com_example_h264_client_H264ClientJni_DestroyClient},
        {"DestroyAllClient", 						"()V", (void *)Java_com_example_h264_client_H264ClientJni_DestroyAllClient},
};

// Register several native methods for one class.
static int registerNativeMethods(JNIEnv *env, const char *className,
                                 JNINativeMethod *gMethods, int numMethods)
{
    jclass clazz;
    //	LOGI("RegisterNatives start for '%s', total method number:%d\n", className, numMethods);
    clazz = env->FindClass(className);
    if (clazz == NULL) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "%s(%d) clazz == NULL. \n", __FUNCTION__, __LINE__);
        return JNI_FALSE;
    }
    //	LOGI("FindClass OK!\n");
    //	LOGI("clazz=%d\n",clazz);

    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "%s(%d) RegisterNatives. \n", __FUNCTION__, __LINE__);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

// Register native methods for all classes we know about.
// returns JNI_TRUE on success.
static int registerNatives(JNIEnv *env)
{
    if (!registerNativeMethods(env, SCClassPathName,
                               sc_methods, sizeof(sc_methods)/sizeof(sc_methods[0]))) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "%s(%d) registerNativeMethods. \n", __FUNCTION__, __LINE__);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) JNI_OnLoad: in, vm=%#x, __cplusplus=%u. \n", __LINE__, vm, __cplusplus);

    jint result = JNI_ERR;

    JNIEnv *env = NULL;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "%s(%d) GetEnv. \n", __FUNCTION__, __LINE__);
        return result;
    }

    if (registerNatives(env) != JNI_TRUE) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "%s(%d) registerNatives. \n", __FUNCTION__, __LINE__);
        goto end;
    }

    result = JNI_VERSION_1_4;
    end:

    return result;
}