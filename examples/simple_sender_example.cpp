#include <v3crtplib/version.h>
#include <v3crtplib/v3c_api.h>

#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
  std::cout << "V3C rtp lib version: " << v3cRTPLib::get_version() << std::endl;
  
  if (argc < 2) {
    std::cout << "Enter bitstream file name as input parameter" << std::endl;
    return EXIT_FAILURE;
  }
  // Open file
  std::ifstream bitstream(argv[1]);
  
  //get length of file
  bitstream.seekg(0, bitstream.end);
  size_t length = bitstream.tellg();
  bitstream.seekg(0, bitstream.beg);

  /* Read the file and its size */
  if (length == 0) {
    return EXIT_FAILURE; //TODO: Raise exception
  }

  if (!bitstream.is_open()) {
    //TODO: Raise exception
    return -1;
  }

  auto buf = std::make_unique<char[]>(length);
  if (buf == nullptr) return EXIT_FAILURE;//TODO: Raise exception

  // read into char*
  if (!(bitstream.read(buf.get(), length))) // read up to the size of the buffer
  {
    if (!bitstream.eof())
    {
      //TODO: Raise exception
      buf = nullptr; // Release allocated memory before returning nullptr
      return -1;
    }
  }

  v3cRTPLib::V3C_State<v3cRTPLib::V3C_Sender> state;
  state.init_sample_stream(buf.get(), length);

  size_t len = 0;
  char* info = state.write_bitstream_info(len);
  std::cout << info << std::endl;
  
  v3cRTPLib::send_bitstream(state);
  
  return EXIT_SUCCESS;
}