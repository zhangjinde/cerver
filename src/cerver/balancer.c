#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/balancer.h"
#include "cerver/cerver.h"
#include "cerver/client.h"
#include "cerver/connection.h"
#include "cerver/packets.h"

#include "cerver/threads/thread.h"

#include "cerver/utils/log.h"
#include "cerver/utils/utils.h"

static void balancer_service_delete (void *service_ptr);

void balancer_service_stats_print (Service *service);

static u8 balancer_client_receive (void *custom_data_ptr);

#pragma region types

const char *balancer_type_to_string (BalancerType type) {

	switch (type) {
		#define XX(num, name, string) case BALANCER_TYPE_##name: return #string;
		BALANCER_TYPE_MAP(XX)
		#undef XX
	}

	return balancer_type_to_string (BALANCER_TYPE_NONE);

}

#pragma endregion

#pragma region stats

static BalancerStats *balancer_stats_new (void) {

	BalancerStats *stats = (BalancerStats *) malloc (sizeof (BalancerStats));
	if (stats) {
		memset (stats, 0, sizeof (BalancerStats));
	}

	return stats;

}

static void balancer_stats_delete (BalancerStats *stats) {

	if (stats) free (stats);

}

void balancer_stats_print (Balancer *balancer) {

	if (balancer) {
		fprintf (stdout, LOG_COLOR_BLUE "Balancer: %s\n" LOG_COLOR_RESET, balancer->name->str);
	
		// good types packets that the balancer can handle
		printf ("Receives done:               %ld\n", balancer->stats->receives_done);
		printf ("Received packets:            %ld\n", balancer->stats->n_packets_received);
		printf ("Received bytes:              %ld\n", balancer->stats->bytes_received);

		// bad types packets - consumed data from sock fd until next header
		printf ("Bad receives done:           %ld\n", balancer->stats->bad_receives_done);
		printf ("Bad received packets:        %ld\n", balancer->stats->bad_n_packets_received);
		printf ("Bad received bytes:          %ld\n", balancer->stats->bad_bytes_received);

		// routed packets to services
		printf ("Routed packets:              %ld\n", balancer->stats->n_packets_routed);
		printf ("Routed bytes:                %ld\n", balancer->stats->total_bytes_routed);

		// responses sent to the original clients
		printf ("Responses sent packets:      %ld\n", balancer->stats->n_packets_sent);
		printf ("Responses sent bytes:        %ld\n", balancer->stats->total_bytes_sent);

		// packets that the balancer was unable to handle
		printf ("Unhandled packets:           %ld\n", balancer->stats->unhandled_packets);
		printf ("Unhandled bytes:             %ld\n", balancer->stats->unhandled_bytes);

		printf ("\n");
		printf ("Received packets:");
		printf ("\n");
		packets_per_type_array_print (balancer->stats->received_packets);

		printf ("\n");
		printf ("Routed packets:");
		printf ("\n");
		packets_per_type_array_print (balancer->stats->routed_packets);

		printf ("\n");
		printf ("Responses packets:");
		printf ("\n");
		packets_per_type_array_print (balancer->stats->sent_packets);

		for (int i = 0; i < balancer->n_services; i++) {
			printf ("\n");
			balancer_service_stats_print (balancer->services[i]);
			printf ("\n");
		}
	}

}

#pragma endregion

#pragma region main

Balancer *balancer_new (void) {

	Balancer *balancer = (Balancer *) malloc (sizeof (Balancer));
	if (balancer) {
		balancer->name = NULL;
		balancer->type = BALANCER_TYPE_NONE;

		balancer->cerver = NULL;
		balancer->client = NULL;

		balancer->next_service = 0;
		balancer->n_services = 0;
		balancer->services = NULL;

		balancer->stats = NULL;

		balancer->mutex = NULL;
	}

	return balancer;

}

void balancer_delete (void *balancer_ptr) {

	if (balancer_ptr) {
		Balancer *balancer = (Balancer *) balancer_ptr;

		str_delete (balancer->name);

		cerver_delete (balancer->cerver);
		client_delete (balancer->client);

		if (balancer->services) {
			for (unsigned int i = 0; i < balancer->n_services; i++)
				balancer_service_delete (balancer->services[i]);

			free (balancer->services);
		}

		balancer_stats_delete (balancer->stats);

		pthread_mutex_delete (balancer->mutex);

		free (balancer_ptr);
	}

}

