#pragma once

#include "uvgv3crtp/global.h"
#include "V3C_Gof.h"
//#include "V3C_Unit.h"
#include "Nalu.h"

#include <map>
#include <vector>
#include <memory>
#include <cstdlib>
#include <stdexcept>

namespace uvgV3CRTP {
   //Forward declaration
  //class V3C_Gof;
  class V3C_Unit;

  //Define an iterator class
  template <typename SampleType, template <typename> class StreamType>
  class SampleStreamIterator {
  public:
    // Define the iterator traits
    using iterator_category = typename StreamType<SampleType>::const_iterator::iterator_category;
    using value_type = SampleType;
    using difference_type = typename StreamType<SampleType>::const_iterator::difference_type;
    using pointer = value_type*;
    using reference = value_type&;

    SampleStreamIterator(typename StreamType<SampleType>::const_iterator it);
    const SampleType& operator*() const;
    SampleStreamIterator& operator++();
    SampleStreamIterator& operator--();
    SampleStreamIterator& operator+=(difference_type n);
    bool operator==(const SampleStreamIterator& other) const;
    bool operator!=(const SampleStreamIterator& other) const;

    //private:
    typename StreamType<SampleType>::const_iterator it;
  };

  // Define template class
  template <SAMPLE_STREAM_TYPE E>
  class Sample_Stream
  {
  };

  // Explicitly define different types
  template <>
  class Sample_Stream<SAMPLE_STREAM_TYPE::V3C>
  {
  public:
    using SampleType = V3C_Gof;
    template <typename ST>
    using StreamType = std::vector<std::pair<std::map<V3C_UNIT_TYPE, size_t>, ST>>;
    using Iterator = SampleStreamIterator<SampleType, StreamType>;

    Sample_Stream(const uint8_t size_precision = static_cast<uint8_t>(-1)) : size_precision_(size_precision) 
    {
      if (size_precision_ > MAX_V3C_SIZE_PREC && size_precision_ != static_cast<uint8_t>(-1)) {
        throw std::invalid_argument("Size precision needs to be [1,8] or (uint8_t)-1.");
      }
    }
    ~Sample_Stream() = default;

    Sample_Stream(const Sample_Stream&) = delete;
    Sample_Stream& operator=(const Sample_Stream&) = delete;

    Sample_Stream(Sample_Stream&&) = default;
    Sample_Stream& operator=(Sample_Stream&&) = default;

    void push_back(V3C_Unit&& unit);
    void push_back(V3C_Gof&& gof);
    void push_back(Sample_Stream<SAMPLE_STREAM_TYPE::V3C>&& other);

    size_t size() const;
    size_t size(Iterator gof_it) const;
    size_t size(Iterator gof_it, const V3C_UNIT_TYPE unit_type) const;
    size_t num_samples() const;

    uint8_t size_precision() const; // Inferred if size_precision_ == (uint8_t)-1

    Iterator begin() const;
    Iterator end() const;

    const SampleType& front() const;
    const SampleType& back() const;

    std::unique_ptr<char, decltype(&free)> get_bitstream() const;
    std::unique_ptr<char, decltype(&free)> get_bitstream(Iterator gof_it) const;
    std::unique_ptr<char, decltype(&free)> get_bitstream(Iterator gof_it, const V3C_UNIT_TYPE unit_type) const;

  protected:
    size_t write_bitstream(char * const bitstream, Iterator gof_it) const;
    size_t write_bitstream(char * const bitstream, Iterator gof_it, V3C_UNIT_TYPE unit_type) const;

    const uint8_t size_precision_;

  private:
    size_t find_free_gof(const V3C_UNIT_TYPE type) const;
    size_t find_timestamp(const uint32_t timestamp) const;

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

    Sample_Stream(const uint8_t size_precision, const size_t header_size) : header_size(header_size), size_precision_(size_precision) 
    {
      if (size_precision_ > MAX_V3C_SIZE_PREC && size_precision_ != static_cast<uint8_t>(-1)) {
        throw std::invalid_argument("Size precision needs to be [1,8] or (uint8_t)-1.");
      }
    }
    ~Sample_Stream() = default;

    Sample_Stream(const Sample_Stream&) = delete;
    Sample_Stream& operator=(const Sample_Stream&) = delete;

    Sample_Stream(Sample_Stream&&) = default;
    Sample_Stream& operator=(Sample_Stream&&) = default;

    void push_back(Nalu&& unit);
    void push_back(Sample_Stream<SAMPLE_STREAM_TYPE::NAL>&& other);

    size_t size() const;
    size_t num_samples() const;

    uint8_t size_precision() const; // Inferred if size_precision_ == (uint8_t)-1
    const size_t header_size;

    Iterator begin() const;
    Iterator end() const;

    const SampleType& front() const;
    const SampleType& back() const;

  protected:

    //friend size_t V3C_Unit::write_bitstream(char* const bitstream);
    friend class V3C_Unit;
    size_t write_bitstream(char * const bitstream) const;

    const uint8_t size_precision_;

  private:
    StreamType<SampleType> stream_;

  };

  // Explicitly define necessary instantiations so code is linked properly
  //extern template class Sample_Stream<SAMPLE_STREAM_TYPE::V3C>;
  //extern template class Sample_Stream<SAMPLE_STREAM_TYPE::NAL>;

  extern template class SampleStreamIterator<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::SampleType,
                                             Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::StreamType>;
  extern template class SampleStreamIterator<Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::SampleType,
                                             Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::StreamType>;

}