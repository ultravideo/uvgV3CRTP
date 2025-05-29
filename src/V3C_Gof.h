#pragma once

#include <map>

#include "v3crtplib/global.h"
#include "V3C_Unit.h"

namespace v3cRTPLib {

  class V3C_Gof
  {
  public:
    V3C_Gof() = default;
    template <typename FirstUnit, typename... OtherUnits>
    V3C_Gof(FirstUnit&& unit, OtherUnits&&... others);

    V3C_Gof(const V3C_Gof&) = delete;
    V3C_Gof& operator=(const V3C_Gof&) = delete;

    V3C_Gof(V3C_Gof&&) = default;
    V3C_Gof& operator=(V3C_Gof&&) = default;

    ~V3C_Gof() = default;

    template <V3C_UNIT_TYPE E>
    inline V3C_Unit& get();
    template <V3C_UNIT_TYPE E>
    inline const V3C_Unit& get() const;

    inline V3C_Unit& get(const V3C_UNIT_TYPE type);
    inline const V3C_Unit& get(const V3C_UNIT_TYPE type) const;

    void set(V3C_Unit&& unit);

    auto begin() { return units_.begin(); }
    auto end() { return units_.end(); }

    auto begin() const { return units_.begin(); }
    auto end() const { return units_.end(); }

    auto cbegin() const { return units_.cbegin(); }
    auto cend() const { return units_.cend(); }

    size_t size() const;
  
  private:

    std::map<V3C_UNIT_TYPE, V3C_Unit> units_;
  };

}