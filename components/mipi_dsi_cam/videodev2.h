/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 *  Video for Linux Two header file - ESP32-P4 version
 *  Simplifié et autonome pour ESPHome - Tout en un seul fichier
 */
#ifndef __LINUX_VIDEODEV2_H
#define __LINUX_VIDEODEV2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <stdint.h>

/* ========== DÉFINITIONS DE TYPES ========== */
#ifndef __u8
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef int64_t __s64;
#endif

/* ========== MACROS IOCTL ========== */
#ifndef _IOC
#define _IOC(dir,type,nr,size) (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#define _IO(type,nr)           _IOC(0,(type),(nr),0)
#define _IOR(type,nr,size)     _IOC(1,(type),(nr),sizeof(size))
#define _IOW(type,nr,size)     _IOC(2,(type),(nr),sizeof(size))
#define _IOWR(type,nr,size)    _IOC(3,(type),(nr),sizeof(size))
#endif

#ifndef __inline__
#define __inline__ inline
#endif

/* ========== CONSTANTES ========== */
#define VIDEO_MAX_FRAME  32
#define VIDEO_MAX_PLANES 8

/* Four-character-code (FOURCC) */
#define v4l2_fourcc(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))
#define v4l2_fourcc_be(a, b, c, d) (v4l2_fourcc(a, b, c, d) | (1U << 31))

/* ========== ENUMERATIONS ========== */

enum v4l2_field {
    V4L2_FIELD_ANY = 0,
    V4L2_FIELD_NONE       = 1,
    V4L2_FIELD_TOP        = 2,
    V4L2_FIELD_BOTTOM     = 3,
    V4L2_FIELD_INTERLACED = 4,
    V4L2_FIELD_SEQ_TB     = 5,
    V4L2_FIELD_SEQ_BT     = 6,
    V4L2_FIELD_ALTERNATE  = 7,
    V4L2_FIELD_INTERLACED_TB = 8,
    V4L2_FIELD_INTERLACED_BT = 9,
};

enum v4l2_buf_type {
    V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,
    V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,
    V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,
    V4L2_BUF_TYPE_VBI_CAPTURE          = 4,
    V4L2_BUF_TYPE_VBI_OUTPUT           = 5,
    V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
    V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
    V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
    V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
    V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
    V4L2_BUF_TYPE_SDR_CAPTURE          = 11,
    V4L2_BUF_TYPE_SDR_OUTPUT           = 12,
    V4L2_BUF_TYPE_META_CAPTURE         = 13,
    V4L2_BUF_TYPE_META_OUTPUT          = 14,
    V4L2_BUF_TYPE_PRIVATE = 0x80,
};

enum v4l2_memory {
    V4L2_MEMORY_MMAP    = 1,
    V4L2_MEMORY_USERPTR = 2,
    V4L2_MEMORY_OVERLAY = 3,
    V4L2_MEMORY_DMABUF  = 4,
};

enum v4l2_colorspace {
    V4L2_COLORSPACE_DEFAULT = 0,
    V4L2_COLORSPACE_SMPTE170M = 1,
    V4L2_COLORSPACE_SMPTE240M = 2,
    V4L2_COLORSPACE_REC709 = 3,
    V4L2_COLORSPACE_BT878 = 4,
    V4L2_COLORSPACE_470_SYSTEM_M = 5,
    V4L2_COLORSPACE_470_SYSTEM_BG = 6,
    V4L2_COLORSPACE_JPEG = 7,
    V4L2_COLORSPACE_SRGB = 8,
    V4L2_COLORSPACE_OPRGB = 9,
    V4L2_COLORSPACE_BT2020 = 10,
    V4L2_COLORSPACE_RAW = 11,
    V4L2_COLORSPACE_DCI_P3 = 12,
};

enum v4l2_priority {
    V4L2_PRIORITY_UNSET       = 0,
    V4L2_PRIORITY_BACKGROUND  = 1,
    V4L2_PRIORITY_INTERACTIVE = 2,
    V4L2_PRIORITY_RECORD      = 3,
    V4L2_PRIORITY_DEFAULT     = V4L2_PRIORITY_INTERACTIVE,
};

