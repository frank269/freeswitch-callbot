#include <switch.h>
#include <unistd.h>

#include <switch.h>
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

struct cap_cb
{
	switch_vad_t *vad;
	switch_mutex_t *mutex;
	struct timeval start;
	struct timeval last_segment_start;
	uint32_t speech_segments;
	long long speech_duration;
	switch_vad_state_t vad_state;
};