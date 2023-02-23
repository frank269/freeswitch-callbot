#include <cstdlib>
#include <algorithm>
#include <future>
#include <switch.h>
#include <switch_json.h>
#include <grpc++/grpc++.h>
#include "mod_call_bot.h"
#include "simple_buffer.h"
#include "smartivrphonegateway.pb.h"
#include "smartivrphonegateway.grpc.pb.h"
#define CHUNKSIZE (320)

using smartivrphonegateway::Config;
using smartivrphonegateway::SmartIVRRequest;
using smartivrphonegateway::SmartIVRResponse;
using smartivrphonegateway::SmartIVRResponseType;
using smartivrphonegateway::Status;

namespace
{
    int case_insensitive_match(std::string s1, std::string s2)
    {
        std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
        std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
        if (s1.compare(s2) == 0)
            return 1; // The strings are same
        return 0;     // not matched
    }
}

class GStreamer
{
public:
    GStreamer(
        switch_core_session_t *session, uint32_t channels, char *lang, int interim) : m_session(session),
                                                                                      m_writesDone(false),
                                                                                      m_connected(false),
                                                                                      m_language(lang),
                                                                                      m_interim(interim),
                                                                                      m_audioBuffer(CHUNKSIZE, 15)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " Create GStreamer\n");
        const char *var;
        char sessionId[256];
        switch_channel_t *channel = switch_core_session_get_channel(session);
        strncpy(m_sessionId, switch_core_session_get_uuid(session), 256);
    }

    ~GStreamer()
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer::~GStreamer - deleting channel and stub: %p\n", (void *)this);
    }

    void print_request()
    {
        cJSON *jResult = cJSON_CreateObject();
        cJSON *jIsPlaying = cJSON_CreateBool(m_request.is_playing());
        cJSON *jKeyPress = cJSON_CreateString(m_request.key_press().c_str());
        cJSON *jConversationId = cJSON_CreateString(m_request.mutable_config()->conversation_id().c_str());
        cJSON *jAudioContent = cJSON_CreateString(m_request.audio_content().c_str());
        cJSON_AddItemToObject(jResult, "is_playing", jIsPlaying);
        cJSON_AddItemToObject(jResult, "key_press", jKeyPress);
        cJSON_AddItemToObject(jResult, "conversation_id", jConversationId);
        cJSON_AddItemToObject(jResult, "audio_content", jAudioContent);

        char *json = cJSON_PrintUnformatted(jResult);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p sending message: %s\n", this, json);
        free(json);
        cJSON_Delete(jResult);
    }

    void print_response(SmartIVRResponse response)
    {
        cJSON *jResult = cJSON_CreateObject();
        cJSON *jType = cJSON_CreateNumber(response.type());
        cJSON *jTextAsr = cJSON_CreateString(response.text_asr().c_str());
        cJSON *jTextBot = cJSON_CreateString(response.text_bot().c_str());
        cJSON *jForwardSipJson = cJSON_CreateString(response.forward_sip_json().c_str());
        cJSON *jStatusCode = cJSON_CreateNumber(response.status().code());
        cJSON *jStatusMessage = cJSON_CreateString(response.status().message().c_str());
        cJSON *jAudioContent = cJSON_CreateString(response.audio_content().c_str());
        cJSON_AddItemToObject(jResult, "type", jType);
        cJSON_AddItemToObject(jResult, "text_asr", jTextAsr);
        cJSON_AddItemToObject(jResult, "text_bot", jTextBot);
        cJSON_AddItemToObject(jResult, "forward_sip_json", jForwardSipJson);
        cJSON_AddItemToObject(jResult, "status_code", jStatusCode);
        cJSON_AddItemToObject(jResult, "status_message", jStatusMessage);
        cJSON_AddItemToObject(jResult, "audio_content", jAudioContent);

        char *json = cJSON_PrintUnformatted(jResult);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p received message: %s\n", this, json);
        free(json);
        cJSON_Delete(jResult);
    }

    void createInitMessage()
    {
        switch_channel_t *channel = switch_core_session_get_channel(m_session);

        const char *var = switch_channel_get_variable(channel, "CALLBOT_MASTER_URI");
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p creating grpc channel to %s\n", this, var);
        const char *var_session_id = switch_channel_get_variable(channel, "SESSION_ID");
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p start master with session id %s\n", this, var_session_id);

        std::shared_ptr<grpc::Channel> grpcChannel = grpc::CreateChannel(var, grpc::InsecureChannelCredentials());
        if (!grpcChannel)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "GStreamer %p failed creating grpc channel to %s\n", this, var);
            throw std::runtime_error(std::string("Error creating grpc channel to ") + var);
        }

        m_stub = std::move(smartivrphonegateway::SmartIVRPhonegateway::NewStub(grpcChannel));

        /* set configuration parameters which are carried in the RecognitionInitMessage */
        auto streaming_config = m_request.mutable_config();
        std::string conversation_id(var_session_id);
        streaming_config->set_conversation_id(conversation_id);
        m_request.set_is_playing(false);
        m_request.set_key_press("");
        m_request.set_audio_content("");
    }

    void connect()
    {
        assert(!m_connected);
        // Begin a stream.
        createInitMessage();
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p creating streamer\n", this);
        m_streamer = m_stub->CallToBot(&m_context);
        m_connected = true;

        // read thread is waiting on this
        m_promise.set_value();

        // Write the first request, containing the config only.
        // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p sending initial message with conversationId: %s\n", this, m_request.mutable_config()->conversation_id().c_str());
        print_request();
        m_streamer->Write(m_request);
        // m_request.clear_config();

        // send any buffered audio
        int nFrames = m_audioBuffer.getNumItems();
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p got stream ready, %d buffered frames\n", this, nFrames);
        if (nFrames)
        {
            char *p;
            do
            {
                p = m_audioBuffer.getNextChunk();
                if (p)
                {
                    write(p, CHUNKSIZE);
                }
            } while (p);
        }
    }

    bool write(void *data, uint32_t datalen)
    {
        if (!m_connected)
        {
            if (datalen % CHUNKSIZE == 0)
            {
                m_audioBuffer.add(data, datalen);
            }
            return true;
        }
        m_request.clear_audio_content();
        m_request.set_audio_content(data, datalen);
        // print_request();
        bool ok = m_streamer->Write(m_request);
        return ok;
    }

    uint32_t nextMessageSize(void)
    {
        uint32_t size = 0;
        m_streamer->NextMessageSize(&size);
        return size;
    }

    bool read(SmartIVRResponse *response)
    {
        return m_streamer->Read(response);
    }

    grpc::Status finish()
    {
        return m_streamer->Finish();
    }

    void startTimers()
    {
        // nr_asr::StreamingRecognizeRequest request;
        // auto msg = request.mutable_control_message()->mutable_start_timers_message();
        // m_streamer->Write(request);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GStreamer %p sent start timers control message\n", this);
    }

    void writesDone()
    {
        // grpc crashes if we call this twice on a stream
        if (!m_connected)
        {
            cancelConnect();
        }
        else if (!m_writesDone)
        {
            m_streamer->WritesDone();
            m_writesDone = true;
        }
    }

    bool waitForConnect()
    {
        std::shared_future<void> sf(m_promise.get_future());
        sf.wait();
        return m_connected;
    }

    void cancelConnect()
    {
        assert(!m_connected);
        m_promise.set_value();
    }

    bool isConnected()
    {
        return m_connected;
    }

