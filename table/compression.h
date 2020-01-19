
#include <algorithm>
#include <limits>
#include <string>
#include <iostream>

#ifdef ALL_COMPRESS
  #define ZSTD
  #define LZ4
  #define ZLIB
  #define BZIP2
#endif

#ifdef ZLIB
#include <zlib.h>
#endif

#ifdef BZIP2
#include <bzlib.h>
#endif

#if defined(LZ4)
#include <lz4.h>
#include <lz4hc.h>
#endif

#if defined(ZSTD)
#include <zstd.h>
#include <zdict.h>
#endif

namespace shannon {

inline bool Snappy_Supported() {
  return true;
}

inline bool Zlib_Supported() {
#ifdef ZLIB
  return true;
#else
  return false;
#endif
}

inline bool BZip2_Supported() {
#ifdef BZIP2
  return true;
#else
  return false;
#endif
}

inline bool LZ4_Supported() {
#ifdef LZ4
  return true;
#else
  return false;
#endif
}

inline bool XPRESS_Supported() {
#ifdef XPRESS
  return true;
#else
  return false;
#endif
}

inline bool ZSTD_Supported() {
#ifdef ZSTD
  // ZSTD format is finalized since version 0.8.0.
  return (ZSTD_versionNumber() >= 800);
#else
  return false;
#endif
}

inline bool GetDecompressedSizeInfo(const char** input_data,
                                    size_t* input_length,
                                    uint32_t* output_len) {
  auto new_input_data =
      GetVarint32Ptr(*input_data, *input_data + *input_length, output_len);
  if (new_input_data == nullptr) {
    return false;
  }
  *input_length -= (new_input_data - *input_data);
  *input_data = new_input_data;
  return true;
}

inline bool ZSTDNotFinal_Supported() {
#ifdef ZSTD
  return true;
#else
  return false;
#endif
}

inline bool CompressionTypeSupported(char compression_type) {
  switch (compression_type) {
    case kNoCompression:
      return true;
    case kSnappyCompression:
      return Snappy_Supported();
    case kZlibCompression:
      return Zlib_Supported();
    case kBZip2Compression:
      return BZip2_Supported();
    case kLZ4Compression:
      return LZ4_Supported();
    case kLZ4HCCompression:
      return LZ4_Supported();
    case kXpressCompression:
      return XPRESS_Supported();
    case kZSTDNotFinalCompression:
      return ZSTDNotFinal_Supported();
    case kZSTD:
      return ZSTD_Supported();
    default:
      assert(false);
      return false;
  }
}

inline bool zlib_uncompress(Slice* result, Slice* input) {
#ifdef ZLIB
  DEBUG("zlib_uncompress\n");
  int compress_format_version = 2;
  const char* input_data = input->data();
  size_t input_length = input->size();
  uint32_t output_len;
  int windowBits = -14;
  const Slice& compression_dict = Slice();
  if (!GetDecompressedSizeInfo(&input_data, &input_length, &output_len)) {
    compress_format_version = 1;
    size_t proposed_output_len = ((input_length * 5) & (~(4096 - 1))) + 4096;
    output_len = static_cast<uint32_t>(std::min(proposed_output_len,
                 static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
  } else {
    compress_format_version = 2;
  }
  DEBUG("compress_format_version: %d\n", compress_format_version);
  z_stream _stream;
  memset(&_stream, 0, sizeof(z_stream));
  int st = inflateInit2(&_stream, windowBits > 0 ? windowBits + 32 : windowBits);
  DEBUG("compression_dict size :%d, input size : %d\n", compression_dict.size(), input_length);
  if (st != Z_OK) {
    return false;
  }

  if (compression_dict.size()) {
    // Initialize the compression library's dictionary
    st = inflateSetDictionary(
        &_stream, reinterpret_cast<const Bytef*>(compression_dict.data()),
        static_cast<unsigned int>(compression_dict.size()));
    if (st != Z_OK) {
      DEBUG("something error\n");
      return false;
    }
  }
  _stream.next_in = (Bytef *)input_data;
  _stream.avail_in = static_cast<unsigned int>(input_length);

  char* output = (char*)malloc(output_len);

  _stream.next_out = (Bytef *)output;
  _stream.avail_out = static_cast<unsigned int>(output_len);

  bool done = false;
  while (!done) {
    st = inflate(&_stream, Z_SYNC_FLUSH);
    DEBUG("st: %d output_len : %d\n", st, output_len);
    switch (st) {
      case Z_STREAM_END:
        done = true;
        break;
      case Z_OK: {
        assert(compress_format_version != 2);
        size_t old_sz = output_len;
        uint32_t output_len_delta = output_len / 5;
        output_len += output_len_delta < 10 ? 10 : output_len_delta;
        char* tmp = (char*) malloc(output_len);
        memcpy(tmp, output, old_sz);
        free(output);
        output = tmp;
        _stream.next_out = (Bytef *)(output + old_sz);
        _stream.avail_out = static_cast<unsigned int>(output_len - old_sz);
        break;
      }
      case Z_BUF_ERROR:
      default:
        free(output);
        inflateEnd(&_stream);
        DEBUG("something error\n");
        return false;
    }
  }
  DEBUG("compress_format_version:%d, done:%d\n", compress_format_version, done);
  assert(compress_format_version != 2 || _stream.avail_out == 0);
  int size = static_cast<int> (output_len - _stream.avail_out);
  inflateEnd(&_stream);
  *result = Slice(output, size);
  return true;
#else
  return false;
#endif
}

inline bool bzip2_uncompress(Slice *result, Slice *input) {
  DEBUG("bzlib2_uncompress\n");
#ifdef BZIP2
  DEBUG("bzlib2_uncompress\n");
  int compress_format_version = 2;
  const char* input_data = input->data();
  size_t input_length = input->size();
  uint32_t output_len;
  int windowBits = -14;
  const Slice& compression_dict = Slice();
  if (!GetDecompressedSizeInfo(&input_data, &input_length, &output_len)) {
    compress_format_version = 1;
    size_t proposed_output_len = ((input_length * 5) & (~(4096 - 1))) + 4096;
    output_len = static_cast<uint32_t>(std::min(proposed_output_len,
                 static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
  } else {
    compress_format_version = 2;
  }
  DEBUG("compress_format_version: %d\n", compress_format_version);
  bz_stream _stream;
  memset(&_stream, 0, sizeof(z_stream));

 int st = BZ2_bzDecompressInit(&_stream, 0, 0);
  if (st != BZ_OK) {
    return false;
  }

  _stream.next_in = (char*) input_data;
  _stream.avail_in = static_cast<unsigned int> (input_length);

  char* output = (char*) malloc(output_len);

  _stream.next_out = (char*) output;
  _stream.avail_out = static_cast<unsigned int> (output_len);

  bool done = false;
  while (!done) {
    st = BZ2_bzDecompress(&_stream);
    switch (st) {
      case BZ_STREAM_END:
        done = true;
        break;
      case BZ_OK: {
        assert(compress_format_version != 2);
        uint32_t old_sz = output_len;
        output_len = output_len * 1.2;
        char* tmp = new char[output_len];
        memcpy(tmp, output, old_sz);
        free(output);
        output = tmp;
        _stream.next_out = (char *) (output + old_sz);
        _stream.avail_out = static_cast<unsigned int>(output_len - old_sz);
        break;
      }
      default:
        free(output);
        BZ2_bzDecompressEnd(&_stream);
        return false;
    }
  }

  DEBUG("compress_format_version:%d, done:%d\n", compress_format_version, done);
  assert(compress_format_version != 2 || _stream.avail_out == 0);
  int size = static_cast<int>(output_len - _stream.avail_out);
  BZ2_bzDecompressEnd(&_stream);
  *result = Slice(output, size);
  return true;
#else
  return false;
#endif
}

inline bool lz4_uncompress(Slice *result, Slice *input) {
  DEBUG("lz4_uncompress\n");
#ifdef LZ4
  int compress_format_version = 2;
  const char* input_data = input->data();
  size_t input_length = input->size();
  uint32_t output_len;
  const Slice& compression_dict = Slice();
  if (!GetDecompressedSizeInfo(&input_data, &input_length, &output_len)) {
    compress_format_version = 1;
    if (input_length < 8) {
      DEBUG("input_length smaller than 8 \n");
      return false;
    }
    memcpy(&output_len, input_data, sizeof(output_len));
    input_length -= 8;
    input_data += 8;
  } else {
    compress_format_version = 2;
  }
  DEBUG("compress_format_version: %d\n", compress_format_version);

  char* output = (char*) malloc(output_len);
  int size = -1;
  LZ4_streamDecode_t* stream = LZ4_createStreamDecode();
  if (compression_dict.size()) {
    LZ4_setStreamDecode(stream, compression_dict.data(),
                        static_cast<int>(compression_dict.size()));
  }
  size = LZ4_decompress_safe_continue(
      stream, input_data, output, static_cast<int>(input_length),
      static_cast<int>(output_len));
  LZ4_freeStreamDecode(stream);
  DEBUG("size: %d output_len : %d input size : %d\n", size, output_len, input->size());
  if (size < 0) {
    free(output);
    DEBUG("size %d\n", size);
    return false;
  }
  assert(size == static_cast<int>(output_len));
  *result = Slice(output, size);
  return true;
#else
  return false;
#endif
}

inline bool lz4hcc_uncompress(Slice *result, Slice *input) {
  DEBUG("lz4hcc_uncompress\n");
  return false;
}

inline bool zstd_uncompress(Slice *result, Slice *input) {
#ifdef ZSTD
  DEBUG("zstd_uncompress\n");
  const char* input_data = input->data();
  size_t input_length = input->size();
  uint32_t output_len;
  const Slice& compression_dict = Slice();
  if (!GetDecompressedSizeInfo(&input_data, &input_length, &output_len)) {
    return false;
  }
  DEBUG("inputsize: %d input_size: %d\n", input_length, input->size());
  char* output = (char*) malloc(output_len);
  int size = -1;

  size_t actual_output_length;
  ZSTD_DCtx* context = ZSTD_createDCtx();
  actual_output_length = ZSTD_decompress_usingDict(
      context, output, output_len, input_data, input_length,
      compression_dict.data(), compression_dict.size());
  ZSTD_freeDCtx(context);
  DEBUG("actual_output_length :%d output_len :%d \n", actual_output_length, output_len);
  assert(actual_output_length == output_len);
  size = static_cast<int>(actual_output_length);
  assert(size >= 0);
  *result = Slice(output, size);
  return true;
#else
  return false;
#endif
}

} //namespace shannon