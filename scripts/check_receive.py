"""Exercise actual receive callback timing and split-body handling."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'main/online.c').read_text(encoding='utf-8')
callback=source[source.index('typedef struct {'):source.index('static cJSON *parse_complete_json(')]
stub=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
enum {HTTP_EVENT_ON_CONNECTED=1,HTTP_EVENT_ON_HEADER,HTTP_EVENT_ERROR,HTTP_EVENT_ON_DATA};
typedef struct {int event_id;void *user_data,*client;const char *data;int data_len;} esp_http_client_event_t;
static int64_t now;static int timeout;static bool stop;
static int64_t esp_timer_get_time(void){return now;}
static bool cancelled(void *p){(void)p;return stop;}
static void esp_http_client_set_timeout_ms(void *client,int ms){assert(client);timeout=ms;}
'''
test=r'''
int main(void){
 Response r={.data=calloc(1,32769),.deadline=20000000,.cancelled=cancelled};
 esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_CONNECTED,.user_data=&r,.client=(void*)1};
 assert(receive(&e)==ESP_OK&&timeout==10000);
 now=19500000;assert(receive(&e)==ESP_OK&&timeout==500);
 now=0;e.event_id=HTTP_EVENT_ON_DATA;e.data="one";e.data_len=3;
 assert(receive(&e)==ESP_OK&&r.used==3);
 e.data="two";assert(receive(&e)==ESP_OK&&r.used==6&&!strcmp(r.data,"onetwo"));
 e.data_len=-1;assert(receive(&e)==ESP_FAIL&&r.overflow&&r.used==6);
 r.overflow=false;r.used=32768;e.data_len=1;
 assert(receive(&e)==ESP_FAIL&&r.overflow&&r.used==32768);
 r.overflow=false;r.used=0;stop=true;assert(receive(&e)==ESP_FAIL&&r.used==0);
 stop=false;now=r.deadline;assert(receive(&e)==ESP_FAIL&&r.used==0);
 free(r.data);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'check.c';path.write_text(stub+callback+test,encoding='utf-8')
    exe=Path(tmp)/'check'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Receive callback: 8 checks passed, separate body timeout, split data, limits and cancellation')