private:
    switch_core_session_t *m_session;
    grpc::ClientContext m_context;
    std::shared_ptr<grpc::Channel> m_channel;
    std::unique_ptr<smartivrphonegateway::SmartIVRPhonegateway::Stub> m_stub;
    SmartIVRRequest m_request;
    std::unique_ptr<grpc::ClientReaderWriterInterface<SmartIVRRequest, SmartIVRResponse>> m_streamer;
    bool m_writesDone;
    bool m_connected;
    bool m_interim;
    std::string m_language;
    std::promise<void> m_promise;
    SimpleBuffer m_audioBuffer;
    char m_sessionId[256];
};

static void *SWITCH_THREAD_FUNC grpc_read_thread(switch_thread_t *thread, void *obj)
{
    switch_frame_t audio_frame = {0};

    struct cap_cb *cb = (struct cap_cb *)obj;
    GStreamer *streamer = (GStreamer *)cb->streamer;

    bool connected = streamer->waitForConnect();
    if (!connected)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "callbot grpc read thread exiting since we didnt connect\n");
        return nullptr;
    }

    switch_core_session_t *session = switch_core_session_locate(cb->sessionId);
    switch_channel_t *channel = switch_core_session_get_channel(session);
    if (!session)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "grpc_read_thread: session %s is gone!\n", cb->sessionId);
    }

    status = switch_core_session_read_frame(session, &audio_frame, SWITCH_IO_FLAG_NONE, 0);
    if (!SWITCH_READ_ACCEPTABLE(status))
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "grpc_read_thread: session %s is not readable!\n", cb->sessionId);
        break;
    }

    // Read responses
    SmartIVRResponse response;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "grpc_read_thread running .... \n");
    while (streamer->read(&response) && switch_channel_ready(channel))
    { // Returns false when no more to read.
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "grpc_read_thread got response .... \n");
        streamer->print_response(response);

        std::string audio_content = response.audio_content();
        audio_frame.data = static_cast<void *>(&audio_content);
        audio_frame.datalen = audio_content.length();
        audio_frame.buflen = audio_content.length();
        switch_core_session_write_frame(session, &audio_frame, SWITCH_IO_FLAG_NONE, 0);

        //     for (int r = 0; r < response.results_size(); ++r)
        //     {
        //         const auto &result = response.results(r);
        //         bool is_final = result.is_final();
        //         int num_alternatives = result.alternatives_size();
        //         int channel_tag = result.channel_tag();
        //         float stability = result.stability();

        //         cJSON *jResult = cJSON_CreateObject();
        //         cJSON *jIsFinal = cJSON_CreateBool(is_final);
        //         cJSON *jAlternatives = cJSON_CreateArray();
        //         cJSON *jAudioProcessed = cJSON_CreateNumber(result.audio_processed());
        //         cJSON *jChannelTag = cJSON_CreateNumber(channel_tag);
        //         cJSON *jStability = cJSON_CreateNumber(stability);
        //         cJSON_AddItemToObject(jResult, "alternatives", jAlternatives);
        //         cJSON_AddItemToObject(jResult, "is_final", jIsFinal);
        //         cJSON_AddItemToObject(jResult, "audio_processed", jAudioProcessed);
        //         cJSON_AddItemToObject(jResult, "stability", jStability);

        //         for (int a = 0; a < num_alternatives; ++a)
        //         {
        //             cJSON *jAlt = cJSON_CreateObject();
        //             cJSON *jTranscript = cJSON_CreateString(result.alternatives(a).transcript().c_str());
        //             cJSON_AddItemToObject(jAlt, "transcript", jTranscript);

        //             if (is_final)
        //             {
        //                 auto confidence = result.alternatives(a).confidence();
        //                 cJSON_AddNumberToObject(jAlt, "confidence", confidence);
        //                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "confidence %.2f\n", confidence);

        //                 int words = result.alternatives(a).words_size();
        //                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "got %d words\n", words);
        //                 if (words > 0)
        //                 {
        //                     cJSON *jWords = cJSON_CreateArray();
        //                     for (int w = 0; w < words; w++)
        //                     {
        //                         cJSON *jWordInfo = cJSON_CreateObject();
        //                         cJSON_AddItemToArray(jWords, jWordInfo);
        //                         auto &wordInfo = result.alternatives(a).words(w);
        //                         cJSON_AddStringToObject(jWordInfo, "word", wordInfo.word().c_str());
        //                         cJSON_AddNumberToObject(jWordInfo, "start_time", wordInfo.start_time());
        //                         cJSON_AddNumberToObject(jWordInfo, "end_time", wordInfo.end_time());
        //                         cJSON_AddNumberToObject(jWordInfo, "confidence", wordInfo.confidence());
        //                         cJSON_AddNumberToObject(jWordInfo, "speaker_tag", wordInfo.speaker_tag());
        //                     }
        //                     cJSON_AddItemToObject(jAlt, "words", jWords);
        //                 }
        //             }
        //             cJSON_AddItemToArray(jAlternatives, jAlt);
        //         }
        //         char *json = cJSON_PrintUnformatted(jResult);
        //         cb->responseHandler(session, (const char *)json, cb->bugname, NULL);
        //         free(json);

        //         cJSON_Delete(jResult);
        //     }
        switch_core_session_rwunlock(session);
    }
    return nullptr;
}

