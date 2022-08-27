# LH Compressor
A command line tool to compress files to the LH (LZ + Huffman) format found in several Wii Games, written in C.

## Usage

```
  LH_C.exe <filename>
```

The above command compresses the given file to the LH format. The file is saved 
in the same directory as the uncompressed file, but with the file 
extension ".LH". For exmaple "TEST.bin" is compressed to "TEST.bin.LH".

All tested files were successfully decompressed with from **ntcompress.exe**.
So all files (should) be decompressable with **ntcompress.exe**.
