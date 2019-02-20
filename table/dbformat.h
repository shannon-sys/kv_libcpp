#ifndef STORAGE_SHANNON_DB_DBFORMAT_H_
#define STORAGE_SHANNON_DB_DBFORMAT_H_

#include "swift/slice.h"
#include "table_builder.h"
#include "util/coding.h"
#include <stdio.h>

namespace shannon {

namespace config {
static const int kNumLevels = 7;

static const int kL0_CompactionTrigger = 4;

static const int kL0_SlowdownWritesTrigger = 8;

static const int kL0_StopWritesTrigger = 12;

static const int kMaxMemCompactLevel = 2;

static const int kReadBytesPeriod = 1048576;

} // namespace config

class InternalKey;

enum ValueType { kTypeDeletion = 0x0, kTypeValue = 0x1 };

static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;

static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

struct ParsedInternalKey {
  Slice user_key;
  SequenceNumber sequence;
  ValueType type;

  ParsedInternalKey() {} // Intentionally left uninitialized (for speed)
  ParsedInternalKey(const Slice &u, const SequenceNumber &seq, ValueType t)
      : user_key(u), sequence(seq), type(t) {}
  std::string DebugString() const;
};

// Return the length of the encoding of "key".
inline size_t InternalKeyEncodingLength(const ParsedInternalKey &key) {
  return key.user_key.size() + 8;
}

// Append the serialization of "key" to *result.
extern void AppendInternalKey(std::string *result,
                              const ParsedInternalKey &key);

// Attempt to parse an internal key from "internal_key".  On success,
// stores the parsed data in "*result", and returns true.
//
// On error, returns false, leaves "*result" in an undefined state.
extern bool ParseInternalKey(const Slice &internal_key,
                             ParsedInternalKey *result);

// Returns the user key portion of an internal key.
inline Slice ExtractUserKey(const Slice &internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - 8);
}

inline ValueType ExtractValueType(const Slice &internal_key) {
  assert(internal_key.size() >= 8);
  const size_t n = internal_key.size();
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  return static_cast<ValueType>(c);
}

// A comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class InternalKeyComparator : public Comparator {
private:
  const Comparator *user_comparator_;

public:
  explicit InternalKeyComparator(const Comparator *c) : user_comparator_(c) {}
  virtual const char *Name() const;
  virtual int Compare(const Slice &a, const Slice &b) const;
  virtual void FindShortestSeparator(std::string *start,
                                     const Slice &limit) const;
  virtual void FindShortSuccessor(std::string *key) const;

  const Comparator *user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey &a, const InternalKey &b) const;
};

// Filter policy wrapper that converts from internal keys to user keys
class InternalFilterPolicy : public FilterPolicy {
private:
  const FilterPolicy *const user_policy_;

public:
  explicit InternalFilterPolicy(const FilterPolicy *p) : user_policy_(p) {}
  virtual const char *Name() const;
  virtual void CreateFilter(const Slice *keys, int n, std::string *dst) const;
  virtual bool KeyMayMatch(const Slice &key, const Slice &filter) const;
};

// Modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an InternalKeyComparator.
class InternalKey {
private:
  std::string rep_;

public:
  InternalKey() {} // Leave rep_ as empty to indicate it is invalid
  InternalKey(const Slice &user_key, SequenceNumber s, ValueType t) {
    AppendInternalKey(&rep_, ParsedInternalKey(user_key, s, t));
  }

  void DecodeFrom(const Slice &s) { rep_.assign(s.data(), s.size()); }
  Slice Encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  Slice user_key() const { return ExtractUserKey(rep_); }

  void SetFrom(const ParsedInternalKey &p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(const InternalKey &a,
                                          const InternalKey &b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice &internal_key,
                             ParsedInternalKey *result) {
  const size_t n = internal_key.size();
  if (n < 8)
    return false;
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  result->user_key = Slice(internal_key.data(), n - 8);
  return (c <= static_cast<unsigned char>(kTypeValue));
}
} // namespace shannon

#endif // STORAGE_SHANNON_DB_DBFORMAT_H_
