/**
 * Ozayn Core — Crypto
 * SHA-256 hashing, AES encryption, random generation
 */

#include "ozayn_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#endif

int ozayn_crypto_hash(const char* input, char* output, int output_len) {
    if (!input || !output || output_len < 65) return -1;

#ifdef __linux__
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)input, strlen(input), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
    return 0;
#else
    return -1;
#endif
}

int ozayn_crypto_random(int length, char* output, int output_len) {
    if (!output || length <= 0 || output_len < length * 2 + 1) return -1;

#ifdef __linux__
    unsigned char* buf = malloc(length);
    if (!buf) return -1;

    if (RAND_bytes(buf, length) != 1) {
        free(buf);
        return -1;
    }

    for (int i = 0; i < length; i++) {
        sprintf(output + (i * 2), "%02x", buf[i]);
    }
    output[length * 2] = '\0';
    free(buf);
    return 0;
#else
    return -1;
#endif
}

int ozayn_crypto_encrypt(const char* data, const char* key, char* output, int output_len) {
    if (!data || !key || !output) return -1;

#ifdef __linux__
    int data_len = strlen(data);
    int padded_len = ((data_len / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE;

    unsigned char* plaintext = calloc(padded_len, 1);
    unsigned char* ciphertext = malloc(padded_len + AES_BLOCK_SIZE);
    unsigned char iv[AES_BLOCK_SIZE];
    unsigned char tag[16];

    if (!plaintext || !ciphertext) {
        free(plaintext);
        free(ciphertext);
        return -1;
    }

    memcpy(plaintext, data, data_len);

    /* Generate random IV */
    if (RAND_bytes(iv, AES_BLOCK_SIZE) != 1) {
        free(plaintext);
        free(ciphertext);
        return -1;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(plaintext);
        free(ciphertext);
        return -1;
    }

    int len = 0, ciphertext_len = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)key, iv);
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, data_len);
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    /* Encode as base64 with IV prepended */
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    BIO_write(b64, iv, AES_BLOCK_SIZE);
    BIO_write(b64, ciphertext, ciphertext_len);
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);

    int copy_len = bptr->length < output_len - 1 ? bptr->length : output_len - 1;
    memcpy(output, bptr->data, copy_len);
    output[copy_len] = '\0';

    BIO_free_all(b64);
    free(plaintext);
    free(ciphertext);
    return copy_len;
#else
    return -1;
#endif
}

int ozayn_crypto_decrypt(const char* data, const char* key, char* output, int output_len) {
    if (!data || !key || !output) return -1;

#ifdef __linux__
    /* Base64 decode */
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(data, strlen(data));
    bmem = BIO_push(b64, bmem);
    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);

    int decoded_len = strlen(data);
    unsigned char* decoded = malloc(decoded_len);
    decoded_len = BIO_read(bmem, decoded, decoded_len);
    BIO_free_all(bmem);

    if (decoded_len <= AES_BLOCK_SIZE) {
        free(decoded);
        return -1;
    }

    unsigned char iv[AES_BLOCK_SIZE];
    memcpy(iv, decoded, AES_BLOCK_SIZE);

    int ciphertext_len = decoded_len - AES_BLOCK_SIZE;
    unsigned char* ciphertext = decoded + AES_BLOCK_SIZE;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(decoded);
        return -1;
    }

    int len = 0, plaintext_len = 0;
    unsigned char* plaintext = malloc(ciphertext_len + 1);

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)key, iv);
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    int copy_len = plaintext_len < output_len - 1 ? plaintext_len : output_len - 1;
    memcpy(output, plaintext, copy_len);
    output[copy_len] = '\0';

    free(decoded);
    free(plaintext);
    return copy_len;
#else
    return -1;
#endif
}
