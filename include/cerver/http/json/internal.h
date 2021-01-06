#ifndef _CERVER_HTTP_JSON_INTERNAL_H_
#define _CERVER_HTTP_JSON_INTERNAL_H_

#include "cerver/config.h"

#include "cerver/http/json/config.h"
#include "cerver/http/json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

extern json_t *json_incref (json_t *json);

/* do not call json_delete directly */
extern void json_delete (json_t *json);

extern void json_decref (json_t *json);

#if defined(__GNUC__) || defined(__clang__)
extern void json_decrefp (json_t **json);

#define json_auto_t json_t __attribute__((cleanup(json_decrefp)))
#endif

#ifdef __cplusplus
}
#endif

#endif