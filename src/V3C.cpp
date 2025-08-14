#include "V3C.h"

#include "V3C_Gof.h"
#include "V3C_Unit.h"
#include "Sample_Stream.h"

#include <cassert>
#include <type_traits>
#include <sstream>
#include <stdexcept>

namespace uvgV3CRTP {

  // Explicitly define necessary instantiations so code is linked properly
  template void V3C::write_out_of_band_info<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(std::ostream&, Sample_Stream<SAMPLE_STREAM_TYPE::V3C> const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C_Gof>(std::ostream&, V3C_Gof const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C_Unit>(std::ostream&, V3C_Unit const&, INFO_FMT);
  template V3C::InfoDataType V3C::read_out_of_band_info<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(std::istream&, INFO_FMT, INIT_FLAGS);
  template size_t V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::V3C>(V3C_UNIT_TYPE type);
  template size_t V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::NAL>(V3C_UNIT_TYPE type);


  V3C::V3C(const INIT_FLAGS init_flags, const char * endpoint_address, const uint16_t port, int stream_flags)
  {
    /* Create the necessary uvgRTP media streams */
    session_ = ctx_.create_session(std::string(endpoint_address));

    if (!session_) {
      throw ConnectionException("Failed to create uvgRTP session. Check address.");
    }

    // Init v3c streams
    stream_flags |= RCE_NO_H26X_PREPEND_SC;

    for (auto unit_type : unit_types_from_init_flag(init_flags)) {
      streams_[unit_type] = session_->create_stream(port, get_format(unit_type), stream_flags);
      if (!streams_[unit_type]) {
        throw ConnectionException("RTP stream creation failed. Check ports and stream flags.");
      }
    }
  }

  V3C::V3C(const INIT_FLAGS init_flags, const char * local_address, const char * remote_address,  const uint16_t src_port, const uint16_t dst_port, int stream_flags)
  {
    /* Create the necessary uvgRTP media streams */
    std::pair<std::string, std::string> addresses(local_address, remote_address);
    session_ = ctx_.create_session(addresses);

    if (!session_) {
      throw ConnectionException("Failed to create uvgRTP session. Check address.");
    }

    // Init v3c streams
    stream_flags |= RCE_NO_H26X_PREPEND_SC;

    for (auto unit_type : unit_types_from_init_flag(init_flags)) {
      streams_[unit_type] = session_->create_stream(src_port, dst_port, get_format(unit_type), stream_flags);
      if (!streams_[unit_type]) {
        throw ConnectionException("RTP stream creation failed. Check ports and stream flags.");
      }
    }
  }


  V3C::~V3C()
  {
    for (auto&[type, stream] : streams_) {
      session_->destroy_stream(stream);
    }
    streams_.clear();

    if (session_)
    {
      // Session must be destroyed manually
      ctx_.destroy_session(session_);
    }
  }

  std::vector<V3C_UNIT_TYPE> V3C::unit_types_from_init_flag(INIT_FLAGS flags)
  {
    auto out = std::vector<V3C_UNIT_TYPE>();

    if (is_set(flags, INIT_FLAGS::VPS)) out.push_back(V3C_VPS);
    if (is_set(flags, INIT_FLAGS::AD)) out.push_back(V3C_AD);
    if (is_set(flags, INIT_FLAGS::OVD)) out.push_back(V3C_OVD);
    if (is_set(flags, INIT_FLAGS::GVD)) out.push_back(V3C_GVD);
    if (is_set(flags, INIT_FLAGS::AVD)) out.push_back(V3C_AVD);
    if (is_set(flags, INIT_FLAGS::PVD)) out.push_back(V3C_PVD);
    if (is_set(flags, INIT_FLAGS::CAD)) out.push_back(V3C_CAD);

    return out;
  }

  INIT_FLAGS V3C::init_flags_from_unit_types(const std::vector<V3C_UNIT_TYPE>& unit_types)
  {
    INIT_FLAGS flags = INIT_FLAGS::NUL;
    for (const auto& type : unit_types)
    {
      switch (type)
      {
      case V3C_VPS:
        flags = flags | INIT_FLAGS::VPS;
        break;
      case V3C_AD:
        flags = flags | INIT_FLAGS::AD;
        break;
      case V3C_OVD:
        flags = flags | INIT_FLAGS::OVD;
        break;
      case V3C_GVD:
        flags = flags | INIT_FLAGS::GVD;
        break;
      case V3C_AVD:
        flags = flags | INIT_FLAGS::AVD;
        break;
      case V3C_PVD:
        flags = flags | INIT_FLAGS::PVD;
        break;
      case V3C_CAD:
        flags = flags | INIT_FLAGS::CAD;
        break;
      default:
        throw std::invalid_argument("Invalid V3C unit type in init_flags_from_unit_types");
      }
    }
    return flags;
  }

  Sample_Stream<SAMPLE_STREAM_TYPE::V3C> V3C::parse_bitstream(const char * const bitstream, const size_t len)
  {
    // First byte should be the v3c sample stream header
    uint8_t v3c_size_precision = parse_size_precision(bitstream);
    if (!(0 < v3c_size_precision && v3c_size_precision <= 8))
    {
      throw ParseException(
        std::string("Error parsing bitstream in ") + __func__ +
        " at " + __FILE__ + ":" + std::to_string(__LINE__) + " with error: sample stream precision should be in range[1, 8]"
      );
    }

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> sample_stream(v3c_size_precision);
    
    // Start processing v3c units
    for (size_t ptr = SAMPLE_STREAM_HDR_LEN; ptr < len;) {

      // Read the V3C unit size 
      size_t v3c_size = parse_sample_stream_size(&bitstream[ptr], v3c_size_precision);
      ptr += v3c_size_precision; // Jump over the V3C unit size bytes

      try
      {
        // Inside v3c unit now
        V3C_Unit new_unit(&bitstream[ptr], v3c_size);
        ptr += v3c_size;

        sample_stream.push_back(std::move(new_unit));

      }
      catch (const std::exception& e)
      {
        throw ParseException(
          std::string("Error parsing bitstream in ") + __func__ +
          " at " + __FILE__ + ":" + std::to_string(__LINE__) + " with error: " + e.what()
        );
      }
    }

    return sample_stream;
  }

  template<SAMPLE_STREAM_TYPE E>
  size_t V3C::sample_stream_header_size(V3C_UNIT_TYPE type)
  {
    if constexpr (E == SAMPLE_STREAM_TYPE::NAL)
    {
      return (type == V3C_AD || type == V3C_CAD) ? SAMPLE_STREAM_HDR_LEN : 0; // TODO: figure out correct procedure for video sub-bitstream;
    }
    else
    {
      return (type != V3C_UNDEF) ? SAMPLE_STREAM_HDR_LEN : 0;
    }
  }

  uint8_t V3C::parse_size_precision(const char * const bitstream)
  {
    return ((bitstream[0] >> 5) & 0b111) + 1;
  }

  size_t V3C::write_size_precision(char * const bitstream, const uint8_t precision)
  {
    const uint8_t hdr = ((precision - 1) & 0b111) << 5;
    bitstream[0] = hdr;
    return SAMPLE_STREAM_HDR_LEN;
  }

  size_t V3C::parse_sample_stream_size(const char * const bitstream, const uint8_t precision)
  {
    if (precision > MAX_V3C_SIZE_PREC || precision <= 0)
    {
      throw ParseException(
        std::string("Error parsing bitstream in ") + __func__ +
        " at " + __FILE__ + ":" + std::to_string(__LINE__) + " with error: sample stream size precision should be in range[1, 8]"
      );
    }
    uint8_t size[MAX_V3C_SIZE_PREC] = { 0 };
    memcpy(size, bitstream, precision);
    size_t combined_size = combineBytes(size, precision);

    return combined_size;
  }

  size_t V3C::write_sample_stream_size(char * const bitstream, const size_t size, const uint8_t precision)
  {
    if (precision <= 0) return 0;
    if (precision > MAX_V3C_SIZE_PREC)
    {
      throw std::length_error(
        std::string("Error writing bitstream in ") + __func__ +
        " at " + __FILE__ + ":" + std::to_string(__LINE__) + " with error: sample stream size precision should be in range[1, 8]"
      );
    } 

    uint8_t size_arr[MAX_V3C_SIZE_PREC] = { 0 };

    convert_size_big_endian(size, size_arr, precision);
    memcpy(bitstream, size_arr, precision);
    return precision;
  }


  uvgrtp::media_stream* V3C::get_stream(const V3C_UNIT_TYPE type) const
  {
    if (streams_.find(type) != streams_.end()) {
      return streams_.at(type);
    } else {
      throw ConnectionException("Media stream for given type not initialized");
    }
  }

  // TODO: Figure out correct format from bitstream etc. if not H265
  RTP_FORMAT V3C::get_format(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case V3C_VPS:
      return RTP_FORMAT_GENERIC;
    case V3C_AD:
      return RTP_FORMAT_ATLAS;
    case V3C_OVD:
      return RTP_FORMAT_H265;
    case V3C_GVD:
      return RTP_FORMAT_H265;
    case V3C_AVD:
      return RTP_FORMAT_H265;
    case V3C_PVD:
      return RTP_FORMAT_H265;
    case V3C_CAD:
      return RTP_FORMAT_ATLAS;
    default:
      return RTP_FORMAT_GENERIC;
    }
  }

  //TODO: SSRC value could be set in a better way (random). Need to match the sender/receiver values respectively
  int V3C::unit_type_to_ssrc(const V3C_UNIT_TYPE type)
  {
    return 1 + static_cast<int>(type);
  }

  RTP_FLAGS V3C::get_flags(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case V3C_VPS:
      return RTP_NO_FLAGS;
    case V3C_AD:
      return RTP_NO_FLAGS;
    case V3C_OVD:
      return RTP_NO_H26X_SCL;
    case V3C_GVD:
      return RTP_NO_H26X_SCL;
    case V3C_AVD:
      return RTP_NO_H26X_SCL;
    case V3C_PVD:
      return RTP_NO_H26X_SCL;
    case V3C_CAD:
      return RTP_NO_FLAGS;
    default:
      return RTP_NO_FLAGS;
    }
  }

