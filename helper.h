#ifndef HELPER_H__
#define HELPER_H__

#include <sqlite3.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "cJSON/cJSON.h"

int insert_sql(sqlite3* db, const char* key, const char* word, sqlite3_stmt* s) {
	sqlite3_clear_bindings(s);

	int rc = sqlite3_bind_text(s, 1, key, strlen(key), NULL);
	if (rc) {
		fprintf(stderr, "error: Bind %s to %d failed, %s\n", key, rc, sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_bind_text(s, 2, word, strlen(word), NULL);
	if (rc) {
		fprintf(stderr, "error: Bind %s to %d failed, %s\n", key, rc, sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_step(s);
	if (SQLITE_DONE != rc) {
		fprintf(stderr, "insert statement didn't return DONE (%i): %s\n", rc, sqlite3_errmsg(db));
		return rc;
	}
	sqlite3_reset(s);

	return rc;
}
bool parse_json(sqlite3 *db,sqlite3_stmt* s_insert,pthread_mutex_t *mutex,const char * word,const char *buf,char *buf_body) {
	cJSON* json = cJSON_Parse(buf);
	if (json == NULL) {
		const char* error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			printf("[E]: %s\n", error_ptr);
			cJSON_Delete(json);
			return false;
		}
	}
	const cJSON* basic = cJSON_GetObjectItem(json, "basic");
	const cJSON* explains = cJSON_GetObjectItem(basic, "explains");
	const cJSON* explain = NULL;
	cJSON_ArrayForEach(explain, explains) {
		strcat(buf_body, explain->valuestring);
		strcat(buf_body, "\n");
	};
	const cJSON* web = cJSON_GetObjectItem(json, "web");
	const cJSON* w = NULL;
	cJSON_ArrayForEach(w, web) {
		strcat(buf_body, cJSON_GetObjectItem(w, "key")->valuestring);
		const cJSON* values = cJSON_GetObjectItem(w, "value");
		const cJSON* value = NULL;
		strcat(buf_body, " ");
		cJSON_ArrayForEach(value, values) {
			strcat(buf_body, value->valuestring);
			strcat(buf_body, ",");
		}
		buf_body[strlen(buf_body) - 1] = '\n';
	}

	if (strlen(buf_body) > 0) {
		//pthread_mutex_lock(mutex);
		insert_sql(db, word, buf_body, s_insert);
		//pthread_mutex_unlock(mutex);
		return true;
	}
	printf("[E]: %s\n", "Result is empty.");
	return false;
}

#endif