extern "C"
{

    switch_status_t call_bot_init()
    {
        return SWITCH_STATUS_SUCCESS;
    }

    switch_status_t call_bot_cleanup()
    {
        return SWITCH_STATUS_SUCCESS;
    }
    switch_status_t call_bot_session_init(switch_core_session_t *session, responseHandler_t responseHandler,
                                          uint32_t samples_per_second, uint32_t channels, char *lang, int interim, char *bugname, void **ppUserData)
    {

        switch_channel_t *channel = switch_core_session_get_channel(session);
        auto read_codec = switch_core_session_get_read_codec(session);
        uint32_t sampleRate = read_codec->implementation->actual_samples_per_second;
        struct cap_cb *cb;
        int err;

        cb = (struct cap_cb *)switch_core_session_alloc(session, sizeof(*cb));
        strncpy(cb->sessionId, switch_core_session_get_uuid(session), MAX_SESSION_ID);
        strncpy(cb->bugname, bugname, MAX_BUG_LEN);
        cb->end_of_utterance = 0;

        switch_mutex_init(&cb->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
        if (sampleRate != 8000)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  initializing resampler\n");
            cb->resampler = speex_resampler_init(channels, sampleRate, 8000, SWITCH_RESAMPLE_QUALITY, &err);
            if (0 != err)
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s: Error initializing resampler: %s.\n",
                                  switch_channel_get_name(channel), speex_resampler_strerror(err));
                return SWITCH_STATUS_FALSE;
            }
        }
        else
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s: no resampling needed for this call\n", switch_channel_get_name(channel));
        }
        cb->responseHandler = responseHandler;

        // allocate vad if we are delaying connecting to the recognizer until we detect speech
        if (switch_channel_var_true(channel, "START_RECOGNIZING_ON_VAD"))
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  initializing vad\n");
            cb->vad = switch_vad_init(sampleRate, channels);
            if (cb->vad)
            {
                const char *var;
                int mode = 2;
                int silence_ms = 150;
                int voice_ms = 250;
                int debug = 0;

                if (var = switch_channel_get_variable(channel, "RECOGNIZER_VAD_MODE"))
                {
                    mode = atoi(var);
                }
                if (var = switch_channel_get_variable(channel, "RECOGNIZER_VAD_SILENCE_MS"))
                {
                    silence_ms = atoi(var);
                }
                if (var = switch_channel_get_variable(channel, "RECOGNIZER_VAD_VOICE_MS"))
                {
                    voice_ms = atoi(var);
                }
                switch_vad_set_mode(cb->vad, mode);
                switch_vad_set_param(cb->vad, "silence_ms", silence_ms);
                switch_vad_set_param(cb->vad, "voice_ms", voice_ms);
                switch_vad_set_param(cb->vad, "debug", debug);
            }
        }

        GStreamer *streamer = NULL;
        try
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  allocating streamer\n");
            streamer = new GStreamer(session, channels, lang, interim);
            cb->streamer = streamer;
        }
        catch (std::exception &e)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s: Error initializing gstreamer: %s.\n",
                              switch_channel_get_name(channel), e.what());
            return SWITCH_STATUS_FALSE;
        }

        if (!cb->vad)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  no vad so connecting to nvidia immediately\n");
            streamer->connect();
        }

        // create the read thread
        switch_threadattr_t *thd_attr = NULL;
        switch_memory_pool_t *pool = switch_core_session_get_pool(session);

        switch_threadattr_create(&thd_attr, pool);
        switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
        switch_thread_create(&cb->thread, thd_attr, grpc_read_thread, cb, pool);

        *ppUserData = cb;
        return SWITCH_STATUS_SUCCESS;
    }

    switch_status_t call_bot_session_cleanup(switch_core_session_t *session, int channelIsClosing, switch_media_bug_t *bug)
    {
        switch_channel_t *channel = switch_core_session_get_channel(session);

        if (bug)
        {
            struct cap_cb *cb = (struct cap_cb *)switch_core_media_bug_get_user_data(bug);
            switch_mutex_lock(cb->mutex);

            if (!switch_channel_get_private(channel, cb->bugname))
            {
                // race condition
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s Bug is not attached (race).\n", switch_channel_get_name(channel));
                switch_mutex_unlock(cb->mutex);
                return SWITCH_STATUS_FALSE;
            }
            switch_channel_set_private(channel, cb->bugname, NULL);

            // close connection and get final responses
            GStreamer *streamer = (GStreamer *)cb->streamer;

            if (streamer)
            {
                streamer->writesDone();

                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_cleanup: GStreamer (%p) waiting for read thread to complete\n", (void *)streamer);
                switch_status_t st;
                switch_thread_join(&st, cb->thread);
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_cleanup:  GStreamer (%p) read thread completed\n", (void *)streamer);

                delete streamer;
                cb->streamer = NULL;
            }

            if (cb->resampler)
            {
                speex_resampler_destroy(cb->resampler);
            }
            if (cb->vad)
            {
                switch_vad_destroy(&cb->vad);
                cb->vad = nullptr;
            }
            if (!channelIsClosing)
            {
                switch_core_media_bug_remove(session, &bug);
            }

            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_cleanup: Closed stream\n");

            switch_mutex_unlock(cb->mutex);

            return SWITCH_STATUS_SUCCESS;
        }

        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s Bug is not attached.\n", switch_channel_get_name(channel));
        return SWITCH_STATUS_FALSE;
    }

    switch_bool_t call_bot_frame(switch_media_bug_t *bug, void *user_data)
    {
        switch_core_session_t *session = switch_core_media_bug_get_session(bug);
        struct cap_cb *cb = (struct cap_cb *)user_data;
        if (cb->streamer && !cb->end_of_utterance)
        {
            GStreamer *streamer = (GStreamer *)cb->streamer;
            uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
            switch_frame_t frame = {};
            frame.data = data;
            frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
            if (switch_mutex_trylock(cb->mutex) == SWITCH_STATUS_SUCCESS)
            {
                while (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && !switch_test_flag((&frame), SFF_CNG))
                {
                    if (frame.datalen)
                    {
                        if (cb->vad && !streamer->isConnected())
                        {
                            switch_vad_state_t state = switch_vad_process(cb->vad, (int16_t *)frame.data, frame.samples);
                            if (state == SWITCH_VAD_STATE_START_TALKING)
                            {
                                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "detected speech, connect to callbot master now\n");
                                streamer->connect();
                                cb->responseHandler(session, "vad_detected", cb->bugname, NULL);
                            }
                        }

                        if (cb->resampler)
                        {
                            spx_int16_t out[SWITCH_RECOMMENDED_BUFFER_SIZE];
                            spx_uint32_t out_len = SWITCH_RECOMMENDED_BUFFER_SIZE;
                            spx_uint32_t in_len = frame.samples;
                            size_t written;

                            speex_resampler_process_interleaved_int(cb->resampler,
                                                                    (const spx_int16_t *)frame.data,
                                                                    (spx_uint32_t *)&in_len,
                                                                    &out[0],
                                                                    &out_len);
                            streamer->write(&out[0], sizeof(spx_int16_t) * out_len);
                        }
                        else
                        {
                            streamer->write(frame.data, sizeof(spx_int16_t) * frame.samples);
                        }
                    }
                }
                switch_mutex_unlock(cb->mutex);
            }
        }
        return SWITCH_TRUE;
    }
}
