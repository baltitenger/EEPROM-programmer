import ycm_core
from glob import glob

variant = "eightanaloginputs"
cpu = "ATmega328P"
clock_mhz = 16
assert variant != "" and cpu != "" and clock_mhz != 0, "Customize your flags!"

# This is the list of all directories to search for header files
libDirs  = glob("/usr/share/arduino/hardware/archlinux-arduino/avr/libraries/*/src")
libDirs += glob("~/Arduino/libraries/*/src")
libDirs += glob(".")

def FlagsForFile( filename, **kwargs ):
  flags = [
    # General flags
    '-Wall',
    '-Wextra',

    '-Wno-attributes',
    '-std=c++17',
    '-x',
    'c++',

    '-fno-exceptions',
    '-fpermissive',
    '-fno-threadsafe-statics',

    '-include/usr/share/arduino/hardware/archlinux-arduino/avr/cores/arduino/Arduino.h',
    '-isystem/usr/share/arduino/hardware/archlinux-arduino/avr/variants/%s' % variant,
    '-isystem/usr/share/arduino/hardware/archlinux-arduino/avr/variants/eightanaloginputs'
    '-isystem/usr/share/arduino/hardware/archlinux-arduino/avr/cores/arduino',
    '-isystem/usr/lib/gcc/avr/8.2.0/include',
    '-I/usr/lib/gcc/avr/8.2.0/plugin/include',
    '-isystem/usr/avr/include',
    '-isystem/usr/local/include',
    '-isystem/usr/include',

    '-mmcu=%s' % cpu,
    '-D__AVR_%s__' % cpu,
    '-D__OPTIMIZE__',
    '-DF_CPU=%s000000' % clock_mhz,
  ]
  for libDir in libDirs:
    flag = '-I' + libDir
    flags.append(flag)
  return {
    'flags': flags,
  }
