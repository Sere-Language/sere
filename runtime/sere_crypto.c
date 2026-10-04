/// @file sere_crypto.c
/// Cryptographic primitives for the Sere stdlib, bound by `stdlib/crypto.sere`.
///
/// The implementation deliberately contains no hand-written cryptography. Every
/// primitive is a thin, validating wrapper over the operating system's vetted
/// provider: Windows CNG (`bcrypt.dll`) on Windows. That gives us the platform
/// FIPS-validated SHA-2/SHA-3, HMAC, PBKDF2, AES-256-GCM,
/// ChaCha20-Poly1305, and the system CSPRNG without vendoring a large
/// dependency or copying an algorithm this project would then have to audit.
///
/// Design rules followed here:
///   * every pointer/length pair from Sere is validated before it reaches CNG;
///   * keys and intermediate secrets are wiped with `SecureZeroMemory`;
///   * no mutable global state, so every entry point is thread safe;
///   * failures call `sere_raise("CryptoError;Exception", ...)` so the Sere
///     layer never observes a silently-wrong result.
///
/// On a non-Windows host the file compiles to stubs that raise
/// `NotImplementedError`; the stdlib surface stays importable so a program can
/// degrade gracefully rather than fail to build.

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "sere_rt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

// Algorithm identifiers shared with `stdlib/crypto.sere`.
#define SERE_CRYPTO_SHA256 1
#define SERE_CRYPTO_SHA384 2
#define SERE_CRYPTO_SHA512 3
#define SERE_CRYPTO_SHA3_256 4
#define SERE_CRYPTO_SHA3_512 5
#define SERE_CRYPTO_HMAC_SHA256 101
#define SERE_CRYPTO_HMAC_SHA512 102

#define SERE_CRYPTO_ERROR "CryptoError;Exception"

// ---------------------------------------------------------------------------
// Sere value helpers
// ---------------------------------------------------------------------------

static SereList* cryptoBytes(int64_t length) {
  if (length < 0) {
    length = 0;
  }
  return (SereList*)sere_array_new(1, length);
}

/// Returns the raw bytes of a Sere `list[byte]`, or NULL when the value is not a
/// byte list. A zero-length list legitimately carries a NULL data pointer.
static const uint8_t* cryptoData(const void* value, int64_t* out_length) {
  const SereList* list = (const SereList*)value;
  if (out_length != NULL) {
    *out_length = 0;
  }
  if (list == NULL || list->stride != 1) {
    return NULL;
  }
  if (out_length != NULL) {
    *out_length = list->len;
  }
  return (const uint8_t*)list->data;
}

static void secureZero(void* pointer, size_t length) {
  if (pointer == NULL || length == 0) {
    return;
  }
#ifdef _WIN32
  SecureZeroMemory(pointer, length);
#else
  volatile uint8_t* bytes = (volatile uint8_t*)pointer;
  while (length-- > 0) {
    *bytes++ = 0;
  }
#endif
}

#ifndef _WIN32
static void cryptoUnsupported(const char* what) {
  char message[128];
  const int written = snprintf(message, sizeof(message),
                               "%s is not available on this platform (no crypto backend)", what);
  sere_raise("NotImplementedError;Exception", message, written < 0 ? 0 : written);
}
#endif

#ifdef _WIN32

static void cryptoRaise(const char* message) {
  sere_raise(SERE_CRYPTO_ERROR, message, (int64_t)strlen(message));
}

static void cryptoRaiseStatus(const char* what, NTSTATUS status) {
  char message[160];
  const int written = snprintf(message, sizeof(message), "%s failed (NTSTATUS 0x%08lX)", what,
                               (unsigned long)status);
  sere_raise(SERE_CRYPTO_ERROR, message, written < 0 ? 0 : (int64_t)written);
}

static void cryptoRaiseLength(const char* what, int64_t expected, int64_t actual) {
  char message[160];
  const int written = snprintf(message, sizeof(message), "%s must be %lld bytes, got %lld", what,
                               (long long)expected, (long long)actual);
  sere_raise(SERE_CRYPTO_ERROR, message, written < 0 ? 0 : (int64_t)written);
}

