#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include "../common/gstreamer_backend.h"
#include <pthread.h>

#define LOG_TAG "RCTGstPlayerController"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

ANativeWindow *native_window;

// Java callbacks
static jmethodID on_player_init_id;
static jmethodID on_state_changed_id;
static jmethodID on_uri_changed_id;
static jmethodID on_eos_id;
static jmethodID on_element_error_id;

// Global context
pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
jobject app;

static JavaVM *jvm;

/* Register this thread with the VM */
static JNIEnv *attach_current_thread(void) {
    JNIEnv *env;
    JavaVMAttachArgs args;

    args.version = JNI_VERSION_1_6;
    args.name = NULL;
    args.group = NULL;

    if ((*jvm)->AttachCurrentThread(jvm, &env, &args) < 0) {
        LOGE("Failed to attach current thread");
        return NULL;
    }

    LOGD("Thread attached: %p", env);
    return env;
}

/* Unregister this thread from the VM */
static void detach_current_thread(void *env) {
    LOGD("Detaching thread %p", g_thread_self());
    (*jvm)->DetachCurrentThread(jvm);
    LOGD("Thread detached: %p", env);
}

/* Retrieve the JNI environment for this thread */
static JNIEnv *get_jni_env(void) {
    JNIEnv *env;

    if ((env = pthread_getspecific(current_jni_env)) == NULL) {
        env = attach_current_thread();
        pthread_setspecific(current_jni_env, env);
    }

    return env;
}

// Bindings methods
static jstring native_rct_gst_get_gstreamer_info(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;

    LOGD("Getting GStreamer info");
    char *version_utf8 = rct_gst_get_info();
    if (version_utf8 != NULL) {
        LOGD("GStreamer version obtained: %s", version_utf8);
    } else {
        LOGE("Failed to obtain GStreamer version");
    }

    jstring version_jstring = (*env)->NewStringUTF(env, version_utf8);
    g_free(version_utf8);
    return version_jstring;
}


static void native_rct_gst_set_drawable_surface(JNIEnv* env, jobject thiz, jobject surface) {
    (void)env;
    (void)thiz;

    if (surface == NULL) {
        LOGI("Surface is NULL");
        return;
    }

    native_window = ANativeWindow_fromSurface(env, surface);
    if (native_window == NULL) {
        LOGE("Failed to get native window from surface");
        return;
    }

    LOGI("Setting drawable surface: %p", native_window);
    LOGD("Before calling rct_gst_set_drawable_surface in native_rct_gst_set_drawable_surface");
    rct_gst_set_drawable_surface((guintptr)native_window);
    LOGD("After calling rct_gst_set_drawable_surface in native_rct_gst_set_drawable_surface");
}

static void native_rct_gst_set_pipeline_state(JNIEnv* env, jobject thiz, jint state) {
    (void)env;
    (void)thiz;

    LOGI("Setting pipeline state: %d", state);
    rct_gst_set_pipeline_state((GstState) state);
}

static void native_rct_gst_set_uri(JNIEnv* env, jobject thiz, jstring uri_j) {
    (void)env;
    (void)thiz;

    const gchar *uri = (*env)->GetStringUTFChars(env, uri_j, 0);
    LOGI("Setting URI: %s", uri);
    rct_gst_set_uri(g_strdup(uri));  // Duplicate the URI here as well.
    (*env)->ReleaseStringUTFChars(env, uri_j, uri);
}

static void native_rct_gst_set_debugging(JNIEnv* env, jobject thiz, jboolean is_debugging) {
    (void)env;
    (void)thiz;
    
    LOGI("Setting debugging: %s", is_debugging ? "true" : "false");
    rct_gst_set_debugging(is_debugging);
}

void native_on_init() {
    JNIEnv *env = get_jni_env();
    LOGD("Calling onInit callback");
    (*env)->CallVoidMethod(env, app, on_player_init_id);
}

void native_on_state_changed(GstState old_state, GstState new_state) {
    JNIEnv *env = get_jni_env();
    LOGI("State changed from %d to %d", old_state, new_state);
    (*env)->CallVoidMethod(env, app, on_state_changed_id, (jint)old_state, (jint)new_state);
}

void native_on_uri_changed(gchar *_new_uri) {
    JNIEnv *env = get_jni_env();
    jstring new_uri = (*env)->NewStringUTF(env, _new_uri);
    LOGI("URI changed: %s", _new_uri);
    (*env)->CallVoidMethod(env, app, on_uri_changed_id, new_uri);
}

