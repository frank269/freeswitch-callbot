#include <switch.h>
#include <unistd.h>

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