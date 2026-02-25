## project address

project：https://github.com/AcademySoftwareFoundation/openapv

version：v0.2.1.1

## info

OS：Ubuntu22.04 TLS

## fuzzer

This directory provides a `libFuzzer` harness for the decoder path:

- `oapvd_info()`
- `oapvd_decode()`

The harness accepts both:

- Raw access-unit bytes
- RBAU stream format (`[4-byte be size][au payload]...`)

```c++
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "oapv.h"

#define ALIGN_VAL(val, align) ((((val) + (align) - 1) / (align)) * (align))

enum {
    MAX_AU_BYTES = 4 * 1024 * 1024,
    MAX_DIMENSION = 4096,
    MAX_PIXELS = 4096 * 4096,
    MAX_PLANE_BYTES = 128 * 1024 * 1024,
    MAX_RBAU_AUS_PER_INPUT = 4
};

static int atomic_inc(volatile int *pcnt)
{
    int ret;

    ret = *pcnt;
    ret++;
    *pcnt = ret;
    return ret;
}

static int atomic_dec(volatile int *pcnt)
{
    int ret;

    ret = *pcnt;
    ret--;
    *pcnt = ret;
    return ret;
}

static int imgb_addref(oapv_imgb_t *imgb)
{
    if(imgb == NULL) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }
    return atomic_inc(&imgb->refcnt);
}

static int imgb_getref(oapv_imgb_t *imgb)
{
    if(imgb == NULL) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }
    return imgb->refcnt;
}

static int imgb_release(oapv_imgb_t *imgb)
{
    int refcnt;
    int i;

    if(imgb == NULL) {
        return OAPV_ERR_INVALID_ARGUMENT;
    }

    refcnt = atomic_dec(&imgb->refcnt);
    if(refcnt == 0) {
        for(i = 0; i < OAPV_MAX_CC; i++) {
            free(imgb->baddr[i]);
        }
        free(imgb);
    }
    return refcnt;
}

static int is_supported_color_format(int cs)
{
    int format;

    format = OAPV_CS_GET_FORMAT(cs);
    return (format == OAPV_CF_YCBCR400 ||
            format == OAPV_CF_YCBCR420 ||
            format == OAPV_CF_YCBCR422 ||
            format == OAPV_CF_YCBCR444 ||
            format == OAPV_CF_YCBCR4444 ||
            format == OAPV_CF_PLANAR2);
}

static int align_up_checked(int value, int align)
{
    if(value <= 0 || align <= 0) {
        return -1;
    }
    if(value > INT_MAX - align + 1) {
        return -1;
    }
    return ALIGN_VAL(value, align);
}

static int frame_info_is_sane(const oapv_frm_info_t *finfo)
{
    size_t pixels;
    int bit_depth;

    if(finfo->w <= 0 || finfo->h <= 0) {
        return 0;
    }
    if(finfo->w > MAX_DIMENSION || finfo->h > MAX_DIMENSION) {
        return 0;
    }
    pixels = (size_t)finfo->w * (size_t)finfo->h;
    if(pixels > MAX_PIXELS) {
        return 0;
    }
    if(!is_supported_color_format(finfo->cs)) {
        return 0;
    }

    bit_depth = OAPV_CS_GET_BIT_DEPTH(finfo->cs);
    if(bit_depth < 8 || bit_depth > 16) {
        return 0;
    }
    return 1;
}

static oapv_imgb_t *fuzz_imgb_create(int w, int h, int cs)
{
    oapv_imgb_t *imgb;
    int i;
    int bd;

    imgb = (oapv_imgb_t *)calloc(1, sizeof(oapv_imgb_t));
    if(imgb == NULL) {
        return NULL;
    }

    bd = OAPV_CS_GET_BYTE_DEPTH(cs);
    if(bd <= 0 || bd > 2) {
        goto ERR;
    }

    imgb->w[0] = w;
    imgb->h[0] = h;
    switch(OAPV_CS_GET_FORMAT(cs)) {
    case OAPV_CF_YCBCR400:
        imgb->w[1] = imgb->w[2] = w;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np = 1;
        break;
    case OAPV_CF_YCBCR420:
        imgb->w[1] = imgb->w[2] = (w + 1) >> 1;
        imgb->h[1] = imgb->h[2] = (h + 1) >> 1;
        imgb->np = 3;
        break;
    case OAPV_CF_YCBCR422:
        imgb->w[1] = imgb->w[2] = (w + 1) >> 1;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np = 3;
        break;
    case OAPV_CF_YCBCR444:
        imgb->w[1] = imgb->w[2] = w;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np = 3;
        break;
    case OAPV_CF_YCBCR4444:
        imgb->w[1] = imgb->w[2] = imgb->w[3] = w;
        imgb->h[1] = imgb->h[2] = imgb->h[3] = h;
        imgb->np = 4;
        break;
    case OAPV_CF_PLANAR2:
        imgb->w[1] = w;
        imgb->h[1] = h;
        imgb->np = 2;
        break;
    default:
        goto ERR;
    }

    for(i = 0; i < imgb->np; i++) {
        size_t stride;
        size_t plane_size;
        int aw;
        int ah;

        aw = align_up_checked(imgb->w[i], OAPV_MB_W);
        ah = align_up_checked(imgb->h[i], OAPV_MB_H);
        if(aw < 0 || ah < 0) {
            goto ERR;
        }

        stride = (size_t)aw * (size_t)bd;
        plane_size = stride * (size_t)ah;
        if(stride > INT_MAX || plane_size > INT_MAX) {
            goto ERR;
        }
        if(plane_size == 0 || plane_size > MAX_PLANE_BYTES) {
            goto ERR;
        }

        imgb->aw[i] = aw;
        imgb->ah[i] = ah;
        imgb->s[i] = (int)stride;
        imgb->e[i] = ah;
        imgb->bsize[i] = (int)plane_size;
        imgb->a[i] = imgb->baddr[i] = calloc(1, plane_size);
        if(imgb->a[i] == NULL) {
            goto ERR;
        }
    }

    imgb->cs = cs;
    imgb->addref = imgb_addref;
    imgb->getref = imgb_getref;
    imgb->release = imgb_release;
    imgb->addref(imgb);
    return imgb;

ERR:
    if(imgb != NULL) {
        for(i = 0; i < OAPV_MAX_CC; i++) {
            free(imgb->baddr[i]);
        }
    }
    free(imgb);
    return NULL;
}

static void release_decoded_frames(oapv_frms_t *ofrms)
{
    int i;

    for(i = 0; i < OAPV_MAX_NUM_FRAMES; i++) {
        if(ofrms->frm[i].imgb != NULL) {
            ofrms->frm[i].imgb->release(ofrms->frm[i].imgb);
            ofrms->frm[i].imgb = NULL;
        }
    }
}

static int fuzz_one_au(const uint8_t *data, size_t size)
{
    oapv_au_info_t aui;
    oapvd_cdesc_t cdesc;
    oapv_frms_t ofrms;
    oapv_bitb_t bitb;
    oapvd_stat_t stat;
    oapvd_t did;
    oapvm_t mid;
    int i;
    int err;

    if(data == NULL || size < 8 || size > MAX_AU_BYTES) {
        return 0;
    }

    memset(&aui, 0, sizeof(aui));
    if(OAPV_FAILED(oapvd_info((void *)data, (int)size, &aui))) {
        return 0;
    }
    if(aui.num_frms <= 0 || aui.num_frms > OAPV_MAX_NUM_FRAMES) {
        return 0;
    }
    for(i = 0; i < aui.num_frms; i++) {
        if(!frame_info_is_sane(&aui.frm_info[i])) {
            return 0;
        }
    }

    memset(&ofrms, 0, sizeof(ofrms));
    memset(&bitb, 0, sizeof(bitb));
    memset(&stat, 0, sizeof(stat));

    did = NULL;
    mid = NULL;

    memset(&cdesc, 0, sizeof(cdesc));
    cdesc.threads = 1;
    err = OAPV_OK;
    did = oapvd_create(&cdesc, &err);
    if(did == NULL || OAPV_FAILED(err)) {
        goto CLEANUP;
    }

    err = OAPV_OK;
    mid = oapvm_create(&err);
    if(mid == NULL || OAPV_FAILED(err)) {
        goto CLEANUP;
    }

    ofrms.num_frms = aui.num_frms;
    for(i = 0; i < ofrms.num_frms; i++) {
        const oapv_frm_info_t *finfo;

        finfo = &aui.frm_info[i];
        ofrms.frm[i].imgb = fuzz_imgb_create(finfo->w, finfo->h, finfo->cs);
        if(ofrms.frm[i].imgb == NULL) {
            goto CLEANUP;
        }
    }

    bitb.addr = (void *)data;
    bitb.bsize = (int)size;
    bitb.ssize = (int)size;
    (void)oapvd_decode(did, &bitb, &ofrms, mid, &stat);

CLEANUP:
    release_decoded_frames(&ofrms);
    if(mid != NULL) {
        oapvm_delete(mid);
    }
    if(did != NULL) {
        oapvd_delete(did);
    }
    return 0;
}

static uint32_t read_be32(const uint8_t *ptr)
{
    return ((uint32_t)ptr[0] << 24) |
           ((uint32_t)ptr[1] << 16) |
           ((uint32_t)ptr[2] << 8) |
           ((uint32_t)ptr[3]);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t offset;
    int num_aus;

    if(data == NULL || size == 0) {
        return 0;
    }

    (void)fuzz_one_au(data, size);

    offset = 0;
    num_aus = 0;
    while(size - offset >= 4 && num_aus < MAX_RBAU_AUS_PER_INPUT) {
        uint32_t au_size;

        au_size = read_be32(data + offset);
        offset += 4;
        if(au_size == 0 || au_size > size - offset || au_size > MAX_AU_BYTES) {
            break;
        }

        (void)fuzz_one_au(data + offset, (size_t)au_size);
        offset += (size_t)au_size;
        num_aus++;
    }
    return 0;
}
```

