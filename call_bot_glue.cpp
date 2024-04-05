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

#include <fstream>
#include <iostream>
#define CHUNKSIZE (320)
#define BPS (10) // batch per second

typedef struct WAV_HEADER
{
    /* RIFF Chunk Descriptor */
    uint8_t RIFF[4] = {'R', 'I', 'F', 'F'}; // RIFF Header Magic header
    uint32_t ChunkSize;                     // RIFF Chunk Size
    uint8_t WAVE[4] = {'W', 'A', 'V', 'E'}; // WAVE Header
    /* "fmt" sub-chunk */
    uint8_t fmt[4] = {'f', 'm', 't', ' '}; // FMT header
    uint32_t Subchunk1Size = 16;           // Size of the fmt chunk
    uint16_t AudioFormat = 1;              // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM
                                           // Mu-Law, 258=IBM A-Law, 259=ADPCM
    uint16_t NumOfChan = 1;                // Number of channels 1=Mono 2=Sterio
    uint32_t SamplesPerSec = 8000;         // Sampling Frequency in Hz
    uint32_t bytesPerSec = 8000 * 2;       // bytes per second
    uint16_t blockAlign = 2;               // 2=16-bit mono, 4=16-bit stereo
    uint16_t bitsPerSample = 16;           // Number of bits per sample
    /* "data" sub-chunk */
    uint8_t Subchunk2ID[4] = {'d', 'a', 't', 'a'}; // "data"  string
    uint32_t Subchunk2Size;                        // Sampled data length
} wav_hdr;

using smartivrphonegateway::Config;
using smartivrphonegateway::SmartIVRRequest;
using smartivrphonegateway::SmartIVRResponse;
using smartivrphonegateway::SmartIVRResponseType;
using smartivrphonegateway::Status;

typedef struct audio_info
{
    char *sessionId;
    switch_core_session_t *session;
    switch_channel_t *channel;
    SmartIVRResponse response;
} Audio_Info;

