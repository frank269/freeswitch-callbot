#ifndef __MOD_CALL_BOT_H__
#define __MOD_CALL_BOT_H__

#include <switch.h>
#include <unistd.h>
#include <speex/speex_resampler.h>

#include <switch_vad.h>
#include <switch_json.h>

#define MAX_SESSION_ID (256)
#define MAX_BUG_LEN (64)
#define MY_BUG_NAME "call_bot"
#define EVENT_PROCESS_RESPONSE "mod_call_bot::process_response"
#define EVENT_STOP_AUDIO "mod_call_bot::stop_audio"

#define HEADER_SESSION_ID "mod_call_bot::session_id"
#define HEADER_RESPONSE_TYPE "mod_call_bot::response_type"
#define HEADER_AUDIO_PATH "mod_call_bot::audio_path"
#define HEADER_TRANSFER_SIP "mod_call_bot::transfer_sip"

#define ACTION_RECOGNIZE "RECOGNIZE"
#define ACTION_RESULT_ASR "RESULT_ASR"
#define ACTION_RESULT_TTS "RESULT_TTS"
#define ACTION_CALL_WAIT "CALL_WAIT"
#define ACTION_CALL_FORWARD "CALL_FORWARD"
#define ACTION_CALL_END "CALL_END"

typedef void (*responseHandler_t)(switch_core_session_t *session,
                                  const char *json, const char *bugname,
                                  const char *details);

struct cap_cb
{
    switch_mutex_t *mutex;
    char bugname[MAX_BUG_LEN + 1];
    char sessionId[MAX_SESSION_ID + 1];
    char *base;
    SpeexResamplerState *resampler;
    void *streamer;
    responseHandler_t responseHandler;
    // switch_memory_pool_t *pool;
    switch_thread_t *thread;
    // switch_thread_t *process_thread;
    int end_of_utterance;
    int play_file;
    switch_vad_t *vad;
    uint32_t samples_per_second;
};

#endif