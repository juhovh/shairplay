import time
from struct import *
from Shairplay import *

hwaddr = pack('BBBBBB', 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB)
class SampleCallbacks(RaopCallbacks):
	def audio_init(self, bits, channels, samplerate):
		print "Initializing " + str(bits) + " " + str(channels) + " " + str(samplerate)
	def audio_process(self, session, buffer):
		print "Processing " + str(len(buffer)) + " bytes of audio"

shairplay = LoadShairplay(".")
callbacks = SampleCallbacks()

raop = RaopService(shairplay, callbacks)
port = raop.start(5000, hwaddr)

dnssd = DnssdService(shairplay)
dnssd.register_raop("RAOP test", port, hwaddr)

time.sleep(50)

dnssd.unregister_raop()
raop.stop()

del dnssd
del raop

