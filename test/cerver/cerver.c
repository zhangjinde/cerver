#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <cerver/cerver.h>
#include <cerver/events.h>

#include "../test.h"

void *on_cever_started (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);
	test_check_null_ptr (event_data->connection);

	test_check_ptr (event_data->action_args);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_cever_teardown (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);
	test_check_null_ptr (event_data->connection);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_client_connected (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_ptr (event_data->client);
	test_check_ptr (event_data->connection);
	test_check_ptr (event_data->connection->socket);
	test_check_int_ne (event_data->connection->socket->sock_fd, 0);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_client_new_connection (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_ptr (event_data->client);
	test_check_ptr (event_data->connection);
	test_check_ptr (event_data->connection->socket);
	test_check_int_ne (event_data->connection->socket->sock_fd, 0);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_client_close_connection (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);
	test_check_null_ptr (event_data->connection);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_hold_connected (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);

	test_check_ptr (event_data->connection);
	test_check_ptr (event_data->connection->socket);
	test_check_int_ne (event_data->connection->socket->sock_fd, 0);
	test_check_int_ne (event_data->connection->socket->sock_fd, -1);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_hold_disconnected (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);
	test_check_null_ptr (event_data->connection);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_hold_drop (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);
	test_check_null_ptr (event_data->connection);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_client_success_auth (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_ptr (event_data->client);

	test_check_ptr (event_data->connection);
	test_check_ptr (event_data->connection->socket);
	test_check_int_ne (event_data->connection->socket->sock_fd, 0);
	test_check_int_ne (event_data->connection->socket->sock_fd, -1);

	cerver_event_data_delete (event_data);

	return NULL;

}

void *on_client_failed_auth (void *event_data_ptr) {

	test_check_ptr (event_data_ptr);

	CerverEventData *event_data = (CerverEventData *) event_data_ptr;

	test_check_ptr (event_data->cerver);
	test_check_ptr (event_data->cerver->info);

	test_check_null_ptr (event_data->client);

	test_check_ptr (event_data->connection);
	test_check_ptr (event_data->connection->socket);
	test_check_int_ne (event_data->connection->socket->sock_fd, 0);
	test_check_int_ne (event_data->connection->socket->sock_fd, -1);

	cerver_event_data_delete (event_data);

	return NULL;

}