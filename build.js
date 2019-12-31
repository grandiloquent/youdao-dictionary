const fs = require('fs');
const path = require('path');

const dir = './mbedtls/library';
const files = fs.readdirSync(dir, {
        withFileTypes: true
    })
    .filter(f => f.isFile() && path.extname(f.name) === '.c');

const result = files.map(f => `gcc -c -O2 -Wall -W -Wdeclaration-after-statement -I../include -D_FILE_OFFSET_BITS=64 ${f.name}`);
const libName = 'mbedtls';

const crypto = ['aes.o', 'aesni.o', 'arc4.o', 'asn1parse.o', 'asn1write.o', 'base64.o', 'bignum.o', 'blowfish.o', 'camellia.o', 'ccm.o', 'cipher.o', 'cipher_wrap.o', 'cmac.o', 'ctr_drbg.o', 'des.o', 'dhm.o', 'ecdh.o', 'ecdsa.o', 'ecjpake.o', 'ecp.o', 'ecp_curves.o', 'entropy.o', 'entropy_poll.o', 'error.o', 'gcm.o', 'havege.o', 'hmac_drbg.o', 'md.o', 'md2.o', 'md4.o', 'md5.o', 'md_wrap.o', 'memory_buffer_alloc.o', 'oid.o', 'padlock.o', 'pem.o', 'pk.o', 'pk_wrap.o', 'pkcs12.o', 'pkcs5.o', 'pkparse.o', 'pkwrite.o', 'platform.o', 'ripemd160.o', 'rsa.o', 'sha1.o', 'sha256.o', 'sha512.o', 'threading.o', 'timing.o', 'version.o', 'version_features.o', 'xtea.o', 'aes.o', 'aesni.o', 'arc4.o', 'asn1parse.o', 'asn1write.o', 'base64.o', 'bignum.o', 'blowfish.o', 'camellia.o', 'ccm.o', 'cipher.o', 'cipher_wrap.o', 'cmac.o', 'ctr_drbg.o', 'des.o', 'dhm.o', 'ecdh.o', 'ecdsa.o', 'ecjpake.o', 'ecp.o', 'ecp_curves.o', 'entropy.o', 'entropy_poll.o', 'error.o', 'gcm.o', 'havege.o', 'hmac_drbg.o', 'md.o', 'md2.o', 'md4.o', 'md5.o', 'md_wrap.o', 'memory_buffer_alloc.o', 'oid.o', 'padlock.o', 'pem.o', 'pk.o', 'pk_wrap.o', 'pkcs12.o', 'pkcs5.o', 'pkparse.o', 'pkwrite.o', 'platform.o', 'ripemd160.o', 'rsa.o', 'sha1.o', 'sha256.o', 'sha512.o', 'threading.o', 'timing.o', 'version.o', 'version_features.o', 'xtea.o'];

result.push(`gcc -shared -Wl,-soname,libmbedcrypto.dll -Wl,--out-implib,libmbedcrypto.dll.a -o libmbedcrypto.dll ${crypto.filter(f=>fs.existsSync(dir+'/'+f.replace(/\.o$/,'.c'))).join(' ')} -lws2_32 -lwinmm -lgdi32 -static-libgcc `)

console.log(result.join('\r\n'));