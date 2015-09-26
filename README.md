# Shairplay
___

**Free, portable AirPlay Server implementation similar to [ShairPort](https://github.com/abrasive/shairport).  
Currently, only AirPort Express emulation is supported.**

## Table of Contents

1. [Prerequisites](#prerequisites)
  * [External Dependencies](#external-Dependencies)
  * [Installation on Linux](#installation-on-linux)
  * [Installation on OSX](#installation-on-osx)
1. [How-to-Build](#how-to-build)
  * [Using Make](#using-make)
  * [Using XCode](#using-xcode)
1. [Installation](#installation)
1. [Airport.key File](#airport.key)
1. [Usage](#usage)
1. [Development](#development) 
  * [Components](#components)
  * [Source Locations](#source-locations)
1. [Disclaimer](#disclaimer)
1. [Related Projects](#related-projects)

Prerequisites<a id="prerequisites"></a>
----

### External Dependencies<a id="external-Dependencies"></a>
Before building, a number of external dependencies needs to be installed (depending on your platform).

| Name | Title |
| :----- | -----------: |
| libao | Cross platform audio library |
| libavahi | DNS/DNS-SD service discovery |
| autoconf | Generatinge configure scripts |
| automake | Generating Makefiles |
| libtool  | Creating portable compiled libraries |
| pkg-config  | Gathering metadata of installed packages|

### Installation on Linux<a id="installation-on-linux"></a>
Using apt-get:

```
sudo apt-get install autoconf automake libtool libltdl-dev
sudo apt-get libao-dev
sudo apt-get libavahi-compat-libdnssd-dev
sudo apt-get install avahi-daemon
```

### Installation on OSX<a id="installation-on-osx"></a>
On Apple OSX, a third-party package manager such as [Homebrew](http://brew.sh) needs to be installed before installing dependencies.

After installing Homebrew, run:

```
brew install autoconf automake libtool pkg-config
brew install libao
```

How-to-Build<a id="how-to-build"></a>
----

In order to build the *shairplay application*, the *libao* audio library dependency needs to be installed - otherwise, only the *library* will be built.


### Using Make<a id="using-make"></a>

#### Preparation

```
./autogen.sh
./configure
```

#### Building

```
make clean
make
```

#### Output

| Output | Location |
| :----- | -----------: |
|  Application Binary | src/shairplay |
|  Static Library | src/lib/.libs/libshairplay.a |
|  Dynamic Library | src/lib/.libs/libshairplay.0.dylib |


### Using XCode<a id="using-xcode"></a>

Both the Shairplay application and library are available as targets within a main XCode project file. Internal dependencies are registered as dependencies within this project.

#### Preparation

```
./autogen.sh
./configure
make clean
```

#### XCode Project Files

| XCode Project File | Location |
| :----- | -----------: |
|  Main Project| extras/xcode/shairplay/shairplay.xcodeproj |
|  Dependency: Crypto | extras/xcode/crypto/crypto.xcodeproj |
|  Dependency: ALAC | extras/xcode/alac/alac.xcodeproj |

#### Output

All XCode build output is deployed the "build" folder.


Installation<a id="installation"></a>
----

After [building](#how-to-build), run:

```
sudo make install
```

The results will be located at

- `/usr/local/bin` (application binary)
- `/usr/local/lib` (static & dynamic libraries)
- `/usr/local/include/shairplay` (library header files)

Usage<a id="usage"></a>
-----

### Running an AirPlay Server

Place the **airport.key** file in your working directory.
Then, start the server with:

```
shairplay [options...]
```

If you are connected to a **Wi-Fi**, the
server should show as an AirPort Express on your iOS devices and Mac OS X computers in the same network.

### Options

- **-a, --apname=AirPort** - Sets Airport name (*Default: Shairplay*)
- **-p, --password=secret**  - Sets password
- **-o, --server_port=5000** - Sets port for RAOP service
- **-h, --help** - Help 
- **--hwaddr=address** - Sets the MAC address, useful if running multiple instances
- **--ao_driver=driver** - Sets the ao driver
- **--ao_devicename=devicename** - Sets the ao device name
- **--ao_deviceid=id** - Sets the ao device id


Example:

```
shairplay --help
```

Airport.key File<a id="airport.key"></a>
-----
You need to have the [airport.key](#airport.key) file in the current working directory when starting the server.

It is not included in the binary for legal reasons.

## Development<a id="development"></a>

### Components<a id="components"></a>
| Component | Folder |
| :----- | -----------: |
| Main Application & Library sources | src/lib/ |
| Main Library public headers | include/shairplay/ |
| Qt4 Application sources | AirTV-Qt/ |
| Python bindings | src/bindings/python/ |
| Qt bindings | src/bindings/qt4/ |

### Source Location<a id="source-locations"></a>

#### Main Application
| File | Contents |
| :----- | -----------: |
|  shairplay.c | Shairplay application |

#### Main Library
| File | Contents |
| :----- | -----------: |
|  base64.* | base64 encoder/decoder |
|  dnssd.* | dnssd helper functions |
|  http_parser.* | HTTP parser from joyent (nginx fork) |
|  http_request.* | Request parser that uses http_parser |
|  http_response.* | Extremely simple HTTP response serializer |
|  httpd.* | Generic HTTP/RTSP server |
|  logger.* | Logging related functions |
|  netutils.* | Mostly socket related code |
|  raop.* | Main RAOP handler, handles all RTSP stuff |
|  raop_rtp.* | Handles the RAOP RTP related stuff (UDP/TCP) |
|  raop_buffer.* | Parses and buffers RAOP packets, resend logic here |
|  rsakey.* | Decrypts and parses the RSA key to bigints |
|  rsapem.* | Converts the RSA PEM key to DER encoded bytes |
|  sdp.* | Extremely simple RAOP specific SDP parser |
|  utils.* | Utils for reading a file and handling strings |

### Qt Application

| File | Contents |
| :----- | -----------: |
| main.cpp | Initializes the application |
| mainapplication.cpp | Creates the tray icon and starts RAOP |
| raopservice.cpp | Handles all communication with the library |
| raopcallbackhandler.cpp | Converts C callbacks to Qt callbacks |
| audiooutput.cpp | Takes care of the actual audio output |

## Disclaimer<a id="disclaimer"></a>
All the resources in this repository are written using only freely available information from the internet. The code and related resources are meant for educational purposes only. It is the responsibility of the user to make sure all local laws are adhered to.

## Related Projects<a id="related-projects"></a>
- [ShairPort](https://github.com/abrasive/shairport), original AirPort Express emulator
- [ALAC](http://craz.net/programs/itunes/alac.html), ALAC decoder by David Hammerton