class GStreamer
{
public:
    GStreamer(
        switch_core_session_t *session, uint32_t channels, char *lang, int interim, int bps = 10) : m_session(session),
                                                                                                    m_writesDone(false),
                                                                                                    m_connected(false),
                                                                                                    m_bot_hangup(false),
                                                                                                    m_bot_transfer(false),
                                                                                                    m_bot_error(false),
                                                                                                    m_language(lang),
                                                                                                    m_interim(interim),
                                                                                                    m_audioBuffer(CHUNKSIZE, 50)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, " Create GStreamer\n");
        strncpy(m_sessionId, switch_core_session_get_uuid(session), 256);
        m_switch_channel = switch_core_session_get_channel(m_session);
        m_pickup_at = switch_micro_time_now() / 1000;
        last_write = switch_micro_time_now();
        m_max_chunks = 50 / bps;
        m_interval = m_max_chunks * 20000;
    }

    ~GStreamer()
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer::~GStreamer - deleting channel and stub: %p\n", (void *)this);
    }

    void print_request()
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_DEBUG, "GStreamer %p audio isplaying: %d, content length: %d\n", this, m_request.is_playing(), m_request.audio_content().length());
    }

    void print_response(SmartIVRResponse response)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "type: %d\n", response.type());
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "text_asr: %s\n", response.text_asr().c_str());
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "text_bot: %s\n", response.text_bot().c_str());
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "forward_sip_json: %s\n", response.forward_sip_json().c_str());
        if (response.has_status())
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "status_code: %d\n", response.status().code());
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "status_message: %s\n", response.status().message());
        }
    }

    char *build_response_json(long long hangup_at, int sip_code, const char *hangup_cause)
    {
        cJSON *jResult = cJSON_CreateObject();
        cJSON *jPickupAt = cJSON_CreateNumber(m_pickup_at);
        cJSON *jHangupAt = cJSON_CreateNumber(hangup_at);
        cJSON *jCallAt = cJSON_CreateNumber(m_call_at);
        cJSON *jConversationId = cJSON_CreateString(m_conversation_id.c_str());
        cJSON *jBotMasterUri = cJSON_CreateString(m_botmaster_uri.c_str());
        cJSON *jPhoneControllerUri = cJSON_CreateString(m_phonecontroller_uri.c_str());
        cJSON *jIsBotHangup = cJSON_CreateBool(m_bot_hangup);
        cJSON *jIsBotTransfer = cJSON_CreateBool(m_bot_transfer);
        cJSON *jHangupCause = cJSON_CreateString(hangup_cause);
        cJSON *jRecordPath = cJSON_CreateString(m_record_path.c_str());
        cJSON *jRecordName = cJSON_CreateString(m_record_name.c_str());
        cJSON *jSessionId = cJSON_CreateString(m_sessionId);
        cJSON *jSipCode = cJSON_CreateNumber(sip_code);
        int status = 101;
        if (m_bot_transfer == true)
        {
            status = 102;
        }
        else if (m_bot_error == true)
        {
            status = 105;
        }
        else if (m_bot_hangup == true)
        {
            status = 100;
        }
        cJSON *jStatus = cJSON_CreateNumber(status);
        cJSON_AddItemToObject(jResult, "pickup_at", jPickupAt);
        cJSON_AddItemToObject(jResult, "call_at", jCallAt);
        cJSON_AddItemToObject(jResult, "hangup_at", jHangupAt);
        cJSON_AddItemToObject(jResult, "conversation_id", jConversationId);
        cJSON_AddItemToObject(jResult, "bot_master_uri", jBotMasterUri);
        cJSON_AddItemToObject(jResult, "phone_controller_uri", jPhoneControllerUri);
        cJSON_AddItemToObject(jResult, "is_bot_hangup", jIsBotHangup);
        cJSON_AddItemToObject(jResult, "is_bot_transfer", jIsBotTransfer);
        cJSON_AddItemToObject(jResult, "sip_code", jSipCode);
        cJSON_AddItemToObject(jResult, "hangup_cause", jHangupCause);
        cJSON_AddItemToObject(jResult, "record_name", jRecordName);
        cJSON_AddItemToObject(jResult, "audio_url", jRecordPath);
        cJSON_AddItemToObject(jResult, "session_id", jSessionId);
        cJSON_AddItemToObject(jResult, "status", jStatus);
        char *json = cJSON_PrintUnformatted(jResult);
        cJSON_Delete(jResult);
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "Gstreamer hangup response json: %s.\n", json);
        return json;
    }

    void createInitMessage()
    {
        m_botmaster_uri = std::string(switch_channel_get_variable(m_switch_channel, "CALLBOT_MASTER_URI"));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p creating grpc channel to %s\n", this, m_botmaster_uri.c_str());
        m_conversation_id = std::string(switch_channel_get_variable(m_switch_channel, "CONVERSATION_ID"));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p start master with session id %s\n", this, m_conversation_id.c_str());
        m_phonecontroller_uri = std::string(switch_channel_get_variable(m_switch_channel, "CALLBOT_CONTROLLER_URI"));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p start master with phone controller uri %s\n", this, m_phonecontroller_uri.c_str());
        m_record_name = std::string(switch_channel_get_variable(m_switch_channel, "record_name"));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p start with record name %s\n", this, m_record_name.c_str());
        m_record_path = std::string(switch_channel_get_variable(m_switch_channel, "record_path"));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p start with record uri %s\n", this, m_record_path.c_str());
        m_call_at = atol(switch_channel_get_variable(m_switch_channel, "CALL_AT"));
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p start call at: %d\n", this, m_call_at);

        std::shared_ptr<grpc::Channel> grpcChannel = grpc::CreateChannel(m_botmaster_uri.c_str(), grpc::InsecureChannelCredentials());
        if (!grpcChannel)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_ERROR, "GStreamer %p failed creating grpc channel to %s\n", this, m_botmaster_uri.c_str());
            switch_channel_hangup(m_switch_channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }

        m_stub = std::move(smartivrphonegateway::SmartIVRPhonegateway::NewStub(grpcChannel));

        if (!m_stub)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_ERROR, "GStreamer %p failed creating grpc channel to %s\n", this, m_botmaster_uri.c_str());
            switch_channel_hangup(m_switch_channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }

        /* set configuration parameters which are carried in the RecognitionInitMessage */
        auto streaming_config = m_request.mutable_config();
        streaming_config->set_conversation_id(m_conversation_id);
        m_request.set_is_playing(false);
        m_request.set_key_press("");
        m_request.set_audio_content("");
    }

    void connect()
    {
        assert(!m_connected);
        // Begin a stream.
        createInitMessage();
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p creating streamer\n", this);
        m_streamer = m_stub->CallToBot(&m_context);
        if (!m_streamer)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_ERROR, "GStreamer %p failed creating grpc channel to %s\n", this, m_botmaster_uri.c_str());
            switch_channel_hangup(m_switch_channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }
        m_connected = true;

        // read thread is waiting on this
        m_promise.set_value();

        // Write the first request, containing the config only.
        print_request();
        m_streamer->Write(m_request);
    }

    bool write(void *data, uint32_t datalen)
    {
        if (isVoiceMail())
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "GStreamer %p:  voicemail detected, hanging up ...\n", this);
            switch_channel_hangup(m_switch_channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }

        if (!m_connected || m_bot_transfer)
        {
            return false;
        }
        add_dtmf_to_request();
        // if (datalen % CHUNKSIZE == 0)
        {
            m_audioBuffer.add(data, datalen);
        }

        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "Data len: %lld, num_item: %d\n", datalen, m_audioBuffer.getNumItems());
        if (m_audioBuffer.getNumItems() == m_max_chunks || switch_micro_time_now() - last_write > m_interval)
        {
            m_request.clear_audio_content();
            m_request.set_key_press("");
            m_request.set_audio_content(m_audioBuffer.getData(), CHUNKSIZE * m_max_chunks * (datalen / CHUNKSIZE));
            m_request.set_is_playing(isPlaying());
            m_request.set_timestamp(switch_micro_time_now() / 1000);
            // print_request();
            if (!m_streamer->Write(m_request))
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_ERROR, "GStreamer %p stream write request failed!\n", this);
                return false;
            }
            last_write = switch_micro_time_now();
            m_audioBuffer.clearData();
        }

        return true;
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

    void tryCancel() // cancel read
    {
        if (m_connected)
        {
            m_context.TryCancel();
        }
    }

    grpc::Status finish()
    {
        return m_streamer->Finish();
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
        if (!m_connected)
        {
            m_promise.set_value();
        }
    }

    bool isConnected()
    {
        return m_connected;
    }

    bool isPlaying()
    {
        const char *var = switch_channel_get_variable(m_switch_channel, "IS_PLAYING");
        if (var && (strcmp(var, "true") == 0))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool isVoiceMail()
    {
        const char *var = switch_channel_get_variable(m_switch_channel, "IS_VOICE_MAIL");
        if (var && (strcmp(var, "true") == 0))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void add_dtmf_to_request()
    {
        if (switch_channel_has_dtmf(m_switch_channel))
        {
            switch_dtmf_t dtmf = {0};
            switch_channel_dequeue_dtmf(m_switch_channel, &dtmf);
            std::string dtmf_string(1, dtmf.digit);
            m_request.set_is_playing(isPlaying());
            m_request.set_key_press(dtmf_string);
            m_request.clear_audio_content();
            if (!m_streamer->Write(m_request))
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_ERROR, "GStreamer %p stream write request failed!\n", this);
            }
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "CALL BOT received dtmf: %s.\n", m_request.key_press().c_str());
        }
        else
        {
            m_request.set_key_press("");
        }
    }

    void set_bot_hangup()
    {
        switch_channel_set_variable(m_switch_channel, "IS_BOT_HANGUP", "true");
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "Gstreamer run set_bot_hangup.\n");
        m_bot_hangup = true;
    }

    bool isBotTransfered()
    {
        return m_bot_transfer;
    }

    void set_bot_transfer()
    {
        switch_channel_set_variable(m_switch_channel, "IS_BOT_TRANSFERED", "true");
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "Gstreamer run set_bot_transfers.\n");
        m_bot_transfer = true;
    }

    void set_bot_error()
    {
        switch_channel_set_variable(m_switch_channel, "IS_BOT_ERROR", "true");
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(m_session), SWITCH_LOG_INFO, "Gstreamer run set_bot_error.\n");
        m_bot_error = true;
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
    switch_channel_t *m_switch_channel;

    // master data
    std::string m_botmaster_uri;
    std::string m_phonecontroller_uri;
    std::string m_conversation_id;
    std::string m_record_name;
    std::string m_record_path;
    long long m_call_at = 0;
    long long m_pickup_at = 0;
    bool m_bot_hangup;
    bool m_bot_transfer;
    bool m_bot_error;
    long long last_write;
    int m_max_chunks;
    int m_interval;
};

