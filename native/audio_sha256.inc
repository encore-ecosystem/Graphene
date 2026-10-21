/* SHA-256 implementation adapted from Encore platform runtime (MIT).
 * Copyright Encore contributors; see
 * https://github.com/encore-ecosystem/encore. */
typedef struct {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t block[64];
  size_t block_length;
} graphene_audio_sha256_context;

static uint32_t graphene_audio_sha256_rotate(uint32_t value,
                                             unsigned int bits) {
  return (value >> bits) | (value << (32U - bits));
}

static void
graphene_audio_sha256_transform(graphene_audio_sha256_context *context,
                                const uint8_t block[64]) {
  static const uint32_t constants[64] = {
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
      0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
      0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
      0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
      0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
      0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
      0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
      0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
      0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
      0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
      0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
      0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
      0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
  uint32_t words[64];
  for (size_t index = 0; index < 16; ++index) {
    size_t offset = index * 4;
    words[index] =
        ((uint32_t)block[offset] << 24) | ((uint32_t)block[offset + 1] << 16) |
        ((uint32_t)block[offset + 2] << 8) | (uint32_t)block[offset + 3];
  }
  for (size_t index = 16; index < 64; ++index) {
    uint32_t x = words[index - 15];
    uint32_t y = words[index - 2];
    uint32_t small0 = graphene_audio_sha256_rotate(x, 7) ^
                      graphene_audio_sha256_rotate(x, 18) ^ (x >> 3);
    uint32_t small1 = graphene_audio_sha256_rotate(y, 17) ^
                      graphene_audio_sha256_rotate(y, 19) ^ (y >> 10);
    words[index] = words[index - 16] + small0 + words[index - 7] + small1;
  }
  uint32_t a = context->state[0], b = context->state[1], c = context->state[2],
           d = context->state[3];
  uint32_t e = context->state[4], f = context->state[5], g = context->state[6],
           h = context->state[7];
  for (size_t index = 0; index < 64; ++index) {
    uint32_t big1 = graphene_audio_sha256_rotate(e, 6) ^
                    graphene_audio_sha256_rotate(e, 11) ^
                    graphene_audio_sha256_rotate(e, 25);
    uint32_t choose = (e & f) ^ ((~e) & g);
    uint32_t temp1 = h + big1 + choose + constants[index] + words[index];
    uint32_t big0 = graphene_audio_sha256_rotate(a, 2) ^
                    graphene_audio_sha256_rotate(a, 13) ^
                    graphene_audio_sha256_rotate(a, 22);
    uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = big0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  context->state[0] += a;
  context->state[1] += b;
  context->state[2] += c;
  context->state[3] += d;
  context->state[4] += e;
  context->state[5] += f;
  context->state[6] += g;
  context->state[7] += h;
}

static void graphene_audio_sha256_init(graphene_audio_sha256_context *context) {
  static const uint32_t initial[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
                                      0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
                                      0x1f83d9abU, 0x5be0cd19U};
  memcpy(context->state, initial, sizeof(initial));
  context->bit_count = 0;
  context->block_length = 0;
}

static void graphene_audio_sha256_update(graphene_audio_sha256_context *context,
                                         const uint8_t *data, size_t length) {
  context->bit_count += (uint64_t)length * 8U;
  while (length > 0) {
    size_t available = 64 - context->block_length;
    size_t count = length < available ? length : available;
    memcpy(context->block + context->block_length, data, count);
    context->block_length += count;
    data += count;
    length -= count;
    if (context->block_length == 64) {
      graphene_audio_sha256_transform(context, context->block);
      context->block_length = 0;
    }
  }
}

static void graphene_audio_sha256_finish(graphene_audio_sha256_context *context,
                                         uint8_t digest[32]) {
  context->block[context->block_length++] = 0x80U;
  if (context->block_length > 56) {
    memset(context->block + context->block_length, 0,
           64 - context->block_length);
    graphene_audio_sha256_transform(context, context->block);
    context->block_length = 0;
  }
  memset(context->block + context->block_length, 0, 56 - context->block_length);
  for (size_t index = 0; index < 8; ++index) {
    context->block[63 - index] = (uint8_t)(context->bit_count >> (index * 8));
  }
  graphene_audio_sha256_transform(context, context->block);
  for (size_t index = 0; index < 8; ++index) {
    digest[index * 4] = (uint8_t)(context->state[index] >> 24);
    digest[index * 4 + 1] = (uint8_t)(context->state[index] >> 16);
    digest[index * 4 + 2] = (uint8_t)(context->state[index] >> 8);
    digest[index * 4 + 3] = (uint8_t)context->state[index];
  }
}

static encore_str
graphene_audio_sha256_finish_text(graphene_audio_sha256_context *context) {
  uint8_t digest[32];
  char encoded[65];
  static const char hexadecimal[] = "0123456789abcdef";
  graphene_audio_sha256_finish(context, digest);
  for (size_t index = 0; index < 32; ++index) {
    encoded[index * 2] = hexadecimal[digest[index] >> 4];
    encoded[index * 2 + 1] = hexadecimal[digest[index] & 15U];
  }
  encoded[64] = '\0';
  return graphene_string(encoded);
}
