#include "logix2mqtt.h"

struct tag_t *read_conf_file(const char *fn, struct mqtt_t *mqtt, struct plc_t *plc, int *num_tags)
{
	struct tag_t *tags = NULL;
	struct stat s = {0};
	FILE *fd = NULL;
	char *buf = NULL;
	size_t bytes_read = 0;
	cJSON *json = NULL, *node = NULL, *key = NULL, *val0 = NULL, *val1 = NULL;
	int ix = 0;

	/* parameter check */
	if (mqtt == NULL || plc == NULL || num_tags == NULL) {
		fprintf(stderr, "Invalid parameters passed to read_conf_file\n");
		return NULL;
	}
	if (strlen(fn) == 0) {
		fprintf(stderr, "Config filename is empty\n");
		return NULL;
	}

	/* stat file for size */
	if (stat(fn, &s) != 0) {
		fprintf(stderr, "Failed to stat config file [%d]: %s\n", errno, strerror(errno));
		return NULL;
	}
	if (s.st_size == 0) {
		fprintf(stderr, "Config file is zero length\n");
		return NULL;
	}
	if (s.st_size > CONFIG_MAX_LENGTH-1) {
		fprintf(stderr, "Config file is greater than %d bytes\n", CONFIG_MAX_LENGTH);
		return NULL;
	}

	/* allocate memory for buffer */
	buf = my_malloc(s.st_size);
	if (buf == NULL) {
		fprintf(stderr, "Failed to allocate memory for config buffer\n");
		return NULL;
	}

	/* open file */
	fd = fopen(fn, "r");
	if (fd == NULL) {
		fprintf(stderr, "Failed to open config file\n");
		free(buf);
		return NULL;
	}

	/* read file into buffer */
	bytes_read = fread(buf, 1, s.st_size, fd);
	if (bytes_read != s.st_size) {
		fprintf(stderr, "Error reading config file\n");
		free(buf);
		fclose(fd);
		return NULL;
	}

	/* close file */
	fclose(fd);
	fd = NULL;

	/* parse json */
	json = cJSON_Parse(buf);
	if (json == NULL) {
		fprintf(stderr, "Failed to parse json in config file\n");
		free(buf);
		return NULL;
	}

	/* parse mqtt object */
	node = cJSON_GetObjectItemCaseSensitive(json, "mqtt");
	if (node == NULL) {
		fprintf(stderr, "Failed to find 'mqtt' object in config\n");
		cJSON_Delete(json);
		free(buf);
		return NULL;
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "broker");
	if (mqtt->broker != NULL) {
		free(mqtt->broker);
		mqtt->broker = NULL;
	}
	if (key != NULL && cJSON_IsString(key) && strlen(key->valuestring) > 0) {
		mqtt->broker = strdup(key->valuestring);
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "username");
	if (mqtt->username != NULL) {
		free(mqtt->username);
		mqtt->username = NULL;
	}
	if (key != NULL && cJSON_IsString(key) && strlen(key->valuestring) > 0) {
		mqtt->username = strdup(key->valuestring);
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "password");
	if (mqtt->password != NULL) {
		free(mqtt->password);
		mqtt->password = NULL;
	}
	if (key != NULL && cJSON_IsString(key) && strlen(key->valuestring) > 0) {
		mqtt->password = strdup(key->valuestring);
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "pub_topic");
	if (mqtt->pubtopic != NULL) {
		free(mqtt->pubtopic);
		mqtt->pubtopic = NULL;
	}
	if (key != NULL && cJSON_IsString(key) && strlen(key->valuestring) > 0) {
		mqtt->pubtopic = strdup(key->valuestring);
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "port");
	if (key != NULL && cJSON_IsNumber(key)) {
		mqtt->port = key->valueint;
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "keepalive");
	if (key != NULL && cJSON_IsNumber(key)) {
		mqtt->keepalive = key->valueint;
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "pub_qos");
	if (key != NULL && cJSON_IsNumber(key)) {
		mqtt->pubqos = key->valueint;
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "pub_retain");
	if (key != NULL && cJSON_IsBool(key)) {
		mqtt->pubretain = (cJSON_IsTrue(key) ? 1 : 0);
	}

	/* parse logix object */
	node = cJSON_GetObjectItemCaseSensitive(json, "logix");
	if (node == NULL) {
		fprintf(stderr, "Failed to find 'logix' object in config\n");
		cJSON_Delete(json);
		free(buf);
		return NULL;
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "gateway");
	if (plc->gateway != NULL) {
		free(plc->gateway);
		plc->gateway = NULL;
	}
	if (key != NULL && cJSON_IsString(key) && strlen(key->valuestring) > 0) {
		plc->gateway = strdup(key->valuestring);
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "path");
	if (plc->path != NULL) {
		free(plc->path);
		plc->path = NULL;
	}
	if (key != NULL && cJSON_IsString(key) && strlen(key->valuestring) > 0) {
		plc->path = strdup(key->valuestring);
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "timeout");
	if (key != NULL && cJSON_IsNumber(key)) {
		plc->timeout = key->valueint;
	}

	key = cJSON_GetObjectItemCaseSensitive(node, "interval");
	if (key != NULL && cJSON_IsNumber(key)) {
		plc->interval = key->valueint;
	}