static int32_t cryptoAlgSpec(int32_t algorithm, LPCWSTR* out_name, int32_t* out_hmac) {
  switch (algorithm) {
    case SERE_CRYPTO_SHA256:
      *out_name = BCRYPT_SHA256_ALGORITHM;
      *out_hmac = 0;
      return 1;
    case SERE_CRYPTO_SHA384:
      *out_name = BCRYPT_SHA384_ALGORITHM;
      *out_hmac = 0;
      return 1;
    case SERE_CRYPTO_SHA512:
      *out_name = BCRYPT_SHA512_ALGORITHM;
      *out_hmac = 0;
      return 1;
    case SERE_CRYPTO_SHA3_256:
      *out_name = BCRYPT_SHA3_256_ALGORITHM;
      *out_hmac = 0;
      return 1;
    case SERE_CRYPTO_SHA3_512:
      *out_name = BCRYPT_SHA3_512_ALGORITHM;
      *out_hmac = 0;
      return 1;
    case SERE_CRYPTO_HMAC_SHA256:
      *out_name = BCRYPT_SHA256_ALGORITHM;
      *out_hmac = 1;
      return 1;
    case SERE_CRYPTO_HMAC_SHA512:
      *out_name = BCRYPT_SHA512_ALGORITHM;
      *out_hmac = 1;
      return 1;
    default:
      return 0;
  }
}

// ---------------------------------------------------------------------------
// Secure randomness
// ---------------------------------------------------------------------------

