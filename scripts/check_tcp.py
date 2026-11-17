"""Run actual bounded TCP probe with socket faults; no real network traffic."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'main/online.c').read_text(encoding='utf-8')
probe=source[source.index('static int tcp_probe('):source.index('/* End raw TCP probe. */')]
stub=r'''
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
static int scenario,closed,connects,selects;
static int lwip_socket(int a,int b,int c){assert(a==AF_INET&&b==SOCK_STREAM&&c==IPPROTO_TCP);if(scenario==0){errno=EMFILE;return -1;}return scenario==5?FD_SETSIZE:7;}
static int mock_fcntl(int fd,int cmd,...){(void)fd;if((scenario==1&&cmd==F_GETFL)||(scenario==2&&cmd==F_SETFL)){errno=ENOTSUP;return -1;}return 0;}
#define fcntl mock_fcntl
static int lwip_connect(int fd,const struct sockaddr *p,socklen_t n){
 (void)fd;connects++;assert(n==sizeof(struct sockaddr_in));
 const struct sockaddr_in *a=(const struct sockaddr_in*)p;
 const unsigned char *port=(const unsigned char*)&a->sin_port;assert(port[0]==1&&port[1]==187);
 const unsigned char *ip=(const unsigned char*)&a->sin_addr.s_addr;assert(ip[0]==1&&ip[3]==4);
 if(scenario==3)return 0;
 errno=scenario==9?ECONNREFUSED:scenario==10?0:EINPROGRESS;return -1;
}
static int lwip_select(int n,fd_set *r,fd_set *w,fd_set *e,struct timeval *t){
 (void)r;(void)e;selects++;assert(n==8&&FD_ISSET(7,w));assert(t->tv_sec==1&&t->tv_usec==500000);
 if(scenario==6)return 0;if(scenario==7){errno=EIO;return -1;}return 1;
}
static int lwip_getpeername(int fd,struct sockaddr *p,socklen_t *n){(void)fd;(void)p;(void)n;if(scenario==8){errno=ENOTCONN;return -1;}return 0;}
static int lwip_close(int fd){(void)fd;closed++;return 0;}
'''
test=r'''
int main(void){
 const int errors[]={EMFILE,ENOTSUP,ENOTSUP,0,0,EINVAL,ETIMEDOUT,EIO,ENOTCONN,ECONNREFUSED,EIO};
 unsigned char ip[]={1,2,3,4};
 for(scenario=0;scenario<11;scenario++){
  errno=closed=connects=selects=0;
  assert(tcp_probe(ip,443,1500)==errors[scenario]);assert(closed==(scenario?1:0));
  if(scenario<3||scenario==5)assert(connects==0&&selects==0);
  if(scenario==3||scenario>=9)assert(selects==0);
 }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'check.c';path.write_text(stub+probe+test,encoding='utf-8')
    exe=Path(tmp)/'check'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('TCP probe: 11 faults/connection cases passed; nonblocking setup and socket cleanup checked')
