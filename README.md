# EEProgrammer
Load any type of data into 24LC512 64k EEProm IC.

These devices are cheap and only need 2 lines to access (I2C). They require a bit of special handling when writing data.  It must be in 128 byte blocks, and takes about 5 milliseconds to do each block.

This program takes care of all of that while receiving 57.6k data with no handshake protocol.  It loads into an Arduino Nano (an Uno would also work), and takes advantage of the hardware serial port to achieve high download speeds.

A simple directory entry is created for each download containing the start address of data plus the length in bytes.  That makes it relatively simple to get the data back when using it in a project.

Here is the directory structue.  Values in [] are 16 bit low byte first (little-endian)

0x10 - first meu entry

[address][length][spare][spare] (1-7 ascii chars),0

0x20 - second menu entry

(etc)
  
last entry is marked with length=0.
  
The default data storage address is 0x200 unless you decide to change that to enlarge the space for more menu entries.

I like using the Nano on a protoboard for easy hookup.  A 6 volt 150ma lamp is included in the power feed from the Nano to protect against accidental shorts.  Incandescent lamps have a 10 to 1 increase in resistance when 'hot'.  This part is not required to use this setup.
  
  
