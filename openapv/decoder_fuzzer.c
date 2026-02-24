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