static int32_t cngRandom(uint8_t* out, int64_t length) {
  NTSTATUS status = BCryptGenRandom(NULL, out, (ULONG)length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  return status == 0 ? 0 : 1;
}

void* sere_crypto_random_bytes(int64_t length) {
  if (length < 0) {
    cryptoRaise("length must be non-negative");
    return cryptoBytes(0);
  }
  if (length > 0x40000000LL) {
    cryptoRaise("length exceeds the maximum random request");
    return cryptoBytes(0);
  }
  SereList* out = cryptoBytes(length);
  if (length == 0) {
    return out;
  }
  if (out == NULL || out->data == NULL) {
    cryptoRaise("out of memory");
    return cryptoBytes(0);
  }
  if (cngRandom((uint8_t*)out->data, length) != 0) {
    cryptoRaise("the operating system random source failed");
    return cryptoBytes(0);
  }
  return out;
}

uint64_t sere_crypto_random_u64(void) {
  uint64_t value = 0;
  if (cngRandom((uint8_t*)&value, (int64_t)sizeof(value)) != 0) {
    cryptoRaise("the operating system random source failed");
    return 0;
  }
  return value;
}

// ---------------------------------------------------------------------------
// Streaming hash contexts
// ---------------------------------------------------------------------------

typedef struct {
  BCRYPT_ALG_HANDLE algorithm;
  BCRYPT_HASH_HANDLE hash;
  uint8_t* object;
  int32_t digestLength;
} SereCryptoHash;

static int32_t cryptoHashInit(
    LPCWSTR name, int32_t hmac, const uint8_t* key, int64_t keyLength, SereCryptoHash** out) {
  SereCryptoHash* context = (SereCryptoHash*)calloc(1, sizeof(SereCryptoHash));
  if (context == NULL) {
    return 1;
  }
  const ULONG flags = hmac ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0;
  NTSTATUS status = BCryptOpenAlgorithmProvider(&context->algorithm, name, NULL, flags);
  if (status != 0) {
    free(context);
    return status;
  }
  ULONG objectLength = 0;
  ULONG written = 0;
  status = BCryptGetProperty(context->algorithm,
                             BCRYPT_OBJECT_LENGTH,
                             (PUCHAR)&objectLength,
                             sizeof(objectLength),
                             &written,
                             0);
  if (status == 0) {
    ULONG digestLength = 0;
    status = BCryptGetProperty(context->algorithm,
                               BCRYPT_HASH_LENGTH,
                               (PUCHAR)&digestLength,
                               sizeof(digestLength),
                               &written,
                               0);
    context->digestLength = (int32_t)digestLength;
    context->object = (uint8_t*)malloc(objectLength == 0 ? 1 : objectLength);
    if (context->object == NULL) {
      status = (NTSTATUS)0xC0000017L;  // STATUS_NO_MEMORY
    }
  }
  if (status == 0) {
    status = BCryptCreateHash(context->algorithm,
                              &context->hash,
                              context->object,
                              objectLength,
                              (PUCHAR)key,
                              (ULONG)keyLength,
                              0);
  }
  if (status != 0) {
    if (context->object != NULL) {
      free(context->object);
    }
    if (context->algorithm != NULL) {
      BCryptCloseAlgorithmProvider(context->algorithm, 0);
    }
    free(context);
    return status;
  }
  *out = context;
  return 0;
}

static int32_t cryptoHashUpdateAll(SereCryptoHash* context, const uint8_t* data, int64_t length) {
  if (length <= 0 || data == NULL) {
    return 0;
  }
  const int64_t kChunk = 0x40000000LL;  // BCryptHashData takes a ULONG.
  int64_t offset = 0;
  while (offset < length) {
    const int64_t remaining = length - offset;
    const ULONG chunk = (ULONG)(remaining > kChunk ? kChunk : remaining);
    const NTSTATUS status = BCryptHashData(context->hash, (PUCHAR)(data + offset), chunk, 0);
    if (status != 0) {
      return status;
    }
    offset += (int64_t)chunk;
  }
  return 0;
}

static void cryptoHashDestroy(SereCryptoHash* context) {
  if (context == NULL) {
    return;
  }
  if (context->hash != NULL) {
    BCryptDestroyHash(context->hash);
  }
  if (context->object != NULL) {
    secureZero(context->object, (size_t)context->digestLength);
    free(context->object);
  }
  if (context->algorithm != NULL) {
    BCryptCloseAlgorithmProvider(context->algorithm, 0);
  }
  free(context);
}

/// One-shot digest. `hmac` selects the HMAC construction, in which case `key`
/// is the MAC key and `keyLength` its length.
static SereList* cryptoHashOnce(
    LPCWSTR name, int32_t hmac, const uint8_t* key, int64_t keyLength, const uint8_t* data,
    int64_t dataLength) {
  SereCryptoHash* context = NULL;
  NTSTATUS status = cryptoHashInit(name, hmac, key, keyLength, &context);
  if (status == 0) {
    status = cryptoHashUpdateAll(context, data, dataLength);
  }
  if (status == 0) {
    SereList* out = cryptoBytes(context->digestLength);
    if (out == NULL || out->data == NULL) {
      status = (NTSTATUS)0xC0000017L;
    } else {
      status = BCryptFinishHash(context->hash, (PUCHAR)out->data, (ULONG)context->digestLength, 0);
      if (status == 0) {
        cryptoHashDestroy(context);
        return out;
      }
    }
  }
  if (context != NULL) {
    cryptoHashDestroy(context);
  }
  cryptoRaiseStatus("hashing", status);
  return cryptoBytes(0);
}

void* sere_crypto_hash(int32_t algorithm, void* data) {
  LPCWSTR name = NULL;
  int32_t hmac = 0;
  if (!cryptoAlgSpec(algorithm, &name, &hmac) || hmac) {
    cryptoRaise("unsupported hash algorithm");
    return cryptoBytes(0);
  }
  int64_t length = 0;
  const uint8_t* bytes = cryptoData(data, &length);
  if (bytes == NULL && length != 0) {
    cryptoRaise("data must be a list of bytes");
    return cryptoBytes(0);
  }
  return cryptoHashOnce(name, 0, NULL, 0, bytes, length);
}

int64_t sere_crypto_hash_new(int32_t algorithm) {
  LPCWSTR name = NULL;
  int32_t hmac = 0;
  if (!cryptoAlgSpec(algorithm, &name, &hmac) || hmac) {
    cryptoRaise("unsupported hash algorithm");
    return 0;
  }
  SereCryptoHash* context = NULL;
  const NTSTATUS status = cryptoHashInit(name, 0, NULL, 0, &context);
  if (status != 0) {
    cryptoRaiseStatus("hashing", status);
    return 0;
  }
  return (int64_t)(intptr_t)context;
}

int64_t sere_crypto_hmac_new(int32_t algorithm, void* key) {
  LPCWSTR name = NULL;
  int32_t hmac = 0;
  if (!cryptoAlgSpec(algorithm, &name, &hmac) || !hmac) {
    cryptoRaise("unsupported MAC algorithm");
    return 0;
  }
  int64_t keyLength = 0;
  const uint8_t* keyBytes = cryptoData(key, &keyLength);
  if (keyBytes == NULL && keyLength != 0) {
    cryptoRaise("key must be a list of bytes");
    return 0;
  }
  SereCryptoHash* context = NULL;
  const NTSTATUS status = cryptoHashInit(name, 1, keyBytes, keyLength, &context);
  if (status != 0) {
    cryptoRaiseStatus("MAC initialization", status);
    return 0;
  }
  return (int64_t)(intptr_t)context;
}

static SereCryptoHash* cryptoHashHandle(int64_t handle) {
  return (SereCryptoHash*)(intptr_t)handle;
}

void sere_crypto_hash_update(int64_t handle, void* data) {
  SereCryptoHash* context = cryptoHashHandle(handle);
  if (context == NULL || context->hash == NULL) {
    cryptoRaise("hash context is not open");
    return;
  }
  int64_t length = 0;
  const uint8_t* bytes = cryptoData(data, &length);
  if (bytes == NULL && length != 0) {
    cryptoRaise("data must be a list of bytes");
    return;
  }
  const NTSTATUS status = cryptoHashUpdateAll(context, bytes, length);
  if (status != 0) {
    cryptoRaiseStatus("hashing", status);
  }
}

void* sere_crypto_hash_finish(int64_t handle) {
  SereCryptoHash* context = cryptoHashHandle(handle);
  if (context == NULL || context->hash == NULL) {
    cryptoRaise("hash context is not open");
    return cryptoBytes(0);
  }
  SereList* out = cryptoBytes(context->digestLength);
  if (out == NULL || out->data == NULL) {
    cryptoHashDestroy(context);
    cryptoRaise("out of memory");
    return cryptoBytes(0);
  }
  const NTSTATUS status =
      BCryptFinishHash(context->hash, (PUCHAR)out->data, (ULONG)context->digestLength, 0);
  cryptoHashDestroy(context);
  if (status != 0) {
    cryptoRaiseStatus("hashing", status);
    return cryptoBytes(0);
  }
  return out;
}

void sere_crypto_hash_free(int64_t handle) {
  SereCryptoHash* context = cryptoHashHandle(handle);
  if (context != NULL) {
    cryptoHashDestroy(context);
  }
}

// ---------------------------------------------------------------------------
// HMAC (one-shot)
// ---------------------------------------------------------------------------

static SereList* cryptoHmacOnce(int32_t algorithm, void* key, void* data) {
  LPCWSTR name = NULL;
  int32_t hmac = 0;
  if (!cryptoAlgSpec(algorithm, &name, &hmac) || !hmac) {
    cryptoRaise("unsupported MAC algorithm");
    return cryptoBytes(0);
  }
  int64_t keyLength = 0;
  int64_t dataLength = 0;
  const uint8_t* keyBytes = cryptoData(key, &keyLength);
  const uint8_t* dataBytes = cryptoData(data, &dataLength);
  if ((keyBytes == NULL && keyLength != 0) || (dataBytes == NULL && dataLength != 0)) {
    cryptoRaise("key and data must be lists of bytes");
    return cryptoBytes(0);
  }
  return cryptoHashOnce(name, 1, keyBytes, keyLength, dataBytes, dataLength);
}

void* sere_crypto_hmac_sha256(void* key, void* data) {
  return cryptoHmacOnce(SERE_CRYPTO_HMAC_SHA256, key, data);
}

void* sere_crypto_hmac_sha512(void* key, void* data) {
  return cryptoHmacOnce(SERE_CRYPTO_HMAC_SHA512, key, data);
}

// ---------------------------------------------------------------------------
// Key derivation
// ---------------------------------------------------------------------------

void* sere_crypto_pbkdf2_sha256(
    void* password, void* salt, int64_t iterations, int64_t length) {
  if (iterations <= 0) {
    cryptoRaise("iteration count must be positive");
    return cryptoBytes(0);
  }
  if (length <= 0 || length > 0x40000000LL) {
    cryptoRaise("derived key length is out of range");
    return cryptoBytes(0);
  }
  int64_t passwordLength = 0;
  int64_t saltLength = 0;
  const uint8_t* passwordBytes = cryptoData(password, &passwordLength);
  const uint8_t* saltBytes = cryptoData(salt, &saltLength);
  if ((passwordBytes == NULL && passwordLength != 0) ||
      (saltBytes == NULL && saltLength != 0)) {
    cryptoRaise("password and salt must be lists of bytes");
    return cryptoBytes(0);
  }
  BCRYPT_ALG_HANDLE algorithm = NULL;
  NTSTATUS status =
      BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL,
                                  BCRYPT_ALG_HANDLE_HMAC_FLAG);
  if (status != 0) {
    cryptoRaiseStatus("PBKDF2", status);
    return cryptoBytes(0);
  }
  SereList* out = cryptoBytes(length);
  if (out == NULL || out->data == NULL) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
    cryptoRaise("out of memory");
    return cryptoBytes(0);
  }
  status = BCryptDeriveKeyPBKDF2(algorithm,
                                 (PUCHAR)passwordBytes,
                                 (ULONG)passwordLength,
                                 (PUCHAR)saltBytes,
                                 (ULONG)saltLength,
                                 (ULONGLONG)iterations,
                                 (PUCHAR)out->data,
                                 (ULONG)length,
                                 0);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status != 0) {
    cryptoRaiseStatus("PBKDF2", status);
    return cryptoBytes(0);
  }
  return out;
}

