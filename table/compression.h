
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
  DEBUG("zlib_uncompress\n");
#ifdef ZLIB
  DEBUG("zlib_uncompress\n");
  return true;
#else
  DEBUG("zlib_uncompress\n");
  return false;
#endif
}

inline bool bzip2_uncompress(Slice *result, Slice *input) {
  DEBUG("bzlib2_uncompress\n");
#ifdef BZIP2
  return true;
#else
  return false;
#endif
}

inline bool lz4_uncompress(Slice *result, Slice *input) {
  DEBUG("lz4_uncompress\n");
#ifdef LZ4
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
  DEBUG("zstd_uncompress\n");
  return false;
}

} //namespace shannon