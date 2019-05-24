#pragma once

#define OFFSET(Type, member) (size_t)&( ((Type*)0)->member)
#define PutFixedAlign(des, src)               \
          if (sizeof(size_t) == 4)            \
            PutFixed32(des, (size_t) (src));  \
          else if (sizeof(size_t) == 8)       \
            PutFixed64(des, (size_t) (src));

#ifndef FALLTHROUGH_INTENDED
#if defined(__clang__)
#define FALLTHROUGH_INTENDED [[clang::fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH_INTENDED [[gnu::fallthrough]]
#else
#define FALLTHROUGH_INTENDED do {} while (0)
#endif
#endif
