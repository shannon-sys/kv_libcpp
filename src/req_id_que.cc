#include "req_id_que.h"
#include "swift/shannon_db.h"

namespace shannon {

void ReqIdQue::init_id_que(const int32_t size) {
  if (reqid_que_free_.size() != 0) {
    return;
  }
  size_ = size;
  std::unique_lock<std::mutex> lck(req_que_lock_);
  reqid_que_free_.clear();
  for (int32_t i = 0; i < size; ++i) {
    reqid_que_free_.push_back(i);
  }
  return;
}

int32_t ReqIdQue::borrow_id() {
  if (isclose_) {
    return -1;
  }
  std::unique_lock<std::mutex> lck(req_que_lock_);
  while (reqid_que_free_.empty()) {
    cond_.wait(lck);
    if (isclose_) {
      return -1;
    }
  }
  int32_t id;
  id = reqid_que_free_.front();
  reqid_que_free_.pop_front();
  return id;
}

void ReqIdQue::give_back_id(const int32_t reqid) {
  req_que_lock_.lock();
  reqid_que_free_.push_back(reqid);
  req_que_lock_.unlock();
  cond_.notify_all();
}

int ReqIdQue::wait_clear() {
  std::unique_lock<std::mutex> lck(req_que_lock_);
  isclose_ = true;
  while (reqid_que_free_.size() != size_ &&
         cond_.wait_for(lck, std::chrono::seconds(3)) !=
             std::cv_status::timeout) {
  }
  cond_.notify_all();
  return size_ - reqid_que_free_.size();
}

}  // namespace shannon