static std::vector<uint8_t> parse_byte_array(std::string str)
{
    std::vector<uint8_t> vec(str.begin(), str.end());
    return vec;
}

static switch_status_t play_audio(char *session_id, std::vector<uint8_t> audio_data, switch_core_session_t *session, switch_channel_t *channel)
{
    long long now = switch_micro_time_now();
    // switch_event_t *event;
    switch_status_t status = SWITCH_STATUS_FALSE;
    auto fsize = audio_data.size();
    char *fileName = (char *)malloc(62 * sizeof(char));
    sprintf(fileName, "/tmp/%s-%lld.wav", session_id, now);
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "play_audio: write %d frame to file: %s!\n", fsize, fileName);
    // write byte to pcm file
    wav_hdr wav;
    wav.ChunkSize = fsize + sizeof(wav_hdr) - 8;
    wav.Subchunk2Size = fsize;
    std::ofstream out(fileName, std::ios::binary);
    if (!out)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "play_audio: error create file!\n");
        return status;
    }
    if (!out.write(reinterpret_cast<const char *>(&wav), sizeof(wav)))
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "play_audio: Error writing WAV file header!\n");
        return status;
    }
    if (!out.write(reinterpret_cast<char *>(&audio_data[0]), fsize * sizeof(uint8_t)))
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "play_audio: Error writing audio data to WAV file!\n");
        return status;
    }
    out.close();
    switch_channel_set_variable(channel, "CUR_FILE", fileName);
    switch_channel_set_variable(channel, "IS_PLAYING", "true");
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "play_audio: session %s set playing to true!\n", session_id);
    status = switch_ivr_broadcast(session_id, fileName, SMF_ECHO_ALEG | SMF_HOLD_BLEG);
    if (status != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
                          "Couldn't play file '%s'\n", fileName);
        switch_channel_set_variable(channel, "IS_PLAYING", "false");
    }
    fileName = NULL;
    return status;
}