void native_on_eos() {
    JNIEnv *env = get_jni_env();
    LOGD("End of stream (EOS) reached");
    (*env)->CallVoidMethod(env, app, on_eos_id);
}

void native_on_element_error(gchar *_source, gchar *_message, gchar *_debug_info) {
    JNIEnv *env = get_jni_env();

    jstring source = (*env)->NewStringUTF(env, _source);
    jstring message = (*env)->NewStringUTF(env, _message);
    jstring debug_info = (*env)->NewStringUTF(env, _debug_info);
    LOGE("Element error - Source: %s, Message: %s, Debug info: %s", _source, _message, _debug_info);
    (*env)->CallVoidMethod(env, app, on_element_error_id, source, message, debug_info);
}

static void native_rct_gst_init_and_run(JNIEnv* env, jobject thiz, jobject j_configuration) {
    LOGD("Initializing and running GStreamer");
    RctGstConfiguration* configuration = rct_gst_get_configuration();
    jclass configuration_class = (*env)->GetObjectClass(env, j_configuration);

    // Creating native code internal data gst_app_thread
    app = (*env)->NewGlobalRef(env, thiz);

    // Defining initial drawable surface (ids)
    jfieldID ids_field_id = (*env)->GetFieldID(env, configuration_class, "initialDrawableSurface", "Landroid/view/Surface;");
    jobject surface = (*env)->GetObjectField(env, j_configuration, ids_field_id);
    native_window = ANativeWindow_fromSurface(env, surface);
    configuration->initialDrawableSurface = (guintptr)native_window;
    LOGI("Initial drawable surface set: %p", native_window);

    // Getting all callbacks
    jclass klass = (*env)->FindClass(env, "com/gstreamertest/RCTGstPlayerController");  // Updated path
    on_player_init_id = (*env)->GetMethodID(env, klass, "onInit", "()V");
    on_state_changed_id = (*env)->GetMethodID(env, klass, "onStateChanged", "(II)V");
    on_uri_changed_id = (*env)->GetMethodID(env, klass, "onUriChanged", "(Ljava/lang/String;)V");
    on_eos_id = (*env)->GetMethodID(env, klass, "onEOS", "()V");
    on_element_error_id = (*env)->GetMethodID(env, klass, "onElementError", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

    configuration->onInit = native_on_init;
    configuration->onStateChanged = native_on_state_changed;
    configuration->onUriChanged = native_on_uri_changed;
    configuration->onEOS = native_on_eos;
    configuration->onElementError = native_on_element_error;

    rct_gst_init(configuration);
    LOGD("GStreamer initialized");

    pthread_create(&gst_app_thread, NULL, (void *)&rct_gst_run_loop, NULL);
    LOGD("GStreamer run loop started");
}

static JNINativeMethod native_methods[] = {
    { "nativeRCTGstGetGStreamerInfo", "()Ljava/lang/String;", (void *) native_rct_gst_get_gstreamer_info },
    { "nativeRCTGstInitAndRun", "(Lcom/gstreamertest/utils/RCTGstConfiguration;)V", (void *) native_rct_gst_init_and_run },
    { "nativeRCTGstSetPipelineState", "(I)V", (void *) native_rct_gst_set_pipeline_state },
    { "nativeRCTGstSetDrawableSurface", "(Landroid/view/Surface;)V", (void *) native_rct_gst_set_drawable_surface },
    { "nativeRCTGstSetUri", "(Ljava/lang/String;)V", (void *) native_rct_gst_set_uri },
    { "nativeRCTGstSetDebugging", "(Z)V", (void *) native_rct_gst_set_debugging }
};

// Called by JNI
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;

    // Storing global context
    jvm = vm;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Could not retrieve JNIEnv");
        return 0;
    }

    jclass klass = (*env)->FindClass(env, "com/gstreamertest/RCTGstPlayerController");  // Updated path
    if (klass == NULL) {
        LOGE("Could not find class");
        return 0;
    }

    if ((*env)->RegisterNatives(env, klass, native_methods, sizeof(native_methods) / sizeof(native_methods[0])) < 0) {
        LOGE("Could not register native methods");
        return 0;
    }

    pthread_key_create(&current_jni_env, detach_current_thread);
    LOGD("JNI_OnLoad completed");

    return JNI_VERSION_1_6;
}
