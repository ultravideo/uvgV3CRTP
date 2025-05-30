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

  //Define an iterator class
  template <typename SampleType, template <typename> class StreamType>
  class SampleStreamIterator {
  public:
    SampleStreamIterator(typename StreamType<SampleType>::const_iterator it);
    const SampleType& operator*() const;
    SampleStreamIterator& operator++();
    bool operator==(const SampleStreamIterator& other) const;
    bool operator!=(const SampleStreamIterator& other) const;

    //private:
    typename StreamType<SampleType>::const_iterator it;
  };

  template <SAMPLE_STREAM_TYPE E>
  class Sample_Stream
  {
  public:
    using SampleType = V3C_Gof;
    template <typename ST>
    using StreamType = std::vector<std::pair<std::map<V3C_UNIT_TYPE, size_t>, ST>>;
    using Iterator = SampleStreamIterator<SampleType, StreamType>;

    Sample_Stream(const uint8_t size_precision);
    ~Sample_Stream() = default;

    Sample_Stream(const Sample_Stream&) = delete;
    Sample_Stream& operator=(const Sample_Stream&) = delete;

    Sample_Stream(Sample_Stream&&) = default;
    Sample_Stream& operator=(Sample_Stream&&) = default;

    void push_back(V3C_Unit&& unit);
    void push_back(V3C_Gof&& gof);

    size_t size() const;
    size_t size(Iterator gof_it) const;
    size_t size(Iterator gof_it, const V3C_UNIT_TYPE unit_type) const;

    const uint8_t size_precision;    

    Iterator begin() const;
    Iterator end() const;

    std::unique_ptr<char[]> get_bitstream() const;
    std::unique_ptr<char[]> get_bitstream(Iterator gof_it) const;
    std::unique_ptr<char[]> get_bitstream(Iterator gof_it, const V3C_UNIT_TYPE unit_type) const;

  protected:
    size_t write_bitstream(char * const bitstream, Iterator gof_it) const;
    size_t write_bitstream(char * const bitstream, Iterator gof_it, V3C_UNIT_TYPE unit_type) const;

  private:
    size_t find_free_gof(const V3C_UNIT_TYPE type) const;

    StreamType<SampleType> stream_;
  };

  template <>
  class Sample_Stream<SAMPLE_STREAM_TYPE::NAL>
  {
  public:
    using SampleType = Nalu;
    template <typename ST>
    using StreamType = std::vector<std::pair<size_t, ST>>;
    using Iterator = SampleStreamIterator<SampleType, StreamType>;

    Sample_Stream(const uint8_t size_precision);
    ~Sample_Stream() = default;

    Sample_Stream(const Sample_Stream&) = delete;
    Sample_Stream& operator=(const Sample_Stream&) = delete;

    Sample_Stream(Sample_Stream&&) = default;
    Sample_Stream& operator=(Sample_Stream&&) = default;

    void push_back(Nalu&& unit);

    size_t size() const;

    const uint8_t size_precision;

    Iterator begin() const;
    Iterator end() const;

  protected:

    //friend size_t V3C_Unit::write_bitstream(char* const bitstream);
    friend class V3C_Unit;
    size_t write_bitstream(char * const bitstream) const;

  private:
    StreamType<SampleType> stream_;

  };

  // Explicitly define necessary instantiations so code is linked properly
  extern template class Sample_Stream<SAMPLE_STREAM_TYPE::V3C>;
  extern template class Sample_Stream<SAMPLE_STREAM_TYPE::NAL>;

}