#include "mod_call_bot.h"
#include <stdlib.h>
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_call_bot_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_call_bot_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_call_bot_load);

SWITCH_MODULE_DEFINITION(mod_call_bot, mod_call_bot_load, mod_call_bot_shutdown, NULL);

static switch_status_t do_stop(switch_core_session_t *session, char *bugname)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug = switch_channel_get_private(channel, bugname);

	if (bug)
	{
		struct cap_cb *cb = (struct cap_cb *)switch_core_media_bug_get_user_data(bug);
		if (!cb)
		{
			return SWITCH_STATUS_FALSE;
		}
		switch_mutex_lock(cb->mutex);
		// try release all resources
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Call bot released!");
		switch_mutex_unlock(cb->mutex);

		switch_core_media_bug_remove(session, &bug);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "callbot stopped .\n");
		stream->write_function(stream, "callbot stopped.\n");
		return SWITCH_STATUS_SUCCESS;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Bug is not attached.\n", switch_channel_get_name(channel));
	stream->write_function(stream, "Bug is not attached.\n");
	return SWITCH_STATUS_FALSE;
}

static switch_bool_t capture_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct cap_cb *cb = (struct cap_cb *)user_data;

	switch (type)
	{
	case SWITCH_ABC_TYPE_INIT:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Callbot got SWITCH_ABC_TYPE_INIT.\n");
		stream->write_function(stream, "Callbot got SWITCH_ABC_TYPE_INIT.\n");
		break;
	case SWITCH_ABC_TYPE_CLOSE:
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Callbot SWITCH_ABC_TYPE_CLOSE\n");
		stream->write_function(stream, "Callbot SWITCH_ABC_TYPE_CLOSE.\n");
		if (cb && cb->vad)
		{
			switch_vad_destroy(&cb->vad);
			cb->vad = NULL;
		}
		if (cb && cb->mutex)
		{
			switch_mutex_destroy(cb->mutex);
			cb->mutex = NULL;
		}
		switch_channel_set_private(channel, MY_BUG_NAME, NULL);
	}
	break;
	case SWITCH_ABC_TYPE_READ:

		if (cb->vad)
		{
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
			switch_frame_t frame = {0};
			frame.data = data;
			frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

			if (switch_mutex_trylock(cb->mutex) == SWITCH_STATUS_SUCCESS)
			{
				while (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && !switch_test_flag((&frame), SFF_CNG))
				{
					if (frame.datalen)
					{
						switch_vad_state_t state = switch_vad_process(cb->vad, frame.data, frame.samples);
						if (state == SWITCH_VAD_STATE_START_TALKING)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "detected speech, connect to bot ....\n");
							stream->write_function(stream, "Callbot detected speech, connect to bot ....\n");
						}
					}
				}
				switch_mutex_unlock(cb->mutex);
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

static switch_status_t start_capture(switch_core_session_t *session, switch_media_bug_flag_t flags, int mode)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_codec_implementation_t read_impl = {0};
	struct cap_cb *cb;

	if (switch_channel_get_private(channel, MY_BUG_NAME))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Already Running.\n");
		stream->write_function(stream, "Callbot already Running.\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_core_session_get_read_impl(session, &read_impl);

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS)
	{
		return SWITCH_STATUS_FALSE;
	}

	cb = switch_core_session_alloc(session, sizeof(*cb));
	switch_mutex_init(&cb->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	cb->vad = switch_vad_init(read_impl.samples_per_second, 1);
	if (!cb->vad)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocating vad\n");
		stream->write_function(stream, "Callbot error allocating vad\n");
		switch_mutex_destroy(cb->mutex);
		return SWITCH_STATUS_FALSE;
	}
	switch_vad_set_mode(cb->vad, mode);
	// switch_vad_set_param(cb->vad, "debug", 10);
	cb->start.tv_sec = 0;
	cb->start.tv_usec = 0;
	cb->speech_segments = 0;
	cb->speech_duration = 0;
	cb->vad_state = SWITCH_VAD_STATE_NONE;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Call bot: starting .........\n");
	stream->write_function(stream, "Callbot starting .... \n");

	if ((status = switch_core_media_bug_add(session, MY_BUG_NAME, NULL, capture_callback, cb, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS)
	{
		return status;
	}

	switch_channel_set_private(channel, MY_BUG_NAME, bug);

	return SWITCH_STATUS_SUCCESS;
}

#define TRANSCRIBE_API_SYNTAX "<uuid> start|stop [<cid_num>] "
SWITCH_STANDARD_API(call_bot_function)
{
	char *mycmd = NULL, *argv[3] = {0};
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;
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
			switch_media_bug_flag_t flags = SMBF_READ_STREAM;
			status = start_capture(lsession, flags, 0);
		}
		switch_core_session_rwunlock(lsession);
	}

	if (status == SWITCH_STATUS_SUCCESS)
	{
		stream->write_function(stream, "+OK Success\n");
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello world \n");

	SWITCH_ADD_API(api_interface, "start_call_with_bot", "Start call with bot API", call_bot_function, TRANSCRIBE_API_SYNTAX);
	switch_console_set_complete("add start_call_with_bot secs");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_call_bot API successfully loaded\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_call_bot_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_call_bot_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}
