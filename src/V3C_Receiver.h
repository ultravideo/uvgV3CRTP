#pragma once

#include "V3C.h"

#include <thread>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <string>


namespace v3cRTPLib {

  class V3C_Receiver :
    public V3C
  {
  public:
    V3C_Receiver();
    ~V3C_Receiver();

    int receive();

    // Used in receiver_hooks to copy the received data
    static void copy_rtp_payload(std::vector<v3c_unit_info>* units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame);

  
    // Hooks for the media streams
    static void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame); // VPS only included for simplicity of demonstration
    static void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
    static void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
    static void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
    static void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

private:
    static constexpr char LOCAL_IP[] = "127.0.0.1";

    // This example runs for 10 seconds
    static constexpr auto RECEIVE_TIME_S = std::chrono::seconds(10);

    uint64_t vps_count;

    /* These values specify the amount of NAL units inside each type of V3C unit. These need to be known to be able to reconstruct the
     * GOFs after receiving. These default values correspond to the provided test sequence, and may be different for other sequences.
     * The sending example has prints that show how many NAL units each V3C unit contain. For other sequences, these values can be
     * modified accordingly. */
    static constexpr int VPS_NALS = 2; // VPS only included for simplicity of demonstration
    static constexpr int AD_NALS = 35;
    static constexpr int OVD_NALS = 35;
    static constexpr int GVD_NALS = 131;
    static constexpr int AVD_NALS = 131;

    // These are signaled to the receiver one way or the other, for example SDP
    static constexpr uint8_t ATLAS_NAL_SIZE_PRECISION = 2;
    static constexpr uint8_t VIDEO_NAL_SIZE_PRECISION = 4;
    static constexpr uint8_t V3C_SIZE_PRECISION = 2;

    /* NOTE: In case where the last GOF has fewer NAL units than specified above, the receiver does not know how many to expect
       and cannot reconstruct that specific GOF. s*/

       /* How many *FULL* Groups of Frames we are expecting to receive */
    static constexpr int EXPECTED_GOFS = 9;

    /* Path to the V3C file that we are receiving. This is included so that you can check that the reconstructed full GOFs are equal to the
     * original ones */
    std::string PATH = "";

    bool write_file(const char* data, size_t len, const std::string& filename);
  };

}