enum v4l2_ctrl_type {
    V4L2_CTRL_TYPE_INTEGER      = 1,
    V4L2_CTRL_TYPE_BOOLEAN      = 2,
    V4L2_CTRL_TYPE_MENU         = 3,
    V4L2_CTRL_TYPE_BUTTON       = 4,
    V4L2_CTRL_TYPE_INTEGER64    = 5,
    V4L2_CTRL_TYPE_CTRL_CLASS   = 6,
    V4L2_CTRL_TYPE_STRING       = 7,
    V4L2_CTRL_TYPE_BITMASK      = 8,
    V4L2_CTRL_TYPE_INTEGER_MENU = 9,
};

/* ========== STRUCTURES ========== */

struct v4l2_rect {
    __s32 left;
    __s32 top;
    __u32 width;
    __u32 height;
};

struct v4l2_fract {
    __u32 numerator;
    __u32 denominator;
};

struct v4l2_area {
    __u32 width;
    __u32 height;
};

struct v4l2_capability {
    __u8 driver[16];
    __u8 card[32];
    __u8 bus_info[32];
    __u32 version;
    __u32 capabilities;
    __u32 device_caps;
    __u32 reserved[3];
};

struct v4l2_pix_format {
    __u32 width;
    __u32 height;
    __u32 pixelformat;
    __u32 field;
    __u32 bytesperline;
    __u32 sizeimage;
    __u32 colorspace;
    __u32 priv;
    __u32 flags;
    union {
        __u32 ycbcr_enc;
        __u32 hsv_enc;
    };
    __u32 quantization;
    __u32 xfer_func;
};

struct v4l2_fmtdesc {
    __u32 index;
    __u32 type;
    __u32 flags;
    __u8 description[32];
    __u32 pixelformat;
    __u32 mbus_code;
    __u32 reserved[3];
};

struct v4l2_format {
    __u32 type;
    union {
        struct v4l2_pix_format pix;
        __u8 raw_data[200];
    } fmt;
};

struct v4l2_control {
    __u32 id;
    __s32 value;
};

struct v4l2_ext_control {
    __u32 id;
    __u32 size;
    __u32 reserved2[1];
    union {
        __s32 value;
        __s64 value64;
        char *string;
        __u8 *p_u8;
        __u16 *p_u16;
        __u32 *p_u32;
        void *ptr;
    };
} __attribute__((packed));

struct v4l2_ext_controls {
    union {
        __u32 ctrl_class;
        __u32 which;
    };
    __u32 count;
    __u32 error_idx;
    __s32 request_fd;
    __u32 reserved[1];
    struct v4l2_ext_control *controls;
};

struct v4l2_queryctrl {
    __u32 id;
    __u32 type;
    __u8 name[32];
    __s32 minimum;
    __s32 maximum;
    __s32 step;
    __s32 default_value;
    __u32 flags;
    __u32 reserved[2];
};

#define V4L2_CTRL_MAX_DIMS (4)

struct v4l2_query_ext_ctrl {
    __u32 id;
    __u32 type;
    char name[32];
    __s64 minimum;
    __s64 maximum;
    __u64 step;
    __s64 default_value;
    __u32 flags;
    __u32 elem_size;
    __u32 elems;
    __u32 nr_of_dims;
    __u32 dims[V4L2_CTRL_MAX_DIMS];
    __u32 reserved[32];
};

/* ========== CAPABILITIES ========== */
#define V4L2_CAP_VIDEO_CAPTURE        0x00000001
#define V4L2_CAP_VIDEO_OUTPUT         0x00000002
#define V4L2_CAP_VIDEO_OVERLAY        0x00000004
#define V4L2_CAP_VBI_CAPTURE          0x00000010
#define V4L2_CAP_VBI_OUTPUT           0x00000020
#define V4L2_CAP_SLICED_VBI_CAPTURE   0x00000040
#define V4L2_CAP_SLICED_VBI_OUTPUT    0x00000080
#define V4L2_CAP_RDS_CAPTURE          0x00000100
#define V4L2_CAP_VIDEO_OUTPUT_OVERLAY 0x00000200
#define V4L2_CAP_HW_FREQ_SEEK         0x00000400
#define V4L2_CAP_RDS_OUTPUT           0x00000800
#define V4L2_CAP_VIDEO_CAPTURE_MPLANE 0x00001000
#define V4L2_CAP_VIDEO_OUTPUT_MPLANE  0x00002000
#define V4L2_CAP_VIDEO_M2M_MPLANE     0x00004000
#define V4L2_CAP_VIDEO_M2M            0x00008000
#define V4L2_CAP_TUNER                0x00010000
#define V4L2_CAP_AUDIO                0x00020000
#define V4L2_CAP_RADIO                0x00040000
#define V4L2_CAP_MODULATOR            0x00080000
#define V4L2_CAP_SDR_CAPTURE          0x00100000
#define V4L2_CAP_EXT_PIX_FORMAT       0x00200000
#define V4L2_CAP_SDR_OUTPUT           0x00400000
#define V4L2_CAP_META_CAPTURE         0x00800000
#define V4L2_CAP_READWRITE            0x01000000
#define V4L2_CAP_ASYNCIO              0x02000000
#define V4L2_CAP_STREAMING            0x04000000
#define V4L2_CAP_META_OUTPUT          0x08000000
#define V4L2_CAP_TOUCH                0x10000000
#define V4L2_CAP_IO_MC                0x20000000
#define V4L2_CAP_DEVICE_CAPS          0x80000000

