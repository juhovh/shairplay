import time
from struct import *
from Shairplay import *

hwaddr = pack('BBBBBB', 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB)

shairplay = LoadShairplay(".")

raop = RaopService(shairplay, RaopCallbacks())
port = raop.start(5000, hwaddr)

dnssd = DnssdService(shairplay)
dnssd.register_raop("RAOP test", port)

time.sleep(50)

dnssd.unregister_raop()
raop.stop()
del raop

