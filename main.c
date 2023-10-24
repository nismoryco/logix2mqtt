#include "logix2mqtt.h"

const char *program = "logix2mqtt";
const char *version = "2023.10.24";

static volatile sig_atomic_t run = 1;

void sig_handler(int signum)
{
	switch (signum) {
	case SIGTERM:
	case SIGINT:
	case SIGQUIT:
		run = 0;
		break;
	case SIGHUP:
		/* do nothing */
		break;
	default:
		break;
	}
}

void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
	struct mqtt_t *mqtt = (struct mqtt_t *)obj;

	if (rc != 0) {
		fprintf(stderr, "Failed to connect to MQTT broker: %s\n", mosquitto_connack_string(rc));
		mosquitto_disconnect(mosq);
		if (mqtt != NULL) {
			mqtt->connected = 0;
		}
		return;
	}
	fprintf(stderr, "Connected to MQTT broker\n");
	if (mqtt != NULL) {
		mqtt->connected = 1;
	}
}

void on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
	struct mqtt_t *mqtt = (struct mqtt_t *)obj;

	if (mqtt != NULL) {
		if (mqtt->connected) {
			fprintf(stderr, "Disconnected from MQTT broker\n");
		}
		mqtt->connected = 0;
	}
}

void publish_tag_data(struct mosquitto *mosq, struct mqtt_t *mqtt, struct tag_t *tags, int num_tags)
{
	int i = 0, rc = 0;
	cJSON *obj = NULL, *val = NULL;
	char *str = NULL;

	if (mosq == NULL || mqtt == NULL || tags == NULL || !mqtt->connected || num_tags < 0) {
		return;
	}

	obj = cJSON_CreateObject();
	if (obj == NULL) {
		fprintf(stderr, "Failed to create json object for publish\n");
		return;
	}
	val = cJSON_CreateNumber((double)time_ms());
	if (val != NULL) {
		cJSON_AddItemToObject(obj, "stamp", val);
	}
	for (i = 0; i < num_tags; i++) {
		if (tags[i].plctag > 0 && tags[i].data != NULL) {
			val = NULL;
			switch (tags[i].data_type) {
			case BIT:
				val = cJSON_CreateNumber((double)plc_tag_get_bit(tags[i].plctag, 0));
				break;
			case BOOL:
			case SINT:
				val = cJSON_CreateNumber((double)*(int8_t *)tags[i].data);
				break;
			case INT:
				val = cJSON_CreateNumber((double)*(int16_t *)tags[i].data);
				break;
			case DINT:
				val = cJSON_CreateNumber((double)*(int32_t *)tags[i].data);
				break;
			case LINT:
				val = cJSON_CreateNumber((double)*(int64_t *)tags[i].data);
				break;
			case STRING:
				val = cJSON_CreateString((const char *)tags[i].data+4);
				break;
			case UNKNOWN:
			default:
				break;
			}
			if (val != NULL) {
				cJSON_AddItemToObject(obj, tags[i].name, val);
			}
		}
	}
	str = cJSON_PrintUnformatted(obj);
	cJSON_Delete(obj);
	if (str == NULL) {
		fprintf(stderr, "Failed to format json object\n");
	} else {
		fprintf(stderr, "%s\n", str);
		rc = mosquitto_publish(mosq, NULL, mqtt->pubtopic, strlen(str), str, mqtt->pubqos, mqtt->pubretain);
		if (rc != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
		}
		free(str);
	}
}