	/* parse tag array */
	node = cJSON_GetObjectItemCaseSensitive(json, "tags");
	if (node == NULL) {
		fprintf(stderr, "Failed to find 'tags' array in config\n");
		cJSON_Delete(json);
		free(buf);
		return NULL;
	}
	if (!cJSON_IsArray(node)) {
		fprintf(stderr, "Tags is not an array in config\n");
	}
	*num_tags = cJSON_GetArraySize(node);
	if (*num_tags > 0) {
		tags = my_malloc(sizeof(struct tag_t)*(*num_tags));
		if (tags == NULL) {
			fprintf(stderr, "Failed to allocate memory for tags\n");
			cJSON_Delete(json);
			free(buf);
			return NULL;
		}
		cJSON_ArrayForEach(key, node) {
			if (cJSON_IsArray(key)) {
				val0 = cJSON_GetArrayItem(key, 0);
				val1 = cJSON_GetArrayItem(key, 1);
				if (val0 != NULL && cJSON_IsString(val0) && strlen(val0->valuestring) > 0 && strlen(val0->valuestring) < TAG_NAME_MAX_LEN-1 && val1 != NULL && cJSON_IsString(val1)) {
					tags[ix].name = strdup(val0->valuestring);
					tags[ix].data_type = get_plc_data_type(val1->valuestring);
					ix++;
				}
			}
		}
	}

	/* delete json data */
	cJSON_Delete(json);
	json = NULL;
	/* free buffer */
	free(buf);
	buf = NULL;

	return tags;
}

int check_config(struct mqtt_t *mqtt, struct plc_t *plc, struct tag_t *tags, int num_tags)
{
	int i = 0;

	if (mqtt == NULL || plc == NULL) {
		fprintf(stderr, "One or more parameters are null\n");
		return 1;
	}
	if (tags == NULL || num_tags < 0) {
		fprintf(stderr, "No tags have been defined\n");
		i++;
	}
	if (mqtt->broker == NULL || strlen(mqtt->broker) == 0) {
		fprintf(stderr, "MQTT broker has not been defined\n");
		i++;
	}
	if (mqtt->port < 0 || mqtt->port > 65535) {
		fprintf(stderr, "MQTT port is invalid\n");
		i++;
	}
	if (mqtt->port == 0) {
		fprintf(stderr, "Using standard MQTT port 1883\n");
		mqtt->port = MQTT_PORT_DEFAULT;
	}
	if (mqtt->keepalive < 0 || mqtt->keepalive > 65535) {
		fprintf(stderr, "Invalid keepalive value for MQTT\n");
		i++;
	}
	if (mqtt->keepalive == 0) {
		fprintf(stderr, "Using default keepalive value of 60 seconds\n");
		mqtt->keepalive = 60;
	}
	if (mqtt->pubtopic == NULL || strlen(mqtt->pubtopic) == 0) {
		fprintf(stderr, "Publish topic has not been defined\n");
		i++;
	}
	if (mqtt->pubqos < 0 || mqtt->pubqos > 2) {
		fprintf(stderr, "Publish QOS is invalid\n");
		i++;
	}
	if (plc->gateway == NULL || strlen(plc->gateway) == 0) {
		fprintf(stderr, "PLC gateway has not been defined\n");
		i++;
	}
	if (plc->path == NULL || strlen(plc->path) == 0) {
		fprintf(stderr, "Using default PLC path 1,0\n");
		plc->path = strdup("1,0");
	}
	if (plc->timeout <= 0) {
		fprintf(stderr, "Using default PLC timeout of %d ms\n", PLC_TIMEOUT_DEFAULT);
		plc->timeout = PLC_TIMEOUT_DEFAULT;
	}
	if (plc->interval <= 0)  {
		fprintf(stderr, "Using default PLC interval of %d ms\n", PLC_INTERVAL_DEFAULT);
		plc->interval = PLC_INTERVAL_DEFAULT;
	}
	return i;
}

void dump_config(struct mqtt_t *mqtt, struct plc_t *plc, struct tag_t *tags, int num_tags)
{
	int i = 0;
	int len = 12;

	fprintf(stderr, "broker       : %s\n", mqtt->broker);
	fprintf(stderr, "port         : %d\n", mqtt->port);
	fprintf(stderr, "keepalive    : %d\n", mqtt->keepalive);
	fprintf(stderr, "username     : %s\n", mqtt->username);
	if (mqtt->password != NULL && strlen(mqtt->password) > 0) {
		fprintf(stderr, "password     : (set)\n");
	} else {
		fprintf(stderr, "password     : (unset)\n");
	}
	fprintf(stderr, "pub_topic    : %s\n", mqtt->pubtopic);
	fprintf(stderr, "pub_qos      : %d\n", mqtt->pubqos);
	fprintf(stderr, "pub_retain   : %d\n", mqtt->pubretain);
	fprintf(stderr, "plc gateway  : %s\n", plc->gateway);
	fprintf(stderr, "plc path     : %s\n", plc->path);
	fprintf(stderr, "plc timeout  : %ld\n", plc->timeout);
	fprintf(stderr, "plc interval : %ld\n", plc->interval);
	fprintf(stderr, "num tags     : %d\n", num_tags);
	for (i = 0; i < num_tags; i++) {
		if (tags[i].name != NULL && strlen(tags[i].name) > len) {
			len = strlen(tags[i].name);
		}
	}
	for (i = 0; i < num_tags; i++) {
		if (tags[i].name != NULL) {
			fprintf(stderr, "%s", tags[i].name);
			pad_spaces(stderr, len-strlen(tags[i].name));
			fprintf(stderr, " [%s]\n", get_plc_data_type_str(tags[i].data_type));
		}
	}
	fprintf(stderr, "\n");
}
