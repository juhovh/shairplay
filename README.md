Shairplay
=========
Free portable AirPlay server implementation similar to [ShairPort](https://github.com/abrasive/shairport).

Currently only AirPort Express emulation is supported.

Disclaimer
----------
All the resources in this repository are written using only freely available
information from the internet. The code and related resources are meant for
educational purposes only. It is up to the user to make sure all local laws are
adhered to.

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

