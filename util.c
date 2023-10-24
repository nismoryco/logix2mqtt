
#include "logix2mqtt.h"

int sleep_ms(int ms)
{
	struct timeval tv;

	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms % 1000)*1000;

	return select(0, NULL, NULL, NULL, &tv);
}

int64_t time_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return ((int64_t)tv.tv_sec*1000) + ((int64_t)tv.tv_usec/1000);
}

void *my_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr != NULL) {
		memset(ptr, 0, size);
	}
	return ptr;
}

char *strlower(char * s)
{
	for (char *p=s; *p; p++) {
		*p=tolower(*p);
	}
	return s;
}

plc_data_type_t get_plc_data_type(const char * s)
{
	plc_data_type_t r = UNKNOWN;

	if (strcmp(s, "lint") == 0) {
		r = LINT;
	} else if (strcmp(s, "dint") == 0) {
		r = DINT;
	} else if (strcmp(s, "int") == 0) {
		r = INT;
	} else if (strcmp(s, "sint") == 0) {
		r = SINT;
	} else if (strcmp(s, "real") == 0) {
		r = REAL;
	} else if (strcmp(s, "string") == 0) {
		r = STRING;
	} else if (strcmp(s, "bool") == 0) {
		r = BOOL;
	} else if (strcmp(s, "bit") == 0) {
		r = BIT;
	}
	return r;
}

const char *get_plc_data_type_str(plc_data_type_t t)
{
	switch (t) {
	case UNKNOWN:
	default:
		return "unknown";
	case LINT:
		return "lint";
	case DINT:
		return "dint";
	case INT:
		return "int";
	case SINT:
		return "sint";
	case REAL:
		return "real";
	case STRING:
		return "string";
	case BOOL:
		return "bool";
	case BIT:
		return "bit";
	}
}

void pad_spaces(FILE *fd, int n)
{
	for (int i = 0; i < n; i++) {
		fprintf(fd, " ");
	}
}
