# ksync

NOTE: this is a rewrite of http://zsync.moria.org.uk/ or at least my understanding of it.

there is a rough list of remaining items to do [here](todo.md)

# pitch

This is the Ubuntu 20.04.01 DVD reconstructing itself over an identical copy with 16kb chunks:

```
O0217 20:30:54.898490 16168 main.cpp:29] ksync v0.1                                                                     
O0217 20:30:56.792490 16168 Monitor.cpp:71] phase 2 |  2533 MB |   1.4s |  1866.8 MB/s |  95% |   1.5s total            
O0217 20:30:57.993489 16168 Monitor.cpp:71] phase 3 |  2458 MB |   1.1s |  2249.5 MB/s |  92% |   2.7s total            
O0217 20:31:01.765489 16168 Monitor.cpp:88] phase 4 |  2656 MB |   3.8s |   704.1 MB/s | 100% |   6.5s total            
O0217 20:31:01.765489 16168 Monitor.cpp:49] //progressPhase=4                                                           
O0217 20:31:01.765489 16168 Monitor.cpp:49] //progressTotalBytes=2785017856                                             
O0217 20:31:01.765489 16168 Monitor.cpp:49] //progressCurrentBytes=2785017856                                           
O0217 20:31:01.765489 16168 Monitor.cpp:49] //weakChecksumMatches=169864                                                
O0217 20:31:01.765489 16168 Monitor.cpp:49] //weakChecksumFalsePositive=7                                               
O0217 20:31:01.765489 16168 Monitor.cpp:49] //strongChecksumMatches=169857                                              
O0217 20:31:01.765489 16168 Monitor.cpp:49] //reusedBytes=2784935936                                                    
O0217 20:31:01.766489 16168 Monitor.cpp:49] //downloadedBytes=81920                                                     
```

This is the Ubuntu 20.04.01 DVD reconstructing itself over 20.10 DVD (6 months apart) with 16kb chunks:
```
O0217 20:30:18.109661  3920 main.cpp:29] ksync v0.1                                                                     
O0217 20:30:25.272665  3920 Monitor.cpp:71] phase 2 |  2806 MB |   6.6s |   424.9 MB/s | 100% |   6.7s total            
O0217 20:30:26.147660  3920 Monitor.cpp:71] phase 3 |  2408 MB |   0.8s |  3144.1 MB/s |  90% |   7.6s total            
O0217 20:30:29.907661  3920 Monitor.cpp:88] phase 4 |  2656 MB |   3.8s |   706.4 MB/s | 100% |  11.5s total            
O0217 20:30:29.907661  3920 Monitor.cpp:49] //progressPhase=4                                                           
O0217 20:30:29.907661  3920 Monitor.cpp:49] //progressTotalBytes=2785017856                                             
O0217 20:30:29.907661  3920 Monitor.cpp:49] //progressCurrentBytes=2785017856                                           
O0217 20:30:29.907661  3920 Monitor.cpp:49] //weakChecksumMatches=128225                                                
O0217 20:30:29.908663  3920 Monitor.cpp:49] //weakChecksumFalsePositive=66015                                           
O0217 20:30:29.908663  3920 Monitor.cpp:49] //strongChecksumMatches=62210                                               
O0217 20:30:29.908663  3920 Monitor.cpp:49] //reusedBytes=1021247488                                                    
O0217 20:30:29.908663  3920 Monitor.cpp:49] //downloadedBytes=1763770368                                                
```
Note that about 1GB got reused and about 1.7GB got downloaded.
For this experiment the download speed is not factored in... just the computational overhead.

Both of the above experiments use 128 threads on:
* Processor	Intel(R) Core(TM) i9-7940X CPU @ 3.10GHz, 3096 Mhz, 14 Core(s), 28 Logical Processor(s)
* BaseBoard Product	TUF X299 MARK 2
* Samsung SSD 970 EVO 2TB

# reference

detailed info at http://zsync.moria.org.uk/papers

more links:

[Rsync1998] The rsync algorithm. 1998-11-09. Andrew Tridgell and Paul Mackerras. http://rsync.samba.org/tech_report/

[CVSup1999] How Does CVSup Go So Fast?. 1999-02-10. John D. Polstra. http://www.cvsup.org/howsofast.html

[Pipe1999] Network Performance Effects of HTTP/1.1, CSS1, and PNG. 1999/10/18. The World Wide Web Consortium. Henrik Frystyk Nielsen, Jim Gettys, Anselm Baird-Smith, Eric Prud'hommeaux, Håkon Wium Lie, and Chris Lilley. http://www.w3.org/TR/NOTE-pipelining-970624

[BitT2003] Incentives Build Robustness in BitTorrent. 2003-05-22. Bram Cohen. http://bittorrent.com/bittorrentecon.pdf

[RProxy] rproxy. 2002/10/03. Martin Pool. http://rproxy.samba.org/

[GzipRsync] (gzip --rsync patch). http://ozlabs.org/~rusty/gzip.rsync.patch2

[CIS2004] Improved Single-Round Protocols for Remote File Synchronization. 2004-09. Utku Irmak, Svilen Mihaylov, and Torsten Suel. http://cis.poly.edu/suel/papers/erasure.pdf

[ModGzip] mod_gzip - serving compressed content by the Apache webserver. Michael Schröpl. http://www.schroepl.net/projekte/mod_gzip/

[ModDeflate] mod_deflate. The Apache Software Foundation. http://httpd.apache.org/docs-2.0/mod/mod_deflate.html
