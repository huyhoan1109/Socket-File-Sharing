#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

int main()
{
    SHA512_CTX ctx;
    SHA512_Init(&ctx);

    // Hash each piece of data as it comes in:
    SHA512_Update(&ctx, "Hello, ", 7);
    SHA512_Update(&ctx, "world!", 6);
    // When you're done with the data, finalize it:
    unsigned char tmphash[SHA_DIGEST_LENGTH];
    SHA512_Final(tmphash, &ctx);
    unsigned char hash[SHA_DIGEST_LENGTH*2];

    int i = 0;
    for (i=0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf((char*)&(hash[i*2]), "%02x", tmphash[i]);
    }
    // And print to stdout
    printf("Hash: %s\n", hash);
    return 0;
}