static void *SWITCH_THREAD_FUNC play_audio_thread(switch_thread_t *thread, void *obj)
{
    struct audio_info *ai = (struct audio_info *)obj;
    if (play_audio(ai->sessionId, parse_byte_array(ai->response.audio_content()), ai->session, ai->channel) == SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play_audio_thread: play file done!\n");
    }
    else
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "play_audio_thread: cannot play file in event handler!\n");
    }
    return nullptr;
}

static void *SWITCH_THREAD_FUNC grpc_read_thread(switch_thread_t *thread, void *obj)
{
    struct cap_cb *cb = (struct cap_cb *)obj;
    GStreamer *streamer = (GStreamer *)cb->streamer;
    char *sessionUUID = cb->sessionId;
    switch_event_t *event;
    switch_channel_t *channel = NULL;
    switch_core_session_t *session = NULL;
    switch_status_t status;

    bool connected = streamer->waitForConnect();
    if (!connected)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "callbot grpc read thread exiting since we didnt connect\n");
        return nullptr;
    }

    session = switch_core_session_locate(sessionUUID);
    if (!session)
    {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "grpc_read_thread: session %s is gone!\n", cb->sessionId);
        sessionUUID = NULL;
        event = NULL;
        streamer->finish();
        return nullptr;
    }

    channel = switch_core_session_get_channel(session);

    // Read responses
    SmartIVRResponse response;
    SmartIVRResponseType previousType = SmartIVRResponseType::CALL_END;
    while (streamer->read(&response) && !streamer->isVoiceMail() && !streamer->isBotTransfered())
    { // Returns false when no more to read.
        streamer->print_response(response);
        SmartIVRResponseType responseType = response.type();
        if (previousType == SmartIVRResponseType::CALL_WAIT)
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread: before type is CALL_WAIT unhold call now!\n");
            switch_channel_stop_broadcast(channel);
            switch_channel_wait_for_flag(channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
        }

        switch (responseType)
        {
        case SmartIVRResponseType::RECOGNIZE:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got type RECOGNIZE.\n");
            break;

        case SmartIVRResponseType::RESULT_ASR:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got type RESULT_ASR.\n");
            break;

        case SmartIVRResponseType::RESULT_TTS:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got type RESULT_TTS.\n");
            if (streamer->isPlaying())
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread: current playing audio, stop it first\n");
                switch_channel_stop_broadcast(channel);
                switch_channel_wait_for_flag(channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread: stop done\n");
            }
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread: playing audio ........\n");
            if (play_audio(sessionUUID, parse_byte_array(response.audio_content()), session, channel) == SWITCH_STATUS_SUCCESS)
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread: play file audio in current thread!\n");
            }
            else
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "grpc_read_thread: cannot play file in current thread!\n");
            }
            break;

        case SmartIVRResponseType::CALL_WAIT:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got type CALL_WAIT.\n");

            if (streamer->isPlaying())
            {
                // wait to audio play done
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "grpc_read_thread: wait to audio play done!\n");
                switch_channel_wait_for_flag(channel, CF_BROADCAST, SWITCH_FALSE, 60000, NULL);
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "grpc_read_thread: play audio done!\n");
            }
            switch_channel_stop_broadcast(channel);
            if (switch_ivr_broadcast(sessionUUID, switch_channel_get_hold_music(channel), SMF_ECHO_ALEG | SMF_HOLD_BLEG | SMF_LOOP) == SWITCH_STATUS_SUCCESS)
            {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "event_process_response_handler: hold call success!\n");
            }
            else
            {
                switch_channel_clear_flag(channel, CF_HOLD);
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "event_process_response_handler: hold call failed!\n");
            }
            break;
        case SmartIVRResponseType::CALL_FORWARD:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got type CALL_FORWARD.\n");
            if (streamer->isPlaying())
            {
                // wait to audio play done
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "grpc_read_thread: wait to audio play done!\n");
                switch_channel_wait_for_flag(channel, CF_BROADCAST, SWITCH_FALSE, 60000, NULL);
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "grpc_read_thread: play audio done!\n");
            }

            streamer->set_bot_transfer();
            if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, EVENT_BOT_TRANSFER) == SWITCH_STATUS_SUCCESS)
            {
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, HEADER_SESSION_ID, sessionUUID);
                switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, HEADER_TRANSFER_JSON, response.forward_sip_json().c_str());
                switch_event_fire(&event);
            }
            break;
        case SmartIVRResponseType::CALL_END:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got type CALL_END.\n");
            switch_channel_stop_broadcast(channel);
            if (!streamer->isBotTransfered())
            {
                streamer->set_bot_hangup();
                switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
            }
        default:
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "grpc_read_thread Got unknown type.\n");
            break;
        }

        previousType = responseType;
    }
    grpc::Status finish_status = streamer->finish();
    if (finish_status.ok())
    {
        // The RPC completed successfully
        if (session)
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Grpc completed ok!\n");
        else
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Grpc completed ok!\n", sessionUUID);
    }
    else
    {
        if (session)
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Grpc completed with status code: %d!\n", finish_status.error_code());
        else
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Grpc completed with status code: %d!\n", sessionUUID, finish_status.error_code());

        if (finish_status.error_code() == grpc::StatusCode::UNAVAILABLE)
        {
            streamer->set_bot_error();
            // The client connection was closed
            if (session)
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Grpc completed error! The client connection was closed!\n");
            else
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Grpc completed error! The client connection was closed!\n", sessionUUID);
        }
    }

    if (&response != nullptr && response.has_status() && response.status().code() == 1)
    {
        streamer->set_bot_error();
    }

    if (!streamer->isBotTransfered())
    {
        switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
    }

    switch_core_session_rwunlock(session);

    sessionUUID = NULL;
    event = NULL;
    channel = NULL;
    session = NULL;
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
        switch_memory_pool_t *pool = switch_core_session_get_pool(session);
        try
        {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  allocating streamer\n");
            const char *var = switch_channel_get_variable(channel, "batch_per_seconds");
            int bps = 10;
            if (var && atol(var) > 0)
            {
                bps = atol(var);
            }
            streamer = new GStreamer(session, channels, lang, interim, bps);
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
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  no vad so connecting to callbot immediately\n");
            streamer->connect();
        }

        switch_threadattr_t *thd_attr = NULL;
        switch_threadattr_create(&thd_attr, pool);
        switch_threadattr_stacksize_set(thd_attr, SWITCH_MAX_STACKS);

        // create the read thread
        switch_thread_create(&cb->thread, thd_attr, grpc_read_thread, cb, pool);

        *ppUserData = cb;
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_init:  initialized! \n");
        return SWITCH_STATUS_SUCCESS;
    }

    switch_status_t call_bot_session_cleanup(switch_core_session_t *session, int channelIsClosing, switch_media_bug_t *bug)
    {
        switch_event_t *event;
        switch_status_t status;
        const char *curFile;

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
                streamer->tryCancel();
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_cleanup: GStreamer (%p) waiting for read thread to complete\n", (void *)streamer);
                switch_thread_join(&status, cb->thread);
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "call_bot_session_cleanup:  GStreamer (%p) read thread completed\n", (void *)streamer);
                delete streamer;
                cb->streamer = NULL;
                cb->thread = NULL;
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

            curFile = switch_channel_get_variable(channel, "CUR_FILE");
            if (curFile)
            {
                remove(curFile);
            };

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
