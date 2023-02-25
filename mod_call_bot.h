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
#define TRANSCRIBE_EVENT_RESULTS "call_bot::transcription"
#define TRANSCRIBE_EVENT_END_OF_UTTERANCE "call_bot::end_of_utterance"
#define TRANSCRIBE_EVENT_START_OF_TRANSCRIPT "call_bot::start_of_transcript"
#define TRANSCRIBE_EVENT_END_OF_TRANSCRIPT "call_bot::end_of_transcript"
#define TRANSCRIBE_EVENT_NO_AUDIO_DETECTED "call_bot::no_audio_detected"
#define TRANSCRIBE_EVENT_MAX_DURATION_EXCEEDED "call_bot::max_duration_exceeded"
#define TRANSCRIBE_EVENT_PLAY_INTERRUPT "call_bot::play_interrupt"
#define TRANSCRIBE_EVENT_VAD_DETECTED "call_bot::vad_detected"

#define EVENT_VAD_CHANGE "call_bot::change"
#define EVENT_VAD_SUMMARY "call_bot::summary"

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
    switch_memory_pool_t *pool;
    switch_thread_t *thread;
    switch_thread_t *process_thread;
    int end_of_utterance;
    int play_file;
    switch_vad_t *vad;
    uint32_t samples_per_second;
};

#endif