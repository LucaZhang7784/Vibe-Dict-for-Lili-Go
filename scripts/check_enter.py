"""Compile the actual click handler with small input stubs; check release routing."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'main/main.c').read_text(encoding='utf-8')
start = source.index('static void clicked(lv_event_t *e) {')
end = source.index('static void perform(intptr_t action) {', start)
handler = source[start:end]
stub = r'''
#include <stdint.h>
#include <assert.h>
#include <stddef.h>
enum { LV_INDEV_TYPE_POINTER=1, LV_INDEV_TYPE_KEYPAD=2, LV_INDEV_TYPE_ENCODER=3 };
typedef struct {int type;} lv_indev_t;
typedef struct {intptr_t action;lv_indev_t *indev;} lv_event_t;
static int calls;static intptr_t last;
static void *lv_event_get_user_data(lv_event_t *e){return (void*)e->action;}
static lv_indev_t *lv_event_get_indev(lv_event_t *e){return e->indev;}
static int lv_indev_get_type(lv_indev_t *i){return i->type;}
static void perform(intptr_t action){calls++;last=action;}
'''
test = r'''
int main(void){
 lv_indev_t keyboard={LV_INDEV_TYPE_KEYPAD},touch={LV_INDEV_TYPE_POINTER},ball={LV_INDEV_TYPE_ENCODER};
 lv_event_t e={9,&keyboard};
 /* KEY Enter submitted a lookup; its CLICKED must not invalidate it. */
 clicked(&e);assert(calls==0);
 e.indev=&touch;clicked(&e);assert(calls==1&&last==9);
 e.indev=&ball;clicked(&e);assert(calls==2&&last==9);
 e.indev=NULL;clicked(&e);assert(calls==3&&last==9);
 e.action=5;e.indev=&keyboard;clicked(&e);assert(calls==4&&last==5);
 e.action=17;clicked(&e);assert(calls==5&&last==17);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'check.c';path.write_text(stub+handler+test,encoding='utf-8')
    exe=Path(tmp)/'check'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Enter release regression: 6 routing checks passed')
