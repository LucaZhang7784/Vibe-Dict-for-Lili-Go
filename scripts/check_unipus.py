"""Build the adapter with the installed IDF cJSON and portable mbedTLS AES sources."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
mbed=Path('/opt/esp/idf/components/mbedtls/mbedtls')
with tempfile.TemporaryDirectory() as tmp:
    folder=Path(tmp)
    (folder/'minimal.h').write_text('''#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_WITH_PADDING
#define MBEDTLS_CIPHER_PADDING_PKCS7
''',encoding='utf-8')
    sources=[root/'main/unipus.c',root/'scripts/check_unipus.c',Path('/opt/esp/idf/components/json/cJSON/cJSON.c')]
    sources += [mbed/'library'/name for name in ['cipher.c','cipher_wrap.c','aes.c','platform_util.c','constant_time.c','block_cipher.c']]
    exe=folder/'check'
    subprocess.run(['gcc','-Wall','-Wextra','-Werror','-Wno-unused-function','-DMBEDTLS_CONFIG_FILE="minimal.h"','-I'+str(folder),'-I'+str(mbed/'include'),'-I'+str(root/'main'),'-I/opt/esp/idf/components/json/cJSON',*[str(p) for p in sources],'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe),str(root/'release/unipus-apple-trial.json'),str(root/'release/unipus-applet-trial.json')],check=True)
