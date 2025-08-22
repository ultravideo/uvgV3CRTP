#include "Timestamp.h"

namespace uvgV3CRTP {

  Timestamp::Timestamp(const uint32_t timestamp)
  {
    set_timestamp(timestamp);
  }

  uint32_t Timestamp::get_timestamp() const {
    return timestamp_;
  }

  void Timestamp::set_timestamp(uint32_t timestamp) const {
      timestamp_ = timestamp;
      timestamp_set_ = true;
  }

  void Timestamp::unset_timestamp() const {
      timestamp_ = 0;
      timestamp_set_ = false;
  }

  bool Timestamp::is_timestamp_set() const {
      return timestamp_set_;
  }

}