int main(int argc, char **argv)
{
	int i = 0;
	int rc = 0;
	int exit_code = 0;
	int delay = 0;
	int done = 0;
	int num_tags = 0;
	int valid_tags = 0;
	int64_t timeout = 0;
	int64_t start = 0;
	int64_t end = 0;
	struct mosquitto *mosq = NULL;
	struct tag_t *tags = NULL;
	struct mqtt_t mqtt = {0};
	struct plc_t plc = {0};

	/* check usage */
	if (argc < 2) {
		fprintf(stderr, "Specify json config file\n");
		exit(1);
	}

	/* print program title and version */
	fprintf(stderr, "%s version %s\n\n", program, version);

	/* install signal handlers */
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* initialize libmosquitto */
	mosquitto_lib_init();
	mosq = mosquitto_new(program, true, (void *)&mqtt);
	if (mosq == NULL) {
		fprintf(stderr, "Failed to initialize libmosquitto\n");
		exit(1);
	}
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_disconnect_callback_set(mosq, on_disconnect);

	/* read json conf file and create tag structure */
	tags = read_conf_file(argv[1], &mqtt, &plc, &num_tags);
	if (check_config(&mqtt, &plc, tags, num_tags) != 0) {
		exit_code = 1;
		goto cleanup;
	}
	/* dump config for debugging purposes */
	dump_config(&mqtt, &plc, tags, num_tags);

	/* connect to mqtt broker */
	mosquitto_username_pw_set(mosq, mqtt.username, mqtt.password);
	rc = mosquitto_connect(mosq, mqtt.broker, mqtt.port, mqtt.keepalive);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Failed to connect to mqtt broker: %s\n", mosquitto_strerror(rc));
		exit_code = 1;
		goto cleanup;
	}

	/* start mqtt network loop */
	mosquitto_loop_start(mosq);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Failed to start mqtt network loop: %s\n", mosquitto_strerror(rc));
		exit_code = 1;
		goto cleanup;
	}

	/* set timeout for tag create and initial read */
	timeout = time_ms() + plc.timeout;

	/* create plc tags */
	for (i = 0; i < num_tags; i++) {
		if (tags[i].name != NULL && strlen(tags[i].name) > 0) {
			if (tags[i].path != NULL) {
				free(tags[i].path);
				tags[i].path = NULL;
			}
			tags[i].path = my_malloc(TAG_PATH_MAX_LEN);
			snprintf(tags[i].path, TAG_PATH_MAX_LEN-1, TAG_PATH_BASE, plc.gateway, plc.path, tags[i].name);
			//printf("%s\n", tags[i].path);
			tags[i].plctag = plc_tag_create(tags[i].path, 0);
			if (tags[i].plctag > 0) {
				valid_tags++;
			} else {
				fprintf(stderr, "Could not create tag [%d]: %s\n", tags[i].plctag, plc_tag_decode_error(tags[i].plctag));
			}
		}
	}

	/* wait for tags to be created */
	do {
		done = 1;
		if (valid_tags > 0) {
			for (i = 0; i < num_tags; i++) {
				if (tags[i].plctag > 0) {
					rc = plc_tag_status(tags[i].plctag);
					if (rc != PLCTAG_STATUS_OK) {
						done = 0;
					}
				}
			}

			if (!done) {
				sleep_ms(1);
			}
		}
	} while (timeout > time_ms() && !done) ;

	if (!done) {
		fprintf(stderr, "Timeout waiting for tags to be ready\n");
		exit_code = 1;
		goto cleanup;
	}

	for (i = 0; i < num_tags; i++) {
		if (tags[i].plctag > 0) {
			rc = plc_tag_read(tags[i].plctag, 0);
			if (rc != PLCTAG_STATUS_OK && rc != PLCTAG_STATUS_PENDING) {
				fprintf(stderr, "Unable to read tag data [%d]: %s\n", rc, plc_tag_decode_error(rc));
			}
		}
	}

	do {
		done = 1;
		if (valid_tags > 0) {
			for (i = 0; i < num_tags; i++) {
				if (tags[i].plctag > 0) {
					rc = plc_tag_status(tags[i].plctag);
					if (rc != PLCTAG_STATUS_OK) {
						done = 0;
					}
				}
			}
			if (!done) {
				sleep_ms(1);
			}
		}
	} while (timeout > time_ms() && !done);

	if (!done) {
		fprintf(stderr, "Timeout waiting for initial tag read\n");
		exit_code = 1;
		goto cleanup;
	}

	/* allocate memory for tag storage */
	for (i = 0; i < num_tags; i++) {
		if (tags[i].plctag > 0) {
			tags[i].elem_size = plc_tag_get_int_attribute(tags[i].plctag, "elem_size", 0);
			tags[i].elem_count = plc_tag_get_int_attribute(tags[i].plctag, "elem_count", 0);
			if (tags[i].data_type == BIT) {
				tags[i].data_size = sizeof(int);
			} else {
				tags[i].data_size = tags[i].elem_size*tags[i].elem_count;
			}
			//fprintf(stderr, "tag %d elem size %d, elem count %d, data size %d\n", i, tags[i].elem_size, tags[i].elem_count, tags[i].data_size);
			tags[i].data = my_malloc(tags[i].data_size);
			if (tags[i].data == NULL) {
				fprintf(stderr, "Failed to allocate memory for tag data\n");
				exit_code = 1;
				goto cleanup;
			}
		}
	}

	/* read loop */
	do {
		start = time_ms();
		timeout = start + plc.timeout;
		for (i = 0; i < num_tags; i++) {
			if (tags[i].plctag > 0) {
				rc = plc_tag_read(tags[i].plctag, 0);
				if (rc != PLCTAG_STATUS_OK && rc != PLCTAG_STATUS_PENDING) {
					fprintf(stderr, "Unable to read tag data [%d]: %s\n", rc, plc_tag_decode_error(rc));
				}
			}
		}

		do {
			done = 1;
			if (valid_tags > 0) {
				for (i = 0; i < num_tags; i++) {
					if (tags[i].plctag > 0) {
						rc = plc_tag_status(tags[i].plctag);
		                if (rc != PLCTAG_STATUS_OK) {
                 		   done = 0;
		                }
					}
				}
				if (!done) {
					sleep_ms(1);
				}
			}
		} while (timeout > time_ms() && !done);

		if (!done) {
			fprintf(stderr, "Timeout waiting for tag read\n");
		} else {
			/* get tag data from read */
			for (i = 0; i < num_tags; i++) {
				if (tags[i].plctag > 0 && tags[i].data != NULL) {
					if (tags[i].data_type == BIT) {
						plc_tag_get_bit(tags[i].plctag, 0);
					} else {
						plc_tag_get_raw_bytes(tags[i].plctag, 0, tags[i].data, tags[i].data_size);
					}
				}
			}
			publish_tag_data(mosq, &mqtt, tags, num_tags);
		}

		end = time_ms();
		delay = plc.interval-(end-start);
		//printf("delay: %d\n", delay);
		if (delay > 0) {
			sleep_ms(delay);
		}
	} while (run); /* run */

