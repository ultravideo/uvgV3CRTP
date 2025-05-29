#include "V3C.h"

#include "V3C_Gof.h"
#include "V3C_Unit.h"
#include "Sample_Stream.h"

#include <cassert>
#include <type_traits>

namespace v3cRTPLib {


  V3C::V3C(const char * local_address, const char * remote_address, const INIT_FLAGS init_flags, const uint16_t src_port, const uint16_t dst_port, int stream_flags)
  {
    /* Create the necessary uvgRTP media streams */
    std::pair<std::string, std::string> addresses_sender(local_address, remote_address);
    session_ = ctx_.create_session(addresses_sender);

    // Init v3c streams
    stream_flags |= RCE_NO_H26X_PREPEND_SC;

    for (auto unit_type : unit_types_from_init_flag(init_flags)) {
      streams_[unit_type] = session_->create_stream(src_port, dst_port, get_format(unit_type), stream_flags);
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

  Sample_Stream<SAMPLE_STREAM_TYPE::V3C> V3C::parse_bitstream(const char * const bitstream, const size_t len)
  {
    // First byte should be the v3c sample stream header
    uint8_t v3c_size_precision = parse_size_precision(bitstream);
    assert(0 < v3c_size_precision && v3c_size_precision <= 8 && "sample stream precision should be in range [1,8]");
    
    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> sample_stream(v3c_size_precision);
    
    // Start processing v3c units
    for (size_t ptr = SAMPLE_STREAM_HDR_LEN; ptr < len;) {

      // Read the V3C unit size 
      size_t v3c_size = parse_sample_stream_size(bitstream, v3c_size_precision);
      ptr += v3c_size_precision; // Jump over the V3C unit size bytes

      // Inside v3c unit now
      size_t v3c_ptr = ptr;
      V3C_Unit new_unit(&bitstream[v3c_ptr], v3c_size);
      ptr += new_unit.size();

      sample_stream.push_back(std::move(new_unit), v3c_size);
    }

    return sample_stream;
  }

  uint8_t V3C::parse_size_precision(const char * const bitstream)
  {
    return (bitstream[0] >> 5) + 1;
  }

  size_t V3C::write_size_precision(char * const bitstream, const uint8_t precision)
  {
    const uint8_t hdr = ((precision - 1) & 0b111) << 5;
    bitstream[0] = hdr;
    return SAMPLE_STREAM_HDR_LEN;
  }

  size_t V3C::parse_sample_stream_size(const char * const bitstream, const uint8_t precision)
  {
    uint8_t size[MAX_V3C_SIZE_PREC] = { 0 };
    memcpy(size, bitstream, precision);
    size_t combined_size = combineBytes(size, precision);

    return combined_size;
  }

  size_t V3C::write_sample_stream_size(char * const bitstream, const size_t size, const uint8_t precision)
  {
    uint8_t size_arr[MAX_V3C_SIZE_PREC] = { 0 };

    convert_size_big_endian(size, size_arr, precision);
    memcpy(bitstream, reinterpret_cast<char *>(size_arr), precision);
    return precision;
  }


  uvgrtp::media_stream* V3C::get_stream(const V3C_UNIT_TYPE type) const
  {
    if (streams_.find(type) != streams_.end()) {
      return streams_.at(type);
    } else {
      throw std::runtime_error("Media stream for given type not initialized");
    }
  }

  RTP_FORMAT V3C::get_format(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case v3cRTPLib::V3C_VPS:
      return RTP_FORMAT_GENERIC;
    case v3cRTPLib::V3C_AD:
      return RTP_FORMAT_ATLAS;
    case v3cRTPLib::V3C_OVD:
      return RTP_FORMAT_H265;
    case v3cRTPLib::V3C_GVD:
      return RTP_FORMAT_H265;
    case v3cRTPLib::V3C_AVD:
      return RTP_FORMAT_H265;
    case v3cRTPLib::V3C_PVD:
      return RTP_FORMAT_H265;
    case v3cRTPLib::V3C_CAD:
      return RTP_FORMAT_ATLAS;
    default:
      return RTP_FORMAT_GENERIC;
    }
  }

  RTP_FLAGS V3C::get_flags(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case v3cRTPLib::V3C_VPS:
      return RTP_NO_FLAGS;
    case v3cRTPLib::V3C_AD:
      return RTP_NO_FLAGS;
    case v3cRTPLib::V3C_OVD:
      return RTP_NO_H26X_SCL;
    case v3cRTPLib::V3C_GVD:
      return RTP_NO_H26X_SCL;
    case v3cRTPLib::V3C_AVD:
      return RTP_NO_H26X_SCL;
    case v3cRTPLib::V3C_PVD:
      return RTP_NO_H26X_SCL;
    case v3cRTPLib::V3C_CAD:
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

  template <typename T, typename DT, typename FT>
  static inline T& get_as( DT& data, const FT& field)
  {
    return static_cast<T&>(data.at(field));
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename TF, typename TV>
  static inline void process(std::ostream& out, TF& field, TV& value)
  {
    if constexpr (F == INFO_FMT::RAW)
    {
      out << value;
    }
    else
    {
      out << field << value << std::endl;
    }
  }

  template<INFO_FMT F = INFO_FMT::LOGGING, typename TF, typename TV>
  static inline void process(std::istream& in, TF& field, TV& value)
  {
    if constexpr (F == INFO_FMT::RAW)
    {
      in >> value;
    }
    else
    {
      in >> field;
      in >> value;
    }
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename TF, typename TV>
  static inline void process_num(Stream& out, TF& field, TV& value)
  {
    if constexpr (F == INFO_FMT::PARAM)
    {
      process<F>(out, std::string({ "constexpr int EXPECTED_NUM_", field, " = " }), value);
    }
    else
    {
      process<F>(out, std::string({ "  number of ", field, ": " }), value);
    }
  }

  template <INFO_FMT F = INFO_FMT::LOGGING, typename Stream, typename TF, typename TV>
  static inline void process_prec(Stream& out, TF& field, TV& value)
  {
    if constexpr (F == INFO_FMT::PARAM)
    {
      process<F>(out, std::string({ "constexpr uint8_t ", field, "_SIZE_PRECISION = " }), value);
    }
    else
    {
      process<F>(out, std::string({ "  ", field, " nalu size precision: " }), value);
    }
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
  static inline void postample(Stream&, T& data)
  {

  }
  template <INFO_FMT F = INFO_FMT::LOGGING, typename T>
  static inline void postample(std::ostream& ostream, T& data)
  {
    if (has_field(data, INFO_FIELDS::VAR_NAL_PREC) && get_as<bool>(data, INFO_FIELDS::VAR_NAL_PREC)) {
      ostream << "  Warning: Variable nal size precision between GOFs!" << std::endl;
    }
    if (has_field(data, INFO_FIELDS::VAR_NAL_NUM) && get_as<bool>(data, INFO_FIELDS::VAR_NAL_NUM)) {
      ostream << "  Warning: Variable number of nals between GOFs!" << std::endl;
    }
  }


  template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass, typename Stream, typename T>
  static void process_out_of_band_info(Stream& stream, T & data)
  {
    preample<F>(stream);

    if constexpr (std::is_same<DataClass, Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>::value)
    {
      if (has_field(data, INFO_FIELDS::NUM_GOF))  process_num<F>(stream, "GOFS", get_as<size_t>(data, INFO_FIELDS::NUM_GOF));
      if (has_field(data, INFO_FIELDS::NUM_VPS_NALU)) process_num<F>(stream, "VPS_NALU", get_as<size_t>(data, INFO_FIELDS::NUM_VPS_NALU));
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

    postample<F>(stream);
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
    info_data[INFO_FIELDS::NUM_GOF] = 0;
    info_data[INFO_FIELDS::VAR_NAL_NUM] = false;
    info_data[INFO_FIELDS::VAR_NAL_PREC] = false;
    for (const auto& gof : data)
    {
      for (const auto&[type, unit] : gof)
      {
        if (info_data[INFO_FIELDS::NUM_GOF] == 0)
        {
          populate_info_data<F>(unit, info_data);
          nal_num[type] = unit.num_nalus();
          nal_prec[type] = unit.nal_size_precision();
        } else
        {
          info_data[INFO_FIELDS::VAR_NAL_NUM] |= (nal_num[type] != unit.num_nalus());
          info_data[INFO_FIELDS::VAR_NAL_PREC] |= (nal_prec[type] != unit.nal_size_precision());
        }
      }
      info_data[INFO_FIELDS::NUM_GOF]++;
    }
    info_data[INFO_FIELDS::V3C_SIZE_PREC] = data.size_precision;
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
    case v3cRTPLib::V3C_VPS:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      break;
    case v3cRTPLib::V3C_AD:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      info_data[INFO_FIELDS::ATLAS_NAL_SIZE_PREC] = data.nal_size_precision();
      break;
    case v3cRTPLib::V3C_OVD:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      info_data[INFO_FIELDS::VIDEO_NAL_SIZE_PREC] = data.nal_size_precision();
      break;
    case v3cRTPLib::V3C_GVD:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      info_data[INFO_FIELDS::VIDEO_NAL_SIZE_PREC] = data.nal_size_precision();
      break;
    case v3cRTPLib::V3C_AVD:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      info_data[INFO_FIELDS::VIDEO_NAL_SIZE_PREC] = data.nal_size_precision();
      break;
    case v3cRTPLib::V3C_PVD:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      info_data[INFO_FIELDS::VIDEO_NAL_SIZE_PREC] = data.nal_size_precision();
      break;
    case v3cRTPLib::V3C_CAD:
      info_data[INFO_FIELDS::NUM_VPS_NALU] = data.num_nalus();
      info_data[INFO_FIELDS::ATLAS_NAL_SIZE_PREC] = data.nal_size_precision();
      break;
    default:
      break;
    }
  }
  

  template<INFO_FMT F, typename DataClass>
  void V3C::write_out_of_band_info(std::ostream & out_stream, const DataClass & data)
  {
    InfoDataType info_data;
    populate_info_data<F>(data, info_data);
    process_out_of_band_info<F, DataClass>(out_stream, info_data);
  }

  template<INFO_FMT F, typename DataClass>
  V3C::InfoDataType V3C::read_out_of_band_info(std::istream & in_stream)
  {
    InfoDataType data;
    process_out_of_band_info<F, DataClass>(in_stream, data);
    return data;
  }

  size_t V3C::combineBytes(const uint8_t *const bytes, const uint8_t num_bytes) {
    uint64_t combined_out = 0;
    for (uint8_t i = 0; i < num_bytes; ++i) {
      combined_out |= (static_cast<uint64_t>(bytes[i]) << (8 * (num_bytes - 1 - i)));
    }
    return combined_out;
  }

  void V3C::convert_size_big_endian(uint64_t in, uint8_t* out, size_t output_size) {
    for (size_t i = 0; i < output_size; ++i) {
      out[output_size - i - 1] = static_cast<uint8_t>(in >> (8 * i));
    }
  }

}