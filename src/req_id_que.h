#include <stdint.h>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace shannon {

class ReqIdQue {
 public:
  void init_id_que(const int32_t size);
  int32_t borrow_id();
  void give_back_id(const int32_t reqid);
  int wait_clear();

 private:
  bool isclose_ = false;
  int32_t size_;
  std::deque<int32_t> reqid_que_free_;
  std::mutex req_que_lock_;
  std::condition_variable cond_;
};

}  // namespace shannon