## Poc

https://github.com/nanhang-950/fuzz_vuln/blob/main/openapv/crash-86ab9b4c09bb4518c1728e0e721af889b7e2082d

## ASAN Info

```shell
❯ ./oapv_decoder_fuzzer ./crash-86ab9b4c09bb4518c1728e0e721af889b7e2082d
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 581490855
INFO: Loaded 1 modules   (647 inline 8-bit counters): 647 [0x560f93472210, 0x560f93472497),
INFO: Loaded 1 PC tables (647 PCs): 647 [0x560f93472498,0x560f93474d08),
./oapv_decoder_fuzzer: Running 1 inputs 1 time(s) each.
Running: ./crash-86ab9b4c09bb4518c1728e0e721af889b7e2082d
AddressSanitizer:DEADLYSIGNAL
=================================================================
==6536==ERROR: AddressSanitizer: SEGV on unknown address 0x560f22ee041a (pc 0x560f9340ed1c bp 0x7ffc51f07d50 sp 0x7ffc51f076b0 T0)
==6536==The signal is caused by a READ memory access.
    #0 0x560f9340ed1c in oapvd_vlc_ac_coef /mnt/h/Security/Binary/Project/openapv/src/oapv_vlc.c:890:9
    #1 0x560f93401d77 in dec_tile_comp /mnt/h/Security/Binary/Project/openapv/src/oapv.c:1640:27
    #2 0x560f93401b04 in dec_tile /mnt/h/Security/Binary/Project/openapv/src/oapv.c:1709:15
    #3 0x560f933fd485 in dec_thread_tile /mnt/h/Security/Binary/Project/openapv/src/oapv.c:1770:15
    #4 0x560f933fcb9b in oapvd_decode /mnt/h/Security/Binary/Project/openapv/src/oapv.c:2010:19
    #5 0x560f933ef830 in fuzz_one_au /mnt/h/Security/Binary/Project/openapv/fuzz/decoder_fuzzer.c:302:11
    #6 0x560f933ee262 in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Project/openapv/fuzz/decoder_fuzzer.c:345:15
    #7 0x560f932f256a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Project/openapv/build-fuzz/bin/oapv_decoder_fuzzer+0x5656a) (BuildId: b257232a7c9ee769ad545f92c7f3f85a68889c5f)
    #8 0x560f932da463 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/Binary/Project/openapv/build-fuzz/bin/oapv_decoder_fuzzer+0x3e463) (BuildId: b257232a7c9ee769ad545f92c7f3f85a68889c5f)
    #9 0x560f932e05a1 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Project/openapv/build-fuzz/bin/oapv_decoder_fuzzer+0x445a1) (BuildId: b257232a7c9ee769ad545f92c7f3f85a68889c5f)
    #10 0x560f9330ca56 in main (/mnt/h/Security/Binary/Project/openapv/build-fuzz/bin/oapv_decoder_fuzzer+0x70a56) (BuildId: b257232a7c9ee769ad545f92c7f3f85a68889c5f)
    #11 0x78f83ca33ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #12 0x78f83ca33d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #13 0x560f932d4a50 in _start (/mnt/h/Security/Binary/Project/openapv/build-fuzz/bin/oapv_decoder_fuzzer+0x38a50) (BuildId: b257232a7c9ee769ad545f92c7f3f85a68889c5f)

==6536==Register values:
rax = 0x0000519000000580  rbx = 0x00007ffc51f07a20  rcx = 0x0000560f9343c5f0  rdx = 0x000078f83c0dfffe
rdi = 0x000000008faa3e2b  rsi = 0xffffffff8faa3e2a  rbp = 0x00007ffc51f07d50  rsp = 0x00007ffc51f076b0
 r8 = 0x00000a327fff80f0   r9 = 0x0000000000000008  r10 = 0x0000000000000008  r11 = 0x0000000000000000
r12 = 0x000078f83c257800  r13 = 0x0000560f93475200  r14 = 0x000078f83c057800  r15 = 0x00000000000a725c
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /mnt/h/Security/Binary/Project/openapv/src/oapv_vlc.c:890:9 in oapvd_vlc_ac_coef
==6536==ABORTING
```

