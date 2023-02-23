#include "mod_call_bot.h"
#include "call_bot_glue.h"
#include <stdlib.h>
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_call_bot_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_call_bot_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_call_bot_load);

SWITCH_MODULE_DEFINITION(mod_call_bot, mod_call_bot_load, mod_call_bot_shutdown, NULL);

static switch_status_t do_stop(switch_core_session_t *session, char *bugname)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug = switch_channel_get_private(channel, bugname);

	if (bug)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Received user command command, calling call_bot_session_cleanup\n");
		status = call_bot_session_cleanup(session, 0, bug);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "callbot stopped\n");
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Bug is not attached.\n", switch_channel_get_name(channel));
	return status;
}

static void responseHandler(switch_core_session_t *session, const char *json, const char *bugname,
							const char *details)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s json payload: %s.\n", bugname ? bugname : "call_bot", json);
	// switch_event_t *event;
	// switch_channel_t *channel = switch_core_session_get_channel(session);

	// if (0 == strcmp("vad_detected", json))
	// {
	// 	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_VAD_DETECTED);
	// 	switch_channel_event_set_data(channel, event);
	// 	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "nvidia");
	// }
	// else if (0 == strcmp("start_of_speech", json))
	// {
	// 	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_START_OF_SPEECH);
	// 	switch_channel_event_set_data(channel, event);
	// 	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "nvidia");
	// }
	// else if (0 == strcmp("end_of_transcription", json))
	// {
	// 	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_TRANSCRIPTION_COMPLETE);
	// 	switch_channel_event_set_data(channel, event);
	// 	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "nvidia");
	// }
	// else if (0 == strcmp("error", json))
	// {
	// 	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_ERROR);
	// 	switch_channel_event_set_data(channel, event);
	// 	switch_event_add_body(event, "%s", details);
	// 	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "nvidia");
	// }
	// else
	// {
	// 	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s json payload: %s.\n", bugname ? bugname : "nvidia_transcribe", json);

	// 	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, TRANSCRIBE_EVENT_RESULTS);
	// 	switch_channel_event_set_data(channel, event);
	// 	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "transcription-vendor", "nvidia");
	// 	switch_event_add_body(event, "%s", json);
	// }
	// if (bugname)
	// 	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "media-bugname", bugname);
	// switch_event_fire(&event);
}

static switch_bool_t capture_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);

	switch (type)
	{
	case SWITCH_ABC_TYPE_INIT:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_INIT.\n");
		break;

	case SWITCH_ABC_TYPE_CLOSE:
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_CLOSE, calling call_bot_session_cleanup.\n");
		call_bot_session_cleanup(session, 1, bug);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Finished SWITCH_ABC_TYPE_CLOSE.\n");
	}
	break;

	case SWITCH_ABC_TYPE_READ:
	{
		return call_bot_frame(bug, user_data);
		return SWITCH_TRUE;
	}
	break;

	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

static switch_status_t start_capture(switch_core_session_t *session, switch_media_bug_flag_t flags, char *lang, int interim, char *bugname)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_codec_implementation_t read_impl = {0};
	void *pUserData;
	uint32_t samples_per_second;
	const char *var;

	if (switch_channel_get_private(channel, bugname))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "removing bug from previous call_bot session\n");
		do_stop(session, bugname);
	}

	switch_core_session_get_read_impl(session, &read_impl);

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS)
	{
		return SWITCH_STATUS_FALSE;
	}
	/* required channel vars */
	var = switch_channel_get_variable(channel, "CALLBOT_MASTER_URI");

	if (!var)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "CALLBOT_MASTER_URI channel var must be defined\n");
		return SWITCH_STATUS_FALSE;
	}

	samples_per_second = !strcasecmp(read_impl.iananame, "g722") ? read_impl.actual_samples_per_second : read_impl.samples_per_second;
	if (SWITCH_STATUS_FALSE == call_bot_session_init(session, responseHandler, samples_per_second, flags & SMBF_STEREO ? 2 : 1, lang, interim, bugname, &pUserData))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error initializing callbot session.\n");
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_core_media_bug_add(session, bugname, NULL, capture_callback, pUserData, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS)
	{
		return status;
	}

	switch_channel_set_private(channel, bugname, bug);

	return SWITCH_STATUS_SUCCESS;
}

#define TRANSCRIBE_API_SYNTAX "<uuid> start|stop [<cid_num>] lang"
SWITCH_STANDARD_API(call_bot_function)
{
	char *mycmd = NULL, *argv[4] = {0};
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_media_bug_flag_t flags = SMBF_READ_STREAM;
	switch_core_session_t *lsession = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd)))
	{
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) ||
		(!strcasecmp(argv[1], "stop") && argc < 2) ||
		(!strcasecmp(argv[1], "start") && argc < 3) ||
		zstr(argv[0]))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error with command %s %s %s.\n", cmd, argv[0], argv[1]);
		stream->write_function(stream, "-USAGE: %s\n", TRANSCRIBE_API_SYNTAX);
		goto done;
	}

	if ((lsession = switch_core_session_locate(argv[0])))
	{
		if (!strcasecmp(argv[1], "stop"))
		{
			status = do_stop(lsession, MY_BUG_NAME);
		}
		else if (!strcasecmp(argv[1], "start"))
		{
			char *lang = argv[3];
			status = start_capture(lsession, flags, lang, 1, MY_BUG_NAME);
		}
		switch_core_session_rwunlock(lsession);
	}

	if (status == SWITCH_STATUS_SUCCESS)
	{
		stream->write_function(stream, "+OK Success 1\n");
	}
	else
	{
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_call_bot_load)
{
	switch_api_interface_t *api_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_call_bot API loading..\n");

	/* create/register custom event message types */
	if (switch_event_reserve_subclass(EVENT_VAD_CHANGE) != SWITCH_STATUS_SUCCESS)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register an event subclass EVENT_VAD_CHANGE for mod_call_bot API.\n");
		return SWITCH_STATUS_TERM;
	}
	if (switch_event_reserve_subclass(EVENT_VAD_SUMMARY) != SWITCH_STATUS_SUCCESS)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register an event subclass EVENT_VAD_SUMMARY for mod_call_bot API.\n");
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "callbot version 1.0.2\n");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Callbot grpc loading..\n");
	if (SWITCH_STATUS_FALSE == call_bot_init())
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed initializing call bot grpc\n");
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Callbot grpc loaded\n");

	SWITCH_ADD_API(api_interface, "start_call_with_bot", "Start call with bot API", call_bot_function, TRANSCRIBE_API_SYNTAX);
	switch_console_set_complete("add start_call_with_bot uuid start number");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_call_bot API successfully loaded\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_call_bot_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_call_bot_shutdown)
{
	// call_bot_cleanup();
	switch_event_free_subclass(EVENT_VAD_CHANGE);
	switch_event_free_subclass(EVENT_VAD_SUMMARY);
	return SWITCH_STATUS_SUCCESS;
}
