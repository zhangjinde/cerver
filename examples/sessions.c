#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <time.h>
#include <signal.h>

#include <cerver/version.h>
#include <cerver/cerver.h>
#include <cerver/auth.h>
#include <cerver/sessions.h>

#include <cerver/utils/log.h>
#include <cerver/utils/utils.h>

typedef enum AppRequest {

	TEST_MSG		= 0

} AppRequest;

typedef struct Credentials {

	char username[64];
	char password[64];

} Credentials;

static Cerver *my_cerver = NULL;

// correctly closes any on-going server and process when quitting the appplication
static void end (int dummy) {
	
	if (my_cerver) {
		cerver_stats_print (my_cerver);
		cerver_teardown (my_cerver);
	} 

	exit (0);

}

static void handle_test_request (Packet *packet) {

	if (packet) {
		cerver_log_debug ("Got a test message from client. Sending another one back...");
		
		Packet *test_packet = packet_generate_request (APP_PACKET, TEST_MSG, NULL, 0);
		if (test_packet) {
			packet_set_network_values (test_packet, NULL, NULL, packet->connection, NULL);
			size_t sent = 0;
			if (packet_send (test_packet, 0, &sent, false)) 
				cerver_log_error ("Failed to send test packet to client!");

			else {
				// printf ("Response packet sent: %ld\n", sent);
			}
			
			packet_delete (test_packet);
		}
	}

}

static void handler (void *data) {

	if (data) {
		Packet *packet = (Packet *) data;
		if (packet->data_size >= sizeof (RequestData)) {
			RequestData *req = (RequestData *) (packet->data);

			switch (req->type) {
				case TEST_MSG: handle_test_request (packet); break;

				default: 
					cerver_log_msg (stderr, LOG_WARNING, LOG_PACKET, "Got an unknown app request.");
					break;
			}
		}
	}

}

static u8 my_auth_method_username (AuthMethod *auth_method, const char *username) {

	u8 retval = 1;

	if (username) {
		if (strlen (username) > 0) {
			if (!strcmp (username, "ermiry")) {
				retval = 0;		// success
			}

			else {
				cerver_log_error ("my_auth_method () - Username does not exists!");
				auth_method->error_message = estring_new ("Username does not exists!");
			}
		}

		else {
			cerver_log_error ("my_auth_method () - Username is required!");
			auth_method->error_message = estring_new ("Username is required!");
		}
	}

	return retval;

}

static u8 my_auth_method_password (AuthMethod *auth_method, const char *password) {
	
	u8 retval = 1;

	if (password) {
		if (strlen (password) > 0) {
			if (!strcmp (password, "hola12")) {
				retval = 0;		// success auth
			}

			else {
				cerver_log_error ("my_auth_method () - Password is incorrect!");
				auth_method->error_message = estring_new ("Password is incorrect!");
			}
		}

		else {
			cerver_log_error ("my_auth_method () - Password is required!");
			auth_method->error_message = estring_new ("Password is required!");
		}
	}

	return retval;

}

static u8 my_auth_method (void *auth_method_ptr)  {

	u8 retval = 1;

	if (auth_method_ptr) {
		AuthMethod *auth_method = (AuthMethod *) auth_method_ptr;
		if (auth_method->auth_data) {
			if (auth_method->auth_data->auth_data_size >= sizeof (Credentials)) {
				Credentials *credentials = (Credentials *) auth_method->auth_data->auth_data;

				printf ("\nReceived credentials: \n");
				printf ("\tUsername: %s\n", credentials->username);
				printf ("\tPassword: %s\n", credentials->password);

				if (!my_auth_method_username (auth_method, credentials->username)) {
					if (!my_auth_method_password (auth_method, credentials->password)) {
						retval = 0;		// success
					}
				}
			}

			else {
				cerver_log_error ("my_auth_method () - auth data is of wrong size!");
				auth_method->error_message = estring_new ("Missing auth data!");
			}
		}

		else {
			cerver_log_error ("my_auth_method () - auth packet does not have auth data!");
		}
	}

	else {
		cerver_log_error ("my_auth_method () - NULL auth_method_ptr!");
	}

	return retval;

}

int main (void) {

	srand (time (NULL));

	// register to the quit signal
	signal (SIGINT, end);

	printf ("\n");
	cerver_version_print_full ();
	printf ("\n");

	cerver_log_debug ("Sessions Example");
	printf ("\n");
	cerver_log_debug ("Cerver with auth & sesions options enabled");
	printf ("\n");

	my_cerver = cerver_create (CUSTOM_CERVER, "my-cerver", 8007, PROTOCOL_TCP, false, 2, 2000);
	if (my_cerver) {
		/*** cerver configuration ***/
		cerver_set_receive_buffer_size (my_cerver, 4096);
		// cerver_set_thpool_n_threads (my_cerver, 4);

		Handler *app_handler = handler_create (handler);
		handler_set_direct_handle (app_handler, true);
		cerver_set_app_handlers (my_cerver, app_handler, NULL);

		/*** cerver auth configuration ***/
		cerver_set_auth (my_cerver, 2, my_auth_method);
		cerver_set_sessions (my_cerver, session_default_generate_id);

		if (cerver_start (my_cerver)) {
			char *s = c_string_create ("Failed to start %s!",
				my_cerver->info->name->str);
			if (s) {
				cerver_log_error (s);
				free (s);
			}

			cerver_delete (my_cerver);
		}
	}

	else {
		cerver_log_error ("Failed to create cerver!");

		// DONT call - cerver_teardown () is called automatically if cerver_create () fails
		// cerver_delete (client_cerver);
	}

	return 0;

}