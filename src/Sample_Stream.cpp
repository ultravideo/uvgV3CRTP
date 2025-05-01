#include "Sample_Stream.h"

#include "V3C.h"
#include "V3C_Gof.h"
#include "V3C_Unit.h"

namespace v3cRTPLib {

  template<SAMPLE_STREAM_TYPE E>
  inline Sample_Stream<E>::Sample_Stream(const uint8_t size_precision) : size_precision(size_precision)
  {
  }

  template<SAMPLE_STREAM_TYPE E>
  void Sample_Stream<E>::push_back(V3C_Unit&& unit, size_t size)
  {
    // Assume each gof only has max one of each type of v3c unit. Add unit to the first gof without a unit of that type.
    size_t push_gof_ind = find_free_gof(unit.type());
    if (push_gof_ind >= stream_.size())
    {
      // Allocate new gof if the existing gofs are "full"
      this->push_back(V3C_Gof());
      push_gof_ind = stream_.size() - 1;
    }
    auto& push_gof = stream_.at(push_gof_ind);
    push_gof.first[unit.type()] = size;
    push_gof.second.set(std::forward<V3C_Unit>(unit));
  }

  template<SAMPLE_STREAM_TYPE E>
  void Sample_Stream<E>::push_back(V3C_Gof && gof)
  {
    std::map<V3C_UNIT_TYPE, size_t> size_map = std::map<V3C_UNIT_TYPE, size_t>{};
    for (const auto&[type, unit] : gof)
    {
      size_map[type] = unit.size();
    }

    // Push gof directly to stream
    stream_.emplace_back(std::move(size_map), std::forward<V3C_Gof>(gof));
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

  template<SAMPLE_STREAM_TYPE E>
  size_t Sample_Stream<E>::size() const
  {
    size_t stream_size = SAMPLE_STREAM_HDR_LEN; // Include sample stream header size
    for (const auto&[size, data] : stream_)
    {
      if constexpr (E == SAMPLE_STREAM_TYPE::NAL) {
        // Account for sample stream unit size size
        stream_size += size_precision;
        stream_size += size;
      } else {
        for (const auto&[type, sub_size] : size)
        {
          // Account for sample stream unit size size
          stream_size += size_precision;
          stream_size += sub_size;
        }
      }
    }
    return stream_size;
  }

  template<SAMPLE_STREAM_TYPE E>
  typename Sample_Stream<E>::Iterator Sample_Stream<E>::begin() const
  {
    return Iterator(stream_.begin());
  }

  template<SAMPLE_STREAM_TYPE E>
  typename Sample_Stream<E>::Iterator Sample_Stream<E>::end() const
  {
    return Iterator(stream_.end());
  }

  template<SAMPLE_STREAM_TYPE E>
  inline Sample_Stream<E>::Iterator::Iterator(typename sample_stream_data::iterator it): it(it)
  {
  }

  template<SAMPLE_STREAM_TYPE E>
  inline const typename Sample_Stream<E>::sample_type & Sample_Stream<E>::Iterator::operator*() const
  {
    return it->second;
  }

  template<SAMPLE_STREAM_TYPE E>
  inline typename Sample_Stream<E>::Iterator & Sample_Stream<E>::Iterator::operator++()
  {
    ++it;
    return *this;
  }

  template<SAMPLE_STREAM_TYPE E>
  inline bool Sample_Stream<E>::Iterator::operator==(const Iterator & other) const
  {
    return it == other.it;
  }

  template<SAMPLE_STREAM_TYPE E>
  inline bool Sample_Stream<E>::Iterator::operator!=(const Iterator & other) const
  {
    return it != other.it;
  }

  template<SAMPLE_STREAM_TYPE E>
  size_t Sample_Stream<E>::find_free_gof(V3C_UNIT_TYPE type) const
  {
    size_t ind = -1;

    for (const auto& gof: stream_)
    {
      ind++;
      if (gof.first.find(type) != gof.first.end())
      {
        break;
      }
    }

    return ind;
  }

  void Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::push_back(Nalu&& unit, size_t size)
  {
    stream_.emplace_back( size, std::forward<Nalu>(unit));
  }


  template<SAMPLE_STREAM_TYPE E>
  std::unique_ptr<char[]> Sample_Stream<E>::get_bitstream() const
  {
    // Allocate enough memory for whole sample stream
    std::unique_ptr<char[]> bitstream = std::make_unique<char[]>(size());
    size_t ptr = 0;

    // Insert sample stream header
    ptr += V3C::write_size_precision(&bitstream.get()[ptr], size_precision);

    // Start copying data from v3c units
    for (const auto&[size, data] : stream_)
    {
      for (const auto&[type, unit] : data)
      {
        // Insert sample stream unit size
        ptr += V3C::write_sample_stream_size(&bitstream.get()[ptr], size[type], size_precision);

        // Write data to bitstream
        ptr += unit.write_bitstream(&bitstream.get()[ptr]);
      }
    }

    return bitstream;
  }

  size_t Sample_Stream<SAMPLE_STREAM_TYPE::NAL>::write_bitstream(char * const bitstream) const
  {
    // Insert sample stream header
    size_t ptr = 0;
    ptr += V3C::write_size_precision(bitstream, size_precision);

    // Start copying data from nal units
    for (const auto&[size, data] : stream_)
    {
      // Insert sample stream unit size
      ptr += V3C::write_sample_stream_size(bitstream, size, size_precision);

      // Write data to bitstream
      memcpy(&bitstream[ptr], data.bitstream(), data.size());
      ptr += data.size();
    }
    return ptr;
  }

}