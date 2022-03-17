# EEProgrammer
Load any type of data into 24LC512 64k EEProm IC.

These devices are cheap and only need 2 lines to access (I2C). They require a bit of special handling when writing data.  It must be in 128 byte blocks, and takes about 5 milliseconds to do each block.

This program takes care of all of that while receiving 57.6k data with no handshake protocol.

A simple directory entry is created for each download containing the start address of data plus the length in bytes.  That makes it relatively simple to get the data back when using it in a project.

Here is the directory structue.  Values in <> are 16 bit low byte first (little-endian)

0x10 - first meu entry
<address><length><spare><spare> [1-7 ascii chars],0
0x20 - second menu entry
(etc)
  
last entry is marked with length=0.

  
  