/// HKDF (RFC 5869) composed from the platform HMAC. Extract and Expand are
/// specified entirely in terms of HMAC, so no primitive is invented here.
void* sere_crypto_hkdf_sha256(void* key_material, void* salt, void* info, int64_t length) {
  const int32_t digestLength = 32;
  if (length <= 0 || length > (int64_t)255 * digestLength) {
    cryptoRaise("HKDF output length is out of range");
    return cryptoBytes(0);
  }
  int64_t keyLength = 0;
  int64_t saltLength = 0;
  int64_t infoLength = 0;
  const uint8_t* keyBytes = cryptoData(key_material, &keyLength);
  const uint8_t* saltBytes = cryptoData(salt, &saltLength);
  const uint8_t* infoBytes = cryptoData(info, &infoLength);
  if ((keyBytes == NULL && keyLength != 0) || (saltBytes == NULL && saltLength != 0) ||
      (infoBytes == NULL && infoLength != 0)) {
    cryptoRaise("key material, salt, and info must be lists of bytes");
    return cryptoBytes(0);
  }

  uint8_t zeros[64];
  memset(zeros, 0, sizeof(zeros));
  const uint8_t* realSalt = saltBytes;
  int64_t realSaltLength = saltLength;
  if (realSaltLength == 0) {
    realSalt = zeros;
    realSaltLength = digestLength;
  }

  // Extract: PRK = HMAC-Hash(salt, IKM).
  SereList* prk = cryptoHashOnce(BCRYPT_SHA256_ALGORITHM, 1, realSalt, realSaltLength, keyBytes,
                                 keyLength);
  if (prk == NULL || prk->len != digestLength) {
    return cryptoBytes(0);
  }
  uint8_t* prkBytes = (uint8_t*)prk->data;

  // Expand: T(i) = HMAC-Hash(PRK, T(i-1) || info || i).
  SereList* out = cryptoBytes(length);
  if (out == NULL || out->data == NULL) {
    secureZero(prkBytes, (size_t)digestLength);
    cryptoRaise("out of memory");
    return cryptoBytes(0);
  }
  uint8_t* outBytes = (uint8_t*)out->data;
  uint8_t block[64];
  int64_t blockLength = 0;
  int64_t produced = 0;
  uint8_t counter = 1;
  // The message fed to each HMAC is at most digestLength + infoLength + 1 bytes.
  while (produced < length) {
    uint8_t* message = (uint8_t*)malloc((size_t)blockLength + (size_t)infoLength + 1);
    if (message == NULL) {
      secureZero(prkBytes, (size_t)digestLength);
      cryptoRaise("out of memory");
      return cryptoBytes(0);
    }
    int64_t messageLength = 0;
    if (blockLength > 0) {
      memcpy(message, block, (size_t)blockLength);
      messageLength += blockLength;
    }
    if (infoLength > 0) {
      memcpy(message + messageLength, infoBytes, (size_t)infoLength);
      messageLength += infoLength;
    }
    message[messageLength++] = counter;
    SereList* next = cryptoHashOnce(BCRYPT_SHA256_ALGORITHM, 1, prkBytes, digestLength, message,
                                    messageLength);
    secureZero(message, (size_t)messageLength);
    free(message);
    if (next == NULL || next->len != digestLength) {
      secureZero(prkBytes, (size_t)digestLength);
      secureZero(block, sizeof(block));
      return cryptoBytes(0);
    }
    memcpy(block, next->data, (size_t)digestLength);
    blockLength = digestLength;
    int64_t remaining = length - produced;
    int64_t take = remaining < digestLength ? remaining : digestLength;
    memcpy(outBytes + produced, block, (size_t)take);
    produced += take;
    counter += 1;
  }
  secureZero(prkBytes, (size_t)digestLength);
  secureZero(block, sizeof(block));
  return out;
}

