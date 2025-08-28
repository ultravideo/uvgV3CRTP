#include "Sample_Stream.h"

#include "V3C.h"
#include "V3C_Gof.h"
#include "V3C_Unit.h"

#include <numeric>
#include <exception>

namespace uvgV3CRTP {

  // Explicitly define necessary instantiations so code is linked properly
  template class Sample_Stream<SAMPLE_STREAM_TYPE::V3C>;
  template class Sample_Stream<SAMPLE_STREAM_TYPE::NAL>;
  template class SampleStreamIterator<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::SampleType,
                                      Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::StreamType>;
  template class SampleStreamIterator<Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::SampleType,
                                      Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::StreamType>;

  // Iterator definition
  template <typename SampleType, template <typename> class StreamType>
  SampleStreamIterator<SampleType, StreamType>::SampleStreamIterator(typename StreamType<SampleType>::const_iterator it) : it(it)
  {
  }

  template <typename SampleType, template <typename> class StreamType>
  const SampleType & SampleStreamIterator<SampleType, StreamType>::operator*() const
  {
    return it->second;
  }

  template <typename SampleType, template <typename> class StreamType>
  SampleStreamIterator<SampleType, StreamType> & SampleStreamIterator<SampleType, StreamType>::operator++()
  {
    ++it;
    return *this;
  }

  template <typename SampleType, template <typename> class StreamType>
  SampleStreamIterator<SampleType, StreamType> & SampleStreamIterator<SampleType, StreamType>::operator--()
  {
    --it;
    return *this;
  }

  template <typename SampleType, template <typename> class StreamType>
  SampleStreamIterator<SampleType, StreamType>& SampleStreamIterator<SampleType, StreamType>::operator+=(difference_type n)
  {
    it += n;
    return *this;
  }

  template <typename SampleType, template <typename> class StreamType>
  bool SampleStreamIterator<SampleType, StreamType>::operator==(const SampleStreamIterator & other) const
  {
    return it == other.it;
  }

  template <typename SampleType, template <typename> class StreamType>
  bool SampleStreamIterator<SampleType, StreamType>::operator!=(const SampleStreamIterator & other) const
  {
    return it != other.it;
  }


  //template<SAMPLE_STREAM_TYPE E>
  //Sample_Stream<E>::Sample_Stream(const uint8_t size_precision) : size_precision_(size_precision)
  //{
  //}

  bool Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::push_back(Nalu&& nalu, const V3C_UNIT_TYPE type)
  {
    if (stream_.empty() || !nalu.is_timestamp_set())
    {
      // No gofs in stream or nalu timestamp not set, cannot push
      return false;
    }

    // Find matching timestamp
    const size_t push_gof_ind = find_timestamp(nalu.get_timestamp());
    if (push_gof_ind >= stream_.size())
    {
      // No matching timestamp found
      return false;
    }

    stream_.at(push_gof_ind).second.get(type).push_back(std::move(nalu));

    return true;
  }

  void Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::push_back(V3C_Unit&& unit)
  {
    // Find matching timestamp. If timestamp not set assume each gof only has max one of each type of v3c unit and add unit to the first gof without a unit of that type.
    size_t push_gof_ind = unit.is_timestamp_set() ? find_timestamp(unit.get_timestamp()) : find_free_gof(unit.type());
    if (push_gof_ind >= stream_.size())
    {
      // Allocate new gof if the existing gofs are "full"
      this->push_back(V3C_Gof(std::move(unit)));
    }
    else
    {
      auto& push_gof = stream_.at(push_gof_ind);
      push_gof.first[unit.type()] = unit.size();
      push_gof.second.set(std::move(unit));
    }
  }

