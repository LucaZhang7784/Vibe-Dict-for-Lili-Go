#pragma once
#include <stdbool.h>
#include <stddef.h>
/* PEM is loaded from app assets. No insecure TLS mode is provided. */
bool online_query(const char *word, bool examples, const char *pem,
                  char *output, size_t capacity,
                  void (*trace)(const char *, void *),
                  bool (*cancelled)(void *), void *context);
