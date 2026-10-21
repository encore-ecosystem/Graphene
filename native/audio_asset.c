/* Streamed GAS1 payloads never materialize the entire asset in Encore memory.
 */
#include "audio_sha256.inc"
uint64_t graphene_audio_file_size(encore_str path) {
  char *p = graphene_c_string(path);
  if (!p)
    return 0;
  FILE *f = fopen(p, "rb");
  free(p);
  if (!f)
    return 0;
  uint64_t n = 0;
  if (!fseeko(f, 0, SEEK_END)) {
    off_t s = ftello(f);
    if (s >= 0)
      n = s;
  }
  fclose(f);
  return n;
}
encore_str graphene_audio_range_hash(encore_str path, uint64_t offset,
                                     uint64_t length) {
  char *p = graphene_c_string(path);
  if (!p)
    return graphene_string("");
  FILE *f = fopen(p, "rb");
  free(p);
  if (!f)
    return graphene_string("");
  if (offset > INT64_MAX || fseeko(f, (off_t)offset, SEEK_SET)) {
    fclose(f);
    return graphene_string("");
  }
  graphene_audio_sha256_context hash;
  graphene_audio_sha256_init(&hash);
  unsigned char buffer[65536];
  bool ok = true;
  while (length) {
    size_t n = length < sizeof buffer ? (size_t)length : sizeof buffer;
    if (fread(buffer, 1, n, f) != n) {
      ok = false;
      break;
    }
    graphene_audio_sha256_update(&hash, buffer, n);
    length -= n;
  }
  fclose(f);
  return ok ? graphene_audio_sha256_finish_text(&hash) : graphene_string("");
}
bool graphene_audio_package(encore_str path, encore_str header,
                            encore_str payload) {
  char *dst = graphene_c_string(path), *src = graphene_c_string(payload);
  if (!dst || !src) {
    free(dst);
    free(src);
    return false;
  }
  size_t n = strlen(dst);
  char *temp = malloc(n + 40);
  if (!temp) {
    free(dst);
    free(src);
    return false;
  }
  snprintf(temp, n + 40, "%s.tmp-%llu", dst,
           (unsigned long long)graphene_perf_counter_ns());
  FILE *in = fopen(src, "rb"), *out = in ? fopen(temp, "wb") : NULL;
  bool ok = in && out;
  uint32_t count = header.object ? header.object->len : 0;
  if (ok) {
    unsigned char prefix[] = {'G',   'A',        'S',         '1',
                              count, count >> 8, count >> 16, count >> 24};
    ok = count <= 1048576 && fwrite(prefix, 1, 8, out) == 8 &&
         fwrite(header.object->data, 1, count, out) == count;
  }
  unsigned char buffer[65536];
  while (ok) {
    size_t got = fread(buffer, 1, sizeof buffer, in);
    if (got && fwrite(buffer, 1, got, out) != got) {
      ok = false;
      break;
    }
    if (got < sizeof buffer) {
      if (ferror(in))
        ok = false;
      break;
    }
  }
  if (in && fclose(in))
    ok = false;
  if (out && fclose(out))
    ok = false;
  if (ok)
    ok = rename(temp, dst) == 0;
  if (!ok)
    remove(temp);
  free(temp);
  free(src);
  free(dst);
  return ok;
}
