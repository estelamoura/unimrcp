/*
 * Copyright 2008-2014 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include <apr_thread_proc.h>
#include "asr_engine.h"

typedef struct {
	const char        *root_dir_path;
	apt_log_priority_e log_priority;
	apt_log_output_e   log_output;
	apr_pool_t        *pool;
} client_options_t;

typedef struct {
	asr_engine_t      *engine;
	const char        *grammar_uri;
	const char        *input_file;
	const char        *recogs_repetition;
	const char        *profile;

	const char        *send_define_grammar;
  const char        *send_set_params;
	int                id;

	apr_thread_t      *thread;
	apr_pool_t        *pool;
} asr_params_t;

int session_index = 0;	/** Global controler of asr sessions sequence for debug */

/** Thread function to run ASR scenario in */
static void* APR_THREAD_FUNC asr_session_run(apr_thread_t *thread, void *data)
{
	asr_params_t *params = data;
	asr_session_t *session = asr_session_create(params->engine,params->profile);
	const char *result = NULL;
	time_t start, end;
	double elapsed_time;
	if(session) {
		int i;
		for(i = 1; i <= atoi(params->recogs_repetition); i++) {
			start = time(NULL);
      result = asr_session_file_recognize(session,params->grammar_uri,params->input_file,params->send_define_grammar, params->send_set_params);
			end = time(NULL);
			elapsed_time = (double)(end - start);
			printf("\n\n*** (Session %d) Profile: %s. Recognition %d finished. Elapsed time %.2f seconds.", params->id, params->profile, i, elapsed_time);
			if(result) {
				printf("\n***** Result: %s\n\n", result);
			}
			else {
				printf("\n***** Result NULL\n\n");
			}
			result = NULL;
		}
		asr_session_destroy(session);
	}

	/* destroy pool params allocated from */
	apr_pool_destroy(params->pool);
	return NULL;
}