// create a new load balancer of the selected type
// set its network values & set the number of services it will handle
Balancer *balancer_create (
	const char *name, BalancerType type,
	u16 port, u16 connection_queue,
	unsigned int n_services
) {

	Balancer *balancer = balancer_new ();
	if (balancer) {
		balancer->name = str_new (name);
		balancer->type = type;

		balancer->cerver = cerver_create (
			CERVER_TYPE_BALANCER,
			"load-balancer-cerver",
			port, PROTOCOL_TCP, false, connection_queue, DEFAULT_POLL_TIMEOUT
		);

		balancer->cerver->balancer = balancer;

		balancer->client = client_create ();
		client_set_name (balancer->client, "balancer-client");

		balancer->n_services = n_services;
		balancer->services = (Service **) calloc (balancer->n_services, sizeof (Service *));
		if (balancer->services) {
			for (unsigned int i = 0; i < balancer->n_services; i++)
				balancer->services[i] = NULL;
		}

		balancer->stats = balancer_stats_new ();

		balancer->mutex = pthread_mutex_new ();
	}

	return balancer;

}

#pragma endregion

#pragma region services

// auxiliary structure to be passed in ConnectionCustomReceiveData
typedef struct {

	Balancer *balancer;
	Service *service;

} BalancerService;

static BalancerService *balancer_service_aux_new (Balancer *balancer, Service *service) {

	BalancerService *bs = (BalancerService *) malloc (sizeof (BalancerService));
	if (bs) {
		bs->balancer = balancer;
		bs->service = service;
	}

	return bs;

}

static void balancer_service_aux_delete (void *bs_ptr) {

	if (bs_ptr) {
		BalancerService *bs = (BalancerService *) bs_ptr;

		bs->balancer = NULL;
		bs->service = NULL;

		free (bs_ptr);
	}

}

const char *balancer_service_status_to_string (ServiceStatus status) {

	switch (status) {
		#define XX(num, name, string, description) case SERVICE_STATUS_##name: return #string;
		SERVICE_STATUS_MAP(XX)
		#undef XX
	}

	return balancer_service_status_to_string (SERVICE_STATUS_NONE);

}

const char *balancer_service_status_description (ServiceStatus status) {

	switch (status) {
		#define XX(num, name, string, description) case SERVICE_STATUS_##name: return #description;
		SERVICE_STATUS_MAP(XX)
		#undef XX
	}

	return balancer_service_status_description (SERVICE_STATUS_NONE);

}

static ServiceStats *balancer_service_stats_new (void) {

	ServiceStats *stats = (ServiceStats *) malloc (sizeof (ServiceStats));
	if (stats) {
		memset (stats, 0, sizeof (ServiceStats));
	}

	return stats;

}

static void balancer_service_stats_delete (ServiceStats *stats) {

	if (stats) free (stats);

}

void balancer_service_stats_print (Service *service) {

	if (service) {
		fprintf (stdout, LOG_COLOR_BLUE "Service: %s\n" LOG_COLOR_RESET, service->connection->name->str);
	
		// routed packets to the service
		printf ("Routed packets:              %ld\n", service->stats->n_packets_routed);
		printf ("Routed bytes:                %ld\n", service->stats->total_bytes_routed);

		// good types packets received from the service
		printf ("Receives done:               %ld\n", service->stats->receives_done);
		printf ("Received packets:            %ld\n", service->stats->n_packets_received);
		printf ("Received bytes:              %ld\n", service->stats->bytes_received);

		// bad types packets - consumed data from sock fd until next header
		printf ("Bad receives done:           %ld\n", service->stats->bad_receives_done);
		printf ("Bad received packets:        %ld\n", service->stats->bad_n_packets_received);
		printf ("Bad received bytes:          %ld\n", service->stats->bad_bytes_received);

		printf ("\n");
		printf ("Routed packets:");
		printf ("\n");
		packets_per_type_array_print (service->stats->routed_packets);

		printf ("\n");
		printf ("Received packets:");
		printf ("\n");
		packets_per_type_array_print (service->stats->received_packets);
	}

}

static Service *balancer_service_new (void) {

	Service *service = (Service *) malloc (sizeof (Service));
	if (service) {
		service->status = SERVICE_STATUS_NONE;

		service->connection = NULL;
		service->reconnect_wait_time = DEFAULT_SERVICE_WAIT_TIME;

		service->stats = NULL;
	}

	return service;

}

static void balancer_service_delete (void *service_ptr) {

	if (service_ptr) {
		Service *service = (Service *) service_ptr;

		service->connection = NULL;

		balancer_service_stats_delete (service->stats);

		free (service_ptr);
	}

}

