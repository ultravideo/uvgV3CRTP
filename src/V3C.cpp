#include "V3C.h"

#include "V3C_Gof.h"
#include "V3C_Unit.h"
#include "Sample_Stream.h"

#include <type_traits>
#include <sstream>
#include <stdexcept>
#include <limits>

namespace uvgV3CRTP {

  // Explicitly define necessary instantiations so code is linked properly
  template void V3C::write_out_of_band_info<V3C::InfoDataType, Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(std::ostream&, Sample_Stream<SAMPLE_STREAM_TYPE::V3C> const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C::InfoDataType, V3C_Gof>(std::ostream&, V3C_Gof const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C::HeaderDataType, V3C_Gof>(std::ostream&, V3C_Gof const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C::InfoDataType, V3C_Unit>(std::ostream&, V3C_Unit const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C::HeaderDataType, V3C_Unit>(std::ostream&, V3C_Unit const&, INFO_FMT);
  template void V3C::write_out_of_band_info<V3C::PayloadDataType, V3C_Unit>(std::ostream&, V3C_Unit const&, INFO_FMT);
  template V3C::InfoDataType V3C::read_out_of_band_info<V3C::InfoDataType, Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(std::istream&, INFO_FMT, INIT_FLAGS);
  template size_t V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::V3C>(V3C_UNIT_TYPE type);
  template size_t V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::NAL>(V3C_UNIT_TYPE type);

  //template void V3C::write_out_of_band_info<V3C_Unit::V3C_Unit_Header>(std::ostream&, V3C_Unit::V3C_Unit_Header const&, INFO_FMT);

  // Helper to map enum types to data types
  namespace {
    template <typename FieldEnum, FieldEnum V, typename = std::enable_if_t<std::is_enum_v<FieldEnum>>>
    struct _FieldDataType;
    template <>
    struct _FieldDataType<INFO_FIELDS, INFO_FIELDS::NUM>
    {
      using type = size_t;
    };
    template <>
    struct _FieldDataType<INFO_FIELDS, INFO_FIELDS::SIZE_PREC>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<INFO_FIELDS, INFO_FIELDS::VAR_NAL_PREC>
    {
      using type = bool;
    };
    template <>
    struct _FieldDataType<INFO_FIELDS, INFO_FIELDS::VAR_NAL_NUM>
    {
      using type = bool;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_UNIT_TYPE>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_ATLAS_ID>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_ATTRIBUTE_INDEX>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_ATTRIBUTE_PARTITION_INDEX>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_MAP_INDEX>
    {
      using type = uint8_t;
    };
    template <>
    struct _FieldDataType<HEADER_FIELDS, HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG>
    {
      using type = bool;
    };
    template <>
    struct _FieldDataType<PAYLOAD_FIELDS, PAYLOAD_FIELDS::PAYLOAD>
    {
      using type = std::string;
    };
    template <>
    struct _FieldDataType<PAYLOAD_FIELDS, PAYLOAD_FIELDS::HEADER>
    {
      using type = std::string;
    };
  }
  template <auto Field>
  using FieldDataType = _FieldDataType<decltype(Field), Field>;

  template <typename T>
  struct always_false : std::false_type {};
  // End of helper

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

  size_t V3C::combineBytes(const uint8_t* const bytes, const uint8_t num_bytes) {
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

  uint32_t V3C::get_new_sampling_instant()
  {
    srand(static_cast<unsigned>(time(NULL)));
    // Use UINT32_MAX instead of std::numeric_limits<uint32_t>::max() to avoid macro issues
    uint32_t timestamp = static_cast<uint32_t>(rand() % UINT32_MAX);
    // get random initial timestamp as per the RTP spec
    return timestamp != 0 ? timestamp : 1; // 0 assumed "no timestamp set" so avoid it
  }

  uint32_t V3C::calc_new_timestamp(const uint32_t old_timestamp, const uint32_t sample_rate, const uint32_t clock_rate)
  {
    if(sample_rate > clock_rate)
    {
      throw std::invalid_argument("Sample rate cannot be greater than clock rate; not enough timestamp resolution");
    }
    return static_cast<uint32_t>(old_timestamp + clock_rate / sample_rate);
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

  static constexpr char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  static std::string enc_base64(const char* const in, const size_t len)
  {
    const size_t out_len = ((len + 2) / 3) << 2;
    const size_t pad = (3 - (len % 3)) % 3;
    const size_t loop_len = len - (len % 3);

    std::string out;
    out.reserve(out_len); // Reserve enough space +1 for null terminator
    
    for (size_t i = 0; i < loop_len; i += 3) {
      // Combine 3 bytes into a 24-bit number
      const uint32_t val = in[i + 0] << 16 
                         | in[i + 1] << 8 
                         | in[i + 2] << 0;
      // Extract 4 6-bit values and map to base64 characters
      out.push_back(b64_alpha[(val >> 18) & 0x3F]);
      out.push_back(b64_alpha[(val >> 12) & 0x3F]);
      out.push_back(b64_alpha[(val >> 6)  & 0x3F]);
      out.push_back(b64_alpha[(val >> 0)  & 0x3F]);
    }
    // Account for last byte(s) and padding
    if (pad != 0) {
      const uint32_t val = pad == 2 ? in[len - 1] << 16                       // If pad = 2, only 1 byte of input left
                                    : in[len - 2] << 16 | (in[len - 1] << 8); // If pad = 1, 2 bytes of input left
      out.push_back(b64_alpha[(val >> 18) & 0x3F]);
      out.push_back(b64_alpha[(val >> 12) & 0x3F]);
      if (pad == 1) {
        out.push_back(b64_alpha[(val >> 6) & 0x3F]);
      } else {
        out.push_back('=');
      }
      out.push_back('=');
    }

    return out;
  }

  static std::string dec_base64(const char* const in, const size_t len)
  {
    if (len % 4 != 0) {
      throw ParseException("Base64 input length must be a multiple of 4");
    }
    // Calculate padding
    size_t pad = 0;
    if (in[len - 1] == '=') pad++;
    if (in[len - 2] == '=') pad++;
    const size_t loop_len = len - (pad ? 4 : 0);
    const size_t out_len = (len >> 2) * 3 - pad;
    std::string out;
    out.reserve(out_len);
    
    for (size_t i = 0; i < loop_len; i += 4)
    {
      // Decode 4 base64 characters into a 24-bit number
      const auto val = (std::strchr(b64_alpha, in[i + 0]) - b64_alpha) << 18
                     | (std::strchr(b64_alpha, in[i + 1]) - b64_alpha) << 12
                     | (std::strchr(b64_alpha, in[i + 2]) - b64_alpha) << 6
                     | (std::strchr(b64_alpha, in[i + 3]) - b64_alpha) << 0;
      // Extract 3 bytes from the 24-bit number
      out.push_back((val >> 16) & 0xFF);
      out.push_back((val >> 8)  & 0xFF);
      out.push_back((val >> 0)  & 0xFF);
    }
    // Handle last 4 characters and padding
    if (pad != 0) {
      const auto val = (std::strchr(b64_alpha, in[loop_len + 0]) - b64_alpha) << 18
                     | (std::strchr(b64_alpha, in[loop_len + 1]) - b64_alpha) << 12
                     | (pad == 1 ? (std::strchr(b64_alpha, in[loop_len + 2]) - b64_alpha) << 6 : 0);
      out.push_back((val >> 16) & 0xFF);
      if (pad == 1) {
        out.push_back((val >> 8) & 0xFF);
      }
    }

    return out;
  }

  template <typename DT, typename FT>
  static inline bool has_field(const DT& data, const FT& field)
  {
    return (data.find(field) != data.end());
  }

  template <auto field, typename DataType>
  static inline typename FieldDataType<field>::type& get_field(DataType& data)
  {
    using _FDType = typename FieldDataType<field>::type;
    if (!has_field(data, field)) data[field] = {};
    if constexpr (std::is_convertible_v < decltype(FIELD_TYPES{}.s)&, _FDType& > )
    {
      return static_cast<_FDType&>(data.at(field).s);
    }
    else if constexpr (std::is_convertible_v < decltype(FIELD_TYPES{}.ui8)&, _FDType& > )
    {
      return static_cast<_FDType&>(data.at(field).ui8);
    }
    else if constexpr (std::is_convertible_v < decltype(FIELD_TYPES{}.b)&, _FDType& > )
    {
      return static_cast<_FDType&>(data.at(field).b);
    }
    else
    {
      return data.at(field); // Return the field directly if it's not in FIELD_TYPES
    }
  }
  template <auto field, typename DataType>
  static inline const typename FieldDataType<field>::type& get_field(const DataType& data)
  {
    using _FDType = typename FieldDataType<field>::type;
    if (data.find(field) == data.end()) {
        throw std::out_of_range(std::string("Field not found in const get_field. Trace: ") 
                                + __func__ + " at " + __FILE__ + ":" + std::to_string(__LINE__));
    }
    if constexpr (std::is_convertible_v < decltype(FIELD_TYPES{}.s)&, _FDType& > )
    {
        return static_cast<const _FDType&>(data.at(field).s);
    }
    else if constexpr (std::is_convertible_v < decltype(FIELD_TYPES{}.ui8)&, _FDType& > )
    {
        return static_cast<const _FDType&>(data.at(field).ui8);
    }
    else if constexpr (std::is_convertible_v < decltype(FIELD_TYPES{}.b)&, _FDType& > )
    {
        return static_cast<const _FDType&>(data.at(field).b);
    }
    else
    {
      return data.at(field); // Return the field directly if it's not in FIELD_TYPES
    }
  }


  template <INFO_FMT F = INFO_FMT::LOGGING, typename TF, typename TV>
  static inline void process(std::ostream& out, TF&& field, TV&& value)
  {
    if constexpr (F == INFO_FMT::RAW)
    {
      (void)field; //Suppress warnings
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
      (void)field; //Suppress warnings
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
      tmp << "  " << field << " size precision: ";
    }
    process<F>(out, tmp.str().c_str(), std::forward<TV>(value));
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename TF, typename TV>
  static inline void process_attr(Stream& out, TF&& field, TV&& value)
  {
    std::ostringstream tmp;
    if constexpr (F == INFO_FMT::LOGGING) tmp << "  ";
    tmp << field << ": ";
    process<F>(out, tmp.str().c_str(), std::forward<TV>(value));
  }

  static constexpr const char* get_unit_type_name(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case V3C_VPS:
      return "V3C_VPS";
    case V3C_AD:
      return "V3C_AD";
    case V3C_OVD:
      return "V3C_OVD";
    case V3C_GVD:
      return "V3C_GVD";
    case V3C_AVD:
      return "V3C_AVD";
    case V3C_PVD:
      return "V3C_PVD";
    case V3C_CAD:
      return "V3C_CAD";
    default:
      return "UNDEF";
    }
  }

  template <INFO_FMT F, typename Stream, typename... Param>
  static inline void preample(Stream&, Param...)
  {

  }
  template <>
  inline void preample<INFO_FMT::LOGGING>(std::ostream& out)
  {
    out << "Summary:" << std::endl;
    out << "--------" << std::endl;
  }
  template <>
  inline void preample<INFO_FMT::LOGGING>(std::ostream& out, const V3C_UNIT_TYPE type)
  {
    out << get_unit_type_name(type) << ":" << std::endl;
    out << "--------" << std::endl;
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename... Param>
  static inline void postample(Stream&, Param...)
  {
    
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataType>
  static inline void postample(std::ostream& ostream, DataType& data)
  {
    if constexpr (F == INFO_FMT::LOGGING)
    {
      if (has_field(data, INFO_FIELDS::VAR_NAL_PREC) && get_field<INFO_FIELDS::VAR_NAL_PREC>(data)) {
        ostream << "  Warning: Variable nal size precision between GOFs!" << std::endl;
      }
      if (has_field(data, INFO_FIELDS::VAR_NAL_NUM) && get_field<INFO_FIELDS::VAR_NAL_NUM>(data)) {
        ostream << "  Warning: Variable number of nals between GOFs!" << std::endl;
      }
    }
  }

  template <INFO_FMT F, V3C_UNIT_TYPE T, HEADER_FIELDS H, typename A = void>
  static constexpr auto get_field_name()
  {
    if constexpr (F == INFO_FMT::SDP)
    {
      switch (H)
      {
      case HEADER_FIELDS::VUH_UNIT_TYPE:
        return "sprop-v3c-unit-type";
      case HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID:
        return "sprop-v3c-vps-id";
      case HEADER_FIELDS::VUH_ATLAS_ID:
        return "sprop-v3c-atlas-id";
      case HEADER_FIELDS::VUH_ATTRIBUTE_INDEX:
        return "sprop-v3c-attr-idx";
      case HEADER_FIELDS::VUH_ATTRIBUTE_PARTITION_INDEX:
        return "sprop-v3c-attr-part-idx";
      case HEADER_FIELDS::VUH_MAP_INDEX:
        return "sprop-v3c-map-idx";
      case HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG:
        return "sprop-v3c-map-idx";
      default:
        return "Unsupported header field in get_field_name for SDP";
      }
    }
    else
    {
      switch (H)
      {
      case HEADER_FIELDS::VUH_UNIT_TYPE:
        return "vuh_unit_type";
      case HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID:
        return "vuh_v3c_parameter_set_id";
      case HEADER_FIELDS::VUH_ATLAS_ID:
        return "vuh_atlas_id";
      case HEADER_FIELDS::VUH_ATTRIBUTE_INDEX:
        return "vuh_attribute_index";
      case HEADER_FIELDS::VUH_ATTRIBUTE_PARTITION_INDEX:
        return "vuh_attribute_partition_index";
      case HEADER_FIELDS::VUH_MAP_INDEX:
        return "vuh_map_index";
      case HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG:
        return "vuh_auxiliary_video_flag";
      default:
        return "Unsupported header field in get_field_name for SDP";
      }
    }
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, V3C_UNIT_TYPE Type, auto Field, typename Stream, typename FieldName, typename DataType>
  static void inline process_field(Stream& stream, FieldName&& field_name, DataType& data)
  {
    if (!has_field(data, Type)) return;
    if (!has_field(data.at(Type), Field)) return;

    auto& unit_data = data.at(Type);

    if constexpr (std::is_same<decltype(Field), INFO_FIELDS>::value)
    {
      if constexpr (Field == INFO_FIELDS::SIZE_PREC)
      {
        process_prec<F>(stream, std::forward<FieldName>(field_name), get_field<Field>(unit_data));
      }
      else if constexpr (Field == INFO_FIELDS::NUM)
      {
        process_num<F>(stream, std::forward<FieldName>(field_name), get_field<Field>(unit_data));
      }
      else
      {
        process_attr<F>(stream, std::forward<FieldName>(field_name), get_field<Field>(unit_data));
      }
    }
    else
    {
      process_attr<F>(stream, std::forward<FieldName>(field_name), get_field<Field>(unit_data));
    }
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass, typename Stream, typename DataType>
  static void process_out_of_band_info(Stream& stream, DataType& data)
  {
    static_assert(always_false<DataType>::value, "Unsupported types in process_out_of_band_info");
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass, typename Stream>
  static void process_out_of_band_info(Stream& stream, V3C::InfoDataType& data)
  {
    preample<F>(stream);
    // Uses NUM_V3C_UNIT_TYPES to store global parameters
    process_field<F, NUM_V3C_UNIT_TYPES, INFO_FIELDS::NUM>(stream, "GOF", data);

    // Process unit type specific fields
    process_field<F, V3C_VPS, INFO_FIELDS::NUM>(stream, "VPS", data);
    process_field<F, V3C_AD, INFO_FIELDS::NUM>(stream, "AD_NALU", data);
    process_field<F, V3C_OVD, INFO_FIELDS::NUM>(stream, "OVD_NALU", data);
    process_field<F, V3C_GVD, INFO_FIELDS::NUM>(stream, "GVD_NALU", data);
    process_field<F, V3C_AVD, INFO_FIELDS::NUM>(stream, "AVD_NALU", data);
    process_field<F, V3C_PVD, INFO_FIELDS::NUM>(stream, "PVD_NALU", data);
    process_field<F, V3C_CAD, INFO_FIELDS::NUM>(stream, "CAD_NALU", data);

    // Uses NUM_V3C_UNIT_TYPES to store global parameters
    process_field<F, NUM_V3C_UNIT_TYPES, INFO_FIELDS::SIZE_PREC>(stream, "V3C", data);

    // Process unit type specific size precisions
    process_field<F, V3C_AD, INFO_FIELDS::SIZE_PREC>(stream, "Atlas_NALU", data);
    if (has_field(data, V3C_OVD)) process_field<F, V3C_OVD, INFO_FIELDS::SIZE_PREC>(stream, "Video", data);
    else if (has_field(data, V3C_GVD)) process_field<F, V3C_GVD, INFO_FIELDS::SIZE_PREC>(stream, "Video", data);
    else if (has_field(data, V3C_AVD)) process_field<F, V3C_AVD, INFO_FIELDS::SIZE_PREC>(stream, "Video", data);
    else if (has_field(data, V3C_PVD)) process_field<F, V3C_PVD, INFO_FIELDS::SIZE_PREC>(stream, "Video", data);
    else if (has_field(data, V3C_PVD)) process_field<F, V3C_PVD, INFO_FIELDS::SIZE_PREC>(stream, "Video", data);

    postample<F>(stream, data.at(NUM_V3C_UNIT_TYPES));
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass, typename Stream>
  static void process_out_of_band_info(Stream& stream, V3C::HeaderDataType& data)
  {
    preample<F>(stream, V3C_VPS);
    process_field<F, V3C_VPS, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_VPS, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_VPS, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_VPS, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    process_field<F, V3C_VPS, HEADER_FIELDS::VUH_ATLAS_ID>(stream, get_field_name<F, V3C_VPS, HEADER_FIELDS::VUH_ATLAS_ID>(), data);
    postample<F>(stream);

    preample<F>(stream, V3C_AD);
    process_field<F, V3C_AD, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_AD, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_AD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_AD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    process_field<F, V3C_AD, HEADER_FIELDS::VUH_ATLAS_ID>(stream, get_field_name<F, V3C_AD, HEADER_FIELDS::VUH_ATLAS_ID>(), data);
    postample<F>(stream);

    preample<F>(stream, V3C_OVD);
    process_field<F, V3C_OVD, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_OVD, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_OVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_OVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    process_field<F, V3C_OVD, HEADER_FIELDS::VUH_ATLAS_ID>(stream, get_field_name<F, V3C_OVD, HEADER_FIELDS::VUH_ATLAS_ID>(), data);
    postample<F>(stream);

    preample<F>(stream, V3C_GVD);
    process_field<F, V3C_GVD, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_GVD, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_GVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_GVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    process_field<F, V3C_GVD, HEADER_FIELDS::VUH_ATLAS_ID>(stream, get_field_name<F, V3C_GVD, HEADER_FIELDS::VUH_ATLAS_ID>(), data);
    process_field<F, V3C_GVD, HEADER_FIELDS::VUH_MAP_INDEX>(stream, get_field_name<F, V3C_GVD, HEADER_FIELDS::VUH_MAP_INDEX>(), data);
    process_field<F, V3C_GVD, HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG>(stream, get_field_name<F, V3C_GVD, HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG>(), data);
    postample<F>(stream);

    preample<F>(stream, V3C_AVD);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_ATLAS_ID>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_ATLAS_ID>(), data);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_ATTRIBUTE_INDEX>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_ATTRIBUTE_INDEX>(), data);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_ATTRIBUTE_PARTITION_INDEX>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_ATTRIBUTE_PARTITION_INDEX>(), data);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_MAP_INDEX>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_MAP_INDEX>(), data);
    process_field<F, V3C_AVD, HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG>(stream, get_field_name<F, V3C_AVD, HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG>(), data);
    postample<F>(stream);

    preample<F>(stream, V3C_PVD);
    process_field<F, V3C_PVD, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_PVD, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_PVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_PVD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    process_field<F, V3C_PVD, HEADER_FIELDS::VUH_ATLAS_ID>(stream, get_field_name<F, V3C_PVD, HEADER_FIELDS::VUH_ATLAS_ID>(), data);
    postample<F>(stream);

    preample<F>(stream, V3C_CAD);
    process_field<F, V3C_CAD, HEADER_FIELDS::VUH_UNIT_TYPE>(stream, get_field_name<F, V3C_CAD, HEADER_FIELDS::VUH_UNIT_TYPE>(), data);
    process_field<F, V3C_CAD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(stream, get_field_name<F, V3C_CAD, HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(), data);
    postample<F>(stream);
  }
  template <INFO_FMT F, typename DataClass, typename Stream>
  static void process_out_of_band_info(Stream& stream, V3C::PayloadDataType& data)
  {
    preample<F>(stream);
    process_field<F, V3C_VPS, PAYLOAD_FIELDS::PAYLOAD>(stream, "VPS", data);
    postample<F>(stream);

    preample<F>(stream);
    process_field<F, V3C_AD, PAYLOAD_FIELDS::PAYLOAD>(stream, "AD", data);
    postample<F>(stream);

    preample<F>(stream);
    process_field<F, V3C_OVD, PAYLOAD_FIELDS::PAYLOAD>(stream, "OVD", data);
    postample<F>(stream);

    preample<F>(stream);
    process_field<F, V3C_GVD, PAYLOAD_FIELDS::PAYLOAD>(stream, "GVD", data);
    postample<F>(stream);

    preample<F>(stream);
    process_field<F, V3C_AVD, PAYLOAD_FIELDS::PAYLOAD>(stream, "AVD", data);
    postample<F>(stream);

    preample<F>(stream);
    process_field<F, V3C_PVD, PAYLOAD_FIELDS::PAYLOAD>(stream, "PVD", data);
    postample<F>(stream);

    preample<F>(stream);
    process_field<F, V3C_CAD, PAYLOAD_FIELDS::PAYLOAD>(stream, "CAD", data);
    postample<F>(stream);

  }


  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataType>
  static void populate_data(const Sample_Stream<SAMPLE_STREAM_TYPE::V3C> & v3c_data, DataType& data)
  {
    // Gather data from all GOFs
    DataType tmp_data = {};
    for (const auto& gof : v3c_data)
    {
      // Set data from first GOF directly else use temporary storage
      if (data.empty())
      {
        populate_data<F>(gof, data);

        if constexpr (std::is_same<DataType, V3C::InfoDataType>::value)
        {
          // Use NUM_V3C_UNIT_TYPES to store global parameters
          data[NUM_V3C_UNIT_TYPES] = {};
          get_field<INFO_FIELDS::NUM>(data.at(NUM_V3C_UNIT_TYPES)) = 1;
          get_field<INFO_FIELDS::SIZE_PREC>(data.at(NUM_V3C_UNIT_TYPES)) = v3c_data.size_precision();
          get_field<INFO_FIELDS::VAR_NAL_PREC>(data.at(NUM_V3C_UNIT_TYPES)) = false;
          get_field<INFO_FIELDS::VAR_NAL_NUM>( data.at(NUM_V3C_UNIT_TYPES)) = false;
        }
      }
      else
      {
        populate_data<F>(gof, tmp_data);

        if constexpr (std::is_same<DataType, V3C::InfoDataType>::value)
        {
          // Merge data from tmp_data to data and check for inconsistencies
          get_field<INFO_FIELDS::NUM>(data.at(NUM_V3C_UNIT_TYPES)) += 1; // Count number of GOFs
          for (const auto& [type, value] : tmp_data)
          {
            if (type == V3C_VPS)
            {
              // For VPS update count
              get_field<INFO_FIELDS::NUM>(data.at(V3C_VPS)) += get_field<INFO_FIELDS::NUM>(value);
            }
            else
            {
              // For other types check for inconsistencies
              get_field<INFO_FIELDS::VAR_NAL_PREC>(data.at(NUM_V3C_UNIT_TYPES)) |=
                get_field<INFO_FIELDS::SIZE_PREC>(value) != get_field<INFO_FIELDS::SIZE_PREC>(data.at(type));
              get_field<INFO_FIELDS::VAR_NAL_NUM>(data.at(NUM_V3C_UNIT_TYPES)) |=
                get_field<INFO_FIELDS::NUM>(value) != get_field<INFO_FIELDS::NUM>(data.at(type));
            }
          }
        }
        tmp_data.clear();
      }
    }
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataType>
  static void populate_data(const V3C_Gof & v3c_data, DataType& data)
  {
    for (const auto&[type, unit] : v3c_data)
    {
      populate_data<F>(unit, data);
    }
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataType>
  static void populate_data(const V3C_Unit& v3c_data, DataType& data)
  {
    if (!has_field(data, v3c_data.type()))
    {
      data[v3c_data.type()] = {};
    }
    if constexpr (std::is_same<DataType, V3C::InfoDataType>::value)
    {
      switch (v3c_data.type())
      {
      case V3C_VPS:
        get_field<INFO_FIELDS::NUM>(data.at(v3c_data.type())) += 1;
        break;
      case V3C_AD:
      case V3C_OVD:
      case V3C_GVD:
      case V3C_AVD:
      case V3C_PVD:
      case V3C_CAD:
        get_field<INFO_FIELDS::NUM>(data.at(v3c_data.type())) = v3c_data.num_nalus();
        get_field<INFO_FIELDS::SIZE_PREC>(data.at(v3c_data.type())) = v3c_data.nal_size_precision();
        break;
      default:
        break;
      }
    }
    else if constexpr (std::is_same<DataType, V3C::HeaderDataType>::value)
    {
      populate_data<F>(v3c_data.header(), data);
    }
  }
  template <INFO_FMT F, typename = typename std::enable_if<F == INFO_FMT::RAW
                                                        || F == INFO_FMT::BASE64>::type>
  static void populate_data(const V3C_Unit& v3c_data, V3C::PayloadDataType& data)
  {
    const V3C_UNIT_TYPE type = v3c_data.type();
    [[maybe_unused]] bool first_nal = true;
    if (!has_field(data, type)) data[type] = {};

    for (const auto& nal : v3c_data.nalus())
    {
      if constexpr (F == INFO_FMT::RAW)
      {
        get_field<PAYLOAD_FIELDS::PAYLOAD>(data.at(type)).append(
          reinterpret_cast<char*>(nal.get().bitstream()), nal.get().size()
        );
      }
      else if constexpr (F == INFO_FMT::BASE64)
      {
        if (!first_nal) {
          get_field<PAYLOAD_FIELDS::PAYLOAD>(data.at(type)).push_back(',');
        }
        else {
          first_nal = false;
        }
        get_field<PAYLOAD_FIELDS::PAYLOAD>(data.at(type)).append(
          enc_base64(reinterpret_cast<char*>(nal.get().bitstream()), nal.get().size())
        );
      }
    }
  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename = typename std::enable_if<F == INFO_FMT::LOGGING
                                                                            || F == INFO_FMT::PARAM
                                                                            || F == INFO_FMT::RAW
                                                                            || F == INFO_FMT::SDP>::type>
  static void populate_data(const V3C_Unit::V3C_Unit_Header& header, V3C::HeaderDataType& data)
  {
    const V3C_UNIT_TYPE type = header.type;
    if (!has_field(data, type)) data[type] = {};
    switch (type)
    {
    case V3C_AVD:
      get_field<HEADER_FIELDS::VUH_ATTRIBUTE_INDEX>(data.at(type)) = header.vuh_attribute_index;
      get_field<HEADER_FIELDS::VUH_ATTRIBUTE_PARTITION_INDEX>(data.at(type)) = header.vuh_attribute_partition_index;
      [[fallthrough]];
    case V3C_GVD:
      get_field<HEADER_FIELDS::VUH_MAP_INDEX>(data.at(type)) = header.vuh_map_index;
      get_field<HEADER_FIELDS::VUH_AUXILIARY_VIDEO_FLAG>(data.at(type)) = header.vuh_auxiliary_video_flag;
      [[fallthrough]];
    case V3C_PVD:
    case V3C_OVD:
    case V3C_AD:
      get_field<HEADER_FIELDS::VUH_ATLAS_ID>(data.at(type)) = header.vuh_atlas_id;
      [[fallthrough]];
    case V3C_CAD:
      get_field<HEADER_FIELDS::VUH_V3C_PARAMETER_SET_ID>(data.at(type)) = header.vuh_v3c_parameter_set_id;
      [[fallthrough]];
    case V3C_VPS:
      get_field<HEADER_FIELDS::VUH_UNIT_TYPE>(data.at(type)) = header.vuh_unit_type;
      break;
    default:
      break;
    }
  }
  template <INFO_FMT F, typename = typename std::enable_if<F == INFO_FMT::RAW
                                                        || F == INFO_FMT::BASE64>::type>
  static void populate_data(const V3C_Unit::V3C_Unit_Header& header, V3C::PayloadDataType& data)
  {
    const V3C_UNIT_TYPE type = header.type;
    if (!has_field(data, type)) data[type] = {};

    std::string raw_header = {};
    raw_header.resize(header.size());
    header.write_header(raw_header.data());
    
    if constexpr (F == INFO_FMT::RAW)
    {
      get_field<PAYLOAD_FIELDS::PAYLOAD>(data.at(type)) = raw_header;
    }
    else if constexpr (F == INFO_FMT::BASE64)
    {
      get_field<PAYLOAD_FIELDS::PAYLOAD>(data.at(type)) = enc_base64(raw_header.c_str(), raw_header.size());
    }
  }

  template<typename DataType = V3C::InfoDataType, typename Stream, typename DataClass>
  static auto _out_of_band_info(Stream& stream, const DataClass& v3c_data, INFO_FMT fmt)
  {
    if constexpr (std::is_same<DataType, V3C::InfoDataType>::value)
    {
      V3C::InfoDataType info_data;
      switch (fmt)
      {
      case INFO_FMT::PARAM:
        populate_data<INFO_FMT::PARAM>(v3c_data, info_data);
        process_out_of_band_info<INFO_FMT::PARAM, DataClass>(stream, info_data);
        break;
      case INFO_FMT::RAW:
        populate_data<INFO_FMT::RAW>(v3c_data, info_data);
        process_out_of_band_info<INFO_FMT::RAW, DataClass>(stream, info_data);
        break;
      case INFO_FMT::LOGGING:
        populate_data<INFO_FMT::LOGGING>(v3c_data, info_data);
        process_out_of_band_info<INFO_FMT::LOGGING, DataClass>(stream, info_data);
        break;
      default:
        throw ParseException("Unsupported format for bitstream info");
      }
      return info_data;
    }
    else if constexpr(std::is_same<DataType, V3C::HeaderDataType>::value)
    {
      V3C::HeaderDataType header_data;
      switch (fmt)
      {
      case INFO_FMT::PARAM:
        populate_data<INFO_FMT::PARAM>(v3c_data, header_data);
        process_out_of_band_info<INFO_FMT::PARAM, DataClass>(stream, header_data);
        break;
      case INFO_FMT::LOGGING:
        populate_data<INFO_FMT::LOGGING>(v3c_data, header_data);
        process_out_of_band_info<INFO_FMT::LOGGING, DataClass>(stream, header_data);
        break;
      case INFO_FMT::RAW:
        populate_data<INFO_FMT::RAW>(v3c_data, header_data);
        process_out_of_band_info<INFO_FMT::RAW, DataClass>(stream, header_data);
        break;
      case INFO_FMT::SDP:
        populate_data<INFO_FMT::SDP>(v3c_data, header_data);
        process_out_of_band_info<INFO_FMT::SDP, DataClass>(stream, header_data);
        break;
      default:
        throw ParseException("Unsupported format for header info");
        break;
      }
      return header_data;
    }
    else if constexpr (std::is_same<DataType, V3C::PayloadDataType>::value)
    {
      V3C::PayloadDataType payload_data;
      switch (fmt)
      {
      case INFO_FMT::RAW:
        populate_data<INFO_FMT::RAW>(v3c_data, payload_data);
        process_out_of_band_info<INFO_FMT::RAW, DataClass>(stream, payload_data);
        break;
      case INFO_FMT::BASE64:
        populate_data<INFO_FMT::BASE64>(v3c_data, payload_data);
        process_out_of_band_info<INFO_FMT::BASE64, DataClass>(stream, payload_data);
        break;
      default:
        throw ParseException("Unsupported format for payload");
        break;
      }
      return payload_data;
    }
    else
    {
      static_assert(always_false<DataType>::value, "Unsupported DataType for out_of_band_info");
    }
  }
  
  template<typename DataType, typename DataClass>
  void V3C::write_out_of_band_info(std::ostream & stream, const DataClass & v3c_data, INFO_FMT fmt)
  {
    _out_of_band_info<DataType>(stream, v3c_data, fmt);
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

  template<typename DataType, typename DataClass>
  DataType V3C::read_out_of_band_info(std::istream & stream, INFO_FMT fmt, INIT_FLAGS init_flags)
  {
    return _out_of_band_info<DataType>(stream, init_class<DataClass>(init_flags), fmt);
  }

  void V3C::populate_bitstream_info(const InfoDataType& in_info, BitstreamInfo& out_info)
  {
    bool video_nal_size_precision_set = false;
    // copy in_info to out_info
    if (has_field(in_info, NUM_V3C_UNIT_TYPES))
    {
      out_info.v3c_size_precision = get_field<INFO_FIELDS::SIZE_PREC>(in_info.at(NUM_V3C_UNIT_TYPES));
      out_info.num_gofs = get_field<INFO_FIELDS::NUM>(in_info.at(NUM_V3C_UNIT_TYPES));
      out_info.var_nal_prec = get_field<INFO_FIELDS::VAR_NAL_PREC>(in_info.at(NUM_V3C_UNIT_TYPES));
      out_info.var_nal_num = get_field<INFO_FIELDS::VAR_NAL_NUM>(in_info.at(NUM_V3C_UNIT_TYPES));
    }
    if (has_field(in_info, V3C_VPS))
    {
      out_info.num_vps = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_VPS));
    }
    if (has_field(in_info, V3C_AD))
    {
      out_info.num_ad_nalu = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_AD));
      out_info.atlas_nal_size_precision = get_field<INFO_FIELDS::SIZE_PREC>(in_info.at(V3C_AD));
    }
    if (has_field(in_info, V3C_OVD))
    {
      out_info.num_ovd_nalu = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_OVD));
      if (!video_nal_size_precision_set)
      {
        out_info.video_nal_size_precision = get_field<INFO_FIELDS::SIZE_PREC>(in_info.at(V3C_OVD));
        video_nal_size_precision_set = true;
      }
    }
    if (has_field(in_info, V3C_GVD))
    {
      out_info.num_gvd_nalu = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_GVD));
      if (!video_nal_size_precision_set)
      {
        out_info.video_nal_size_precision = get_field<INFO_FIELDS::SIZE_PREC>(in_info.at(V3C_GVD));
        video_nal_size_precision_set = true;
      }
    }
    if (has_field(in_info, V3C_AVD))
    {
      out_info.num_avd_nalu = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_AVD));
      if (!video_nal_size_precision_set)
      {
        out_info.video_nal_size_precision = get_field<INFO_FIELDS::SIZE_PREC>(in_info.at(V3C_AVD));
        video_nal_size_precision_set = true;
      }
    }
    if (has_field(in_info, V3C_PVD))
    {
      out_info.num_pvd_nalu = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_PVD));
      if (!video_nal_size_precision_set)
      {
        out_info.video_nal_size_precision = get_field<INFO_FIELDS::SIZE_PREC>(in_info.at(V3C_PVD));
        video_nal_size_precision_set = true;
      }
    }
    if (has_field(in_info, V3C_CAD))
    {
      out_info.num_cad_nalu = get_field<INFO_FIELDS::NUM>(in_info.at(V3C_CAD));
    }
  }
}