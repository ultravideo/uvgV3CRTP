#pragma once

#include "v3crtplib/global.h"
//#include "V3C_Gof.h"
//#include "V3C_Unit.h"
#include "Nalu.h"

#include <map>
#include <vector>
#include <memory>

namespace v3cRTPLib {
   //Forward declaration
  class V3C_Gof;
  class V3C_Unit;

  template <SAMPLE_STREAM_TYPE E>
  class Sample_Stream
  {
  public:
    Sample_Stream(const uint8_t size_precision);
    ~Sample_Stream() = default;

    Sample_Stream(const Sample_Stream&) = delete;
    Sample_Stream& operator=(const Sample_Stream&) = delete;

    Sample_Stream(Sample_Stream&&) = default;
    Sample_Stream& operator=(Sample_Stream&&) = default;

    void push_back(V3C_Unit&& unit, size_t size);
    void push_back(V3C_Gof&& gof);

    size_t size() const;

    const uint8_t size_precision;

    using sample_type = V3C_Gof;
    using sample_stream_data = std::vector<std::pair<std::map<V3C_UNIT_TYPE, size_t>, sample_type>>;
    class Iterator {
    public:
      Iterator(typename sample_stream_data::iterator it);
      const sample_type& operator*() const;
      Iterator& operator++();
      bool operator==(const Iterator& other) const;
      bool operator!=(const Iterator& other) const;

    private:
      typename sample_stream_data::const_iterator it;
    };

    Iterator begin() const;
    Iterator end() const;

    std::unique_ptr<char[]> get_bitstream() const;

  private:
    size_t find_free_gof(const V3C_UNIT_TYPE type) const;

    sample_stream_data stream_;
  };

  template <>
  class Sample_Stream<SAMPLE_STREAM_TYPE::NAL>
  {
  public:
    Sample_Stream(const uint8_t size_precision);
    ~Sample_Stream() = default;

    Sample_Stream(const Sample_Stream&) = delete;
    Sample_Stream& operator=(const Sample_Stream&) = delete;

    Sample_Stream(Sample_Stream&&) = default;
    Sample_Stream& operator=(Sample_Stream&&) = default;

    void push_back(Nalu&& unit, size_t size);

    size_t size() const;

    const uint8_t size_precision;

    using sample_type = Nalu;
    using sample_stream_data = std::vector<std::pair<size_t, sample_type>>;

    class Iterator {
    public:
      Iterator(typename sample_stream_data::iterator it);
      const sample_type& operator*() const;
      Iterator& operator++();
      bool operator==(const Iterator& other) const;
      bool operator!=(const Iterator& other) const;

    private:
      typename sample_stream_data::const_iterator it;
    };

    Iterator begin() const;
    Iterator end() const;

  protected:

    //friend size_t V3C_Unit::write_bitstream(char* const bitstream);
    friend class V3C_Unit;
    size_t write_bitstream(char * const bitstream) const;

  private:
    sample_stream_data stream_;

  };

}