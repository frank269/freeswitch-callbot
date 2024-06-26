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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Received user command command, calling call_bot_session_cleanup\n");
		status = call_bot_session_cleanup(session, 0, bug);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "callbot stopped\n");
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s Bug is not attached.\n", switch_channel_get_name(channel));
	return status;
}

static void responseHandler(switch_core_session_t *session, const char *json, const char *bugname,
							const char *details)
{
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s json payload: %s.\n", bugname ? bugname : "call_bot", json);
}

char *copyArrayFromIndex(char *originalArray, int startIndex)
{
	char *newArray;
	int newArrayLength = 37;
	int length = strlen(originalArray);
	if (length < 40)
	{
		return NULL;
	}

	newArray = (char *)malloc(newArrayLength * sizeof(char));
	if (newArray == NULL)
	{
		printf("Memory allocation failed.\n");
		return NULL;
	}

	strncpy(newArray, originalArray + startIndex, newArrayLength);
	newArray[newArrayLength - 1] = '\0'; // Null-terminate the new array

	return newArray;
}

static void event_stop_audio_handler(switch_event_t *event)
{
	char *sessionId;
	switch_channel_t *channel;
	switch_core_session_t *session;
	const char *curFile, *filePath = switch_event_get_header(event, "Playback-File-Path");
	if (!filePath)
	{
		return;
	}

	sessionId = copyArrayFromIndex(strdup(filePath), 5);

	if (sessionId == NULL)
	{
		filePath = NULL;
		curFile = NULL;
		return;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s event_stop_audio_handler: stop play file %s!\n", sessionId, filePath);
	remove(filePath);
	session = switch_core_session_locate(sessionId);
	if (!session)
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "event_stop_audio_handler: session %s is gone!\n", sessionId);
		filePath = NULL;
		curFile = NULL;
		free(sessionId);
		return;
	}
	channel = switch_core_session_get_channel(session);

	curFile = switch_channel_get_variable(channel, "CUR_FILE");
	if (curFile && (strcmp(curFile, filePath) == 0))
	{
		switch_channel_set_variable(channel, "IS_PLAYING", "false");
		switch_channel_set_variable(channel, "CUR_FILE", "");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "event_stop_audio_handler: session %s set playing to false!\n", sessionId);
	};
	switch_core_session_rwunlock(session);
	filePath = NULL;
	curFile = NULL;
	free(sessionId);
}

static switch_bool_t capture_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);

	switch (type)
	{
	case SWITCH_ABC_TYPE_INIT:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_INIT.\n");
		break;

	case SWITCH_ABC_TYPE_CLOSE:
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_CLOSE, calling call_bot_session_cleanup.\n");
		call_bot_session_cleanup(session, 1, bug);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Finished SWITCH_ABC_TYPE_CLOSE.\n");
	}
	break;

	case SWITCH_ABC_TYPE_READ:
	{
		// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Finished SWITCH_ABC_TYPE_READ.\n");
		return call_bot_frame(bug, user_data);
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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "removing bug from previous call_bot session\n");
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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
						  "CALLBOT_MASTER_URI channel var must be defined\n");
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return SWITCH_STATUS_FALSE;
	}

	samples_per_second = !strcasecmp(read_impl.iananame, "g722") ? read_impl.actual_samples_per_second : read_impl.samples_per_second;
	if (SWITCH_STATUS_FALSE == call_bot_session_init(session, responseHandler, samples_per_second, flags & SMBF_STEREO ? 2 : 1, lang, interim, bugname, &pUserData))
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error initializing callbot session.\n");
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_core_media_bug_add(session, bugname, NULL, capture_callback, pUserData, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS)
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error add bug.\n");
		return status;
	}

	switch_channel_set_private(channel, bugname, bug);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_to_silence_session(switch_core_session_t *session, switch_input_args_t *args)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	unsigned char isStarted = 0;

	unsigned char *abuf = NULL;
	switch_frame_t write_frame = {0};
	switch_codec_t codec = {0};
	switch_codec_implementation_t imp = {0};

	switch_zmalloc(abuf, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS)
	{
		return SWITCH_STATUS_FALSE;
	}

	arg_recursion_check_start(args);

	while (switch_channel_ready(channel))
	{
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status))
		{
			continue;
		}
		switch_ivr_parse_all_events(session);

		if (!isStarted)
		{
			const char *start_bot = switch_channel_get_variable(channel, "START_BOT");
			if (start_bot && (strcmp(start_bot, "true") == 0))
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CALL_WITH_BOT Start capture....\n");
				status = start_capture(session, SMBF_READ_STREAM, "", 1, MY_BUG_NAME);
				isStarted = 1;
			}
		}

		if (switch_channel_media_ready(channel))
		{
			switch_core_session_get_read_impl(session, &imp);

			if (switch_core_codec_init(&codec,
									   "L16",
									   NULL,
									   NULL,
									   imp.actual_samples_per_second,
									   imp.microseconds_per_packet / 1000,
									   imp.number_of_channels,
									   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
									   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS)
			{
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error L16@%uhz %u channels %dms\n",
								  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);
				continue;
			}
			write_frame.codec = &codec;
			write_frame.data = abuf;
			write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
			write_frame.datalen = imp.decoded_bytes_per_packet;
			write_frame.samples = write_frame.datalen / sizeof(int16_t);
			memset((int16_t *)write_frame.data, 0, write_frame.samples * 2);

			switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		}
	}

	if (write_frame.codec)
	{
		switch_core_codec_destroy(&codec);
	}
	read_frame = NULL;
	channel = NULL;
	switch_safe_free(abuf);
	switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(call_bot_app_function)
{
	switch_to_silence_session(session, NULL);
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
		zstr(argv[2]))
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
	switch_application_interface_t *app_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_call_bot API loading..\n");

	/* create/register custom event message types */
	if (switch_event_bind_removable(modname, SWITCH_EVENT_PLAYBACK_STOP, NULL, event_stop_audio_handler, NULL, NULL) != SWITCH_STATUS_SUCCESS)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind stop audio event!\n");
		return SWITCH_STATUS_GENERR;
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
	switch_console_set_complete("add start_call_with_bot ::console::list_uuid start 1 2");

	SWITCH_ADD_APP(app_interface, "start_call_with_bot", "Start call with bot API", "Start call with bot API", call_bot_app_function, TRANSCRIBE_API_SYNTAX, SAF_NONE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_call_bot API successfully loaded\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_call_bot_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_call_bot_shutdown)
{
	switch_event_unbind_callback(event_stop_audio_handler);
	call_bot_cleanup();
	return SWITCH_STATUS_SUCCESS;
}