/* ========== PIXEL FORMATS ========== */

/* RGB formats */
#define V4L2_PIX_FMT_RGB332   v4l2_fourcc('R', 'G', 'B', '1')
#define V4L2_PIX_FMT_RGB444   v4l2_fourcc('R', '4', '4', '4')
#define V4L2_PIX_FMT_RGB555   v4l2_fourcc('R', 'G', 'B', 'O')
#define V4L2_PIX_FMT_RGB565   v4l2_fourcc('R', 'G', 'B', 'P')
#define V4L2_PIX_FMT_RGB24    v4l2_fourcc('R', 'G', 'B', '3')
#define V4L2_PIX_FMT_BGR24    v4l2_fourcc('B', 'G', 'R', '3')
#define V4L2_PIX_FMT_RGB32    v4l2_fourcc('R', 'G', 'B', '4')
#define V4L2_PIX_FMT_BGR32    v4l2_fourcc('B', 'G', 'R', '4')

/* Grey formats */
#define V4L2_PIX_FMT_GREY     v4l2_fourcc('G', 'R', 'E', 'Y')
#define V4L2_PIX_FMT_Y10      v4l2_fourcc('Y', '1', '0', ' ')
#define V4L2_PIX_FMT_Y12      v4l2_fourcc('Y', '1', '2', ' ')
#define V4L2_PIX_FMT_Y16      v4l2_fourcc('Y', '1', '6', ' ')

/* Luminance+Chrominance formats */
#define V4L2_PIX_FMT_YUYV     v4l2_fourcc('Y', 'U', 'Y', 'V')
#define V4L2_PIX_FMT_UYVY     v4l2_fourcc('U', 'Y', 'V', 'Y')
#define V4L2_PIX_FMT_YUV422P  v4l2_fourcc('4', '2', '2', 'P')
#define V4L2_PIX_FMT_YUV420   v4l2_fourcc('Y', 'U', '1', '2')
#define V4L2_PIX_FMT_YVU420   v4l2_fourcc('Y', 'V', '1', '2')

/* Two planes -- Y + CbCr interleaved */
#define V4L2_PIX_FMT_NV12     v4l2_fourcc('N', 'V', '1', '2')
#define V4L2_PIX_FMT_NV21     v4l2_fourcc('N', 'V', '2', '1')
#define V4L2_PIX_FMT_NV16     v4l2_fourcc('N', 'V', '1', '6')
#define V4L2_PIX_FMT_NV61     v4l2_fourcc('N', 'V', '6', '1')

/* Bayer formats */
#define V4L2_PIX_FMT_SBGGR8   v4l2_fourcc('B', 'A', '8', '1')
#define V4L2_PIX_FMT_SGBRG8   v4l2_fourcc('G', 'B', 'R', 'G')
#define V4L2_PIX_FMT_SGRBG8   v4l2_fourcc('G', 'R', 'B', 'G')
#define V4L2_PIX_FMT_SRGGB8   v4l2_fourcc('R', 'G', 'G', 'B')
#define V4L2_PIX_FMT_SBGGR10  v4l2_fourcc('B', 'G', '1', '0')
#define V4L2_PIX_FMT_SGBRG10  v4l2_fourcc('G', 'B', '1', '0')
#define V4L2_PIX_FMT_SGRBG10  v4l2_fourcc('B', 'A', '1', '0')
#define V4L2_PIX_FMT_SRGGB10  v4l2_fourcc('R', 'G', '1', '0')

/* Compressed formats */
#define V4L2_PIX_FMT_MJPEG    v4l2_fourcc('M', 'J', 'P', 'G')
#define V4L2_PIX_FMT_JPEG     v4l2_fourcc('J', 'P', 'E', 'G')
#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4')

/* ========== CONTROL IDs ========== */