/** Launch demo ASR session */
static apt_bool_t asr_session_launch(asr_engine_t *engine, const char *grammar_uri, const char *input_file,
																		 const char *profile, const char *recogs_repetition, const char *send_define_grammar,
                                     const char *send_set_params)
{
	apr_pool_t *pool;
	asr_params_t *params;

	/* create pool to allocate params from */
	apr_pool_create(&pool,NULL);
	params = apr_palloc(pool,sizeof(asr_params_t));
	params->pool = pool;
	params->engine = engine;

  if(send_set_params) {
    params->send_set_params = apr_pstrdup(pool,send_set_params);
  }
  else {
    apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Empty parameter: send_set_params (input help for usage)");
    return FALSE;
  }

	if(send_define_grammar) {
		params->send_define_grammar = apr_pstrdup(pool,send_define_grammar);
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Empty parameter: send_define_grammar (input help for usage)");
		return FALSE;
	}

	if(grammar_uri) {
		params->grammar_uri = apr_pstrdup(pool,grammar_uri);
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Empty parameter: grammar_uri (input help for usage)");
		return FALSE;
	}

	if(input_file) {
		params->input_file = apr_pstrdup(pool,input_file);
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Empty parameter: input_file (input help for usage)");
		return FALSE;
	}

	if(profile) {
		params->profile = apr_pstrdup(pool,profile);
	}
	else {
		params->profile = "uni2";
	}

	if(recogs_repetition) {
		params->recogs_repetition = apr_pstrdup(pool,recogs_repetition);
	} else {
		params->recogs_repetition = "1";
	}
	session_index++;
	params->id = session_index;

	printf("\nParameters: %s - %s - %s - %s\n", params->grammar_uri, params->input_file, params->recogs_repetition, params->profile);
	/* Launch a thread to run demo ASR session in */
	if(apr_thread_create(&params->thread,NULL,asr_session_run,params,pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return FALSE;
	}

	return TRUE;
}

static apt_bool_t cmdline_process(asr_engine_t *engine, char *cmdline)
{
	apt_bool_t running = TRUE;
	char *name;
	char *last;
	name = apr_strtok(cmdline, " ", &last);

	if(strcasecmp(name,"run") == 0) {
    char *send_set_params = apr_strtok(NULL, " ", &last);
		char *send_define_grammar = apr_strtok(NULL, " ", &last);
		char *grammar = apr_strtok(NULL, " ", &last);
		char *input = apr_strtok(NULL, " ", &last);
    char *recogs_repetition = apr_strtok(NULL, " ", &last);
		char *profile = apr_strtok(NULL, " ", &last);
    asr_session_launch(engine,grammar,input,profile,recogs_repetition,send_define_grammar,send_set_params);
	}
	else if(strcasecmp(name,"loglevel") == 0) {
		char *priority = apr_strtok(NULL, " ", &last);
		if(priority) {
			asr_engine_log_priority_set(atol(priority));
		}
	}
	else if(strcasecmp(name,"exit") == 0 || strcmp(name,"quit") == 0) {
		running = FALSE;
	}
	else if(strcasecmp(name,"help") == 0) {
		printf("\nUsage:"
      "\n"
      "    run <send_set_params> <send_define_grammar> <grammar_uri> <audio_input_file> [recogs_repetition] [profile_name]\n\n"
      "       1- send_set_params: 'y' to send SET-PARAMS message or any other value to not send it\n"
      "          The sent parameters are:\n"
      "            |__________________________________|\n"
      "            |  confidence_threshold = 0.9      |\n"
      "            |  n_best_list_length = 2          |\n"
      "            |  no_input_timeout = 1000         |\n"
      "            |  recognition_timeout = 5000      |\n"
      "            |  start_input_timers = FALSE      |\n"
      "            |__________________________________|\n\n"
      "       2- send_define_grammar: 'y' to send DEFINE-GRAMMAR message or any other value to not send it\n\n"
      "       3- grammar_uri: is the path of the slm or grammar to be used in the recognition\n\n"
			"       4- audio_input_file: is the name of an audio file (if the audio is in the data dir)\n"
      "          or the full path of the audio (if it is not in the data dir)\n\n"
      "       5- recogs_repetition: is the number of recognitions in the same session (default = 1)\n\n"
			"       6- profile_name: is one of 'uni2', 'uni1'\n"
      "          (by default. You can add more in the file conf/client-profiles/unimrcp.xml)\n"
			"\n"
      "       Examples of run command: \n"
			"         run y y builtin:lm pt-br-male-8KHz.raw\n"
      "         run n n builtin:lm pt-br-male-8KHz.raw 5\n"
			"         run y n builtin:lm pt-br-male-8KHz.raw 3 uni1\n"
      "\n"
      "    loglevel [level] (set loglevel, one of 0,1...7)\n"
      "\n"
      "    quit, exit\n"
      "\n"
      "NOTE: Some parameters are sent in the RECOGNIZE message header.\n"
      "      The sent parameters are:\n"
      "        |__________________________________|\n"
      "        |  confidence_threshold = 0.7      |\n"
      "        |  n_best_list_length = 4          |\n"
      "        |  no_input_timeout = 2000         |\n"
      "        |  recognition_timeout = 11000     |\n"
      "        |  start_input_timers = TRUE       |\n"
      "        |__________________________________|\n");
	}
	else {
		printf("Unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

static apt_bool_t cmdline_run(asr_engine_t *engine)
{
	apt_bool_t running = TRUE;
	char cmdline[1024];
	apr_size_t i;
	do {
		printf("\nasrclient-cli> ");
		memset(&cmdline, 0, sizeof(cmdline));
		for(i = 0; i < sizeof(cmdline); i++) {
			cmdline[i] = (char) getchar();
			if(cmdline[i] == '\n') {
				cmdline[i] = '\0';
				break;
			}
		}
		if(*cmdline) {
			running = cmdline_process(engine,cmdline);
		}
	}
	while(running != 0);
	return TRUE;
}

static void usage(void)
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  asrclient [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -r [--root-dir] path     : Set the project root directory path.\n"
		"\n"
		"   -l [--log-prio] priority : Set the log priority.\n"
		"                              (0-emergency, ..., 7-debug)\n"
		"\n"
		"   -o [--log-output] mode   : Set the log output mode.\n"
		"                              (0-none, 1-console only, 2-file only, 3-both)\n"
		"\n"
		"   -h [--help]              : Show the help.\n"
		"\n");
}

static void options_destroy(client_options_t *options)
{
	if(options->pool) {
		apr_pool_destroy(options->pool);
	}
}

static client_options_t* options_load(int argc, const char * const *argv)
{
	apr_status_t rv;
	apr_getopt_t *opt = NULL;
	int optch;
	const char *optarg;
	apr_pool_t *pool;
	client_options_t *options;

	const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "root-dir",    'r', TRUE,  "path to root dir" },  /* -r arg or --root-dir arg */
		{ "log-prio",    'l', TRUE,  "log priority" },      /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },   /* -o arg or --log-output arg */
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* create APR pool to allocate options from */
	apr_pool_create(&pool,NULL);
	if(!pool) {
		return NULL;
	}
	options = apr_palloc(pool,sizeof(client_options_t));
	options->pool = pool;
	/* set the default options */
	options->root_dir_path = NULL;
	options->log_priority = APT_PRIO_INFO;
	options->log_output = APT_LOG_OUTPUT_CONSOLE;


	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		options_destroy(options);
		return NULL;
	}

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'r':
				options->root_dir_path = optarg;
				break;
			case 'l':
				if(optarg) {
					options->log_priority = atoi(optarg);
				}
				break;
			case 'o':
				if(optarg) {
					options->log_output = atoi(optarg);
				}
				break;
			case 'h':
				usage();
				return FALSE;
		}
	}

	if(rv != APR_EOF) {
		usage();
		options_destroy(options);
		return NULL;
	}

	return options;
}

int main(int argc, const char * const *argv)
{
	client_options_t *options;
	asr_engine_t *engine;

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* load options */
	options = options_load(argc,argv);
	if(!options) {
		apr_terminate();
		return 0;
	}

	/* create asr engine */
	engine = asr_engine_create(
				options->root_dir_path,
				options->log_priority,
				options->log_output);
	if(engine) {
		/* run command line  */
		cmdline_run(engine);
		/* destroy demo framework */
		asr_engine_destroy(engine);
	}

	/* destroy options */
	options_destroy(options);

	/* APR global termination */
	apr_terminate();
	return 0;
}