  void Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::push_back(V3C_Gof && gof)
  {
    std::map<V3C_UNIT_TYPE, size_t> size_map = std::map<V3C_UNIT_TYPE, size_t>{};
    for (const auto&[type, unit] : gof)
    {
      size_map[type] = unit.size();
    }
    
    // Check timestamps to decide how to proceed
    const bool need_init_timestamp = !stream_.empty() && this->back().is_timestamp_set() &&
                                                                 !gof.is_timestamp_set();
    const bool is_timestamp_contiguous = stream_.empty() ||
                                       !(this->back().is_timestamp_set() && gof.is_timestamp_set()) ||
                                        (V3C::calc_new_timestamp(this->back().get_timestamp(), DEFAULT_FRAME_RATE, RTP_CLOCK_RATE) == gof.get_timestamp());
    //if (is_timestamp_contiguous)
    //{
    //  // If timestamps need to be initialized, do it here
    //  if (need_init_timestamp)
    //  {
    //    const auto timestamp = V3C::calc_new_timestamp(this->back().get_timestamp(), DEFAULT_FRAME_RATE, RTP_CLOCK_RATE);
    //    gof.set_timestamp(timestamp);
    //  }
    //  // Push gof directly to stream
    //  stream_.emplace_back(std::move(size_map), std::move(gof));
    //}
    //else
    //{
    //  // gof timestamp will always be set here, so find correct place based on timestamp
    //  // Insert gof to the correct place in the stream
    //  auto insert_ind = 0;
    //  stream_.emplace(std::next(stream_.begin(), insert_ind), std::move(size_map), std::move(gof));
    //}
    // If timestamps need to be initialized, do it here
    if (need_init_timestamp)
    {
      const auto timestamp = V3C::calc_new_timestamp(this->back().get_timestamp(), DEFAULT_FRAME_RATE, RTP_CLOCK_RATE);
      gof.set_timestamp(timestamp);
    }
    // Push gof directly to stream
    stream_.emplace_back(std::move(size_map), std::move(gof));
  
    if (!is_timestamp_contiguous)
    {
      throw TimestampException("Gof timestamp is not contigious with previous gof timestamp");
    }
  }

  void Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::push_back(Sample_Stream<SAMPLE_STREAM_TYPE::V3C>&& other)
  {
    if (this == &other || other.stream_.empty()) return; // No-op if trying to push self or other stream is empty

    // Check if timestamps are contiguous
    const bool need_init_timestamp = !this->stream_.empty() && this->back().is_timestamp_set() &&
                                                              !other.front().is_timestamp_set();
    const bool are_timestamps_contiguous = this->stream_.empty() ||
                                         !(this->back().is_timestamp_set() && other.front().is_timestamp_set()) ||
                                          (V3C::calc_new_timestamp(this->back().get_timestamp(), DEFAULT_FRAME_RATE, RTP_CLOCK_RATE) == other.front().get_timestamp());
    auto timestamp = this->back().get_timestamp();

    // Insert existing stream from other to the end of this stream
    for (auto& [size_map, gof] : other.stream_)
    {
      // If timestamps need to be initialized, do it here
      if (need_init_timestamp)
      {
        timestamp = V3C::calc_new_timestamp(timestamp, DEFAULT_FRAME_RATE, RTP_CLOCK_RATE);
        gof.set_timestamp(timestamp);
      }
      // Push v3c units back individually in case of partial gofs
      for (auto& [type, unit] : gof)
      {
        // Push each unit to this stream
        this->push_back(std::move(unit));
      }
    }

    // Clear other stream
    other.stream_.clear();

    // Raise exception if timestamps are not contigious
    if (!are_timestamps_contiguous)
    {
      throw TimestampException("Timestamps are not contigious concatenating two Sample_Stream<SAMPLE_STREAM_TYPE::V3C> streams");
    }
  }

