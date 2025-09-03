uvgV3CRTP
=======

uvgV3CRTP is an academic open-source library for transmitting V3C bitstreams over RTP. uvgV3CRTP is build on top of uvgRTP and is being developed in C++ under the BSD-3-Clause license. 
              
uvgV3CRTP takes in a V3C bitstream, parses it, and creates the necessary RTP frames for transmission. For more information on V3C transmission over RTP refer to [draft-ietf-avtcore-rtp-v3c-11]() (work in progress, to be replaced with final document when ready).
  
uvgV3CRTP is still under development. API/ABI is still subject to change.


## Table of Contents

- [Compiling uvgV3CRTP](#compilation)
  - [Dependencies](#dependencies)
  - [Compilation](#compile-uvgv3crtp)
- [Using uvgV3CRTP](#using-uvgv3crtp)
  - [Example](#example)


## Compilation
### Dependencies
To compile the library and the application, please install ```CMake```, ```g++```, and ```git```.
On Debian/Ubuntu:
```
sudo apt install cmake g++ git
```

uvgV3CRTP also depends on uvgRTP, but Cmake should handle fetching and building uvgRTP. However, if this fails uvgRTP should be cloned under [dependencies/uvgRTP](dependencies/uvgRTP).

### Compile uvgV3CRTP
First, create a folder for build configuration:

```
mkdir build && cd build
```

Then generate build configuration with CMake using the following command:

```
cmake ..
```

After generating the build configuration, run the following commands to compile and install uvgV3CRTP:
```
make
sudo make install
```

```sudo make install``` can be skipped if shared libraries are not needed.

#### Additional compilation parameters

The following CMake flags can be used to customise the compilation:
- Does not buld examples: ```cmake -DUVGV3CRTP_DISABLE_EXAMPLES=1 ..```
- Does not attempt to create a shared library: ```cmake -DUVGV3CRTP_DISABLE_INSTALL=1 ..```
- Does not ignore compiler warnings: ```cmake -DUVGV3CRTP_DISABLE_WERROR=0 ..```


## Using uvgV3CRTP

For working examples of using uvgV3CRTP see [examples/](examples/). The full API can be seen in [include/uvgv3crtp/v3c_api.h](include/uvgv3crtp/v3c_api.h). Below is a simple example of using uvgV3CRTP.


### Example:

To use uvgV3CRTP, include the API header
```
#include <uvgv3crtp/v3c_api.h>
```

#### Sender

A state object is used to manage the V3C bitstream and RTP streams. A state object for sending can be created as follows:

```
uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Sender> state(uvgV3CRTP::INIT_FLAGS::ALL, "127.0.0.1", 8890);
```

This creates a state and initializes RTP media streams for sending to the address ```127.0.0.1:8890```. Using ```INIT_FLAGS``` (see [global.h](include/uvgv3crtp/global.h) it is possible to control what types of V3C units are sent. A separate RTP media stream is created for each type of V3C unit and the streames are multiplexed to the same port using SSRC (hardcoded to be ```vuh_unit_type + 1```).

Preparing the bitstream for sending can be done with
```
state.init_sample_stream(<V3C_bitstream>, <bitstream_length>);
```
This parses the bitstream into a form that can be sent over RTP. Note: the bitstream is expected to contain sample stream headers in order to parse correctly. If sample stream headers are not present V3C units need to be added one at a time using
```
state.append_to_sample_stream(<V3C_Unit_bitstream>, <bitstream_length>);
```

After the bitstream has been prepared, it can be sent using
```
uvgV3CRTP::send_bitstream(&state);
```
(for sending one Unit or GoF at a time see [examples/](examples/))

Finally, for viewing information about the current state
```
state.print_state();
```
and for getting bitstream related info (needed by the receiver to receive correctly)
```
state.print_bitstream_info();
```


### Receiver

To receive data from the sender, state needs to be created in the receiver configuration:
```
uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver> state(uvgV3CRTP::INIT_FLAGS::ALL, "127.0.0.1", 8890);
```
Here the address should match the one set in the sender.

To receive the whole bitstream from the sender:
```
uvgV3CRTP::receive_bitstream(&state, <v3c_size_precision>, <v3c_unit_size_precisions>, <expected_number_of_gof>, <num_nalus>, <header_defs>, <timeout(ms)>);
```

For more information about the parameters and further examples see [examples/](examples/).
Note: in order to correctly receive and reconstruct the bitstream certain out-of-band information, that is not transported over RTP, is needed. uvgV3CRTP does not by default pass along this information so it is up to the user to handle.


