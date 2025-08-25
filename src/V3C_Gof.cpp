#include "V3C_Gof.h"
#include "V3C_Unit.h"
#include "V3C.h"

#include <numeric>

namespace uvgV3CRTP {


  V3C_Unit& V3C_Gof::get(const V3C_UNIT_TYPE type)
  { 
    return units_.at(type);
  }

  const V3C_Unit& V3C_Gof::get(const V3C_UNIT_TYPE type) const 
  {
    return units_.at(type);
  }

  void V3C_Gof::set(V3C_Unit&& unit)
  {
    if (units_.empty() && !is_timestamp_set() && unit.is_timestamp_set())
    {
      // If timestamp is not set and this is the first v3c unit, set the gof timestamp to the v3c units timestamp
      set_timestamp(unit.get_timestamp());
    }
    else if (is_timestamp_set() && !unit.is_timestamp_set())
    {
      // If gof timestamp is set and v3c unit timestamp is not set, set the v3c unit timestamp to the gof timestamp
      unit.set_timestamp(get_timestamp());
    }
    // Check that the v3c unit timestamp matches gof timestamp, if not this v3c unit does not belong to this gof
    else if (is_timestamp_set() && unit.get_timestamp() != get_timestamp())
    {
      throw TimestampException("Nalu timestamp does not match V3C unit timestamp");
    }
    const auto type = unit.type();
    units_.emplace(type, std::move(unit));
  }

  size_t V3C_Gof::size() const
  {
    size_t size = 0;
    for (const auto&[type, unit] : units_)
    {
      size += unit.size();
    }
    return size;
  }

  void V3C_Gof::set_timestamp(const uint32_t timestamp) const
  {
    Timestamp::set_timestamp(timestamp);
    // Also set the timestamp for all units in the GOF
    for (const auto&[type, unit] : units_)
    {
      unit.set_timestamp(timestamp);
    }
  }

  void V3C_Gof::unset_timestamp() const
  {
    Timestamp::unset_timestamp();
    // Also unset the timestamp for all units in the GOF
    for (const auto&[type, unit] : units_)
    {
      unit.unset_timestamp();
    }
  }

}