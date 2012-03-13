
AddOption('--force-mingw32',
          action='store_true', dest='mingw32', default=False,
          help='Cross compile using MinGW for Windows')

AddOption('--force-mingw64',
          action='store_true', dest='mingw64', default=False,
          help='Cross compile using MinGW for Windows 64-bit')

AddOption('--force-32bit',
          action='store_true', dest='gcc32', default=False,
          help='Always compile using 32-bit compiler flags')

AddOption('--force-64bit',
          action='store_true', dest='gcc64', default=False,
          help='Always compile using 64-bit compiler flags')

AddOption('--build-universal',
          action='store_true', dest='universal', default=False,
          help='Create Mac 32-bit and 64-bit universal binaries')

VariantDir('build', 'src')
env = Environment()

if env['PLATFORM'] == 'win32' or GetOption('mingw32') or GetOption('mingw64'):
	env.Append(CPPDEFINES = ['WINVER=0x0501'])

if GetOption('mingw32'):
	env.Tool('crossmingw', toolpath = ['scons-tools'])

if GetOption('mingw64'):
	env.Tool('crossmingw64', toolpath = ['scons-tools'])

if GetOption('gcc32'):
	env.Append(CFLAGS = ['-m32'])
	env.Append(CPPFLAGS = ['-m32'])
	env.Append(CXXFLAGS = ['-m32'])
	env.Append(LINKFLAGS = ['-m32'])

if GetOption('gcc64'):
	env.Append(CFLAGS = ['-m64'])
	env.Append(CPPFLAGS = ['-m64'])
	env.Append(CXXFLAGS = ['-m64'])
	env.Append(LINKFLAGS = ['-m64'])

env.Append(CFLAGS = ['-Wall', '-Werror', '-O2'])

conf = Configure(env)
conf.CheckLib('socket')
if conf.CheckFunc('getaddrinfo'):
	env.Append(CPPDEFINES = ['HAVE_GETADDRINFO'])
else:
	if conf.CheckLibWithHeader('ws2_32', 'ws2tcpip.h','c','getaddrinfo(0,0,0,0);'):
		env.Append(CPPDEFINES = ['HAVE_GETADDRINFO'])
	else:
		if conf.CheckLib('ws2_32'):
			# We have windows socket lib without getaddrinfo, disable IPv6
			env.Append(CPPDEFINES = ['DISABLE_IPV6'])
env = conf.Finish()

env.SConscript('build/SConscript', exports='env')

