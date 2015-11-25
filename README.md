Shairplay
=========
Free portable AirPlay server implementation similar to [ShairPort](https://github.com/abrasive/shairport).

Currently only AirPort Express emulation is supported.
Update by foxsen, 2015/11/25:
  * small fixes for AAC-ELD/ipv6 handling etc. Thanks to HaoLi for pointing them out. (untested)
  * I have figured out the different handling of iso9 and will update the code when having more time.
  * I am also learned most of the necessary information for the video mirroring part, but don't have enough time and interest to implement all the details and maintain them. If some guys would like to implement a fully open source shairplay that can handle both audio and video, I would like to help. Instead of the protocol implementation, I am interested more on the fairplay part and will try to get the hidden private key at spare time. I think I am close to that.
  
Update by foxsen, 2015/4/20: 
Experimental support for fairplay protocol and airplay:
  * fairplay encrypted audio is supported (et == 3)
  * AAC-ELD audio is supported
  * airplay service framework is added, up to the point that the mirroring 
    connection starts streaming. But the UI and callbacks need to be done.
  * fairplay support is performed via interactions with a server

Disclaimer
----------
All the resources in this repository are written using only freely available
information from the internet. The code and related resources are meant for
educational purposes only. It is the responsibility of the user to make sure
all local laws are adhered to.

Installation
------------

First you need to install some dependencies, for example on Ubuntu you would
write:
```
sudo apt-get install autoconf automake libtool
sudo apt-get install libltdl-dev libao-dev libavahi-compat-libdnssd-dev \
			libplist-dev libfdk-aac-dev libssl-dev
sudo apt-get install avahi-daemon
```

Note: The Raspbian apt repositories do not have [libplist](https://github.com/Chronic-Dev/libplist) or [libfdk-aac](https://github.com/mstorsjo/fdk-aac). You will need to compile these from source.

```
./autogen.sh
./configure
make
sudo make install
```

Notice that libao is required in order to install the shairplay binary,
otherwise only the library is compiled and installed.

Usage
-----

Check available options with ```shairplay --help```:

```
Usage: shairplay [OPTION...]

  -a, --apname=AirPort            Sets Airport name
  -p, --password=secret           Sets password
  -o, --server_port=5000          Sets port for RAOP service
      --ao_driver=driver          Sets the ao driver (optional)
      --ao_devicename=devicename  Sets the ao device name (optional)
      --ao_deviceid=id            Sets the ao device id (optional)
      --enable_airplay            Start airplay service
  -h, --help                      This help
```

Start the server with ```shairplay```, if you are connected to a Wi-Fi the
server should show as an AirPort Express on your iOS devices and Mac OS X
computers in the same network.

Notice that you need to have the airport.key file in your working directory when
starting the shairplay service. It is not included in the binary for possible
legal reasons.

Related software
----------------

* [ShairPort](https://github.com/abrasive/shairport), original AirPort Express emulator
* [ALAC](http://craz.net/programs/itunes/alac.html), ALAC decoder by David Hammerton

Description
-----------

Short description about what each file in the main library does:

```
src/lib/base64.*         - base64 encoder/decoder
src/lib/dnssd.*          - dnssd helper functions
src/lib/http_parser.*    - HTTP parser from joyent (nginx fork)
src/lib/http_request.*   - Request parser that uses http_parser
src/lib/http_response.*  - Extremely simple HTTP response serializer
src/lib/httpd.*          - Generic HTTP/RTSP server
src/lib/logger.*         - Logging related functions
src/lib/netutils.*       - Mostly socket related code
src/lib/raop.*           - Main RAOP handler, handles all RTSP stuff
src/lib/raop_rtp.*       - Handles the RAOP RTP related stuff (UDP/TCP)
src/lib/raop_buffer.*    - Parses and buffers RAOP packets, resend logic here
src/lib/rsakey.*         - Decrypts and parses the RSA key to bigints
src/lib/rsapem.*         - Converts the RSA PEM key to DER encoded bytes
src/lib/sdp.*            - Extremely simple RAOP specific SDP parser
src/lib/utils.*          - Utils for reading a file and handling strings
```

Short description about what each file in the Qt application does:

```
AirTV-Qt/main.cpp                 - Initializes the application
AirTV-Qt/mainapplication.cpp      - Creates the tray icon and starts RAOP
AirTV-Qt/raopservice.cpp          - Handles all communication with the library
AirTV-Qt/raopcallbackhandler.cpp  - Converts C callbacks to Qt callbacks
AirTV-Qt/audiooutput.cpp          - Takes care of the actual audio output
```

