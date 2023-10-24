#ifndef _LOGIX2MQTT_H_
#define _LOGIX2MQTT_H_

#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <libplctag.h>
#include <mosquitto.h>
#include <cjson/cJSON.h>

#define TAG_PATH_BASE "protocol=ab-eip&plc=ControlLogix&gateway=%s&path=%s&name=%s"
#define TAG_NAME_MAX_LEN (50)
#define TAG_PATH_MAX_LEN (200)
#define CONFIG_MAX_LENGTH (4096)
#define MQTT_PORT_DEFAULT (1883)
#define PLC_TIMEOUT_DEFAULT (5000)
#define PLC_INTERVAL_DEFAULT (1000)

typedef enum { UNKNOWN = 0, LINT, DINT, INT, SINT, REAL, STRING, BOOL, BIT } plc_data_type_t;

struct mqtt_t {
	char *broker;
	char *username;
	char *password;
	char *pubtopic;
	int port;
	int keepalive;
	int pubqos;
	int pubretain;
	int connected;
};

struct plc_t {
	char *gateway;
	char *path;
	int64_t timeout;
	int64_t interval;
};

struct tag_t {
	char *name;
	char *path;
	int status;
	int elem_count;
	int elem_size;
	plc_data_type_t data_type;
	size_t data_size;
	int32_t plctag;
	void *data;
};

/* defined in util.c */
int sleep_ms(int ms);
int64_t time_ms(void);
void *my_malloc(size_t size);
char *strlower(char * s);
plc_data_type_t get_plc_data_type(const char * s);
const char *get_plc_data_type_str(plc_data_type_t t);
void pad_spaces(FILE *fd, int n);

/* defined in config.c */
struct tag_t *read_conf_file(const char *fn, struct mqtt_t *mqtt, struct plc_t *plc, int *num_tags);
int check_config(struct mqtt_t *mqtt, struct plc_t *plc, struct tag_t *tags, int num_tags);
void dump_config(struct mqtt_t *mqtt, struct plc_t *plc, struct tag_t *tags, int num_tags);

#endif
