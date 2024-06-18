#include "gstreamer_backend.h"
#include <android/log.h>
#include <gst/gst.h>

#define LOG_TAG "GStreamerBackend"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Globals items
RctGstConfiguration* configuration;

GstElement *pipeline;
GstElement *source, *depay, *parser, *decoder, *conv, *sink;
GMainLoop *main_loop;
guint bus_watch_id;
GstBus *bus;

// Video
guintptr drawable_surface;
GstVideoOverlay* video_overlay;

// Function declaration for the pad-added callback
void on_pad_added(GstElement *src, GstPad *new_pad, gpointer depay);

// Function to reset the pipeline
void reset_pipeline();

// Getters
RctGstConfiguration *rct_gst_get_configuration()
{
    if (!configuration) {
        configuration = g_malloc(sizeof(RctGstConfiguration));
        configuration->uri = NULL;
        configuration->isDebugging = FALSE;
        
        configuration->onElementError = NULL;
        configuration->onStateChanged = NULL;
        configuration->onUriChanged = NULL;
        
        configuration->onInit = NULL;
        configuration->onEOS = NULL;
    }
    return configuration;
}

// Setters
void rct_gst_set_uri(gchar* _uri) {
    LOGD("Setting URI: %s", _uri);
    rct_gst_get_configuration()->uri = g_strdup(_uri); // Duplicate the URI to avoid issues with the original string.
    if (pipeline) {
        apply_uri();
    }
}

void rct_gst_set_debugging(gboolean is_debugging)
{
    LOGD("Setting debugging: %s", is_debugging ? "true" : "false");
    rct_gst_get_configuration()->isDebugging = is_debugging;
    // TODO: Recreate pipeline...
}

/**********************
 VIDEO HANDLING METHODS
 *********************/
void rct_gst_set_drawable_surface(guintptr _drawableSurface) {
    LOGD("Setting drawable surface from C: %p", (void*)_drawableSurface);
    drawable_surface = _drawableSurface;
    
    if (pipeline && GST_IS_VIDEO_OVERLAY(sink)) {
        LOGD("Setting window handle on video overlay");
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), drawable_surface);
    } else {
        LOGE("Pipeline or video overlay is NULL, cannot set drawable surface");
    }
}

GstBusSyncReply cb_create_window(GstBus *bus, GstMessage *message, gpointer user_data)
{
    if (!gst_is_video_overlay_prepare_window_handle_message(message)) {
        return GST_BUS_PASS;
    }

    if (drawable_surface != 0) {
        LOGD("Setting window handle from message sync");
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), drawable_surface);
    }
    
    gst_message_unref(message);
    return GST_BUS_DROP;
}

/*********************
 APPLICATION CALLBACKS
 ********************/
static void cb_error(GstBus *bus, GstMessage *msg, gpointer *user_data)
{
    GError *err;
    gchar *debug_info;
    
    gst_message_parse_error(msg, &err, &debug_info);
    LOGE("Error received from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
    if (rct_gst_get_configuration()->onElementError) {
        rct_gst_get_configuration()->onElementError(GST_OBJECT_NAME(msg->src), err->message, debug_info);
    }
    g_clear_error(&err);
    g_free(debug_info);
    reset_pipeline(); // Reset pipeline on error
}

static void cb_eos(GstBus *bus, GstMessage *msg, gpointer *user_data)
{
    LOGD("End of stream (EOS) received");
    if (rct_gst_get_configuration()->onEOS) {
        rct_gst_get_configuration()->onEOS();
    }
    reset_pipeline(); // Reset pipeline on EOS
}

static void cb_state_changed(GstBus *bus, GstMessage *msg, gpointer *user_data)
{
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
        LOGD("Pipeline state changed from %s to %s", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        if (rct_gst_get_configuration()->onStateChanged) {
            rct_gst_get_configuration()->onStateChanged(old_state, new_state);
        }
    }
}

static gboolean cb_message_element(GstBus *bus, GstMessage *msg, gpointer user_data)
{
    return TRUE;
}

static gboolean cb_async_done(GstBus *bus, GstMessage *message, gpointer user_data)
{
    LOGD("Async done message received");
    return TRUE;
}

static gboolean cb_bus_watch(GstBus *bus, GstMessage *message, gpointer user_data)
{
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR:
            cb_error(bus, message, user_data);
            break;
            
        case GST_MESSAGE_EOS:
            cb_eos(bus, message, user_data);
            break;
            
        case GST_MESSAGE_STATE_CHANGED:
            cb_state_changed(bus, message, user_data);
            break;
            
        case GST_MESSAGE_ELEMENT:
            cb_message_element(bus, message, user_data);
            break;
            
        case GST_MESSAGE_ASYNC_DONE:
            cb_async_done(bus, message, user_data);
            break;
            
        default:
            LOGD("Unhandled message type: %s", GST_MESSAGE_TYPE_NAME(message));
            break;
    }
    
    return TRUE;
}

/*************
 OTHER METHODS
 ************/
GstStateChangeReturn rct_gst_set_pipeline_state(GstState state)
{
    LOGD("Setting pipeline state: %s", gst_element_state_get_name(state));
    GstStateChangeReturn validity = gst_element_set_state(pipeline, state);
    LOGD("State change return: %s", gst_element_state_change_return_get_name(validity));
    return validity;
}

