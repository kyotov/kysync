- write a benchmark for the running window checksum
- document readers api
- make the install directory of each project different
- make a wcs collision test
- figure out if we can have in-memory tests without files
- make StrongChecksumBuilder a real streambuf
- stable monitor row (no shortening)
- show total time and phase time
- write while analyzing...
- make parallelize a member function
- make sure the final stats of each phase are shown?
- rename warmup to "skip" or something of that sort
- ~~fails... kysync.exe -command sync -data-uri="file://c:\Users\Kamen Yotov\Downloads\ubuntu-20.04.1-desktop-amd64.iso" -input-filename="C:\Users\Kamen Yotov\Downloads\ubuntu-20.10-desktop-amd64.iso" -output-filename=x.iso -threads=24~~
* make the code build on mac / linux (i have only done windows so far)
* add compression (currently only does uncompressed files)
* ~~implement http 1.1 reader~~
* improve algorithm to use http reader with bigger blocks or more blocks... 
* HttpReader fails with no message when diff is too big, e.g. kysync.exe -command sync -data-uri="http://mirror.math.princeton.edu/pub/ubuntu-iso/20.04/ubuntu-20.04.2.0-desktop-amd64.iso" -metadata-uri="file://C:\Users\Kamen Yotov\Downloads\ubuntu-20.04.2.0-desktop-amd64.iso.ksync" -input-filename="C:\Users\Kamen Yotov\Downloads\ubuntu-20.04.1-desktop-amd64.iso" -output-filename=x.iso -threads=128