  /* Calculate GOF size
    *
    + 1 byte of Sample Stream Precision
    +----------------------------------------------------------------+
    + V3C_SIZE_PRECISION bytes of V3C Unit size
    + x1 bytes of whole V3C VPS unit (incl. header)
    +----------------------------------------------------------------+
      Atlas V3C unit
    + V3C_SIZE_PRECISION bytes for V3C Unit size
    + 4 bytes of V3C header
    + 1 byte of NAL Unit Size Precision (x1)
    + NALs count (x1 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+
      Video V3C unit
    + V3C_SIZE_PRECISION bytes for V3C Unit size
    + 4 bytes of V3C header
    + NALs count (4 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+
                .
                .
                .
    +----------------------------------------------------------------+
      Video V3C unit
    + V3C_SIZE_PRECISION bytes for V3C Unit size
    + 4 bytes of V3C header
    + NALs count (4 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+ */

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::size() const
  {
    size_t stream_size = header_size; // Include sample stream header size
    for (Iterator it = stream_.begin(); it != stream_.end(); ++it)
    {
      // Account for sample stream unit size size
      stream_size += size_precision();
      stream_size += it.it->first;
    }
    return stream_size;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::size() const
  {
    size_t stream_size = SAMPLE_STREAM_HDR_LEN; // Include sample stream header size
    for (Iterator it = stream_.begin(); it != stream_.end(); ++it)
    {
      stream_size += size(it);
    }
    return stream_size;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::size(Iterator gof_it) const
  {
    size_t stream_size = 0;
    for (const auto&[type, sub_size] : gof_it.it->first)
    {
      // Get size of v3c unit
      stream_size += size(gof_it, type);
    }
    return stream_size;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::size(Iterator gof_it, const V3C_UNIT_TYPE unit_type) const
  {
    // Account for sample stream unit size size
    return size_precision() + gof_it.it->first.at(unit_type);
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::num_samples() const
  {
    return stream_.size();
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::num_samples() const
  {
    return stream_.size();
  }

  static uint8_t calc_min_size_precision(const size_t size)
  {
    if (size < (1LLU << (1 * SIZE_PREC_MULT))) return 1; // 1 is the smallest allowed precision and number of size bits is prec * SIZE_PREC_MULT
    
    // Precision should be in range [1, 8] so start checking from 4
    if (size < (1LLU << (4 * SIZE_PREC_MULT)))
    {
      if (size < (1LLU << (3 * SIZE_PREC_MULT)))
      {
        if (size < (1LLU << (2 * SIZE_PREC_MULT)))
        {
          return 2;
        } 
        else //(1 << 2 * SIZE_PREC_MULT)) <= size < (1 << 3 * SIZE_PREC_MULT)) 
        {
          return 3;
        }
      }
      else //(1 << 3 * SIZE_PREC_MULT)) <= size < (1 << 4 * SIZE_PREC_MULT)) 
      {
        return 4;
      }
    } else // (size >= (1 << 4 * SIZE_PREC_MULT))
    {
      if (size < (1LLU << (6 * SIZE_PREC_MULT)))
      {
        if (size < (1LLU << (5 * SIZE_PREC_MULT)))
        {
          return 5;
        } 
        else //(1 << 5 * SIZE_PREC_MULT)) <= size < (1 << 6 * SIZE_PREC_MULT)) 
        {
          return 6;
        }
      } 
      else if /*(1 << 6 * SIZE_PREC_MULT) <=*/ (size < (1LLU << (7 * SIZE_PREC_MULT)))
      {
        return 7;
      }
    }

    // Would overflow size_t anyway
    //if (size >= (1LLU << (8 * SIZE_PREC_MULT))) throw std::runtime_error("Size exceeds maximum size precision range");

    return 8;
  }

  uint8_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::size_precision() const
  {
    // If size_precision_ is -1, infer it from max size sample size
    if (size_precision_ != static_cast<uint8_t>(-1)) return size_precision_;

    size_t max_sample_size = 0;
    for (const auto& [sizes, sample]: stream_)
    {
      size_t cur_size = std::accumulate(sizes.cbegin(), sizes.cend(), 0LLU,
        [](const size_t a, decltype(*sizes.cbegin()) b)
        {
          return a + b.second;
        });
      if (max_sample_size < cur_size)
      {
        max_sample_size = cur_size;
      }
    }

    return calc_min_size_precision(max_sample_size);
  }

  uint8_t Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::size_precision() const
  {
    // If size_precision_ is -1, infer it from max size sample size
    if (size_precision_ != static_cast<uint8_t>(-1)) return size_precision_;

    size_t max_sample_size = 0;
    for (const auto&[size, sample] : stream_)
    {
      if (max_sample_size < size)
      {
        max_sample_size = size;
      }
    }

    return calc_min_size_precision(max_sample_size);
  }

  typename Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::Iterator Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::begin() const
  {
    return Iterator(stream_.begin());
  }

  typename Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::Iterator Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::end() const
  {
    return Iterator(stream_.end());
  }

  const Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::SampleType& Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::front() const
  {
    return stream_.front().second;
  }

  const Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::SampleType& Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::back() const
  {
    return stream_.back().second;
  }

  typename Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::Iterator Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::begin() const
  {
    return Iterator(stream_.begin());
  }

  typename Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::Iterator Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::end() const
  {
    return Iterator(stream_.end());
  }

  const Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::SampleType& Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::front() const
  {
    return stream_.front().second;
  }

  const Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::SampleType& Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::back() const
  {
    return stream_.back().second;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::find_free_gof(const V3C_UNIT_TYPE type) const
  {
    if (stream_.empty()) return 0; // No gofs yet so return first index

    // Iterate backwards until a gof is found that has the respective v3c unit and return the next index
    size_t ind = stream_.size();
    for (auto it = std::prev(this->end()); it != this->begin(); --it)
    {
      if ((*it).find(type) != (*it).cend()) // Check if type exists in cur gof
      {
        //Found a full gof, return next index (i.e. current ind)
        return ind;
      }
      ind--;
    }

    // Need to check if first gof is free. If it is return 0, else the second gof index i.e. ind = 1
    return (this->front().find(type) != this->front().cend()) ? ind : 0;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::find_timestamp(const uint32_t timestamp) const
  {
    if (stream_.empty()) return 0; // No gofs yet so return first index

    const uint32_t first_timestamp = this->front().get_timestamp();
    const uint32_t last_timestamp = this->back().get_timestamp();
    const bool in_range = first_timestamp <= last_timestamp ?
                         (first_timestamp <= timestamp && timestamp <= last_timestamp) :
                         (timestamp <= last_timestamp || first_timestamp <= timestamp); // Account for wrap-around
    if (!in_range) return stream_.size(); // Timestamp out of range
    if (timestamp == first_timestamp) return 0;
    if (timestamp == last_timestamp) return stream_.size() - 1;

    size_t ind = stream_.size();
    for (auto it = std::prev(this->end()); it != this->begin(); --it )
    {
      ind--;
      if ((*it).get_timestamp() == timestamp)
      {
        // Found matching timestamp return ind
        return ind;
      }
    }

    // Matching timestamp not found
    return stream_.size();
  }

  void Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::push_back(Nalu&& unit)
  {
    const auto size = unit.size();
    stream_.emplace_back( size, std::move(unit));
  }

  void Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::push_back(Sample_Stream<SAMPLE_STREAM_TYPE::NAL>&& other)
  {
    // Insert existing stream from other to the end of this stream
    stream_.insert(stream_.end(),
      std::make_move_iterator(other.stream_.begin()),
      std::make_move_iterator(other.stream_.end()));

    // Clear other stream
    other.stream_.clear();
  }


  std::unique_ptr<char, decltype(&free)> Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::get_bitstream() const
  {
    // Allocate enough memory for whole sample stream
    const size_t len = size();
    std::unique_ptr<char, decltype(&free)> bitstream((char *)malloc(len), &free);
    size_t ptr = 0;

    // Insert sample stream header
    ptr += V3C::write_size_precision(&bitstream.get()[ptr], size_precision());

    // Start copying data from v3c units
    for (Iterator it = stream_.begin(); it != stream_.end(); ++it)
    {
      if (ptr + (*it).size() > len) throw std::length_error(std::string("Error: about to exceed allocated memory in ") + __func__ +
        " at " + __FILE__ + ":" + std::to_string(__LINE__));
      // Write current gof
      ptr += write_bitstream(&bitstream.get()[ptr], it);
    }

    if (ptr != len) throw std::logic_error(std::string("Error: size mismatch in ") + __func__ +
      " at " + __FILE__ + ":" + std::to_string(__LINE__));

    return bitstream;
  }

  std::unique_ptr<char, decltype(&free)> Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::get_bitstream(Iterator gof_it) const
  {
    const size_t len = size(gof_it);
    std::unique_ptr<char, decltype(&free)> bitstream((char *)malloc(len), &free);

    size_t ptr = write_bitstream(bitstream.get(), gof_it);

    if (ptr != len) throw std::logic_error(std::string("Error: size mismatch in ") + __func__ +
      " at " + __FILE__ + ":" + std::to_string(__LINE__));

    return bitstream;
  }

  std::unique_ptr<char, decltype(&free)> Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::get_bitstream(Iterator gof_it, const V3C_UNIT_TYPE unit_type) const
  {
    const size_t len = size(gof_it, unit_type);
    std::unique_ptr<char, decltype(&free)> bitstream((char *)malloc(len), &free);

    size_t ptr = write_bitstream(bitstream.get(), gof_it, unit_type);
    
    if (ptr != len) throw std::logic_error(std::string("Error: size mismatch in ") + __func__ +
      " at " + __FILE__ + ":" + std::to_string(__LINE__));

    return bitstream;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::write_bitstream(char * const bitstream, Iterator gof_it) const
  {
    size_t ptr = 0;
    for (const auto&[type, unit] : gof_it.it->second)
    {
      // Write v3c unit bitstreams
      ptr += write_bitstream(&bitstream[ptr], gof_it, type);
    }
    return ptr;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::write_bitstream(char * const bitstream, Iterator gof_it, V3C_UNIT_TYPE unit_type) const
  {
    size_t ptr = 0;

    // Insert sample stream unit size
    ptr += V3C::write_sample_stream_size(&bitstream[ptr], gof_it.it->first.at(unit_type), size_precision());

    // Write data to bitstream
    ptr += (*gof_it).get(unit_type).write_bitstream(&bitstream[ptr]);
    
    return ptr;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::write_bitstream(char * const bitstream) const
  {
    // Insert sample stream header
    size_t ptr = 0;
    if (header_size > 0)
    {
      ptr += V3C::write_size_precision(&bitstream[ptr], size_precision()); // TODO: check that header_size matches how much is written
    }

    // Start copying data from nal units
    for (const auto&[size, data] : stream_)
    {
      // Insert sample stream unit size
      ptr += V3C::write_sample_stream_size(&bitstream[ptr], size, size_precision());

      // Write data to bitstream
      memcpy(&bitstream[ptr], data.bitstream(), data.size());
      ptr += data.size();
    }
    return ptr;
  }

}