#ifndef DB_H
#define DB_H

#include <stdint.h>

#include "json.h"
#include "db_types.h"

#define DEF_DB_ADDRESS "localhost"
#define DEF_DB_PORT "5984"
#define DEF_DB_USERNAME ""
#define DEF_DB_PASSWORD ""
#define DEF_DB_BASE_NAME "penndb1"
#define DEF_DB_VIEWDOC "_design/view_doc/_view"

int get_new_id(char* newid);

int parse_fec_hw(JsonNode* value,mb_t* mb);
int parse_fec_debug(JsonNode* value,mb_t* mb);
int swap_fec_db(mb_t* mb);
int parse_mtc(JsonNode* value,mtc_t* mtc);

int create_fec_db_doc(int crate, int card, JsonNode** doc,fd_set *thread_fdset);
int add_ecal_test_results(JsonNode *fec_doc, JsonNode *test_doc);
int post_fec_db_doc(int crate, int slot, JsonNode *doc);

int post_debug_doc(int crate, int card, JsonNode* doc, fd_set *thread_fdset);
int post_debug_doc_with_id(int crate, int card, char *id, JsonNode* doc, fd_set *thread_fdset);
int post_debug_doc_mem_test(int crate, int card, JsonNode* doc, fd_set *thread_fdset);
int post_ecal_doc(uint32_t crate_mask, uint16_t *slot_mask, char *logfile, char *id);

#endif