void rct_gst_init(RctGstConfiguration *configuration)
{
    LOGD("Initializing GStreamer pipeline");

    // Create the elements
    pipeline = gst_pipeline_new("pipeline");
    source = gst_element_factory_make("rtspsrc", "source");
    depay = gst_element_factory_make("rtph264depay", "depay");
    parser = gst_element_factory_make("h264parse", "parser");
    decoder = gst_element_factory_make("avdec_h264", "decoder");
    conv = gst_element_factory_make("videoconvert", "conv");
    sink = gst_element_factory_make("glimagesink", "video_sink");

    if (!pipeline || !source || !depay || !parser || !decoder || !conv || !sink) {
        LOGE("Failed to create elements");
        return;
    }

    // Set the URI property on the source element
    gchar *uri = rct_gst_get_configuration()->uri;
    LOGD("Setting URI on source element: %s", uri);
    g_object_set(G_OBJECT(source), "location", uri, NULL);
    g_object_set(G_OBJECT(source), "buffer-size", 2097152, NULL);
    g_object_set(G_OBJECT(source), "latency", 0, NULL);

    // Enable low-latency mode where possible
    g_object_set(G_OBJECT(source), "do-retransmission", FALSE, NULL);
    g_object_set(G_OBJECT(parser), "disable-passthrough", TRUE, NULL);
    g_object_set(G_OBJECT(decoder), "low-latency", TRUE, NULL);
    g_object_set(G_OBJECT(sink), "sync", FALSE, NULL);
    g_object_set(G_OBJECT(sink), "max-lateness", -1, NULL);

    // Build the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, depay, parser, decoder, conv, sink, NULL);
    if (!gst_element_link_many(depay, parser, decoder, conv, sink, NULL)) {
        LOGE("Elements could not be linked");
        gst_object_unref(pipeline);
        return;
    }

    // Connect the source element's pad-added signal to the depay element
    g_signal_connect(source, "pad-added", G_CALLBACK(on_pad_added), depay);

    gchar *pipeline_description = gst_debug_bin_to_dot_data(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL);
    LOGD("Pipeline description:\n%s", pipeline_description);

    bus = gst_element_get_bus(pipeline);
    bus_watch_id = gst_bus_add_watch(bus, cb_bus_watch, NULL);
    
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)cb_create_window, pipeline, NULL);
    gst_object_unref(bus);

    if (drawable_surface != 0) {
        rct_gst_set_drawable_surface(drawable_surface);
    }

    apply_uri();
    
    if (rct_gst_get_configuration()->onInit) {
        rct_gst_get_configuration()->onInit();
    }
    LOGD("GStreamer initialization complete");

    // Log the pipeline description
    gchar *pipeline_description2 = gst_debug_bin_to_dot_data(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL);
    LOGD("Pipeline description2:\n%s", pipeline_description2);
    g_free(pipeline_description);
}


void rct_gst_run_loop()
{
    LOGD("Starting GStreamer main loop");
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    LOGD("GStreamer main loop terminated");
}

void rct_gst_terminate()
{
    LOGD("Terminating GStreamer");
    if (sink != NULL) {
        gst_object_unref(sink);
    }
    if (video_overlay != NULL) {
        gst_object_unref(video_overlay);
    }
    drawable_surface = 0;
    
    rct_gst_set_pipeline_state(GST_STATE_NULL);
    gst_object_unref(pipeline);
    
    g_source_remove(bus_watch_id);
    g_main_loop_unref(main_loop);
    
    g_free(configuration);
    
    pipeline = NULL;
    configuration = NULL;
    LOGD("GStreamer terminated");
}

gchar *rct_gst_get_info()
{
    LOGD("Entered rct_gst_get_info function");

    gchar *version = gst_version_string();
    if (version != NULL) {
        LOGI("GStreamer version from C: %s", version);
    } else {
        LOGE("Failed to get GStreamer version string");
    }

    return version;
}

void apply_uri() {
    gchar* uri = rct_gst_get_configuration()->uri;
    LOGD("Applying URI: %s", uri);
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    LOGD("Set pipeline state to NULL, return value: %s", gst_element_state_change_return_get_name(ret));
    g_object_set(source, "location", uri, NULL);
    LOGD("URI set on pipeline");
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    LOGD("Set pipeline state to PLAYING, return value: %s", gst_element_state_change_return_get_name(ret));
    if (rct_gst_get_configuration()->onUriChanged) {
        rct_gst_get_configuration()->onUriChanged(uri);
    }

    // Log the pipeline description
    gchar *pipeline_description = gst_debug_bin_to_dot_data(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL);
    LOGD("Pipeline description after setting URI:\n%s", pipeline_description);
    g_free(pipeline_description);
}

void on_pad_added(GstElement *src, GstPad *new_pad, gpointer depay) {
    GstPad *sink_pad = gst_element_get_static_pad(GST_ELEMENT(depay), "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    LOGD("Received new pad '%s' from '%s':", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    // Check the new pad's type
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (!g_str_has_prefix(new_pad_type, "application/x-rtp")) {
        LOGD("  It has type '%s' which is not application/x-rtp. Ignoring.", new_pad_type);
        if (new_pad_caps != NULL) {
            gst_caps_unref(new_pad_caps);
        }
        return;
    }

    // Attempt to link
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        LOGE("  Type is '%s' but link failed.");
    } else {
        LOGD("  Link succeeded (type '%s').");
    }

    if (new_pad_caps != NULL) {
        gst_caps_unref(new_pad_caps);
    }

    gst_object_unref(sink_pad);
}

void reset_pipeline() {
    LOGD("Resetting pipeline");

    // Set pipeline to NULL state
    gst_element_set_state(pipeline, GST_STATE_NULL);

    // Set the URI again
    apply_uri();

    // Set pipeline to PLAYING state
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    LOGD("Pipeline reset complete");
}
