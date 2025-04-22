#include "V3C_Receiver.h"

#include <bitset>

namespace v3cRTPLib {

  V3C_Receiver::V3C_Receiver()
  {
  }


  V3C_Receiver::~V3C_Receiver()
  {
  }

  int V3C_Receiver::receive()
  {
    std::cout << "Starting uvgRTP V3C receive hook example" << std::endl;

    /* Initialize uvgRTP context and session*/
    uvgrtp::context ctx;
    std::pair<std::string, std::string> addresses_receiver(LOCAL_IP, LOCAL_IP);
    uvgrtp::session* sess = ctx.create_session(addresses_receiver);
    int flags = 0;

    // Create the uvgRTP media streams with the correct RTP format
    v3c_streams streams = init_v3c_streams(sess, 8890, 8892, flags, true);

    // Initialize memory map
    v3c_file_map mmap = init_mmap(ATLAS_NAL_SIZE_PRECISION, VIDEO_NAL_SIZE_PRECISION);

    streams.vps->install_receive_hook(&mmap.vps_units, &vps_receive_hook);
    streams.ad->install_receive_hook(&mmap.ad_units, &ad_receive_hook);
    streams.ovd->install_receive_hook(&mmap.ovd_units, &ovd_receive_hook);
    streams.gvd->install_receive_hook(&mmap.gvd_units, &gvd_receive_hook);
    streams.avd->install_receive_hook(&mmap.avd_units, &avd_receive_hook);
    streams.avd->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);

    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_S.count() << " s" << std::endl;

    uint64_t ngofs = 0;     // Number of received GOFs
    uint64_t bytes = 0;     // Number of bytes in reconstructed bitstream
    uint64_t ptr = 0;       // Pointer of current position on the received file
    bool hdb = true;        // Write header byte or not. True only for first GOF of file.
    bool timeout = false;   // Has receiving timedout

    // Save each GOF into data structures
    struct gof_info {
      uint64_t size = 0;
      char* buf = nullptr;
    };
    std::map<uint64_t, gof_info> gofs_buf = {};

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();;

    while (ngofs < EXPECTED_GOFS) {
      // Check if we received enough NAL units of a GOF
      if (is_gof_ready(ngofs, mmap)) {

        // Get GOF size and initialize a new data structure for it
        uint64_t gof_len = get_gof_size(hdb, ngofs, mmap, V3C_SIZE_PRECISION);
        gof_info cur = { gof_len, new char[gof_len] };
        gofs_buf.insert({ ngofs, cur });

        ptr = 0; //Don't reset the ptr because we are writing the whole file into the buffer

        // Reconstruct the GOF from NAL units and update size of the to-be complete file. NOTE: Not the same as the amount of
        // received bytes, because we add new info such as V3C unit headers here.
        bytes += reconstruct_v3c_gof(hdb, gofs_buf.at(ngofs).buf, ptr, mmap, ngofs, V3C_SIZE_PRECISION);
        std::cout << "Full GOF received, num: " << ngofs << std::endl;
        ngofs++;
        hdb = false; // Only add the V3C Sample Stream header byte to only the first GOF
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      auto runtime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

      if (runtime > RECEIVE_TIME_S.count() * 1000) {
        if (!timeout) {
          std::cout << "Timeout, finalize GoFs" << std::endl;
          finalize_gof(mmap);
          timeout = true;
        } else {
          break;
        }
      }
    }
    std::cout << ngofs << " full GOFs received" << std::endl;

    /* Reception done, clean up uvgRTP */
    sess->destroy_stream(streams.vps);
    sess->destroy_stream(streams.ad);
    sess->destroy_stream(streams.ovd);
    sess->destroy_stream(streams.gvd);
    sess->destroy_stream(streams.avd);

    ctx.destroy_session(sess);

    // Not we have all the GOFs constructed. Next up save them all into a single file
    auto out_buf = std::make_unique<char[]>(bytes);
    std::memset(out_buf.get(), 0, bytes); // Initialize with zeros

    uint64_t ptr2 = 0;
    // Reconstruct file from GOFs
    for (auto& p : gofs_buf) {
      memcpy(&out_buf[ptr2], p.second.buf, p.second.size);
      ptr2 += p.second.size;
    }

    /* Read the original file and its size for verification */
    uint64_t len = get_size(PATH);
    if (len == 0) {
      return EXIT_FAILURE;
    }
    std::cout << "Reading original file for comparison " << std::endl;

    auto original_buf = get_cmem(PATH);
    if (original_buf == nullptr) return EXIT_FAILURE;

    bool diff = false;
    if (len != bytes) {
      std::cout << "Error: Original bitstream size does not match reconstructed bitstream size: " << (int)len << " vs. " << (int)bytes << std::endl;
      diff = true;
    }

    // Compare reconstructed file with the original one
    for (int i = 0; i < (len < bytes ? len : bytes); ++i) {
      if (original_buf[i] != out_buf[i]) {
        diff = true;
        std::cout << "Difference found in " << i << std::endl;
        std::cout << "  orig byte: " << std::bitset<8>(original_buf[i]) << std::endl;
        std::cout << "  recv byte: " << std::bitset<8>(out_buf[i]) << std::endl;

        break;
      }
    }
    if (!diff) {
      std::cout << "No difference found in " << ngofs << " GOFs" << std::endl;
    }

    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
  }

  void V3C_Receiver::copy_rtp_payload(std::vector<v3c_unit_info>* units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame)
  {
    if (units->back().nal_infos.size() == max_size) {
      v3c_unit_header hdr(units->back().header.vuh_unit_type);
      v3c_unit_info info = { 0, hdr, {}, 0, false };
      switch (units->back().header.vuh_unit_type) {
      case V3C_AD: {
        info.sample_stream_nal_precision = DEFAULT_ATLAS_NAL_SIZE_PRECISION;
        info.header.ad = { (uint8_t)units->size(), 0 };
        break;
      }
      case V3C_OVD: {
        info.sample_stream_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
        info.header.ovd = { (uint8_t)units->size(), 0 };
        break;
      }
      case V3C_GVD: {
        info.sample_stream_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
        info.header.gvd = { (uint8_t)units->size(), 0, 0, 0 };
        break;
      }
      case V3C_AVD: {
        info.sample_stream_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
        info.header.avd = { (uint8_t)units->size(), 0 };
        break;
      }
      }
      units->push_back(info);
    }

    if (units->back().nal_infos.size() <= max_size) {
      char* cbuf = new char[frame->payload_len];
      memcpy(cbuf, frame->payload, frame->payload_len);
      nal_info nalu = { units->back().ptr, frame->payload_len, cbuf };
      units->back().nal_infos.push_back(nalu);
      units->back().ptr += frame->payload_len;
    }
    if (units->back().nal_infos.size() == max_size) {
      units->back().ready = true;
    }
  }

  void V3C_Receiver::vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
  {
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    char* cbuf = new char[frame->payload_len];
    memcpy(cbuf, frame->payload, frame->payload_len);
    v3c_unit_info vps = { 0, {}, {{0, frame->payload_len, cbuf}} };
    vec->push_back(vps);

    (void)uvgrtp::frame::dealloc_frame(frame);
  }
  void V3C_Receiver::ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
  {
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, AD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
  }
  void V3C_Receiver::ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
  {
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, OVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
  }
  void V3C_Receiver::gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
  {
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, GVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
  }
  void V3C_Receiver::avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
  {
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, AVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
  }

  bool V3C_Receiver::write_file(const char* data, size_t len, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);

    if (!file.is_open()) {
      return false; // Failed to open the file
    }

    file.write(data, len);

    file.close();
    return true; // File write successful
  }

}