static Service *balancer_service_create (void) {

	Service *service = balancer_service_new ();
	if (service) {
		service->stats = balancer_service_stats_new ();
	}

	return service;

}

static void balancer_service_set_status (Service *service, ServiceStatus status) {

	if (service) {
		service->status = status;
	}

}

// static ServiceStatus balancer_service_get_status (Service *service) {

// 	ServiceStatus retval = SERVICE_STATUS_NONE;

// 	if (service) {
// 		retval =  service->status;
// 	}

// 	return retval;

// }

static inline int balancer_get_next_service_round_robin (Balancer *balancer) {

	balancer->next_service += 1;
	if (balancer->next_service >= balancer->n_services)
		balancer->next_service = 0;

	return balancer->next_service;

}

static int balancer_get_next_service (Balancer *balancer) {

	int retval = -1;

	pthread_mutex_lock (balancer->mutex);

	switch (balancer->type) {
		case BALANCER_TYPE_ROUND_ROBIN: {
			int count = 0;
			do {
				retval = balancer_get_next_service_round_robin (balancer);
				if (balancer->services[retval]->status == SERVICE_STATUS_WORKING) break;
				else count += 1;
			} while (count < balancer->n_services);

			if (count >= balancer->n_services) retval = -1;
		} break;

		default: break;
	}

	pthread_mutex_unlock (balancer->mutex);

	return retval;

}

// registers a new service to the load balancer
// a dedicated connection will be created when the balancer starts to handle traffic to & from the service
// returns 0 on success, 1 on error
u8 balancer_service_register (
	Balancer *balancer,
	const char *ip_address, u16 port
) {

	u8 retval = 1;

	if (balancer && ip_address) {
		if ((balancer->next_service + 1) <= balancer->n_services) {
			Service *service = balancer_service_create ();
			if (service) {
				Connection *connection = client_connection_create (
					balancer->client,
					ip_address, port,
					PROTOCOL_TCP, false
				);

				if (connection) {
					service->connection = connection;

					char name[64] = { 0 };
					snprintf (name, 64, "service-%d", balancer->next_service);

					connection_set_name (connection, name);
					connection_set_max_sleep (connection, 30);

					connection_set_custom_receive (
						connection, 
						balancer_client_receive,
						balancer_service_aux_new (balancer, service),
						balancer_service_aux_delete
					);

					balancer->services[balancer->next_service] = service;
					balancer->next_service += 1;

					retval = 0;
				}
			}
		}
	}

	return retval;

}

// sets the service's name
void balancer_service_set_name (Service *service, const char *name) {

	if (service && name) {
		connection_set_name (service->connection, name);
	}

}

// sets the time (in secs) to wait to attempt a reconnection whenever the service disconnects
// the default value is 20 secs
void balancer_service_set_reconnect_wait_time (Service *service, unsigned int wait_time) {

	if (service) service->reconnect_wait_time = wait_time;

}

// sends a test message to the service & waits for the request
static u8 balancer_service_test (Balancer *balancer, Service *service) {

	u8 retval = 1;

	Packet *packet = packet_generate_request (PACKET_TYPE_TEST, 0, NULL, 0);
	if (packet) {
		// packet_set_network_values (packet, balancer->cerver, balancer->client, service, NULL);
		if (!client_request_to_cerver (balancer->client, service->connection, packet)) {
			retval = 0;
		}

		else {
			char *status = c_string_create ("Failed to send test request to %s", service->connection->name->str);
			if (status) {
				cerver_log_error (status);
				free (status);
			}
		}

		packet_delete (packet);
	}

	return retval;

}

// connects to the service & sends a test packet to check its ability to handle requests
// returns 0 on success, 1 on error
static u8 balancer_service_connect (Balancer *balancer, Service *service) {

	u8 retval = 1;

	balancer_service_set_status (service, SERVICE_STATUS_CONNECTING);

	if (!client_connect_to_cerver (balancer->client, service->connection)) {
		char *status = c_string_create ("Connected to %s", service->connection->name->str);
		if (status) {
			cerver_log_success (status);
			free (status);
		}

		// we are ready to test service handler
		balancer_service_set_status (service, SERVICE_STATUS_READY);

		// send test message to service
		if (!balancer_service_test (balancer, service)) {
			if (!client_connection_start (balancer->client, service->connection)) {
				// the service is able to handle packets
				balancer_service_set_status (service, SERVICE_STATUS_WORKING);

				retval = 0;
			}
		}
	}

	else {
		char *status = c_string_create ("Failed to connect to %s", service->connection->name->str);
		if (status) {
			cerver_log_error (status);
			free (status);
		}

		balancer_service_set_status (service, SERVICE_STATUS_UNAVAILABLE);
	}

	return retval;

}

