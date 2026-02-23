```
❯ ./vpx_codec_control_fuzzer_vp9
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 3776836915
INFO: Loaded 1 modules   (105 inline 8-bit counters): 105 [0x59f52e3e9588, 0x59f52e3e95f1),
INFO: Loaded 1 PC tables (105 PCs): 105 [0x59f52e3e95f8,0x59f52e3e9c88),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2      INITED cov: 2 ft: 2 corp: 1/1b exec/s: 0 rss: 31Mb
#7732   NEW    cov: 12 ft: 12 corp: 2/79b lim: 80 exec/s: 0 rss: 47Mb L: 78/78 MS: 5 ChangeBit-CMP-InsertRepeatedBytes-EraseBytes-InsertRepeatedBytes- DE: "\001\000\000\000\000\000\000L"-
#7733   NEW    cov: 17 ft: 17 corp: 3/159b lim: 80 exec/s: 0 rss: 60Mb L: 80/80 MS: 1 CrossOver-
#7751   NEW    cov: 19 ft: 19 corp: 4/239b lim: 80 exec/s: 0 rss: 187Mb L: 80/80 MS: 3 ChangeBinInt-CMP-ChangeBinInt- DE: "\377\377\377\377"-
#7758   REDUCE cov: 19 ft: 19 corp: 4/235b lim: 80 exec/s: 0 rss: 200Mb L: 76/80 MS: 2 EraseBytes-CopyPart-
#7785   NEW    cov: 20 ft: 20 corp: 5/315b lim: 80 exec/s: 0 rss: 365Mb L: 80/80 MS: 2 ChangeByte-CopyPart-
#7953   REDUCE cov: 20 ft: 20 corp: 5/313b lim: 80 exec/s: 0 rss: 367Mb L: 76/80 MS: 3 EraseBytes-InsertRepeatedBytes-InsertByte-
#7987   REDUCE cov: 29 ft: 29 corp: 6/391b lim: 80 exec/s: 7987 rss: 367Mb L: 78/80 MS: 4 CopyPart-ChangeBit-InsertByte-ShuffleBytes-
#8208   REDUCE cov: 29 ft: 29 corp: 6/390b lim: 80 exec/s: 8208 rss: 367Mb L: 77/80 MS: 1 EraseBytes-
#8224   NEW    cov: 31 ft: 31 corp: 7/470b lim: 80 exec/s: 8224 rss: 367Mb L: 80/80 MS: 1 CrossOver-
#8303   REDUCE cov: 33 ft: 33 corp: 8/549b lim: 80 exec/s: 4151 rss: 367Mb L: 79/80 MS: 4 ShuffleBytes-CMP-ChangeByte-ShuffleBytes- DE: "\001\000"-
#8446   REDUCE cov: 33 ft: 33 corp: 8/546b lim: 80 exec/s: 4223 rss: 367Mb L: 77/80 MS: 3 ShuffleBytes-ChangeByte-EraseBytes-
#8493   NEW    cov: 40 ft: 40 corp: 9/626b lim: 80 exec/s: 4246 rss: 367Mb L: 80/80 MS: 2 CopyPart-CMP- DE: "\011\000\000\000\000\000\000\000"-
#8502   REDUCE cov: 44 ft: 44 corp: 10/706b lim: 80 exec/s: 4251 rss: 367Mb L: 80/80 MS: 4 CopyPart-ChangeByte-ShuffleBytes-CrossOver-
#8527   REDUCE cov: 44 ft: 44 corp: 10/704b lim: 80 exec/s: 4263 rss: 367Mb L: 78/80 MS: 5 ChangeBinInt-CopyPart-ChangeByte-EraseBytes-InsertRepeatedBytes-
#8564   REDUCE cov: 45 ft: 45 corp: 11/781b lim: 80 exec/s: 4282 rss: 367Mb L: 77/80 MS: 2 CopyPart-ChangeBinInt-
#8577   REDUCE cov: 46 ft: 46 corp: 12/858b lim: 80 exec/s: 4288 rss: 367Mb L: 77/80 MS: 3 ChangeByte-CrossOver-PersAutoDict- DE: "\011\000\000\000\000\000\000\000"-
#8709   REDUCE cov: 46 ft: 46 corp: 12/857b lim: 80 exec/s: 4354 rss: 367Mb L: 79/80 MS: 2 EraseBytes-InsertRepeatedBytes-
#8744   NEW    cov: 49 ft: 49 corp: 13/937b lim: 80 exec/s: 2914 rss: 367Mb L: 80/80 MS: 5 ShuffleBytes-PersAutoDict-CrossOver-ChangeByte-ChangeBit- DE: "\001\000"-
#8831   REDUCE cov: 51 ft: 51 corp: 14/1014b lim: 80 exec/s: 2943 rss: 367Mb L: 77/80 MS: 2 ChangeBit-ChangeBinInt-
#8871   REDUCE cov: 51 ft: 51 corp: 14/1011b lim: 80 exec/s: 2957 rss: 367Mb L: 77/80 MS: 5 CrossOver-InsertByte-InsertRepeatedBytes-ShuffleBytes-CrossOver-
#8969   REDUCE cov: 51 ft: 51 corp: 14/1010b lim: 80 exec/s: 2989 rss: 367Mb L: 77/80 MS: 3 EraseBytes-CrossOver-CopyPart-
#9295   NEW    cov: 53 ft: 53 corp: 15/1087b lim: 80 exec/s: 2323 rss: 367Mb L: 77/80 MS: 1 ChangeBinInt-
#9424   REDUCE cov: 55 ft: 55 corp: 16/1167b lim: 80 exec/s: 2356 rss: 367Mb L: 80/80 MS: 4 InsertByte-EraseBytes-InsertRepeatedBytes-InsertRepeatedBytes-
#9483   REDUCE cov: 55 ft: 55 corp: 16/1165b lim: 80 exec/s: 2370 rss: 367Mb L: 77/80 MS: 4 InsertByte-CopyPart-CopyPart-CrossOver-
#9525   REDUCE cov: 55 ft: 55 corp: 16/1164b lim: 80 exec/s: 1905 rss: 367Mb L: 76/80 MS: 2 CrossOver-EraseBytes-
#9873   REDUCE cov: 55 ft: 55 corp: 16/1163b lim: 80 exec/s: 1974 rss: 367Mb L: 76/80 MS: 3 EraseBytes-PersAutoDict-InsertByte- DE: "\377\377\377\377"-
#9878   REDUCE cov: 55 ft: 55 corp: 16/1159b lim: 80 exec/s: 1975 rss: 367Mb L: 76/80 MS: 5 ChangeBit-ChangeBinInt-PersAutoDict-CopyPart-EraseBytes- DE: "\011\000\000\000\000\000\000\000"-
#10102  REDUCE cov: 55 ft: 55 corp: 16/1158b lim: 80 exec/s: 1683 rss: 367Mb L: 76/80 MS: 4 PersAutoDict-EraseBytes-CrossOver-InsertByte- DE: "\001\000"-
#10109  REDUCE cov: 56 ft: 56 corp: 17/1235b lim: 80 exec/s: 1684 rss: 367Mb L: 77/80 MS: 2 ChangeByte-InsertByte-
#10393  REDUCE cov: 56 ft: 56 corp: 17/1234b lim: 80 exec/s: 1484 rss: 367Mb L: 76/80 MS: 4 EraseBytes-ShuffleBytes-CrossOver-InsertByte-
#10499  REDUCE cov: 56 ft: 56 corp: 17/1233b lim: 80 exec/s: 1499 rss: 367Mb L: 79/80 MS: 1 EraseBytes-
#10704  REDUCE cov: 56 ft: 56 corp: 17/1230b lim: 80 exec/s: 1529 rss: 367Mb L: 76/80 MS: 5 EraseBytes-CMP-CrossOver-ChangeBit-InsertByte- DE: "\377\377X\365.8\2408"-
#11003  REDUCE cov: 56 ft: 56 corp: 17/1229b lim: 80 exec/s: 1375 rss: 367Mb L: 76/80 MS: 4 EraseBytes-ChangeByte-ChangeBit-CrossOver-
#11613  REDUCE cov: 56 ft: 59 corp: 18/1313b lim: 86 exec/s: 1290 rss: 367Mb L: 84/84 MS: 5 CopyPart-InsertRepeatedBytes-ChangeBinInt-PersAutoDict-InsertByte- DE: "\377\377X\365.8\2408"-
#11683  REDUCE cov: 56 ft: 59 corp: 18/1312b lim: 86 exec/s: 1168 rss: 367Mb L: 76/84 MS: 5 CopyPart-EraseBytes-CMP-CrossOver-InsertRepeatedBytes- DE: "\377\377"-
#11847  REDUCE cov: 57 ft: 60 corp: 19/1395b lim: 86 exec/s: 1184 rss: 367Mb L: 83/84 MS: 4 CopyPart-ChangeBit-CrossOver-ChangeBit-
#13253  REDUCE cov: 57 ft: 60 corp: 19/1392b lim: 98 exec/s: 946 rss: 367Mb L: 80/84 MS: 1 EraseBytes-
#13655  REDUCE cov: 57 ft: 60 corp: 19/1391b lim: 98 exec/s: 910 rss: 367Mb L: 78/84 MS: 2 ChangeBinInt-EraseBytes-
#13772  REDUCE cov: 57 ft: 60 corp: 19/1389b lim: 98 exec/s: 918 rss: 367Mb L: 78/84 MS: 2 ShuffleBytes-EraseBytes-
#13871  REDUCE cov: 57 ft: 60 corp: 19/1387b lim: 98 exec/s: 924 rss: 367Mb L: 76/84 MS: 4 CopyPart-ChangeBit-CMP-EraseBytes- DE: "\377\377"-
#16384  pulse  cov: 57 ft: 60 corp: 19/1387b lim: 122 exec/s: 712 rss: 367Mb
#18334  REDUCE cov: 57 ft: 60 corp: 19/1385b lim: 142 exec/s: 654 rss: 367Mb L: 76/84 MS: 3 EraseBytes-CopyPart-CrossOver-
#18710  REDUCE cov: 57 ft: 60 corp: 19/1384b lim: 142 exec/s: 645 rss: 367Mb L: 76/84 MS: 1 EraseBytes-
#18716  REDUCE cov: 57 ft: 60 corp: 19/1383b lim: 142 exec/s: 645 rss: 367Mb L: 76/84 MS: 1 EraseBytes-
#20245  REDUCE cov: 57 ft: 60 corp: 19/1382b lim: 156 exec/s: 595 rss: 367Mb L: 76/84 MS: 4 CrossOver-EraseBytes-EraseBytes-InsertRepeatedBytes-
#20367  REDUCE cov: 57 ft: 60 corp: 19/1381b lim: 156 exec/s: 599 rss: 367Mb L: 76/84 MS: 2 CopyPart-EraseBytes-
AddressSanitizer:DEADLYSIGNAL
=================================================================
==16824==ERROR: AddressSanitizer: SEGV on unknown address 0x00004c000038 (pc 0x59f52e2b5530 bp 0x00004c000000 sp 0x7ffec7062b38 T0)
==16824==The signal is caused by a READ memory access.
    #0 0x59f52e2b5530 in image2yuvconfig /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp9/vp9_iface_common.c:80:18
    #1 0x59f52e2653c6 in ctrl_set_reference /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp9/vp9_dx_iface.c:457:5
    #2 0x59f52e202caf in vpx_codec_control_ /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vpx/src/vpx_codec.c:106:15
    #3 0x59f52e2010e9 in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_codec_control_fuzzer.c:237:13
    #4 0x59f52e104eba in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x55eba) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #5 0x59f52e1044c9 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x554c9) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #6 0x59f52e105f95 in fuzzer::Fuzzer::MutateAndTestOne() (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x56f95) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #7 0x59f52e106a85 in fuzzer::Fuzzer::Loop(std::vector<fuzzer::SizedFile, std::allocator<fuzzer::SizedFile>>&) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x57a85) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #8 0x59f52e0f3045 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x44045) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #9 0x59f52e11f3a6 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x703a6) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #10 0x7316a6806ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #11 0x7316a6806d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #12 0x59f52e0e73a0 in _start (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x383a0) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)

==16824==Register values:
rax = 0x0000000000000018  rbx = 0x00005160005e2690  rcx = 0x0000000000000000  rdx = 0x00007ffec7062c30
rdi = 0x000000004c000008  rsi = 0x00007ffec7062b40  rbp = 0x000000004c000000  rsp = 0x00007ffec7062b38
 r8 = 0x000059f52e3ea200   r9 = 0x0000000000000000  r10 = 0x0000000000000004  r11 = 0x0000000000000000
r12 = 0x00007ffec7062b40  r13 = 0x00007316a4c35c00  r14 = 0x0000000000000001  r15 = 0x00000e635497eb80
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp9/vp9_iface_common.c:80:18 in image2yuvconfig
==16824==ABORTING
MS: 3 CopyPart-CrossOver-PersAutoDict- DE: "\001\000\000\000\000\000\000L"-; base unit: 05fa2e75d1612c5270d65fa7ee3c2e92599ab181
0x0,0x0,0xff,0xff,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x4c,0xff,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3a,0xff,0x8,0x48,0x8,0x8,0x8,0x8,0x8,0x8,0xff,0xff,0x58,0xf5,0x2e,0x38,0xa0,0x38,0x8,0x2e,0x38,0xa0,0x38,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0xff,0xff,0xff,0xff,0xff,0x8a,
\000\000\377\377\001\000\000\000\000\000\000L\377\377\000\377\377\377\377\377\377\377:\377\010H\010\010\010\010\010\010\377\377X\365.8\2408\010.8\2408\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\000\000\000\000\000\000\000\000\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\377\377\377\377\377\212
artifact_prefix='./'; Test unit written to ./crash-33724cd6b286c15eb766589eeaaea79e90ff0691
Base64: AAD//wEAAAAAAABM//8A/////////zr/CEgICAgICAj//1j1LjigOAguOKA4CAgICAgICAgICAgICAgICAgICAgAAAAAAAAAAAgICAgICAgICAgICAgICP//////ig==
```

