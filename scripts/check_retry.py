"""Fault-inject the actual request-attempt block without making network calls."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'main/online.c').read_text(encoding='utf-8')
start = source.index('  /* Request attempts:')
end = source.index('  /* End request attempts. */', start)
block = source[start:end]
stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define ESP_OK 0
#define ESP_ERR_HTTP_EAGAIN 1
#define ESP_ERR_TIMEOUT 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_HTTP_CONNECT 4
#define ESP_ERR_HTTP_INCOMPLETE_DATA 5
#define ESP_ERR_HTTP_READ_TIMEOUT 6
#define ESP_ERR_HTTP_CONNECTION_CLOSED 7
#define ESP_HTTP_CLIENT_TLS_VER_TLS_1_2 1
#define HTTP_METHOD_POST 1
#define UNIPUS_TRIAL_APP_ID "public-test-app"
#define UNIPUS_HOST "open.unipus.cn"
#define ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT 10
#define ESP_ERR_ESP_TLS_SERVER_HANDSHAKE_TIMEOUT 11
#define ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST 12
#define ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME 13
#define pdMS_TO_TICKS(x) (x)
typedef int esp_err_t;
typedef struct {int dummy;} cJSON;
static cJSON recovered;
typedef struct {char *data;size_t used;bool overflow,header_seen;int64_t deadline;} Response;
typedef struct {int timeout_ms,tls_version;} esp_http_client_config_t;
typedef struct {int id;} Client;
typedef Client *esp_http_client_handle_t;
static int scenario,inits,cleanups,performs;static int64_t now;
static Response *response;
static int64_t esp_timer_get_time(void){return now;}
static void vTaskDelay(int ms){now+=(int64_t)ms*1000;}
static bool is_cancelled(void *p){(void)p;return scenario==5;}
static const char *esp_err_to_name(int error){(void)error;return "error";}
static Client *esp_http_client_init(esp_http_client_config_t *cfg){
 assert(cfg->timeout_ms>0&&cfg->timeout_ms<=5000);
 assert(cfg->tls_version==(inits?ESP_HTTP_CLIENT_TLS_VER_TLS_1_2:0));
 Client *c=malloc(sizeof(*c));c->id=++inits;return c;
}
static void esp_http_client_set_header(Client *c,const char *a,const char *b){(void)c;(void)a;(void)b;}
static void esp_http_client_set_method(Client *c,int method){(void)c;assert(method==HTTP_METHOD_POST);}
static void esp_http_client_set_post_field(Client *c,const char *p,int n){(void)c;assert(n==(int)strlen(p));}
static void esp_http_client_set_timeout_ms(Client *c,int ms){(void)c;assert(ms>0&&ms<=5000);}
static int esp_http_client_perform(Client *c){
 performs++;assert(response->used==0&&!response->overflow&&!response->header_seen);
 if(scenario==7)now=19700000;
 if(scenario==8)response->overflow=true;
 if(scenario==9)response->used=3;
 if(scenario>=10){
  if(scenario==13&&c->id==1)return ESP_ERR_HTTP_READ_TIMEOUT;
  if(scenario==12||scenario==11||scenario==14||c->id==1){response->used=3;return ESP_ERR_HTTP_INCOMPLETE_DATA;}
  return ESP_OK;
 }
 if((scenario==0&&c->id==2)||scenario==1||scenario==4)return ESP_OK;
 return ESP_ERR_HTTP_CONNECT;
}
static int esp_http_client_get_errno(Client *c){(void)c;return 0;}
static int esp_http_client_get_status_code(Client *c){return scenario==15?404:scenario==16?429:scenario>=10?200:scenario==4?404:((scenario==1||(scenario==0&&c->id==2))?200:0);}
static int64_t esp_http_client_get_content_length(Client *c){(void)c;return -1;}
static bool esp_http_client_is_chunked_response(Client *c){(void)c;return true;}
static bool esp_http_client_is_complete_data_received(Client *c){(void)c;return scenario==1;}
static cJSON *parse_complete_json(const char *p,size_t used,int64_t expected,bool chunked,bool examples){
 (void)p;(void)used;(void)expected;(void)chunked;(void)examples;return scenario==12?&recovered:NULL;
}
static int esp_http_client_get_and_clear_last_tls_error(Client *c,int *tls,int *flags){
 (void)c;*tls=scenario==2?-123:scenario==3?32512:0;*flags=(scenario==2||scenario==14)?1:0;
 return ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT;
}
static void esp_http_client_cleanup(Client *c){cleanups++;free(c);}
static bool run(void){
 Response r={.data=calloc(1,32769),.deadline=20000000};response=&r;
 esp_http_client_config_t config={0};char out[512];size_t cap=sizeof(out);
 const char *token="test-token",*body="{\"name\":\"apple\"}";
 bool examples=false;void *context=NULL;bool (*cancelled)(void*)=is_cancelled;
 void (*trace)(const char*,void*)=NULL;
 cJSON *json=NULL;int64_t expected=-1;bool chunked=false;
'''
tail = r'''
 free(r.data);return error==ESP_OK&&status==200;
}
int main(void){
 const int expected[]={2,1,1,1,1,0,2,1,1,1,2,2,1,2,1,1,1};
 for(scenario=0;scenario<17;scenario++){
  inits=cleanups=performs=0;now=0;
  bool ok=run();assert(ok==(scenario==0||scenario==1||scenario==10||scenario==12||scenario==13));
  assert(inits==expected[scenario]&&cleanups==inits);
  assert(performs==inits);
 }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'check.c';path.write_text(stub+block+tail,encoding='utf-8')
    exe=Path(tmp)/'check'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror','-Wno-unused-but-set-variable',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Retry fault injection: 17 cases passed, fresh clients cleaned up, no partial concatenation')
