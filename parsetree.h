#ifndef __PARSETREE_H__
#define __PARSETREE_H__

/* stream record messages */
typedef enum stream_type {
	CONSOLE_STREAM,
	TARGET_STREAM,
	LOG_STREAM
} stream_type_t;

typedef struct stream_record {
	stream_type_t stype;
	char *cstr;
} stream_record_t;

/* async record messages */
typedef enum async_class {
	ASYNC_STOPPED,
} async_class_t;

struct result;

typedef struct async_output {
	async_class_t aclass;
	struct result *result_ptr;
} async_output_t;

typedef enum async_type {
	EXEC_ASYNC,
	STATUS_ASYNC,
	NOTIFY_ASYNC
} async_type_t;

typedef struct async_record {
	async_type_t atype;
	char *token;
	async_output_t *async_out_ptr;
} async_record_t;

/* oob record */
typedef enum record_type {
	STREAM_RECORD,
	ASYNC_RECORD
} record_type_t;

typedef struct oob_record {
	record_type_t rtype;
	union {
		stream_record_t *stream_rec_ptr;
		async_record_t *async_rec_ptr;
	} r;
	struct oob_record *next;
} oob_record_t;

/* Every command produces a result */
struct result;
struct value;

typedef enum list_type {
	RESULT,
	VALUE,
} list_type_t;

typedef struct list {
	list_type_t ltype;
	union {
		struct result *result_ptr;
		struct value *value_ptr;
	} data;
} list_t;

typedef struct tuple {
	struct result *result_ptr;
} tuple_t;

typedef enum value_type {
	CSTRING,
	TUPLE,
	LIST
} value_type_t;

typedef struct value {
	value_type_t vtype;
	union {
		char *cstr;
		struct tuple *tuple_ptr;
		struct list *list_ptr;
	} data;
	struct value *next;
} value_t;

typedef struct result {
	char *identifier;
	value_t *val_ptr;
	struct result *next;
} result_t;

typedef enum result_class {
	RESULT_DONE,
	RESULT_RUNNING,
	RESULT_CONNECTED,
	RESULT_ERROR,
	RESULT_EXIT
} result_class_t;

typedef struct result_record {
	char *token;
	result_class_t rclass;
	result_t *result_ptr;
} result_record_t;

/* Consists of zero or more oob records and of zero or one result record */
typedef struct gdbmi_output {
	oob_record_t *oob_rec_ptr;
	result_record_t *result_rec_ptr;
	struct gdbmi_output *next;
} gdbmi_output_t;

/* Global definitions */
gdbmi_output_t *gdbmi_out_ptr;

/* Function declarations */
list_t *create_list(list_type_t ltype, void *data);
void destroy_list(list_t *list_ptr);
void print_list(list_t *list_ptr);

tuple_t *create_tuple(result_t *result_ptr);
void destroy_tuple(tuple_t *tuple_ptr);
void print_tuple(tuple_t *tuple_ptr);

value_t *create_value(value_type_t vtype, void *data);
value_t *append_value(value_t *head, value_t *new);
void destroy_value(value_t *value_ptr);
void print_value(value_t *value_ptr);
void destroy_value_list(value_t *value_ptr);
void print_value_list(value_t *value_ptr);

result_t *create_result(char *identifier, value_t *val_ptr);
result_t *append_result(result_t *prev, result_t *new);
void destroy_result(result_t *result_ptr);
void print_result(result_t *result_ptr);
void destroy_result_list(result_t *result_ptr);
void print_result_list(result_t *result_ptr);

async_output_t *create_async_output(async_class_t aclass, result_t *result_ptr);
void destroy_async_output(async_output_t *async_out_ptr);
void print_async_output(async_output_t *async_out_ptr);

async_record_t *create_async_record(async_type_t atype, char *token,
				    async_output_t *async_out_ptr);
void destroy_async_record(async_record_t *async_rec_ptr);
void print_async_record(async_record_t *async_rec_ptr);

stream_record_t *create_stream_record(stream_type_t stype, char *str);
void destroy_stream_record(stream_record_t *stream_rec_ptr);
void print_stream_record(stream_record_t *stream_rec_ptr);

result_record_t *create_result_record(char *token, result_class_t rclass,
				      result_t *result_ptr);
void destroy_result_record(result_record_t *result_rec_ptr);
void print_result_record(result_record_t *result_rec_ptr);

oob_record_t *create_oob_record(record_type_t rtype, void *data);
oob_record_t *append_oob_record(oob_record_t *head, oob_record_t *new);
void destroy_oob_record(oob_record_t *oob_rec_ptr);
void print_oob_record(oob_record_t *oob_rec_ptr);

gdbmi_output_t *create_gdbmi_output(oob_record_t *oob_rec_ptr,
				    result_record_t *result_rec_ptr);
gdbmi_output_t *append_gdbmi_output(gdbmi_output_t *head, gdbmi_output_t *new);
void destroy_gdbmi_output(void);
void print_gdmi_output(void);

#endif /* __PARSETREE_H__ */
