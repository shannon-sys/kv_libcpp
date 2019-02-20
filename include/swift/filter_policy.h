
#ifndef STORAGE_SHANNON_INCLUDE_FILTER_POLICY_H_
#define STORAGE_SHANNON_INCLUDE_FILTER_POLICY_H_

#include <string>

namespace shannon {

class Slice;

class FilterPolicy {
public:
  virtual ~FilterPolicy();

  virtual const char *Name() const = 0;

  virtual void CreateFilter(const Slice *keys, int n,
                            std::string *dst) const = 0;

  virtual bool KeyMayMatch(const Slice &key, const Slice &filter) const = 0;
};

const FilterPolicy *NewBloomFilterPolicy(int bits_per_key);
}

#endif // STORAGE_SHANNON_INCLUDE_FILTER_POLICY_H_
