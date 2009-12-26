#ifndef __PARSER_H__
#define __PARSER_H__

/* Frame information */
typedef struct frame_info {
	int level;
	char *addr;
	char *func;
	char *args;
	char *file;
	char *fullname;
	char *line;
	char *from;
} frame_info_t;

/* Function prototypes */

#endif /* __PARSER_H__ */
