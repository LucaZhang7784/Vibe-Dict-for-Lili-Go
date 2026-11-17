"""Exercise the actual complete-JSON validator with the IDF cJSON source."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'main/online.c').read_text(encoding='utf-8')
validator=source[source.index('static cJSON *parse_complete_json('):source.index('bool online_query(')]
test=r'''
static void check(const char *data,size_t n,int64_t expected,bool chunked,bool examples,bool want){
 cJSON *j=parse_complete_json(data,n,expected,chunked,examples);
 assert((j!=NULL)==want);cJSON_Delete(j);
}
int main(void){
 const char *good="{\"code\":0,\"value\":{\"list\":[{\"wordValue\":{\"word\":\"apple\",\"category\":[]}}]}}";size_t n=strlen(good);
 check(good,n,n,false,false,true);
 check(good,n,-1,true,false,true);
 check(good,n,0,true,false,true);
 check(good,n,n+1,false,false,false);
 check(good,n,n+1,true,false,false);
 char shortdoc[256];strcpy(shortdoc,good);shortdoc[n-1]=0;
 check(shortdoc,n-1,-1,true,false,false);
 char trailing[256];snprintf(trailing,sizeof(trailing),"%s junk",good);
 check(trailing,strlen(trailing),-1,true,false,false);
 snprintf(trailing,sizeof(trailing),"%s \n\t",good);
 check(trailing,strlen(trailing),-1,true,false,true);
 const char embedded[]="{\"code\":0,\"value\":{}}\0junk";
 check(embedded,sizeof(embedded)-1,-1,true,false,false);
 check("1",1,-1,true,false,false);
 const char *examples="{\"code\":0,\"value\":{\"dictExist\":false}}";
 check(examples,strlen(examples),-1,true,true,true);
 check(examples,strlen(examples),-1,true,false,true);
 check("{}",2,-1,true,true,false);
 check("[]",2,-1,true,false,false);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'check.c'
    path.write_text('#include <cJSON.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n#include <stdio.h>\n#include <assert.h>\n'+validator+test,encoding='utf-8')
    exe=Path(tmp)/'check'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror','-I/opt/esp/idf/components/json/cJSON',str(path),'/opt/esp/idf/components/json/cJSON/cJSON.c','-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Complete JSON: 14 cases passed, rejects truncation, trailing data, length mismatch and wrong schema')