static void *balancer_service_reconnect_thread (void *bs_ptr) {

	BalancerService *bs = (BalancerService *) bs_ptr;

	Balancer *balancer = bs->balancer;
	Service *service = bs->service;

	(void) connection_init (service->connection);

	do {
		sleep (service->reconnect_wait_time);

		char *status = c_string_create (
			"Attempting connection to balancer %s service %s",
			balancer->name->str, service->connection->name->str
		);

		if (status) {
			cerver_log_debug (status);
			free (status);
		}

	} while (balancer_service_connect (balancer, service));

	return NULL;

}

#pragma endregion

#pragma region start

static u8 balancer_start_check (Balancer *balancer) {

	u8 retval = 1;

	if (balancer->next_service) {
		if (balancer->next_service == balancer->n_services) {
			// dirty fix to use first service on the first loop
			balancer->next_service = balancer->n_services;

			retval = 0;
		}

		else {
			char *status = c_string_create (
				"Balancer registered services doesn't match the configured number: %d != %d",
				balancer->next_service, balancer->n_services
			);

			if (status) {
				cerver_log_error (status);
				free (status);
			}
		}
	}

	else {
		cerver_log_error ("No service has been registered to the load balancer!");
	}

	return retval;

}

static u8 balancer_start_client (Balancer *balancer) {

	u8 errors = 0;

	for (unsigned int i = 0; i < balancer->n_services; i++) {
		errors |= balancer_service_connect (balancer, balancer->services[i]);
	}

	return errors;

}

static u8 balancer_start_cerver (Balancer *balancer) {

	u8 errors = 0;

	if (cerver_start (balancer->cerver)) {
		cerver_log_error ("Failed to start load balancer's cerver!");
		errors = 1;
	}

	return errors;

}

// starts the load balancer by first connecting to the registered services
// and checking their ability to handle requests
// then the cerver gets started to enable client connections
// returns 0 on success, 1 on error
u8 balancer_start (Balancer *balancer) {

	u8 retval = 1;

	if (balancer) {
		if (!balancer_start_check (balancer)) {
			if (!balancer_start_client (balancer)) {
				retval = balancer_start_cerver (balancer);
			}
		}
	}

	return retval;

}

#pragma endregion

#pragma region route

// routes the received packet to a service to be handled
// first sends the packet header with the correct sock fd
// if any data, it is forwarded from one sock fd to another using splice ()
void balancer_route_to_service (
	CerverReceive *cr,
	Balancer *balancer, Connection *connection,
	PacketHeader *header
) {

	int selected_service = balancer_get_next_service (balancer);
	if (selected_service >= 0) {
		Service *service = balancer->services[selected_service];

		header->sock_fd = connection->socket->sock_fd;

		size_t sent = 0;
		if (!packet_route_between_connections (
			connection, service->connection,
			header, &sent
		)) {
			char *status = c_string_create (
				"Routed %ld between %d (original) -> %d (%s)",
				sent,
				connection->socket->sock_fd, 
				service->connection->socket->sock_fd, service->connection->name->str
			);

			if (status) {
				cerver_log_debug (status);
				free (status);
			}
		}

		else {
			#ifdef BALANCER_DEBUG
			char *status = c_string_create (
				"Packet routing between %d (original) -> %d (%s) has failed!",
				connection->socket->sock_fd, 
				service->connection->socket->sock_fd, service->connection->name->str
			);

			if (status) {
				cerver_log_error (status);
				free (status);
			}
			#endif
		}
	}

	else {
		cerver_log_warning ("No available services to handle packets!");

		error_packet_generate_and_send (
			CERVER_ERROR_PACKET_ERROR, "Services unavailable",
			balancer->cerver, balancer->client, connection
		);

		if (header->packet_size > sizeof (PacketHeader)) {
			// consume data from socket to get next packet
			balancer_receive_consume_from_connection (cr, header->packet_size - sizeof (PacketHeader));
		}

		balancer->stats->unhandled_packets += 1;
		balancer->stats->unhandled_bytes += header->packet_size;
	}

}

#pragma endregion

#pragma region handler

static void balancer_client_receive_handle_failed (
	BalancerService *bs, 
	Client *client, Connection *connection
);

