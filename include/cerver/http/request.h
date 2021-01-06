#ifndef _CERVER_HTTP_REQUEST_H_
#define _CERVER_HTTP_REQUEST_H_

#include "cerver/types/string.h"

#include "cerver/collections/dlist.h"

#include "cerver/config.h"

#include "cerver/http/multipart.h"

#define REQUEST_METHOD_UNDEFINED		5

#ifdef __cplusplus
extern "C" {
#endif

#define REQUEST_METHOD_MAP(XX)			\
	XX(0,  DELETE,      DELETE)       	\
	XX(1,  GET,         GET)          	\
	XX(2,  HEAD,        HEAD)         	\
	XX(3,  POST,        POST)         	\
	XX(4,  PUT,         PUT)          	\

typedef enum RequestMethod {

	#define XX(num, name, string) REQUEST_METHOD_##name = num,
	REQUEST_METHOD_MAP(XX)
	#undef XX

} RequestMethod;

// returns a string version of the HTTP request method
CERVER_PUBLIC const char *http_request_method_str (
	const RequestMethod request_method
);

#define REQUEST_HEADER_MAP(XX)													\
	XX(0,  ACCEPT,      					Accept)								\
	XX(1,  ACCEPT_CHARSET,					Accept-Charset)						\
	XX(2,  ACCEPT_ENCODING,      			Accept-Encoding)					\
	XX(3,  ACCEPT_LANGUAGE,      			Accept-Language)					\
	XX(4,  ACCESS_CONTROL_REQUEST_HEADERS,	Access-Control-Request-Headers)		\
	XX(5,  AUTHORIZATION,					Authorization)						\
	XX(6,  CACHE_CONTROL,					Cache-Control)						\
	XX(7,  CONNECTION,						Connection)							\
	XX(8,  CONTENT_LENGTH,					Content-Length)						\
	XX(9,  CONTENT_TYPE,					Content-Type)						\
	XX(10,  COOKIE,							Cookie)								\
	XX(11,  DATE,							Date)								\
	XX(12,  EXPECT,							Expect)								\
	XX(13,  HOST,							Host)								\
	XX(14,  ORIGIN,							Origin)								\
	XX(15,  PROXY_AUTHORIZATION,			Proxy-Authorization)				\
	XX(16,  UPGRADE,						Upgrade)							\
	XX(17,  USER_AGENT,						User-Agent)							\
	XX(18,  WEB_SOCKET_KEY,					Sec-WebSocket-Key)					\
	XX(19,  WEB_SOCKET_VERSION,				Sec-WebSocket-Version)				\
	XX(32,  INVALID,      					Undefined)

typedef enum RequestHeader {

	#define XX(num, name, string) REQUEST_HEADER_##name = num,
	REQUEST_HEADER_MAP(XX)
	#undef XX

} RequestHeader;

CERVER_PUBLIC const char *http_request_header_str (
	const RequestHeader header
);

#define REQUEST_HEADERS_MAX				19

#define REQUEST_HEADERS_SIZE			32

#define REQUEST_PARAMS_SIZE				8

struct _HttpRequest {

	RequestMethod method;

	String *url;
	String *query;

	// parsed query params (key-value pairs)
	DoubleList *query_params;

	unsigned int n_params;
	String *params[REQUEST_PARAMS_SIZE];

	RequestHeader next_header;
	String *headers[REQUEST_HEADERS_SIZE];

	// decoded data from jwt
	void *decoded_data;
	void (*delete_decoded_data)(void *);
	
	String *body;

	MultiPart *current_part;
	DoubleList *multi_parts;
	u8 n_files;
	u8 n_values;
	String *dirname;
	
	// body key-value pairs
	// parsed from x-www-form-urlencoded data
	DoubleList *body_values;

	bool keep_files;

};

typedef struct _HttpRequest HttpRequest;

CERVER_PUBLIC HttpRequest *http_request_new (void);

CERVER_PUBLIC void http_request_delete (
	HttpRequest *http_request
);

CERVER_PUBLIC HttpRequest *http_request_create (void);

// destroys any information related to the request
CERVER_PRIVATE void http_request_destroy (
	HttpRequest *http_request
);

CERVER_PUBLIC void http_request_headers_print (
	const HttpRequest *http_request
);

CERVER_PUBLIC void http_request_query_params_print (
	const HttpRequest *http_request
);

// returns the matching query param value for the specified key
// returns NULL if no match or no query params
CERVER_EXPORT const String *http_request_query_params_get_value (
	const HttpRequest *http_request, const char *key
);

// searches the request's multi parts values for one with matching key & type
// returns a constant Stirng that should not be deleted if found, NULL if not match
CERVER_EXPORT const MultiPart *http_request_multi_parts_get (
	const HttpRequest *http_request, const char *key
);

// searches the request's multi parts values for a value with matching key
// returns a constant String that should not be deleted if found, NULL if not match
CERVER_EXPORT const String *http_request_multi_parts_get_value (
	const HttpRequest *http_request, const char *key
);

// searches the request's multi parts values for a filename with matching key
// returns a constant c string that should not be deleted if found, NULL if not match
CERVER_EXPORT const char *http_request_multi_parts_get_filename (
	const HttpRequest *http_request, const char *key
);

// searches the request's multi parts values for a saved filename with matching key
// returns a constant c string that should not be deleted if found, NULL if not match
CERVER_EXPORT const char *http_request_multi_parts_get_saved_filename (
	const HttpRequest *http_request, const char *key
);

// returns a dlist with constant c strings values (that should not be deleted) with all the filenames from the request
// the dlist must be deleted using http_request_multi_parts_all_filenames_delete ()
CERVER_EXPORT DoubleList *http_request_multi_parts_get_all_filenames (
	const HttpRequest *http_request
);

// returns a dlist with constant c strings values (that should not be deleted) with all the saved filenames from the request
// the dlist must be deleted using http_request_multi_parts_all_filenames_delete ()
CERVER_EXPORT DoubleList *http_request_multi_parts_get_all_saved_filenames (
	const HttpRequest *http_request
);

// used to safely delete a dlist generated by 
// http_request_multi_parts_get_all_filenames () or http_request_multi_parts_get_all_saved_filenames ()
CERVER_EXPORT void http_request_multi_parts_all_filenames_delete (
	DoubleList *all_filenames
);

// signals that the files referenced by the request should be kept
// so they won't be deleted after the request has ended
// files only get deleted if the http cerver's uploads_delete_when_done
// is set to TRUE
CERVER_EXPORT void http_request_multi_part_keep_files (
	HttpRequest *http_request
);

// discards all the saved files from the multipart request
CERVER_PUBLIC void http_request_multi_part_discard_files (
	const HttpRequest *http_request
);

CERVER_PUBLIC void http_request_multi_parts_print (
	const HttpRequest *http_request
);

// search request's body values for matching value by key
// returns a constant String that should not be deleted if match, NULL if not found
CERVER_EXPORT const String *http_request_body_get_value (
	const HttpRequest *http_request, const char *key
);

#ifdef __cplusplus
}
#endif

#endif