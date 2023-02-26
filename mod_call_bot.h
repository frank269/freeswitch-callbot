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

// enum SmartIVRResponseType : int {
//   RECOGNIZE = 0,
//   RESULT_ASR = 1,
//   RESULT_TTS = 2,
//   CALL_WAIT = 3,
//   CALL_FORWARD = 4,
//   CALL_END = 5,
//   SmartIVRResponseType_INT_MIN_SENTINEL_DO_NOT_USE_ = std::numeric_limits<::PROTOBUF_NAMESPACE_ID::int32>::min(),
//   SmartIVRResponseType_INT_MAX_SENTINEL_DO_NOT_USE_ = std::numeric_limits<::PROTOBUF_NAMESPACE_ID::int32>::max()
// };

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