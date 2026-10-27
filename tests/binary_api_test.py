"""Inspect the actual Metamod entry point without starting a CS2 server."""
import ctypes
import pathlib
import sys

library = ctypes.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
factory = library.CreateInterface
factory.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
factory.restype = ctypes.c_void_p
code = ctypes.c_int(-1)
plugin = factory(b"ISmmPlugin", ctypes.byref(code))
assert plugin and code.value == 0, (plugin, code.value)
vtable = ctypes.cast(plugin, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
api_version = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)(vtable[0])(plugin)
assert api_version == 18, api_version
assert not factory(b"UnsupportedInterface", ctypes.byref(code))
print("binary CreateInterface / Metamod API18: OK")
