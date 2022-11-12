
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <unistd.h>
#include <jni.h>
#include <assert.h>

// for native asset manager
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "base/debug-msg-c.h"
#include "net/asio/tcp_server/live_stream_server-c.h"
#include "net/asio/tcp_server/server_push_frame_mock.h"

#include "native-lib.h"

void OnStatus(std::string addr, H264RelayServerJni::STREAM_SERVER_LIVE_STATUS sts, const void *user_arg)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) OnStatus: addr=%s, status=%d. \n", __LINE__, addr.c_str(), sts);

    std::unique_lock<std::mutex> lock(H264RelayServerJni::Instance().jobj_mutex());

    JNIEnv *jnienv = NULL;
    bool ret = true;
    jint jres = -1;

    JavaVM *jvm = H264RelayServerJni::Instance().jvm();
    jclass jcls = H264RelayServerJni::Instance().jcls();

    do {

        jres = jvm->AttachCurrentThread(&jnienv, NULL);
        if (jres < 0) {
            ret = false;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) OnStatus: AttachCurrentThread, error%d. \n", __LINE__, jres);
            break;
        }

        jstring j_addr = jnienv->NewStringUTF(addr.c_str());
        jnienv->CallStaticVoidMethod(jcls, H264RelayServerJni::Instance().on_status_mid(), j_addr, sts);

        jres = -1;
        jres = jvm->DetachCurrentThread();
        if (jres < 0) {
            ret = false;
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) OnStatus: AttachCurrentThread, error%d. \n", __LINE__, jres);
            break;
        }

    } while(0);

    return;
}

void OnTcpServerStatus(const char *addr, LIVE_STATUS sts, const void *user_arg)
{
    H264RelayServerJni::STREAM_SERVER_LIVE_STATUS out_sts;

    switch(sts) {
        case LIVE_STATUS_CONNECT_STARTED:
            out_sts = H264RelayServerJni::STREAM_SERVER_LIVE_STATUS_CONNECT_STARTED;
            break;
        case LIVE_STATUS_CONNECT_DESTROYED:
            out_sts = H264RelayServerJni::STREAM_SERVER_LIVE_STATUS_CONNECT_DESTROYED;
            break;
    }

    OnStatus(addr, out_sts, user_arg);
}

void H264RelayServerJni::InitJobj(JNIEnv *env, jclass clazz)
{
    if (NULL == jvm_) {
        if (env->GetJavaVM(&jvm_) < 0) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? (%u) H264RelayServerJni::InitJobj, error. \n", __LINE__);
        }
    }

    if (NULL == jenv_) {
        jenv_ = env;
    }

    if (NULL == jcls_) {
        //jobj_ = env->NewGlobalRef(thiz);
        //jclass cls = env->GetObjectClass(thiz);

        jcls_ = (jclass)env->NewGlobalRef(clazz);


        if (NULL == MID_ServerJni_OnStatus_)
            MID_ServerJni_OnStatus_ = env->GetStaticMethodID(jcls(), "OnStatus", "(Ljava/lang/String;I)V");
        if (NULL == MID_ServerJni_OnStatus_) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) H264RelayServerJni::InitJobj: GetMethodID failed. \n", __LINE__);
        }
    }
}

JNIEXPORT jboolean JNICALL
Java_com_example_h264_relay_server_ServerJni_CreateServer(JNIEnv *env, jclass clazz)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "20200109.1000 (%u) Java_com_example_h264_relay_server_ServerJni_CreateServer in env=%#x, clazz=%#x. \n",
                __LINE__, env, clazz);

    std::unique_lock<std::mutex> lock(H264RelayServerJni::Instance().jobj_mutex());

    jboolean res = true;

    H264RelayServerJni::Instance().InitJobj(env, clazz);


    do {
        LiveCbs cbs;
        cbs.on_msg = OnTcpServerStatus;
        res = CreateServer("20300", cbs, NULL);
        if (true != res)
            break;
    } while(0);

    return res;
}

JNIEXPORT void JNICALL
Java_com_example_h264_relay_server_ServerJni_ReleaseServer(JNIEnv *env, jclass clazz)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) Java_com_example_h264_relay_server_ServerJni_ReleaseServer in. \n", __LINE__);

    std::unique_lock<std::mutex> lock(H264RelayServerJni::Instance().jobj_mutex());

    ReleaseServer();

    return;
}

JNIEXPORT void JNICALL
Java_com_example_h264_relay_server_ServerJni_PushVideoFrame(JNIEnv *env, jclass clazz,
                                                                 jint fmt, jobject frame, jint frame_len)
{
    //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) Java_com_example_h264_relay_server_ServerJni_PushVideoFrame in. \n", __LINE__);

    unsigned char *frame_buff = (unsigned char *)(env->GetDirectBufferAddress(frame));

    PushFrame((MEDIA_FRAME_FMT)fmt, frame_buff, frame_len);

    return;
}

JNIEXPORT void JNICALL
Java_com_example_h264_relay_server_ServerJni_EnableUnittest(JNIEnv *env, jclass clazz, jobject assetManager, jstring file_path)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) Java_com_example_h264_relay_server_ServerJni_EnableUnittest in. \n", __LINE__);

    // convert Java string to UTF-8
    const char *file_path_tmp = env->GetStringUTFChars(file_path, NULL);

    // use asset manager to open asset by filename
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    assert(NULL != mgr);
    AAsset *asset = AAssetManager_open(mgr, file_path_tmp, AASSET_MODE_UNKNOWN);
    assert(NULL != asset);

    // release the Java string and UTF-8
    env->ReleaseStringUTFChars(file_path, file_path_tmp);

    net::ServerPushFrameFileMock::Instance().set_buff((unsigned char *)AAsset_getBuffer(asset), AAsset_getLength(asset));
    net::ServerPushFrameFileMock::Instance().UnitTest();

    AAsset_close(asset);

    return;
}

static const char *SCClassPathName = "com/example/h264_relay_server/H264RelayServerJni";
static JNINativeMethod sc_methods[] = {
        {"CreateServer", 							"()Z", (void *)Java_com_example_h264_relay_server_ServerJni_CreateServer},
        {"ReleaseServer", 						    "()V", (void *)Java_com_example_h264_relay_server_ServerJni_ReleaseServer},
        {"PushVideoFrame",                          "(ILjava/nio/ByteBuffer;I)V", (void *)Java_com_example_h264_relay_server_ServerJni_PushVideoFrame},
        {"EnableUnittest", 					        "(Landroid/content/res/AssetManager;Ljava/lang/String;)V", (void *)Java_com_example_h264_relay_server_ServerJni_EnableUnittest},
};

/*
* Register several native methods for one class.
*/
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

/*
* Register native methods for all classes we know about.
*
* returns JNI_TRUE on success.
*/
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
/*#if __cplusplus > 201402L
    // C__17 code here
  ...
#endif*/
    DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) JNI_OnLoad: in, vm=%#x, __cplusplus=%u. \n", __LINE__, vm, __cplusplus);

    jint result = JNI_ERR;

    JNIEnv *env = NULL;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
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