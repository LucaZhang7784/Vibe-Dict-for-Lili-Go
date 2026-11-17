"""Select the Mozilla trust root for the UNIPUS domestic-service trial."""
from pathlib import Path
import socket
import ssl
import certifi
from cryptography import x509
from cryptography.x509.oid import NameOID

names={'Sectigo Public Server Authentication Root E46'}
selected=[]
for block in Path(certifi.where()).read_text().split('-----END CERTIFICATE-----'):
    if '-----BEGIN CERTIFICATE-----' not in block:continue
    pem=block[block.index('-----BEGIN CERTIFICATE-----'):].strip()+'\n-----END CERTIFICATE-----\n'
    certificate=x509.load_pem_x509_certificate(pem.encode())
    cn=certificate.subject.get_attributes_for_oid(NameOID.COMMON_NAME)
    if cn and cn[0].value in names:selected.append(pem)
assert len(selected)==len(names)
out=Path(__file__).resolve().parents[1]/'assets/roots.pem'
out.write_text(''.join(selected),encoding='ascii')
context=ssl.create_default_context(cafile=str(out))
for host in ['open.unipus.cn']:
    with socket.create_connection((host,443),timeout=8) as connection:
        with context.wrap_socket(connection,server_hostname=host):print('TLS verified',host)
print('ROOTS',out.stat().st_size)