// ---------------------------------------------------------------------------
// Authenticated encryption
// ---------------------------------------------------------------------------

static SereList* cryptoAead(LPCWSTR algorithmName,
                            LPCWSTR chainMode,
                            int32_t keyLength,
                            int32_t nonceLength,
                            int32_t tagLength,
                            int32_t encrypting,
                            void* keyValue,
                            void* nonceValue,
                            void* inputValue,
                            void* aadValue) {
  int64_t keyBytesLength = 0;
  int64_t nonceBytesLength = 0;
  int64_t inputLength = 0;
  int64_t aadLength = 0;
  const uint8_t* keyBytes = cryptoData(keyValue, &keyBytesLength);
  const uint8_t* nonceBytes = cryptoData(nonceValue, &nonceBytesLength);
  const uint8_t* inputBytes = cryptoData(inputValue, &inputLength);
  const uint8_t* aadBytes = cryptoData(aadValue, &aadLength);
  if (keyBytes == NULL && keyBytesLength != 0) {
    cryptoRaise("key must be a list of bytes");
    return cryptoBytes(0);
  }
  if (nonceBytes == NULL && nonceBytesLength != 0) {
    cryptoRaise("nonce must be a list of bytes");
    return cryptoBytes(0);
  }
  if (inputBytes == NULL && inputLength != 0) {
    cryptoRaise("input must be a list of bytes");
    return cryptoBytes(0);
  }
  if (aadBytes == NULL && aadLength != 0) {
    cryptoRaise("additional data must be a list of bytes");
    return cryptoBytes(0);
  }
  if (keyBytesLength != keyLength) {
    cryptoRaiseLength("key", keyLength, keyBytesLength);
    return cryptoBytes(0);
  }
  if (nonceBytesLength != nonceLength) {
    cryptoRaiseLength("nonce", nonceLength, nonceBytesLength);
    return cryptoBytes(0);
  }
  if (inputLength > 0x40000000LL) {
    cryptoRaise("input exceeds the maximum supported size");
    return cryptoBytes(0);
  }
  if (!encrypting && inputLength < tagLength) {
    cryptoRaise("ciphertext is too short to contain an authentication tag");
    return cryptoBytes(0);
  }
  const int64_t cipherLength = encrypting ? inputLength : inputLength - tagLength;

  BCRYPT_ALG_HANDLE algorithm = NULL;
  NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, algorithmName, NULL, 0);
  if (status == 0 && chainMode != NULL) {
    status = BCryptSetProperty(algorithm,
                               BCRYPT_CHAINING_MODE,
                               (PUCHAR)chainMode,
                               (ULONG)((wcslen(chainMode) + 1) * sizeof(WCHAR)),
                               0);
  }
  ULONG objectLength = 0;
  ULONG written = 0;
  uint8_t* object = NULL;
  BCRYPT_KEY_HANDLE keyHandle = NULL;
  if (status == 0) {
    status = BCryptGetProperty(algorithm,
                               BCRYPT_OBJECT_LENGTH,
                               (PUCHAR)&objectLength,
                               sizeof(objectLength),
                               &written,
                               0);
    object = (uint8_t*)malloc(objectLength == 0 ? 1 : objectLength);
    if (object == NULL) {
      status = (NTSTATUS)0xC0000017L;
    }
  }
  if (status == 0) {
    status = BCryptGenerateSymmetricKey(algorithm,
                                        &keyHandle,
                                        object,
                                        objectLength,
                                        (PUCHAR)keyBytes,
                                        (ULONG)keyLength,
                                        0);
  }
  if (status != 0) {
    if (object != NULL) {
      secureZero(object, objectLength);
      free(object);
    }
    if (algorithm != NULL) {
      BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    cryptoRaiseStatus("cipher setup", status);
    return cryptoBytes(0);
  }

  uint8_t* tagBuffer = encrypting ? NULL : (uint8_t*)(inputBytes + cipherLength);
  const int64_t resultLength = encrypting ? inputLength + tagLength : cipherLength;
  SereList* out = cryptoBytes(resultLength);
  // A zero-length result legitimately carries a NULL data pointer, so only a
  // non-empty result with no storage counts as an allocation failure.
  if (out == NULL || (resultLength > 0 && out->data == NULL)) {
    cryptoRaise("out of memory");
    BCryptDestroyKey(keyHandle);
    secureZero(object, objectLength);
    free(object);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return cryptoBytes(0);
  }

  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
  BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
  authInfo.pbNonce = (PUCHAR)nonceBytes;
  authInfo.cbNonce = (ULONG)nonceBytesLength;
  authInfo.pbAuthData = (PUCHAR)aadBytes;
  authInfo.cbAuthData = (ULONG)aadLength;
  authInfo.pbTag = encrypting ? ((uint8_t*)out->data + inputLength) : tagBuffer;
  authInfo.cbTag = (ULONG)tagLength;

  // CNG wants a valid output pointer even for a zero-length payload.
  uint8_t scratch[1] = {0};
  uint8_t* outputPointer = out->data != NULL ? (uint8_t*)out->data : scratch;
  const ULONG outputLength = (ULONG)(encrypting ? inputLength : cipherLength);

  ULONG produced = 0;
  if (encrypting) {
    status = BCryptEncrypt(keyHandle,
                           (PUCHAR)inputBytes,
                           (ULONG)inputLength,
                           &authInfo,
                           NULL,
                           0,
                           outputPointer,
                           outputLength,
                           &produced,
                           0);
  } else {
    status = BCryptDecrypt(keyHandle,
                           (PUCHAR)inputBytes,
                           (ULONG)cipherLength,
                           &authInfo,
                           NULL,
                           0,
                           outputPointer,
                           outputLength,
                           &produced,
                           0);
  }

  BCryptDestroyKey(keyHandle);
  secureZero(object, objectLength);
  free(object);
  BCryptCloseAlgorithmProvider(algorithm, 0);

  if (status != 0) {
    if (!encrypting) {
      cryptoRaise("authentication failed: the ciphertext or tag is invalid");
    } else {
      cryptoRaiseStatus("encryption", status);
    }
    return cryptoBytes(0);
  }
  return out;
}

