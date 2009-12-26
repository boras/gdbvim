#include <stdio.h>
#include <stdlib.h>
#include "parsetree.h"

list_t *create_list(list_type_t ltype, void *data)
{
	list_t *list_ptr;

	if (!(list_ptr = (list_t *)calloc(1, sizeof(list_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	list_ptr->ltype = ltype;
	switch (ltype) {
	case RESULT:
		list_ptr->data.result_ptr = (result_t *)data;
		break;
	case VALUE:
		list_ptr->data.value_ptr = (value_t *)data;
		break;
	}

	return list_ptr;
}

tuple_t *create_tuple(result_t *result_ptr)
{
	tuple_t *tuple_ptr;

	if (!(tuple_ptr = (tuple_t *)calloc(1, sizeof(tuple_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	tuple_ptr->result_ptr = result_ptr;

	return tuple_ptr;
}

value_t *create_value(value_type_t vtype, void *data)
{
	value_t *val_ptr;

	if (!(val_ptr = (value_t *)calloc(1, sizeof(value_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	val_ptr->vtype = vtype;
	switch (vtype) {
	case CSTRING:
		val_ptr->data.cstr = (char *)data;
		break;
	case TUPLE:
		val_ptr->data.tuple_ptr = (tuple_t *)data;
		break;
	case LIST:
		val_ptr->data.list_ptr = (list_t *)data;
		break;
	}

	return val_ptr;
}

result_t *create_result(char *identifier, value_t *val_ptr)
{
	result_t *res;

	if (!(res = (result_t *)calloc(1, sizeof(result_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	res->identifier = identifier;
	res->val_ptr = val_ptr;

	return res;
}

value_t *append_value(value_t *head, value_t *new)
{
	value_t *v = head;

	while (v->next)
		v = v->next;
	v->next = new;

	return head;
}

result_t *append_result(result_t *head, result_t *new)
{
	result_t *r = head;

	if (!head)
		return head = new;

	while (r->next)
		r = r->next;
	r->next = new;

	return head;
}

async_record_t *create_async_record(async_type_t atype, char *token,
				    async_output_t *async_out_ptr)
{
	async_record_t *async_rec_ptr;

	async_rec_ptr = (async_record_t *)malloc(sizeof(async_record_t));
	if (!async_rec_ptr) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}
	async_rec_ptr->atype = atype;
	async_rec_ptr->token = token;
	async_rec_ptr->async_out_ptr = async_out_ptr;

	return async_rec_ptr;
}

stream_record_t *create_stream_record(stream_type_t stype, char *str)
{
	stream_record_t *stream_rec_ptr;

	stream_rec_ptr = (stream_record_t *)malloc(sizeof(stream_record_t));
	if (!stream_rec_ptr) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}
	stream_rec_ptr->stype = stype;
	stream_rec_ptr->cstr = str;

	return stream_rec_ptr;
}

async_output_t *create_async_output(async_class_t aclass, result_t *result_ptr)
{
	async_output_t *ao;

	ao = (async_output_t *)calloc(1, sizeof(async_output_t));
	if (!ao) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}
	ao->aclass = aclass;
	ao->result_ptr = result_ptr;

	return ao;
}

result_record_t *create_result_record(char *token, result_class_t rclass,
				      result_t *result_ptr)
{
	result_record_t *rr;

	rr = (result_record_t *)calloc(1, sizeof(result_record_t));
	if (!rr) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}
	rr->token = token;
	rr->rclass = rclass;
	rr->result_ptr = result_ptr;

	return rr;
}

oob_record_t *append_oob_record(oob_record_t *head, oob_record_t *new)
{
	oob_record_t *or = head;

	if (!head)
		return head = new;

	while (or->next)
		or = or->next;
	or->next = new;

	return head;
}

oob_record_t *create_oob_record(record_type_t rtype, void *data)
{
	oob_record_t *rec;

	if (!(rec = (oob_record_t *)calloc(1, sizeof(oob_record_t)))) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	if (rtype == STREAM_RECORD)
		rec->r.stream_rec_ptr = (stream_record_t *)data;
	else if (rtype == ASYNC_RECORD)
		rec->r.async_rec_ptr = (async_record_t *)data;
	rec->rtype = rtype;

	return rec;
}

gdbmi_output_t *append_gdbmi_output(gdbmi_output_t *head, gdbmi_output_t *new)
{
	gdbmi_output_t *gout = head;

	while (gout->next)
		gout = gout->next;
	gout->next = new;

	return head;
}

gdbmi_output_t *create_gdbmi_output(oob_record_t *oob_rec_ptr,
				    result_record_t *result_rec_ptr)
{
	gdbmi_output_t *go;

	go = (gdbmi_output_t *)calloc(1, sizeof(gdbmi_output_t));
	if (!go) {
		fprintf(stderr, "Cannot allocate memory\n");
		return NULL;
	}

	go->oob_rec_ptr = oob_rec_ptr;
	go->result_rec_ptr = result_rec_ptr;

	return go;
}

void destroy_list(list_t *list_ptr)
{
	/* If it is not empty */
	if (list_ptr) {
		switch (list_ptr->ltype) {
		case RESULT:
			{
				result_t *r = list_ptr->data.result_ptr;
				destroy_result_list(r);
			}
			break;
		case VALUE:
			{
				value_t *v = list_ptr->data.value_ptr;
				destroy_value_list(v);
			}
			break;
		}
		free(list_ptr);
	}
}

void print_list(list_t *list_ptr)
{
	if (!list_ptr)
		printf("[]");
	else {
		printf("[");
		switch (list_ptr->ltype) {
		case RESULT:
			{
				result_t *r = list_ptr->data.result_ptr;
				print_result(r);
				if (r->next)
					print_result_list(r->next);
			}
			break;
		case VALUE:
			{
				value_t *v = list_ptr->data.value_ptr;
				print_value(v);
				if (v->next)
					print_value_list(v->next);
			}
			break;
		}
		printf("]");
	}
}

void destroy_tuple(tuple_t *tuple_ptr)
{
	/* If it is not empty */
	if (tuple_ptr) {
		destroy_result_list(tuple_ptr->result_ptr);
		free(tuple_ptr);
	}
}

void print_tuple(tuple_t *tuple_ptr)
{
	if (!tuple_ptr)
		printf("{}");
	else {
		printf("{");
		print_result(tuple_ptr->result_ptr);
		if (tuple_ptr->result_ptr->next)
			print_result_list(tuple_ptr->result_ptr->next);
		printf("}");
	}
}

void destroy_value(value_t *value_ptr)
{
	switch (value_ptr->vtype) {
	case CSTRING:
		free(value_ptr->data.cstr);
		break;
	case TUPLE:
		destroy_tuple(value_ptr->data.tuple_ptr);
		break;
	case LIST:
		destroy_list(value_ptr->data.list_ptr);
		break;
	}
	free(value_ptr);
}

void print_value(value_t *value_ptr)
{
	switch (value_ptr->vtype) {
	case CSTRING:
		printf("%s", value_ptr->data.cstr);
		break;
	case TUPLE:
		print_tuple(value_ptr->data.tuple_ptr);
		break;
	case LIST:
		print_list(value_ptr->data.list_ptr);
		break;
	}
}

void destroy_value_list(value_t *value_ptr)
{
	value_t *cur = value_ptr;
	value_t *next = cur;

	while (next) {
		next = cur->next;
		destroy_value(cur);
		cur = next;
	}
}

void print_value_list(value_t *value_ptr)
{
	value_t *v = value_ptr;

	while (v != NULL) {
		putchar(',');
		print_value(v);
		v = v->next;
	}
}

void destroy_result(result_t *result_ptr)
{
	free(result_ptr->identifier);
	destroy_value(result_ptr->val_ptr);
	free(result_ptr);
}

void print_result(result_t *result_ptr)
{
	printf("%s=", result_ptr->identifier);
	print_value(result_ptr->val_ptr);
}

void destroy_result_list(result_t *result_ptr)
{
	result_t *cur = result_ptr;
	result_t *next = cur;

	while (next) {
		next = cur->next;
		destroy_result(cur);
		cur = next;
	}
}

void print_result_list(result_t *result_ptr)
{
	result_t *r = result_ptr;

	while (r != NULL) {
		putchar(',');
		print_result(r);
		r = r->next;
	}

}

void destroy_async_output(async_output_t *async_out_ptr)
{
	destroy_result_list(async_out_ptr->result_ptr);
	free(async_out_ptr);
}

void print_async_output(async_output_t *async_out_ptr)
{
	switch (async_out_ptr->aclass) {
	case ASYNC_STOPPED:
		printf("stopped");
		break;
	}
	print_result_list(async_out_ptr->result_ptr);
	putchar('\n');
}

void destroy_async_record(async_record_t *async_rec_ptr)
{
	if (async_rec_ptr->token)
		free(async_rec_ptr->token);
	destroy_async_output(async_rec_ptr->async_out_ptr);
	free(async_rec_ptr);
}

void print_async_record(async_record_t *async_rec_ptr)
{
	if (async_rec_ptr->token)
		printf("%s", async_rec_ptr->token);
	if (async_rec_ptr->atype == EXEC_ASYNC)
		putchar('*');
	else if (async_rec_ptr->atype == STATUS_ASYNC)
		putchar('+');
	else if (async_rec_ptr->atype == NOTIFY_ASYNC)
		putchar('=');
	print_async_output(async_rec_ptr->async_out_ptr);
}

void destroy_stream_record(stream_record_t *stream_rec_ptr)
{
	free(stream_rec_ptr->cstr);
	free(stream_rec_ptr);
}

void print_stream_record(stream_record_t *stream_rec_ptr)
{
	if (stream_rec_ptr->stype == CONSOLE_STREAM)
		printf("console_stream_output = %s\n", stream_rec_ptr->cstr);
	else if (stream_rec_ptr->stype == TARGET_STREAM)
		printf("target_stream_output = %s\n", stream_rec_ptr->cstr);
	else if (stream_rec_ptr->stype == LOG_STREAM)
		printf("log_stream_output = %s\n", stream_rec_ptr->cstr);
}

void destroy_result_record(result_record_t *result_rec_ptr)
{
	if (result_rec_ptr) {
		if (result_rec_ptr->token)
			free(result_rec_ptr->token);
		destroy_result_list(result_rec_ptr->result_ptr);
		free(result_rec_ptr);
	}
}

void print_result_record(result_record_t *result_rec_ptr)
{
	if (result_rec_ptr) {
		if (result_rec_ptr->token)
			printf("%s", result_rec_ptr->token);
		putchar('^');
		switch (result_rec_ptr->rclass) {
		case RESULT_DONE:
			printf("done");
			break;
		case RESULT_RUNNING:
			printf("running");
			break;
		case RESULT_CONNECTED:
			printf("connected");
			break;
		case RESULT_ERROR:
			printf("error");
			break;
		case RESULT_EXIT:
			printf("exit");
			break;
		}
		print_result_list(result_rec_ptr->result_ptr);
		putchar('\n');
	}
}

void destroy_oob_record(oob_record_t *oob_rec_ptr)
{
	oob_record_t *cur = oob_rec_ptr;
	oob_record_t *prev;

	while (cur) {
		if (cur->rtype == STREAM_RECORD)
			destroy_stream_record(cur->r.stream_rec_ptr);
		else if (cur->rtype == ASYNC_RECORD)
			destroy_async_record(cur->r.async_rec_ptr);
		prev = cur;
		cur = cur->next;
		free(prev);
	}
}

void print_oob_record(oob_record_t *oob_rec_ptr)
{
	oob_record_t *cur = oob_rec_ptr;

	while (cur != NULL) {
		if (cur->rtype == STREAM_RECORD)
			print_stream_record(cur->r.stream_rec_ptr);
		else if (cur->rtype == ASYNC_RECORD)
			print_async_record(cur->r.async_rec_ptr);
		cur = cur->next;
	}
}

void destroy_gdbmi_output(void)
{
	gdbmi_output_t *cur = gdbmi_out_ptr;
	gdbmi_output_t *prev;

	if (cur) {
		do {
			destroy_oob_record(cur->oob_rec_ptr);
			destroy_result_record(cur->result_rec_ptr);
			prev = cur;
			cur = cur->next;
			free(prev);
		} while (cur);
	}
}

void print_gdbmi_output(void)
{
	gdbmi_output_t *cur = gdbmi_out_ptr;

	if (cur) {
		do {
			print_oob_record(cur->oob_rec_ptr);
			print_result_record(cur->result_rec_ptr);
		} while (cur = cur->next);
	}
}

void yyerror(const char *str)
{
	printf("%s: %s\n", __FUNCTION__, str);
}
