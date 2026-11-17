#pragma once
#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define UNIPUS_HOST "open.unipus.cn"
/* Public integration-test account, expressly published by the provider. */
#define UNIPUS_TRIAL_APP_ID "yf_test_2rruoul4ihwalflr"
bool unipus_token(int64_t timestamp, char output[64]);
bool unipus_format(cJSON *json, bool examples, char *output, size_t capacity);