void* sere_crypto_aes_gcm_encrypt(void* key, void* nonce, void* plaintext, void* aad) {
  return cryptoAead(BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_GCM, 32, 12, 16, 1, key, nonce,
                    plaintext, aad);
}

void* sere_crypto_aes_gcm_decrypt(void* key, void* nonce, void* ciphertext, void* aad) {
  return cryptoAead(BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_GCM, 32, 12, 16, 0, key, nonce,
                    ciphertext, aad);
}

void* sere_crypto_chacha20_poly1305_encrypt(
    void* key, void* nonce, void* plaintext, void* aad) {
  return cryptoAead(BCRYPT_CHACHA20_POLY1305_ALGORITHM, NULL, 32, 12, 16, 1, key, nonce, plaintext,
                    aad);
}

void* sere_crypto_chacha20_poly1305_decrypt(
    void* key, void* nonce, void* ciphertext, void* aad) {
  return cryptoAead(BCRYPT_CHACHA20_POLY1305_ALGORITHM, NULL, 32, 12, 16, 0, key, nonce, ciphertext,
                    aad);
}

// ---------------------------------------------------------------------------
// Constant-time comparison
// ---------------------------------------------------------------------------

/// Wipe a byte list in place. Uses the compiler-resistant primitive so a
/// temporary key copy cannot be optimized away before it is cleared.
void sere_crypto_zero(void* buffer) {
  int64_t length = 0;
  uint8_t* bytes = (uint8_t*)cryptoData(buffer, &length);
  secureZero(bytes, (size_t)length);
}

