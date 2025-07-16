#include "v3crtplib/v3c_api.h"

#include "V3C_Receiver.h"
#include "V3C_Sender.h"
#include "Sample_Stream.h"

#include <type_traits>
#include <utility>
#include <array>
#include <sstream>

namespace v3cRTPLib {

  template class V3C_State<V3C_Sender>;
  template class V3C_State<V3C_Receiver>;


  // Some macros for handling exceptions
#define V3C_STATE_TRY(state)         \
  auto& _err = state->error_;         \
  auto& _err_msg = state->error_msg_; \
  try
#define V3C_STATE_CATCH(return_error)                            \
  catch (const TimeoutException& e) {                            \
    _err = ERROR_TYPE::TIMEOUT;                                  \
    _err_msg = std::string("Timeout: ") + e.what();              \
  }                                                              \
  catch (const std::exception& e) {                              \
    _err = ERROR_TYPE::GENERAL;                                  \
    _err_msg = std::string("Undefined exception: ") + e.what();  \
  }                                                              \
  catch (...) {                                                  \
    _err = ERROR_TYPE::GENERAL;                                  \
    _err_msg = std::string("Unknown exception");                 \
  }                                                              \
  if constexpr (return_error) {                                  \
    return _err;                                                 \
  }

  template<typename T>
  ERROR_TYPE V3C_State<T>::set_error(ERROR_TYPE error, std::string msg) const {
    error_ = error;
    error_msg_ = std::string(msg);
    return error_;
  }


  namespace {
    using Iterator = typename Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::Iterator;
  }

  static inline Iterator& get_it(void * it_ptr)
  {
    return *(static_cast<Iterator*>(it_ptr));
  }
  static inline Iterator* get_it_ptr(void * it_ptr)
  {
    return static_cast<Iterator*>(it_ptr);
  }
  static inline void* unget_it_ptr(Iterator * it_ptr)
  {
    return static_cast<void *>(it_ptr);
  }

  template<typename T>
  V3C_State<T>::V3C_State(INIT_FLAGS flags, const char* endpoint_address, uint16_t port): connection_(nullptr), data_(nullptr), cur_gof_it_(nullptr), is_gof_it_valid_(false), error_(ERROR_TYPE::OK), error_msg_("")
  {
    init_connection(flags, endpoint_address, port);
  }

  template<typename T>
  V3C_State<T>::V3C_State(const uint8_t size_precision, INIT_FLAGS flags, const char* endpoint_address, uint16_t port) : connection_(nullptr), data_(nullptr), cur_gof_it_(nullptr), is_gof_it_valid_(false), error_(ERROR_TYPE::OK), error_msg_("")
  {
    init_connection(flags, endpoint_address, port);
    init_sample_stream(size_precision);
  }
  
  template<typename T>
  V3C_State<T>::V3C_State(const char* bitstream, size_t len, INIT_FLAGS flags, const char* endpoint_address, uint16_t port) : connection_(nullptr), data_(nullptr), cur_gof_it_(nullptr), is_gof_it_valid_(false), error_(ERROR_TYPE::OK), error_msg_("")
  {
    init_connection(flags, endpoint_address, port);
    init_sample_stream(bitstream, len);
  }


  template<typename T>
  V3C_State<T>::~V3C_State()
  {
    if (connection_) delete connection_;
    if (data_)       delete data_;
    if (cur_gof_it_) delete get_it_ptr(cur_gof_it_);
  }

  template<typename T>
  void V3C_State<T>::init_sample_stream(const uint8_t size_precision)
  {
    data_ = new Sample_Stream<SAMPLE_STREAM_TYPE::V3C>(size_precision);
  }

  template<typename T>
  void V3C_State<T>::init_sample_stream(const char * bitstream, size_t len)
  {
    data_ = new Sample_Stream<SAMPLE_STREAM_TYPE::V3C>(V3C::parse_bitstream(bitstream, len));
    init_cur_gof();
  }