// consume from the service's connection socket until the next packet header
// returns 0 on success, 1 on error
static u8 balancer_client_consume_from_service (BalancerService *bs, PacketHeader *header) {

	u8 retval = 1;

	if (header->packet_size > sizeof (PacketHeader)) {
		char buffer[SERVICE_CONSUME_BUFFER_SIZE] = { 0 };

		size_t data_size = header->packet_size - sizeof (PacketHeader);

		size_t to_read = 0;
		size_t rc = 0;

		do {
			to_read = data_size >= SERVICE_CONSUME_BUFFER_SIZE ? SERVICE_CONSUME_BUFFER_SIZE : data_size;
			rc = recv (bs->service->connection->socket->sock_fd, buffer, to_read, 0);
			if (rc <= 0) {
				// #ifdef BALANCER_DEBUG
				snprintf (
					buffer, SERVICE_CONSUME_BUFFER_SIZE, 
					"balancer_client_consume_from_service () - rc <= 0 - service %s", 
					bs->service->connection->name->str
				);
				cerver_log_warning (buffer);
				// #endif

				// end the connection & flag the service as unavailable
				balancer_client_receive_handle_failed (
					bs, 
					bs->balancer->client, bs->service->connection
				);
				break;
			}

			data_size -= to_read;
		} while (data_size <= 0);

		if (!data_size) retval = 0;
	}

	return retval;

}

// TODO: update stats
// route the service's response back to the original client
static void balancer_client_route_response (
	BalancerService *bs,
	Client *client, Connection *connection,
	PacketHeader *header
) {

	// get the original connection by the sockfd from the packet header
	Client *original_client = client_get_by_sock_fd (bs->balancer->cerver, header->sock_fd);
	if (original_client) {
		Connection *original_connection = connection_get_by_sock_fd_from_client (original_client, header->sock_fd);
		if (original_connection) {
			size_t sent = 0;
			if (!packet_route_between_connections (
				connection, original_connection,
				header, &sent
			)) {
				char *status = c_string_create (
					"Routed %ld between %d (%s) -> %d (original)",
					sent,
					connection->socket->sock_fd, connection->name->str,
					original_connection->socket->sock_fd
				);

				if (status) {
					cerver_log_debug (status);
					free (status);
				}
			}

			else {
				#ifdef BALANCER_DEBUG
				char *status = c_string_create (
					"Packet routing between %d (%s) -> %d (original) has failed!",
					connection->socket->sock_fd, connection->name->str,
					original_connection->socket->sock_fd
				);

				if (status) {
					cerver_log_error (status);
					free (status);
				}
				#endif
			}
		}

		else {
			char *status = c_string_create (
				"balancer_client_route_response () - unable to find CONNECTION with sock fd %d", 
				header->sock_fd
			);

			if (status) {
				cerver_log_error (status);
				free (status);
			}

			// TODO: consume data from socket to get next packet
		}
	}

	else {
		char *status = c_string_create (
			"balancer_client_route_response () - unable to find CLIENT with sock fd %d", 
			header->sock_fd
		);

		if (status) {
			cerver_log_error (status);
			free (status);
		}

		// TODO: consume data from socket to get next packet
	}

}

static void balancer_client_receive_success (
	BalancerService *bs,
	Client *client, Connection *connection,
	PacketHeader *header
) {

	bs->service->stats->received_packets[
		header->packet_type < PACKET_TYPE_BAD ? header->packet_type : PACKET_TYPE_BAD
	] += 1;

	switch (header->packet_type) {
		case PACKET_TYPE_CLIENT:
			balancer_client_consume_from_service (bs, header);
			break;

		case PACKET_TYPE_AUTH:
			balancer_client_consume_from_service (bs, header);
			break;

		// TODO: handle whether the response was sent by the handler or by a client
		case PACKET_TYPE_TEST: {
			client->stats->received_packets->n_test_packets += 1;
			connection->stats->received_packets->n_test_packets += 1;
			cerver_log_msg (stdout, LOG_TYPE_TEST, LOG_TYPE_HANDLER, "Got a test packet from service");
		} break;

		// only route packets of these types back to original client
		case PACKET_TYPE_CERVER:
		case PACKET_TYPE_ERROR:
		case PACKET_TYPE_REQUEST:
		case PACKET_TYPE_GAME:
		case PACKET_TYPE_APP:
		case PACKET_TYPE_APP_ERROR:
		case PACKET_TYPE_CUSTOM: {
			balancer_client_route_response (
				bs,
				client, connection,
				header
			);
		} break;

		case PACKET_TYPE_NONE:
		default: {
			client->stats->received_packets->n_bad_packets += 1;
			connection->stats->received_packets->n_bad_packets += 1;
			#ifdef BALANCER_DEBUG
			cerver_log_msg (stdout, LOG_TYPE_WARNING, LOG_TYPE_HANDLER, "Got a packet of unknown type.");
			#endif

			balancer_client_consume_from_service (bs, header);
		} break;
	}

	// FIXME: update stats
	// client->stats->n_receives_done += 1;
	// client->stats->total_bytes_received += rc;

	// connection->stats->n_receives_done += 1;
	// connection->stats->total_bytes_received += rc;

}