  template <typename DT, typename FT>
  static inline bool has_field(const DT& data, const FT& field)
  {
    return (data.find(field) != data.end());
  }

  template <typename T = size_t, typename FT>
  static inline T& get_as( V3C::InfoDataType& data, FT&& field)
  {
    if (!has_field(data, field)) data[field] = {};
    if constexpr (std::is_convertible_v < decltype(INFO_FIELD_TYPES{}.s)&, T& > )
    {
      return static_cast<T&>(data.at(field).s);
    }
    else if constexpr (std::is_convertible_v < decltype(INFO_FIELD_TYPES{}.ui8)&, T& > )
    {
      return static_cast<T&>(data.at(field).ui8);
    }
    else if constexpr (std::is_convertible_v < decltype(INFO_FIELD_TYPES{}.b)&, T& > )
    {
      return static_cast<T&>(data.at(field).b);
    }
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename TF, typename TV>
  static inline void process(std::ostream& out, TF&& field, TV&& value)
  {
    if constexpr (F == INFO_FMT::RAW)
    {
      assert(field); //Suppress warnings
      out.write(reinterpret_cast<const char*>(&value), sizeof(TV));
    }
    else
    {
      if constexpr (std::is_integral<typename std::decay<TV>::type>::value)
      {
        out << field << static_cast<int>(value) << std::endl;
      }
      else 
      {
        out << field << value << std::endl;
      }
    }
  }

  template<typename T>
  constexpr bool need_integral_conversion()
  {
    return std::is_same<typename std::decay<T>::type, std::uint8_t>::value |
           std::is_same<typename std::decay<T>::type, std::int8_t>::value;
  }

  template<INFO_FMT F = INFO_FMT::LOGGING, typename TF, typename TV>
  static inline void process(std::istream& in, TF&& field, TV&& value, const char delim = '\n')
  {
    if constexpr (F == INFO_FMT::RAW)
    {
      assert(field); //Suppress warnings
      in.read(reinterpret_cast<char*>(&value), sizeof(TV));
    }
    else
    {
      std::string in_field(field);
      std::string field_str(in_field.size(), '\0');
      in.read(&field_str[0], in_field.size());
      if (field_str == in_field)
      {
        // Read the value if the field matches
        if constexpr (need_integral_conversion<TV>())
        {
          // If value is interpreted as a char, make sure to convert it to an integer
          int64_t tmp;
          in >> tmp >> std::ws; // Read the value as an integer
          value = static_cast<typename std::decay<TV>::type>(tmp);
        }
        else {
          in >> value >> std::ws; // Consume any whitespace after the value
        }
        if (in.peek() == delim) {
          in.ignore(); // Ignore the delimiter if present (non-whitespace)
        }
      }
      else
      {
        // Try reading from the start in case fields are not in the expected order
        in.clear(); // Clear any error flags
        in.seekg(0, std::ios::beg); // Reset the stream position
        for(std::string line; std::getline(in, line, delim);) {
          if (line.find(in_field) != std::string::npos) {
            // Found the field, read the value
            std::istringstream(line.substr(in_field.size() + 1)) >> value; // Skip the field name and read the value
            return;
          }
        }
        // Failed to find the field, throw an error
        throw ParseException("Field mismatch: did not find field '" + in_field + "'");
      }
    }
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename TF, typename TV>
  static inline void process_num(Stream& out, TF&& field, TV&& value)
  {
    std::ostringstream tmp;
    if constexpr (F == INFO_FMT::PARAM)
    {
      tmp << "constexpr size_t EXPECTED_NUM_" << field << " = ";
    }
    else
    {
      tmp << "  number of " << field << ": ";
    }
    process<F>(out, tmp.str().c_str(), std::forward<TV>(value));
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename TF, typename TV>
  static inline void process_prec(Stream& out, TF&& field, TV&& value)
  {
    std::ostringstream tmp;
    if constexpr (F == INFO_FMT::PARAM)
    {
      tmp << "constexpr uint8_t " << field << "_SIZE_PRECISION = ";
    }
    else
    {
      tmp << "  " << field << " nalu size precision: ";
    }
    process<F>(out, tmp.str().c_str(), std::forward<TV>(value));
  }

  template <INFO_FMT F, typename Stream>
  static inline void preample(Stream&)
  {

  }
  template <>
  inline void preample<INFO_FMT::LOGGING>(std::ostream& out)
  {
    out << "Summary:" << std::endl;
    out << "--------" << std::endl;
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename T>
  static inline void postample(Stream&, T&)
  {
    
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename T>
  static inline void postample(std::ostream& ostream, T& data)
  {
    if constexpr (F == INFO_FMT::LOGGING)
    {
      if (has_field(data, INFO_FIELDS::VAR_NAL_PREC) && get_as<bool>(data, INFO_FIELDS::VAR_NAL_PREC)) {
        ostream << "  Warning: Variable nal size precision between GOFs!" << std::endl;
      }
      if (has_field(data, INFO_FIELDS::VAR_NAL_NUM) && get_as<bool>(data, INFO_FIELDS::VAR_NAL_NUM)) {
        ostream << "  Warning: Variable number of nals between GOFs!" << std::endl;
      }
    }
  }


  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass, typename Stream, typename T>
  static void process_out_of_band_info(Stream& stream, T & data)
  {
    preample<F>(stream);

    if constexpr (std::is_same<DataClass, Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>::value)
    {
      if (has_field(data, INFO_FIELDS::NUM_GOF))  process_num<F>(stream, "GOFs", get_as<size_t>(data, INFO_FIELDS::NUM_GOF));
      if (has_field(data, INFO_FIELDS::NUM_VPS)) process_num<F>(stream, "VPSs", get_as<size_t>(data, INFO_FIELDS::NUM_VPS));
      if (has_field(data, INFO_FIELDS::NUM_AD_NALU))  process_num<F>(stream, "AD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_AD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_OVD_NALU)) process_num<F>(stream, "OVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_OVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_GVD_NALU)) process_num<F>(stream, "GVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_GVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_AVD_NALU)) process_num<F>(stream, "AVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_AVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_CAD_NALU)) process_num<F>(stream, "CAD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_PVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_PVD_NALU)) process_num<F>(stream, "PVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_PVD_NALU));
      if (has_field(data, INFO_FIELDS::V3C_SIZE_PREC)) process_prec<F>(stream, "V3C", get_as<uint8_t>(data, INFO_FIELDS::V3C_SIZE_PREC));
      if (has_field(data, INFO_FIELDS::ATLAS_NAL_SIZE_PREC)) process_prec<F>(stream, "AtlasNAL", get_as<uint8_t>(data, INFO_FIELDS::ATLAS_NAL_SIZE_PREC));
      if (has_field(data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC)) process_prec<F>(stream, "Video", get_as<uint8_t>(data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC));
    }
    else if constexpr (std::is_same<DataClass, V3C_Gof>::value || std::is_same<DataClass, V3C_Unit>::value)
    {
      if (has_field(data, INFO_FIELDS::NUM_VPS)) process_num<F>(stream, "VPSs", get_as<size_t>(data, INFO_FIELDS::NUM_VPS));
      if (has_field(data, INFO_FIELDS::NUM_AD_NALU))  process_num<F>(stream, "AD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_AD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_OVD_NALU)) process_num<F>(stream, "OVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_OVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_GVD_NALU)) process_num<F>(stream, "GVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_GVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_AVD_NALU)) process_num<F>(stream, "AVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_AVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_CAD_NALU)) process_num<F>(stream, "CAD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_PVD_NALU));
      if (has_field(data, INFO_FIELDS::NUM_PVD_NALU)) process_num<F>(stream, "PVD_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_PVD_NALU));
      if (has_field(data, INFO_FIELDS::ATLAS_NAL_SIZE_PREC)) process_prec<F>(stream, "AtlasNAL", get_as<uint8_t>(data, INFO_FIELDS::ATLAS_NAL_SIZE_PREC));
      if (has_field(data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC)) process_prec<F>(stream, "Video", get_as<uint8_t>(data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC));
    }

    postample<F>(stream, data);
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass, typename T>
  static void populate_info_data(const DataClass & data, T& info_data)
  {

  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename T>
  static void populate_info_data(const Sample_Stream<SAMPLE_STREAM_TYPE::V3C> & data, T& info_data)
  {
    std::map<V3C_UNIT_TYPE, uint8_t> nal_prec;
    std::map<V3C_UNIT_TYPE, size_t> nal_num;
    get_as(info_data, INFO_FIELDS::NUM_GOF) = 0;
    get_as<bool>(info_data, INFO_FIELDS::VAR_NAL_NUM) = false;
    get_as<bool>(info_data, INFO_FIELDS::VAR_NAL_PREC) = false;
    for (const auto& gof : data)
    {
      for (const auto&[type, unit] : gof)
      {
        if (get_as(info_data, INFO_FIELDS::NUM_GOF) == 0 || !has_field(nal_num, type))
        {
          populate_info_data<F>(unit, info_data);
          nal_num[type] = unit.num_nalus();
          nal_prec[type] = unit.nal_size_precision();
        } 
        else if (type != V3C_VPS)
        {
          get_as<bool>(info_data, INFO_FIELDS::VAR_NAL_NUM) |= (nal_num[type] != unit.num_nalus());
          get_as<bool>(info_data, INFO_FIELDS::VAR_NAL_PREC) |= (nal_prec[type] != unit.nal_size_precision());
        }
        else
        {
          get_as(info_data, INFO_FIELDS::NUM_VPS) += 1;
        }
      }
      get_as(info_data, INFO_FIELDS::NUM_GOF) += 1;
    }
    get_as<uint8_t>(info_data, INFO_FIELDS::V3C_SIZE_PREC) = data.size_precision();
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename T>
  static void populate_info_data(const V3C_Gof & data, T& info_data)
  {
    for (const auto&[type, unit] : data)
    {
      populate_info_data<F>(unit, info_data);
    }
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename T>
  static void populate_info_data(const V3C_Unit & data, T& info_data)
  {
    switch (data.type())
    {
    case V3C_VPS:
      get_as(info_data, INFO_FIELDS::NUM_VPS) += 1;
      break;
    case V3C_AD:
      get_as(info_data, INFO_FIELDS::NUM_AD_NALU) = data.num_nalus();
      get_as<uint8_t>(info_data, INFO_FIELDS::ATLAS_NAL_SIZE_PREC) = data.nal_size_precision();
      break;
    case V3C_OVD:
      get_as(info_data, INFO_FIELDS::NUM_OVD_NALU) = data.num_nalus();
      get_as<uint8_t>(info_data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC) = data.nal_size_precision();
      break;
    case V3C_GVD:
      get_as(info_data, INFO_FIELDS::NUM_GVD_NALU) = data.num_nalus();
      get_as<uint8_t>(info_data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC) = data.nal_size_precision();
      break;
    case V3C_AVD:
      get_as(info_data, INFO_FIELDS::NUM_AVD_NALU) = data.num_nalus();
      get_as<uint8_t>(info_data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC) = data.nal_size_precision();
      break;
    case V3C_PVD:
      get_as(info_data, INFO_FIELDS::NUM_PVD_NALU) = data.num_nalus();
      get_as<uint8_t>(info_data, INFO_FIELDS::VIDEO_NAL_SIZE_PREC) = data.nal_size_precision();
      break;
    case V3C_CAD:
      get_as(info_data, INFO_FIELDS::NUM_CAD_NALU) = data.num_nalus();
      get_as<uint8_t>(info_data, INFO_FIELDS::ATLAS_NAL_SIZE_PREC) = data.nal_size_precision();
      break;
    default:
      break;
    }
  }

  template<typename Stream, typename DataClass>
  static V3C::InfoDataType _out_of_band_info(Stream& stream, const DataClass& data, INFO_FMT fmt)
  {
    V3C::InfoDataType info_data;
    switch (fmt)
    {
    case INFO_FMT::PARAM:
      populate_info_data<INFO_FMT::PARAM>(data, info_data);
      process_out_of_band_info<INFO_FMT::PARAM, DataClass>(stream, info_data);
      break;
    case INFO_FMT::RAW:
      populate_info_data<INFO_FMT::RAW>(data, info_data);
      process_out_of_band_info<INFO_FMT::RAW, DataClass>(stream, info_data);
      break;
    case INFO_FMT::BASE64:
      populate_info_data<INFO_FMT::BASE64>(data, info_data);
      process_out_of_band_info<INFO_FMT::BASE64, DataClass>(stream, info_data);
      break;
    default:
      populate_info_data<INFO_FMT::LOGGING>(data, info_data);
      process_out_of_band_info<INFO_FMT::LOGGING, DataClass>(stream, info_data);
      break;
    }
    return info_data;
  }
  
  template<typename DataClass>
  void V3C::write_out_of_band_info(std::ostream & stream, const DataClass & data, INFO_FMT fmt)
  {
    _out_of_band_info(stream, std::move(data), fmt);
  }

  template<typename DataClass>
  static DataClass init_class(INIT_FLAGS init_flags)
  { 
    return DataClass();
  }
  template<>
  V3C_Unit init_class<V3C_Unit>(INIT_FLAGS init_flags)
  {
    if (init_flags != INIT_FLAGS::NUL)
    {
      return V3C_Unit(V3C::unit_types_from_init_flag(init_flags).front());
    }
    return V3C_Unit();
  }
  template<>
  V3C_Gof init_class<V3C_Gof>(INIT_FLAGS init_flags)
  {
    auto gof = V3C_Gof();
    if (init_flags != INIT_FLAGS::NUL)
    {
      for (const auto unit_type : V3C::unit_types_from_init_flag(init_flags))
      {
        gof.set(init_class<V3C_Unit>(V3C::init_flags_from_unit_types({ unit_type })));
      }
    }
    return gof;
  }
  template<>
  Sample_Stream<SAMPLE_STREAM_TYPE::V3C> init_class<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(INIT_FLAGS init_flags)
  {
    auto sample_stream = Sample_Stream<SAMPLE_STREAM_TYPE::V3C>();
    if (init_flags != INIT_FLAGS::NUL)
    {
      sample_stream.push_back(init_class<V3C_Gof>(init_flags));
    }
    return sample_stream;
  }

  template<typename DataClass>
  V3C::InfoDataType V3C::read_out_of_band_info(std::istream & stream, INFO_FMT fmt, INIT_FLAGS init_flags)
  {
    return _out_of_band_info(stream, init_class<DataClass>(init_flags), fmt);
  }

  size_t V3C::combineBytes(const uint8_t *const bytes, const uint8_t num_bytes) {
    size_t combined_out = 0;
    for (uint8_t i = 0; i < num_bytes; ++i) {
      combined_out |= (static_cast<size_t>(bytes[i]) << (8 * (num_bytes - 1 - i)));
    }
    return combined_out;
  }

  void V3C::convert_size_big_endian(const uint64_t in, uint8_t* const out, const size_t output_size) {
    for (size_t i = 0; i < output_size; ++i) {
      out[output_size - i - 1] = static_cast<uint8_t>(in >> (8 * i));
    }
  }

}