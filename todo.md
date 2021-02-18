## bugs
* HttpReader fails with no message when diff is too big, e.g. kysync.exe -command sync -data-uri="http://mirror.math.princeton.edu/pub/ubuntu-iso/20.04/ubuntu-20.04.2.0-desktop-amd64.iso" -metadata-uri="file://C:\Users\Kamen Yotov\Downloads\ubuntu-20.04.2.0-desktop-amd64.iso.ksync" -input-filename="C:\Users\Kamen Yotov\Downloads\ubuntu-20.04.1-desktop-amd64.iso" -output-filename=x.iso -threads=128
  * this is due to F0218 00:41:16.029953 22644 HttpReader.cpp:39] Check failed: res.error() == httplib::Success (4 vs. 0)
  * 4 means "Read" error... possibly I am storming the server too much and they slap me. 

## hygiene
* document all api
* rename warmup to "skip" or something of that sort
* make parallelize a member function
* make sure the final stats of each phase are shown?
* make StrongChecksumBuilder a real streambuf
* figure out if we can have in-memory tests without files
* create a wcs collision test
* make the install directory of each project different
* write benchmarks (e.g. for the running window checksum)

## improvements
* improve algorithm to use http reader with bigger blocks or more blocks...
* make the code build on mac / linux (i have only done windows so far)
* write while analyzing...

## new features
* add compression (currently only does uncompressed files)

## old (and done)
- ~~stable monitor row (no shortening)~~
- ~~show total time and phase time~~
- ~~fails... kysync.exe -command sync -data-uri="file://c:\Users\Kamen Yotov\Downloads\ubuntu-20.04.1-desktop-amd64.iso" -input-filename="C:\Users\Kamen Yotov\Downloads\ubuntu-20.10-desktop-amd64.iso" -output-filename=x.iso -threads=24~~
* ~~implement http 1.1 reader~~

