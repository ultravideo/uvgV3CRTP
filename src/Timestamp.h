#pragma once

#include <cstdint>

namespace uvgV3CRTP {

class Timestamp {
public:
    Timestamp() = default;
    Timestamp(const uint32_t timestamp);
    ~Timestamp() = default;

    uint32_t get_timestamp() const;
    virtual void set_timestamp(uint32_t timestamp) const;
    virtual void unset_timestamp() const;
    bool is_timestamp_set() const;

private:
    mutable uint32_t timestamp_ = 0;
    mutable bool timestamp_set_ = false;
};

}
