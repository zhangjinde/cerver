#ifndef _CERVER_HTTP_JSON_INTERNAL_H_
#define _CERVER_HTTP_JSON_INTERNAL_H_

#include "cerver/config.h"

#include "cerver/http/json/config.h"
#include "cerver/http/json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

static CERVER_INLINE json_t *json_incref (json_t *json) {

	if (json && json->refcount != (size_t) - 1)
		JSON_INTERNAL_INCREF(json);
	
	return json;

}

/* do not call json_delete directly */
extern void json_delete (json_t *json);

static CERVER_INLINE void json_decref (json_t *json) {

	if (
		json
		&& json->refcount != (size_t) - 1
		&& JSON_INTERNAL_DECREF(json) == 0
	) {
		json_delete(json);
	}

}

#if defined(__GNUC__) || defined(__clang__)
static CERVER_INLINE void json_decrefp (json_t **json) {

	if (json) {
		json_decref(*json);
		*json = NULL;
	}

}

#define json_auto_t json_t __attribute__((cleanup(json_decrefp)))
#endif

#ifdef __cplusplus
}
#endif

#endif