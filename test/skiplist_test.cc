#include <iostream>
#include <assert.h>
#include "../util/random.h"
#include "swift/comparator.h"
#include "../util/skiplist.h"
#include "../util/arena.h"
#include <set>

using namespace shannon;
using namespace std;

typedef uint64_t Key;

struct TestComparator {
  int operator()(const Key &a, const Key &b) const {
    if (a < b) {
      return -1;
    } else if (a > b) {
      return +1;
    } else {
      return 0;
    }
  }
};

int main() {
  int i = 0;
  const int N = 2000;
  const int R = 5000;
  Random rnd(1000);
  std::set<Key> keys;
  Arena arena;
  TestComparator cmp;
  SkipList<Key, TestComparator> list(cmp, &arena);
  for (int i = 0; i < N; i++) {
    Key key = rnd.Next() % R;
    if (keys.insert(key).second) {
      list.Insert(key);
    }
  }
  SkipList<Key, TestComparator>::Iterator iter(&list);
  iter.SeekToFirst();
  for (iter.SeekToFirst(); iter.Valid(); iter.Next()) {
    Key key1 = iter.key();
    std::cout << " : " << key1 << endl;
    ++i;
  }
  std::cout << "Hello world" << std::endl;
  return 0;
}
