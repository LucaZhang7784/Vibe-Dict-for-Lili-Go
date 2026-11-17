#include "unipus.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  assert(argc == 3);
  const int64_t timestamps[] = {1700000000, 1794890000, 2147483648LL};
  const char *expected[] = {
      "mrrsZSjCEzpuZMQhSCC9Sg==", "MyaDiw2u5gAB412d2aqwsw==",
      "HCfhQPUwCgu/ohxmaKxc+Q=="};
  char token[64];
  for (unsigned i = 0; i < 3; i++) {
    assert(unipus_token(timestamps[i], token));
    assert(!strcmp(token, expected[i]));
  }
  assert(!unipus_token(0, token) && !*token);
  for (int i = 1; i < argc; i++) {
    FILE *f = fopen(argv[i], "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);
    char *data = calloc(1, (size_t)length + 1);
    assert(data);
    assert(fread(data, 1, (size_t)length, f) == (size_t)length);
    fclose(f);
    cJSON *json = cJSON_Parse(data);
    assert(json);
    free(data);
    char out[4096];
    assert(unipus_format(json, false, out, sizeof(out)));
    if (i == 1) {
      assert(strstr(out, "apple") && strstr(out, "苹果"));
      assert(!strstr(out, "An apple a day"));
      assert(unipus_format(json, true, out, sizeof(out)));
      assert(strstr(out, "An apple a day") && strstr(out, "His daughter"));
      assert(strstr(out, "来源：外研在线"));
      char small[32];
      assert(unipus_format(json, true, small, sizeof(small)) &&
             strlen(small) < sizeof(small));
    } else
      assert(strstr(out, "未返回词条"));
    cJSON_Delete(json);
  }
  cJSON *error = cJSON_Parse("{\"code\":1001,\"msg\":\"denied\"}");
  char out[512];
  assert(!unipus_format(error, false, out, sizeof(out)) && strstr(out, "1001"));
  cJSON_Delete(error);
  char untouched = 'x';
  assert(!unipus_format(NULL, false, &untouched, 0) && untouched == 'x');
  puts("UNIPUS auth vectors, schema response formatting, bilingual examples, "
       "miss/error and bounds passed");
}
