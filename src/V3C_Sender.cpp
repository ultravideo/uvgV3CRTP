#include "V3C_Sender.h"


namespace v3cRTPLib {

  V3C_Sender::V3C_Sender()
  {
  }


  V3C_Sender::~V3C_Sender()
  {
  }

  int V3C_Sender::send()
  {
    std::atomic<uint64_t> bytes_sent;
    v3c_file_map mmap;
    bitstream_info info;

    /* Read the file and its size */
    uint64_t len = get_size(PATH);
    if (len == 0) {
      return EXIT_FAILURE;
    }
    auto cbuf = get_cmem(PATH);
    if (cbuf == nullptr) return EXIT_FAILURE;

    std::cout << "Parsing V3C file, size " << len << std::endl;

    /* Map the locations and sizes of Atlas and video NAL units with the mmap_v3c_file function */
    mmap_v3c_file(cbuf.get(), len, mmap, info);

    std::cout << "Sending V3C NAL units via uvgRTP" << std::endl;

    /* Create the necessary uvgRTP media streams */
    uvgrtp::context ctx;
    std::pair<std::string, std::string> addresses_sender(LOCAL_IP, LOCAL_IP);
    uvgrtp::session* sess = ctx.create_session(addresses_sender);

    int flags = 0;
    v3c_streams streams = init_v3c_streams(sess, 8892, 8890, flags, false);

    /* Start sending data */
    //std::unique_ptr<std::thread> vps_thread = 
    //  std::unique_ptr<std::thread>(new std::thread(&sender_func, streams.vps, cbuf.get(), mmap.vps_units, RTP_NO_FLAGS, V3C_VPS, bytes_sent));

    //std::unique_ptr<std::thread> ad_thread =
    //  std::unique_ptr<std::thread>(new std::thread(&sender_func, streams.ad, cbuf.get(), mmap.ad_units, RTP_NO_FLAGS, V3C_AD, bytes_sent));

    //std::unique_ptr<std::thread> ovd_thread =
    //  std::unique_ptr<std::thread>(new std::thread(&sender_func, streams.ovd, cbuf.get(), mmap.ovd_units, RTP_NO_H26X_SCL, V3C_OVD, bytes_sent));

    //std::unique_ptr<std::thread> gvd_thread =
    //  std::unique_ptr<std::thread>(new std::thread(&sender_func, streams.gvd, cbuf.get(), mmap.gvd_units, RTP_NO_H26X_SCL, V3C_GVD, bytes_sent));

    //std::unique_ptr<std::thread> avd_thread =
    //  std::unique_ptr<std::thread>(new std::thread(&sender_func, streams.avd, cbuf.get(), mmap.avd_units, RTP_NO_H26X_SCL, V3C_AVD, bytes_sent));

    //if (vps_thread && vps_thread->joinable())
    //{
    //  vps_thread->join();
    //}
    //if (ad_thread && ad_thread->joinable())
    //{
    //  ad_thread->join();
    //}
    //if (ovd_thread && ovd_thread->joinable())
    //{
    //  ovd_thread->join();
    //}
    //if (gvd_thread && gvd_thread->joinable())
    //{
    //  gvd_thread->join();
    //}
    //if (avd_thread && avd_thread->joinable())
    //{
    //  avd_thread->join();
    //}

    sess->destroy_stream(streams.vps);
    sess->destroy_stream(streams.ad);
    sess->destroy_stream(streams.ovd);
    sess->destroy_stream(streams.gvd);
    sess->destroy_stream(streams.avd);

    std::cout << "Sending finished, " << bytes_sent << " bytes sent" << std::endl;

    // Print side-channel information needed by the receiver to receive the bitstream correctly.
    // Check INFO_FMT for available formats. Writing to file also possible.
    write_out_bitstream_info(info, std::cout, INFO_FMT::LOGGING);

    if (sess)
    {
      // Session must be destroyed manually
      ctx.destroy_session(sess);
    }
    return EXIT_SUCCESS;
  }

  void V3C_Sender::sender_func(uvgrtp::media_stream * stream, const char * cbuf, const std::vector<v3c_unit_info>& units, rtp_flags_t flags, int fmt, std::atomic<size_t> &bytes_sent)
  {
    for (auto& p : units) {
      for (auto i : p.nal_infos) {
        rtp_error_t ret = RTP_OK;
        //std::cout << "Sending NAL unit in location " << i.location << " with size " << i.size << std::endl;
        ret = stream->push_frame((uint8_t*)cbuf + i.location, i.size, flags);
        if (ret != RTP_OK) {
          std::cout << "Failed to send RTP frame!" << std::endl;
        }
        bytes_sent += i.size;
      }
    }
  }

}