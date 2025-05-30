#include "V3C_Gof.h"

#include <numeric>

namespace v3cRTPLib {


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
    units_.emplace(unit.type(), std::move(unit));
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
}