// handles a failed receive from the client's connection
// ends the connection & flags the service as unavailable to attempt a reconnection
static void balancer_client_receive_handle_failed (
	BalancerService *bs, 
	Client *client, Connection *connection
) {

	(void) client_connection_stop (client, connection);

	char *status = c_string_create (
		"Balancer %s - service %s has disconnected!\n",
		bs->balancer->name->str, bs->service->connection->name->str
	);

	if (status) {
		printf ("\n");
		cerver_log_warning (status);
		free (status);
	}

	balancer_service_set_status (bs->service, SERVICE_STATUS_DISCONNECTED);

	pthread_t thread_id = 0;
	if (thread_create_detachable (&thread_id, balancer_service_reconnect_thread, bs)) {
		char *status = c_string_create (
			"Failed to create reconnect thread for balancer %s service %s",
			bs->balancer->name->str, bs->service->connection->name->str
		);

		if (status) {
			cerver_log_error (status);
			free (status);
		}
	}

}

// custom receive method for packets comming from the services
// returns 0 on success handle, 1 if any error ocurred and must likely the connection was ended
static u8 balancer_client_receive (void *custom_data_ptr) {

	unsigned int retval = 1;

	ConnectionCustomReceiveData *custom_data = (ConnectionCustomReceiveData *) custom_data_ptr;

	if (custom_data->client && custom_data->connection) {
		char header_buffer[sizeof (PacketHeader)] = { 0 };
		ssize_t rc = recv (custom_data->connection->socket->sock_fd, header_buffer, sizeof (PacketHeader), MSG_DONTWAIT);

		switch (rc) {
			case -1: {
				if (errno != EWOULDBLOCK) {
					#ifdef BALANCER_DEBUG
					char *s = c_string_create ("balancer_client_receive () - rc < 0 - sock fd: %d", custom_data->connection->socket->sock_fd);
					if (s) {
						cerver_log_msg (stderr, LOG_TYPE_ERROR, LOG_TYPE_HANDLER, s);
						free (s);
					}
					perror ("Error");
					#endif
					
					balancer_client_receive_handle_failed (
						(BalancerService *) custom_data->args, 
						custom_data->client, custom_data->connection
					);
				}
			} break;

			case 0: {
				#ifdef BALANCER_DEBUG
				char *s = c_string_create ("balancer_client_receive () - rc == 0 - sock fd: %d", custom_data->connection->socket->sock_fd);
				if (s) {
					cerver_log_msg (stdout, LOG_TYPE_DEBUG, LOG_TYPE_HANDLER, s);
					free (s);
				}
				#endif

				balancer_client_receive_handle_failed (
					(BalancerService *) custom_data->args, 
					custom_data->client, custom_data->connection
				);
			} break;

			default: {
				char *s = c_string_create ("Connection %s rc: %ld", custom_data->connection->name->str, rc);
				if (s) {
					cerver_log_msg (stdout, LOG_TYPE_DEBUG, LOG_TYPE_CLIENT, s);
					free (s);
				}

				balancer_client_receive_success (
					(BalancerService *) custom_data->args,
					custom_data->client, custom_data->connection,
					(PacketHeader *) header_buffer
				);

				retval = 0;
			} break;
		}
	}

	return retval;

}

#pragma endregion

#pragma region end

// first ends and destroys the balancer's internal cerver
// then disconnects from each of the registered services
// last frees any balancer memory left
// returns 0 on success, 1 on error
u8 balancer_teardown (Balancer *balancer) {

	u8 retval = 1;

	if (balancer) {
		(void) cerver_teardown (balancer->cerver);
		balancer->cerver = NULL;

		client_teardown (balancer->client);
		balancer->client = NULL;

		balancer_delete (balancer);
	}

	return retval;

}

#pragma endregion