  template<typename T>
  void V3C_State<T>::init_connection(INIT_FLAGS flags, const char* endpoint_address, uint16_t port)
  {
    if (connection_)
    {
      set_error(ERROR_TYPE::CONNECTION, "A connection object already exists");
      return;
    }
    connection_ = new T(flags, endpoint_address, port);
  }

  template<typename T>
  void V3C_State<T>::init_cur_gof(bool to_last)
  {
    if ((cur_gof_it_ && is_gof_it_valid_) || !data_)
    {
      set_error(ERROR_TYPE::DATA, "Gof iterator already initialized or no data exists");
      return;
    }
    if (cur_gof_it_ && !is_gof_it_valid_) delete get_it_ptr(cur_gof_it_);
    if (to_last)
    {
      cur_gof_it_ = unget_it_ptr(new Iterator(--(data_->end())));
    }
    else
    {
      cur_gof_it_ = unget_it_ptr(new Iterator(data_->begin()));
    }
    is_gof_it_valid_ = true;
  }

  template<typename T>
  char* v3cRTPLib::V3C_State<T>::get_bitstream(size_t* length) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return nullptr;
    }
    if (length) *length = data_->size();
    return data_->get_bitstream().release();
  }

  template<typename T>
  char* V3C_State<T>::get_bitstream_cur_gof(size_t* length) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return nullptr;
    }
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
      return nullptr;
    }
    if (length) *length = data_->size(get_it(cur_gof_it_));
    return data_->get_bitstream(get_it(cur_gof_it_)).release();
  }

  template<typename T>
  char* V3C_State<T>::get_bitstream_cur_gof_unit(const V3C_UNIT_TYPE type, size_t* length) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return nullptr;
    } 
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
      return nullptr;
    }
    if (length) *length = data_->size(get_it(cur_gof_it_), type);
    return data_->get_bitstream(get_it(cur_gof_it_), type).release();
  }

  template<typename T>
  ERROR_TYPE V3C_State<T>::first_gof()
  {
    if (!data_)
    {
      return set_error(ERROR_TYPE::DATA, "No data exists");
    }
    is_gof_it_valid_ = false; //Invalidate iterator so it can be initialized
    init_cur_gof();

    return ERROR_TYPE::OK;
  }

  template<typename T>
  ERROR_TYPE V3C_State<T>::last_gof()
  {
    if (!data_ )
    {
      return set_error(ERROR_TYPE::DATA, "No data exists");
    }
    is_gof_it_valid_ = false; //Invalidate iterator so it can be initialized
    init_cur_gof(true);

    return ERROR_TYPE::OK;
  }

  template<typename T>
  ERROR_TYPE V3C_State<T>::next_gof()
  {
    if (!data_)
    {
      return set_error(ERROR_TYPE::DATA, "No data exists");
    } 
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      return set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
    }
    ++get_it(cur_gof_it_);

    if (get_it(cur_gof_it_) == data_->end()) return set_error(ERROR_TYPE::EOS, "End of stream reached");

    return ERROR_TYPE::OK;
  }

  template<typename T>
  ERROR_TYPE V3C_State<T>::prev_gof()
  {
    if (!data_)
    {
      return set_error(ERROR_TYPE::DATA, "No data exists");
    } 
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      return set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
    }
    // Only decrement iterator if not at the first element
    if (get_it(cur_gof_it_) != data_->begin()) --get_it(cur_gof_it_);

    if (get_it(cur_gof_it_) == data_->end()) return set_error(ERROR_TYPE::EOS, "End of stream reached");

    return ERROR_TYPE::OK;
  }

  template<typename T>
  bool V3C_State<T>::cur_gof_has_unit(V3C_UNIT_TYPE unit) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return false;
    } 
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
      return false;
    }
    return (*get_it(cur_gof_it_)).find(unit) != (*get_it(cur_gof_it_)).end();
  }


  ERROR_TYPE send_bitstream(V3C_State<V3C_Sender>* state)
  {
    V3C_STATE_TRY(state)
    {
      state->connection_->send_bitstream(*state->data_);
    }
    V3C_STATE_CATCH(true)
  }

  ERROR_TYPE send_gof(V3C_State<V3C_Sender>* state)
  {
    V3C_STATE_TRY(state)
    {
      state->connection_->send_gof(*get_it(state->cur_gof_it_));
    }
    V3C_STATE_CATCH(true)
  }

  ERROR_TYPE send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type)
  {
    V3C_STATE_TRY(state)
    {
      state->connection_->send_v3c_unit((*get_it(state->cur_gof_it_)).get(type));
    }
    V3C_STATE_CATCH(true)
  }


  static V3C_Unit::V3C_Unit_Header make_header_from_struct(const HeaderStruct& hs)
  {
    switch (V3C_Unit::V3C_Unit_Header::vuh_to_type(hs.vuh_unit_type))
    {
    case v3cRTPLib::V3C_VPS:
      return V3C_Unit::V3C_Unit_Header(v3cRTPLib::V3C_VPS);

    case v3cRTPLib::V3C_AD:
      return V3C_Unit::V3C_Unit_Header(
        hs.vuh_unit_type,
        hs.vuh_v3c_parameter_set_id,
        hs.vuh_atlas_id
      );

    case v3cRTPLib::V3C_OVD:
      return V3C_Unit::V3C_Unit_Header(
        hs.vuh_unit_type,
        hs.vuh_v3c_parameter_set_id,
        hs.vuh_atlas_id
      );

    case v3cRTPLib::V3C_GVD:
      return V3C_Unit::V3C_Unit_Header(
        hs.vuh_unit_type,
        hs.vuh_v3c_parameter_set_id,
        hs.vuh_atlas_id,
        hs.vuh_map_index,
        hs.vuh_auxiliary_video_flag
      );

    case v3cRTPLib::V3C_AVD:
      return V3C_Unit::V3C_Unit_Header(
        hs.vuh_unit_type,
        hs.vuh_v3c_parameter_set_id,
        hs.vuh_atlas_id,
        hs.vuh_attribute_index,
        hs.vuh_attribute_partition_index,
        hs.vuh_map_index,
        hs.vuh_auxiliary_video_flag
      );

    case v3cRTPLib::V3C_PVD:
      return V3C_Unit::V3C_Unit_Header(
        hs.vuh_unit_type,
        hs.vuh_v3c_parameter_set_id,
        hs.vuh_atlas_id
      );

    case v3cRTPLib::V3C_CAD:
      return V3C_Unit::V3C_Unit_Header(
        hs.vuh_unit_type,
        hs.vuh_v3c_parameter_set_id
      );

    default:
      // TODO: Raise error
      return V3C_Unit::V3C_Unit_Header();
    }
  }

  static std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header> make_header_map_from_struct_array(const HeaderStruct hss[NUM_V3C_UNIT_TYPES])
  {
    std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header> headers;
    for (size_t t = 0; t < NUM_V3C_UNIT_TYPES; t++)
    {
      headers.emplace(V3C_UNIT_TYPE(t), make_header_from_struct(hss[t]));
    }
    return headers;
  }

  template <typename E, typename T, std::size_t N, typename = typename std::enable_if<std::is_enum<E>::value>::type>
  static std::map<E, T> array_to_enum_map(const T arr[N])
  {
    std::map<E, T> enum_map;
    for (size_t t = 0; t < N; t++)
    {
      enum_map[E(t)] = arr[t];
    }
    return enum_map;
  }


  ERROR_TYPE receive_bitstream(V3C_State<V3C_Receiver>* state, const uint8_t v3c_size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout)
  {
    V3C_STATE_TRY(state)
    {
      if (state->data_)
      {
        return state->set_error(ERROR_TYPE::DATA, "No data exists");
      }

      state->data_ = new Sample_Stream<SAMPLE_STREAM_TYPE::V3C>(
        state->connection_->receive_bitstream(
          v3c_size_precision,
          array_to_enum_map<V3C_UNIT_TYPE, uint8_t, NUM_V3C_UNIT_TYPES>(size_precisions),
          expected_num_gofs,
          array_to_enum_map<V3C_UNIT_TYPE, size_t, NUM_V3C_UNIT_TYPES>(num_nalus),
          make_header_map_from_struct_array(header_defs),
          timeout)
      );

      state->init_cur_gof();
    }
    V3C_STATE_CATCH(true)
  }

  ERROR_TYPE receive_gof(V3C_State<V3C_Receiver>* state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout)
  {
    V3C_STATE_TRY(state)
    {
      if (!state->data_)
      {
        return state->set_error(ERROR_TYPE::DATA, "No data exists");

      }
      try {
        state->data_->push_back(
          state->connection_->receive_gof(
            array_to_enum_map<V3C_UNIT_TYPE, uint8_t, NUM_V3C_UNIT_TYPES>(size_precisions),
            array_to_enum_map<V3C_UNIT_TYPE, size_t, NUM_V3C_UNIT_TYPES>(num_nalus),
            make_header_map_from_struct_array(header_defs),
            timeout,
            true
          )
        );
        state->is_gof_it_valid_ = false;
      }
      catch (const TimeoutException& e)
      {
        // Timeout trying to receive anymore gofs
        std::cerr << "Timeout: " << e.what() << std::endl;
        throw e;
      }

      if (!state->cur_gof_it_)
      {
        state->init_cur_gof();
      }
    }
    V3C_STATE_CATCH(true)
  }

  ERROR_TYPE receive_unit(V3C_State<V3C_Receiver>* state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct header_def, int timeout)
  {
    V3C_STATE_TRY(state)
    {
      if (!state->data_)
      {
        return state->set_error(ERROR_TYPE::DATA, "No data exists");
      }

      try {
        state->data_->push_back(
          state->connection_->receive_v3c_unit(
            unit_type,
            size_precision,
            expected_size,
            make_header_from_struct(header_def),
            timeout
          )
        );
        state->is_gof_it_valid_ = false; // Might allocate a new gof so it may not be valid anymore
      }
      catch (const TimeoutException& e)
      {
        // Timeout trying to receive v3c unit
        std::cerr << "Timeout: " << e.what() << " in unit type id " << static_cast<int>(unit_type) << std::endl;
        throw TimeoutException(std::string(e.what()) + " in unit type id " + std::to_string(unit_type));
      }

      if (!state->cur_gof_it_)
      {
        state->init_cur_gof();
      }
    }
    V3C_STATE_CATCH(true)
  }

  template<typename D>
  static char * write_info(const D& data, size_t* out_len, const INFO_FMT fmt)
  {
    // Write info to string stream
    std::ostringstream oss;
    V3C::write_out_of_band_info(oss, data, fmt);
    std::string output = oss.str();

    // copy output to char*
    *out_len = output.size() + 1; // Include null-termination byte
    char * out_char = static_cast<char*>(malloc(*out_len));
    if (out_char) memcpy(out_char, output.c_str(), *out_len);

    return out_char;
  }

  template<typename T>
  char * V3C_State<T>::get_bitstream_info_string(size_t* out_len, const INFO_FMT fmt) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return nullptr;
    } 

    return write_info(*data_, out_len, fmt);
  }

  template<typename T>
  char * V3C_State<T>::get_cur_gof_info_string(size_t* out_len, const INFO_FMT fmt) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return nullptr;
    }
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
      return nullptr;
    }

    return write_info(*get_it(cur_gof_it_), out_len, fmt);
  }

  template<typename T>
  char * V3C_State<T>::get_cur_gof_info_string(size_t* out_len, const V3C_UNIT_TYPE type, const INFO_FMT fmt) const
  {
    if (!data_)
    {
      set_error(ERROR_TYPE::DATA, "No data exists");
      return nullptr;
    } 
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
      return nullptr;
    }

    return write_info((*get_it(cur_gof_it_)).get(type), out_len, fmt);
  }


  template<typename T>
  ERROR_TYPE V3C_State<T>::print_state(const bool print_nalus, size_t num_gofs) const
  {
    if (!data_)
    {
      return set_error(ERROR_TYPE::DATA, "No data exists");
    }
    else if (!cur_gof_it_ || !is_gof_it_valid_)
    {
      return set_error(ERROR_TYPE::INVALID_IT, "Gof iterator not initialized correctly");
    }

    std::cout << "Sample stream of size " << data_->size() << " (v3c size precision: " << (int)data_->size_precision() << ") with state:" << std::endl;
    size_t gof_ind = 0;
    for (auto it = data_->begin(); it != data_->end(); ++it)
    {
      if (gof_ind >= num_gofs) {
        std::cout << "." << std::endl << "." << std::endl << "." << std::endl << "(showing only the first " << (int)num_gofs << " Gofs)" << std::endl;
        break;
      }

      std::cout << "|--Gof #" << gof_ind;
      if (it == get_it(cur_gof_it_)) std::cout << " [cur gof]";
      std::cout << std::endl;

      for (const auto&[type, unit] : *it)
      {
        switch (type)
        {
        case V3C_VPS:
          std::cout << "|  |--V3C unit of type VPS ";
          break;
        case V3C_AD:
          std::cout << "|  |--V3C unit of type AD  ";
          break;
        case V3C_OVD:
          std::cout << "|  |--V3C unit of type OVD ";
          break;
        case V3C_GVD:
          std::cout << "|  |--V3C unit of type GVD ";
          break;
        case V3C_AVD:
          std::cout << "|  |--V3C unit of type AVD ";
          break;
        case V3C_PVD:
          std::cout << "|  |--V3C unit of type PVD ";
          break;
        case V3C_CAD:
          std::cout << "|  |--V3C unit of type CAD ";
          break;

        default:
          // TODO: give error
          std::cout << "    not a valid unit type";
          break;
        }
        std::cout << "(nal size precision: " << (int)unit.nal_size_precision() << ", size: " << unit.size();
        if (type != V3C_VPS)
        {
          std::cout << ", num nalu: " << unit.num_nalus();
        }
        std::cout << ")" << std::endl;

        if (print_nalus)
        {
          for (const auto& nal : unit.nalus())
          {
            std::cout << "|  |  |--NAL unit of type " << (int)nal.get().nal_unit_type() << " (size: " << nal.get().size() << ")" << std::endl;
          }
        }
      }

      gof_ind++;
    }
    return ERROR_TYPE::OK;
  }

  template<typename C, typename R, typename... P>
  static void print_info(const C* obj, R (C::*info_func)(size_t*, P...) const, P... param)
  {
    size_t len = 0;
    auto info = std::unique_ptr<char, decltype(&free)>((obj->*info_func)(&len, param...), &free);
    std::cout << info.get() << std::endl;
  }

  template<typename T>
  void V3C_State<T>::print_bitstream_info(const INFO_FMT fmt) const
  {
    print_info(this, &V3C_State<T>::get_bitstream_info_string, fmt);
  }
  template<typename T>
  void V3C_State<T>::print_cur_gof_info(const INFO_FMT fmt) const
  {
    print_info(this, static_cast<char*(V3C_State<T>::*)(size_t*, INFO_FMT)const>(&V3C_State<T>::get_cur_gof_info_string), fmt);
  }
  template<typename T>
  void V3C_State<T>::print_cur_gof_info(const V3C_UNIT_TYPE type, const INFO_FMT fmt) const
  {
    print_info(this, static_cast<char*(V3C_State<T>::*)(size_t*, V3C_UNIT_TYPE, INFO_FMT)const>(&V3C_State<T>::get_cur_gof_info_string), type, fmt);
  }
  template<typename T>
  ERROR_TYPE V3C_State<T>::get_error_flag() const
  {
    return error_;
  }
  template<typename T>
  const char * V3C_State<T>::get_error_msg() const
  {
    return error_msg_.c_str();
  }
  template<typename T>
  void V3C_State<T>::reset_error_flag() const
  {
    error_ = ERROR_TYPE::OK;
    error_msg_ = std::string("");
  }
}