/* Control classes */
#define V4L2_CTRL_CLASS_USER            0x00980000
#define V4L2_CTRL_CLASS_CODEC           0x00990000
#define V4L2_CTRL_CLASS_CAMERA          0x009a0000
#define V4L2_CTRL_CLASS_FM_TX           0x009b0000
#define V4L2_CTRL_CLASS_FLASH           0x009c0000
#define V4L2_CTRL_CLASS_JPEG            0x009d0000
#define V4L2_CTRL_CLASS_IMAGE_SOURCE    0x009e0000
#define V4L2_CTRL_CLASS_IMAGE_PROC      0x009f0000

/* User-class control IDs */
#define V4L2_CID_BASE               (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_USER_BASE          V4L2_CID_BASE
#define V4L2_CID_USER_CLASS         (V4L2_CTRL_CLASS_USER | 1)
#define V4L2_CID_BRIGHTNESS         (V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST           (V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION         (V4L2_CID_BASE + 2)
#define V4L2_CID_HUE                (V4L2_CID_BASE + 3)
#define V4L2_CID_AUTO_WHITE_BALANCE (V4L2_CID_BASE + 12)
#define V4L2_CID_DO_WHITE_BALANCE   (V4L2_CID_BASE + 13)
#define V4L2_CID_RED_BALANCE        (V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE       (V4L2_CID_BASE + 15)
#define V4L2_CID_GAMMA              (V4L2_CID_BASE + 16)
#define V4L2_CID_EXPOSURE           (V4L2_CID_BASE + 17)
#define V4L2_CID_AUTOGAIN           (V4L2_CID_BASE + 18)
#define V4L2_CID_GAIN               (V4L2_CID_BASE + 19)
#define V4L2_CID_HFLIP              (V4L2_CID_BASE + 20)
#define V4L2_CID_VFLIP              (V4L2_CID_BASE + 21)
#define V4L2_CID_POWER_LINE_FREQUENCY (V4L2_CID_BASE + 24)
#define V4L2_CID_HUE_AUTO                  (V4L2_CID_BASE + 25)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE (V4L2_CID_BASE + 26)
#define V4L2_CID_SHARPNESS                 (V4L2_CID_BASE + 27)
#define V4L2_CID_BACKLIGHT_COMPENSATION    (V4L2_CID_BASE + 28)

/* Camera class control IDs */
#define V4L2_CID_CAMERA_CLASS_BASE  (V4L2_CTRL_CLASS_CAMERA | 0x900)
#define V4L2_CID_CAMERA_CLASS       (V4L2_CTRL_CLASS_CAMERA | 1)
#define V4L2_CID_EXPOSURE_ABSOLUTE        (V4L2_CID_CAMERA_CLASS_BASE + 17)
#define V4L2_CID_EXPOSURE_AUTO            (V4L2_CID_CAMERA_CLASS_BASE + 1)
#define V4L2_CID_AUTO_EXPOSURE_BIAS       (V4L2_CID_CAMERA_CLASS_BASE + 42)

/* ========== IOCTL CODES ========== */
#define VIDIOC_QUERYCAP       _IOR('V', 0, struct v4l2_capability)
#define VIDIOC_ENUM_FMT       _IOWR('V', 2, struct v4l2_fmtdesc)
#define VIDIOC_G_FMT          _IOWR('V', 4, struct v4l2_format)
#define VIDIOC_S_FMT          _IOWR('V', 5, struct v4l2_format)
#define VIDIOC_STREAMON       _IOW('V', 18, int)
#define VIDIOC_STREAMOFF      _IOW('V', 19, int)
#define VIDIOC_G_CTRL         _IOWR('V', 27, struct v4l2_control)
#define VIDIOC_S_CTRL         _IOWR('V', 28, struct v4l2_control)
#define VIDIOC_QUERYCTRL      _IOWR('V', 36, struct v4l2_queryctrl)
#define VIDIOC_G_EXT_CTRLS    _IOWR('V', 71, struct v4l2_ext_controls)
#define VIDIOC_S_EXT_CTRLS    _IOWR('V', 72, struct v4l2_ext_controls)
#define VIDIOC_TRY_EXT_CTRLS  _IOWR('V', 73, struct v4l2_ext_controls)
#define VIDIOC_QUERY_EXT_CTRL _IOWR('V', 103, struct v4l2_query_ext_ctrl)

#ifdef __cplusplus
}
#endif

#endif /* __LINUX_VIDEODEV2_H */