int32_t sere_crypto_secure_equal(void* left, void* right) {
  int64_t leftLength = 0;
  int64_t rightLength = 0;
  const uint8_t* leftBytes = cryptoData(left, &leftLength);
  const uint8_t* rightBytes = cryptoData(right, &rightLength);
  if (leftLength != rightLength) {
    return 0;
  }
  if (leftLength == 0) {
    return 1;
  }
  if (leftBytes == NULL || rightBytes == NULL) {
    return 0;
  }
  uint8_t diff = 0;
  for (int64_t index = 0; index < leftLength; ++index) {
    diff |= (uint8_t)(leftBytes[index] ^ rightBytes[index]);
  }
  return diff == 0 ? 1 : 0;
}

#else  // !_WIN32

// Non-Windows hosts: keep the ABI so the stdlib imports, but report clearly
// that no backend is wired up instead of returning a wrong answer.
void* sere_crypto_random_bytes(int64_t length) {
  (void)length;
  cryptoUnsupported("crypto.random_bytes");
  return cryptoBytes(0);
}
uint64_t sere_crypto_random_u64(void) {
  cryptoUnsupported("crypto.random_u64");
  return 0;
}
void* sere_crypto_hash(int32_t algorithm, void* data) {
  (void)algorithm;
  (void)data;
  cryptoUnsupported("crypto hashing");
  return cryptoBytes(0);
}
int64_t sere_crypto_hash_new(int32_t algorithm) {
  (void)algorithm;
  cryptoUnsupported("crypto hashing");
  return 0;
}
int64_t sere_crypto_hmac_new(int32_t algorithm, void* key) {
  (void)algorithm;
  (void)key;
  cryptoUnsupported("crypto MAC");
  return 0;
}
void sere_crypto_hash_update(int64_t handle, void* data) {
  (void)handle;
  (void)data;
  cryptoUnsupported("crypto hashing");
}
void* sere_crypto_hash_finish(int64_t handle) {
  (void)handle;
  cryptoUnsupported("crypto hashing");
  return cryptoBytes(0);
}
void sere_crypto_hash_free(int64_t handle) { (void)handle; }
void* sere_crypto_hmac_sha256(void* key, void* data) {
  (void)key;
  (void)data;
  cryptoUnsupported("crypto.hmac_sha256");
  return cryptoBytes(0);
}
void* sere_crypto_hmac_sha512(void* key, void* data) {
  (void)key;
  (void)data;
  cryptoUnsupported("crypto.hmac_sha512");
  return cryptoBytes(0);
}
void* sere_crypto_pbkdf2_sha256(void* password, void* salt, int64_t iterations, int64_t length) {
  (void)password;
  (void)salt;
  (void)iterations;
  (void)length;
  cryptoUnsupported("crypto.pbkdf2_sha256");
  return cryptoBytes(0);
}
void* sere_crypto_hkdf_sha256(void* key_material, void* salt, void* info, int64_t length) {
  (void)key_material;
  (void)salt;
  (void)info;
  (void)length;
  cryptoUnsupported("crypto.hkdf_sha256");
  return cryptoBytes(0);
}
void* sere_crypto_aes_gcm_encrypt(void* key, void* nonce, void* plaintext, void* aad) {
  (void)key;
  (void)nonce;
  (void)plaintext;
  (void)aad;
  cryptoUnsupported("crypto.aes_gcm_encrypt");
  return cryptoBytes(0);
}
void* sere_crypto_aes_gcm_decrypt(void* key, void* nonce, void* ciphertext, void* aad) {
  (void)key;
  (void)nonce;
  (void)ciphertext;
  (void)aad;
  cryptoUnsupported("crypto.aes_gcm_decrypt");
  return cryptoBytes(0);
}
void* sere_crypto_chacha20_poly1305_encrypt(void* key, void* nonce, void* plaintext, void* aad) {
  (void)key;
  (void)nonce;
  (void)plaintext;
  (void)aad;
  cryptoUnsupported("crypto.chacha20_poly1305_encrypt");
  return cryptoBytes(0);
}
void* sere_crypto_chacha20_poly1305_decrypt(void* key, void* nonce, void* ciphertext, void* aad) {
  (void)key;
  (void)nonce;
  (void)ciphertext;
  (void)aad;
  cryptoUnsupported("crypto.chacha20_poly1305_decrypt");
  return cryptoBytes(0);
}
void sere_crypto_zero(void* buffer) {
  int64_t length = 0;
  uint8_t* bytes = (uint8_t*)cryptoData(buffer, &length);
  secureZero(bytes, (size_t)length);
}
int32_t sere_crypto_secure_equal(void* left, void* right) {
  int64_t leftLength = 0;
  int64_t rightLength = 0;
  const uint8_t* leftBytes = cryptoData(left, &leftLength);
  const uint8_t* rightBytes = cryptoData(right, &rightLength);
  if (leftLength != rightLength) {
    return 0;
  }
  if (leftLength == 0) {
    return 1;
  }
  if (leftBytes == NULL || rightBytes == NULL) {
    return 0;
  }
  uint8_t diff = 0;
  for (int64_t index = 0; index < leftLength; ++index) {
    diff |= (uint8_t)(leftBytes[index] ^ rightBytes[index]);
  }
  return diff == 0 ? 1 : 0;
}

#endif  // _WIN32