cleanup:
	/* destroy tags and cleanup data */
	if (tags != NULL) {
		for (i = 0; i < num_tags; i++) {
			if (tags[i].plctag > 0) {
				plc_tag_destroy(tags[i].plctag);
				tags[i].plctag = 0;
			}
			if (tags[i].name != NULL) {
				free(tags[i].name);
				tags[i].name = NULL;
			}
			if (tags[i].path != NULL) {
				free(tags[i].path);
				tags[i].path = NULL;
			}
			if (tags[i].data != NULL) {
				free(tags[i].data);
				tags[i].data = NULL;
			}
		}
		free(tags);
		tags = NULL;
	}

	/* cleanup libplctag */
	plc_tag_shutdown();

	/* disconnect from broker */
	if (mosq != NULL && mqtt.connected) {
		mosquitto_disconnect(mosq);
	}

	/* stop mqtt network loop */
	if (mosq != NULL) {
		mosquitto_loop_stop(mosq, (exit_code == 0) ? false : true);
	}

	/* destroy mosquitto instance */
	if (mosq != NULL) {
		mosquitto_destroy(mosq);
		mosq = NULL;
	}

	/* cleanup libmosquitto */
	mosquitto_lib_cleanup();

	/* cleanup mqtt data */
	if (mqtt.broker != NULL) {
		free(mqtt.broker);
	}
	if (mqtt.username != NULL) {
		free(mqtt.username);
	}
	if (mqtt.password != NULL) {
		free(mqtt.password);
	}
	if (mqtt.pubtopic != NULL) {
		free(mqtt.pubtopic);
	}

	/* cleanup plc data */
	if (plc.gateway != NULL) {
		free(plc.gateway);
	}
	if (plc.path != NULL) {
		free(plc.path);
	}

	return exit_code;
}
