SENSOR_INFO = {
    'name': 'ov02c10',
    'manufacturer': 'OmniVision',
    'pid': 0x5602,
    'i2c_address': 0x36,
    'lane_count': 1,  # 1 lane pour le capteur original
    'bayer_pattern': 1,  # GBRG (ESP_CAM_SENSOR_BAYER_GBRG)
    'lane_bitrate_mbps': 500,  # 100MHz * 4 = 400 Mbps
    'width': 1288,
    'height': 728,
    'fps': 30,
}

REGISTERS = {
    'sensor_id_h': 0x300a,
    'sensor_id_l': 0x300b,
    'stream_mode': 0x0100,
    'software_reset': 0x0103,
    # Gain registers
    'gain_dig_fine_h': 0x350c,
    'gain_dig_fine_l': 0x350d,
    'gain_dig_coarse': 0x350a,
    'gain_analog': 0x3508,
    # Exposure registers  
    'exposure_h': 0x3500,
    'exposure_m': 0x3501,
    'exposure_l': 0x3502,
    # Group hold
    'group_hold': 0x3208,
}

INIT_SEQUENCE = [
    # Reset et configuration de base
    (0x0100, 0x00, 0),   # Stream off
    (0x0103, 0x01, 10),  # Software reset, wait 10ms
    
    # PLL Configuration
    (0x0301, 0x08, 0),   # PLL pre-divider
    (0x0303, 0x06, 0),   # PLL divider
    (0x0304, 0x01, 0),   # PLL multiplier LSB
    (0x0305, 0x77, 0),   # PLL multiplier MSB (0x177 = 375)
    (0x0313, 0x40, 0),   # PLL control
    (0x031c, 0x4f, 0),   # System control
    
    # MIPI configuration - 1 LANE
    (0x3016, 0x12, 0),   # MIPI control - 1 lane
    (0x301b, 0xf0, 0),   # MIPI timing
    (0x3020, 0x97, 0),   # MIPI control
    (0x3021, 0x23, 0),   # MIPI control
    (0x3022, 0x01, 0),   # MIPI control
    (0x3026, 0xb4, 0),   # MIPI setup
    (0x3027, 0xf1, 0),   # MIPI setup
    
    # PLL and clock
    (0x303b, 0x00, 0),
    (0x303c, 0x4f, 0),
    (0x303d, 0xe6, 0),
    (0x303e, 0x00, 0),
    (0x303f, 0x03, 0),
    
    # Exposure et gain initiaux
    (0x3501, 0x04, 0),   # Exposition H
    (0x3502, 0x6c, 0),   # Exposition L
    (0x3504, 0x0c, 0),   # Analog gain control
    (0x3507, 0x00, 0),   # Manual gain H
    (0x3508, 0x08, 0),   # Analog gain
    (0x3509, 0x00, 0),   # Analog fine gain
    (0x350a, 0x01, 0),   # Digital coarse gain
    (0x350b, 0x00, 0),   # Digital coarse gain
    (0x350c, 0x41, 0),   # Digital fine gain
    
    # Analog control
    (0x3600, 0x84, 0),
    (0x3603, 0x08, 0),
    (0x3610, 0x57, 0),
    (0x3611, 0x1b, 0),
    (0x3613, 0x78, 0),
    (0x3623, 0x00, 0),
    (0x3632, 0xa0, 0),
    (0x3642, 0xe8, 0),
    (0x364c, 0x70, 0),
    (0x365d, 0x00, 0),
    (0x365f, 0x0f, 0),
    
    # Sensor core control
    (0x3708, 0x30, 0),
    (0x3714, 0x24, 0),
    (0x3725, 0x02, 0),
    (0x3737, 0x08, 0),
    (0x3739, 0x28, 0),
    (0x3749, 0x32, 0),
    (0x374a, 0x32, 0),
    (0x374b, 0x32, 0),
    (0x374c, 0x32, 0),
    (0x374d, 0x81, 0),
    (0x374e, 0x81, 0),
    (0x374f, 0x81, 0),
    (0x3752, 0x36, 0),
    (0x3753, 0x36, 0),
    (0x3754, 0x36, 0),
    (0x3761, 0x00, 0),
    (0x376c, 0x81, 0),
    (0x3774, 0x18, 0),
    (0x3776, 0x08, 0),
    (0x377c, 0x81, 0),
    (0x377d, 0x81, 0),
    (0x377e, 0x81, 0),
    (0x37a0, 0x44, 0),
    (0x37a6, 0x44, 0),
    (0x37aa, 0x0d, 0),
    (0x37ae, 0x00, 0),
    (0x37cb, 0x03, 0),
    (0x37cc, 0x01, 0),
    (0x37d8, 0x02, 0),
    (0x37d9, 0x10, 0),
    (0x37e1, 0x10, 0),
    (0x37e2, 0x18, 0),
    (0x37e3, 0x08, 0),
    (0x37e4, 0x08, 0),
    (0x37e5, 0x02, 0),
    (0x37e6, 0x08, 0),
    
    # Timing configuration pour 1288x728
    # X address start: 320 (0x140)
    (0x3800, 0x01, 0),   # X start H
    (0x3801, 0x40, 0),   # X start L
    
    # Y address start: 180 (0xB4)
    (0x3802, 0x00, 0),   # Y start H
    (0x3803, 0xb4, 0),   # Y start L
    
    # X address end: 1615 (0x64F)
    (0x3804, 0x06, 0),   # X end H
    (0x3805, 0x4f, 0),   # X end L
    
    # Y address end: 915 (0x38F)
    (0x3806, 0x03, 0),   # Y end H
    (0x3807, 0x8f, 0),   # Y end L
    
    # Output size: 1288x728
    (0x3808, 0x05, 0),   # Output width H (0x508 = 1288)
    (0x3809, 0x08, 0),   # Output width L
    (0x380a, 0x02, 0),   # Output height H (0x2D8 = 728)
    (0x380b, 0xd8, 0),   # Output height L
    
    # HTS/VTS - Horizontal/Vertical Total Size
    # HTS: 2280 pixels (0x8E8) - pour timing stable
    (0x380c, 0x08, 0),   # HTS H
    (0x380d, 0xe8, 0),   # HTS L
    
    # VTS: 1164 lignes (0x48C) - pour 30fps
    (0x380e, 0x04, 0),   # VTS H  
    (0x380f, 0x8c, 0),   # VTS L
    
    # Offset
    (0x3810, 0x00, 0),   # H offset H
    (0x3811, 0x07, 0),   # H offset L
    (0x3812, 0x00, 0),   # V offset H
    (0x3813, 0x04, 0),   # V offset L
    
    # Subsample
    (0x3814, 0x01, 0),   # H subsample odd
    (0x3815, 0x01, 0),   # V subsample odd
    (0x3816, 0x01, 0),   # H subsample even
    (0x3817, 0x01, 0),   # V subsample even
    
    # Mirror/Flip control
    (0x3820, 0xa0, 0),   # Vertical setting
    (0x3821, 0x00, 0),   # Horizontal setting
    (0x3822, 0x80, 0),
    (0x3823, 0x08, 0),
    (0x3824, 0x00, 0),
    (0x3825, 0x20, 0),
    (0x3826, 0x00, 0),
    (0x3827, 0x08, 0),
    (0x382a, 0x00, 0),
    (0x382b, 0x08, 0),
    (0x382d, 0x00, 0),
    (0x382e, 0x00, 0),
    (0x382f, 0x23, 0),
    (0x3834, 0x00, 0),
    (0x3839, 0x00, 0),
    (0x383a, 0xd1, 0),
    (0x383e, 0x03, 0),
    
    # PSRAM and processing
    (0x393d, 0x29, 0),
    (0x393f, 0x6e, 0),
    (0x394b, 0x06, 0),
    (0x394c, 0x06, 0),
    (0x394d, 0x08, 0),
    (0x394e, 0x0a, 0),
    (0x394f, 0x01, 0),
    (0x3950, 0x01, 0),
    (0x3951, 0x01, 0),
    (0x3952, 0x01, 0),
    (0x3953, 0x01, 0),
    (0x3954, 0x01, 0),
    (0x3955, 0x01, 0),
    (0x3956, 0x01, 0),
    (0x3957, 0x0e, 0),
    (0x3958, 0x08, 0),
    (0x3959, 0x08, 0),
    (0x395a, 0x08, 0),
    (0x395b, 0x13, 0),
    (0x395c, 0x09, 0),
    (0x395d, 0x05, 0),
    (0x395e, 0x02, 0),
    (0x395f, 0x00, 0),
    (0x395f, 0x00, 0),  # Répété dans l'original
    (0x3960, 0x00, 0),
    (0x3961, 0x00, 0),
    (0x3962, 0x00, 0),
    (0x3963, 0x00, 0),
    (0x3964, 0x00, 0),
    (0x3965, 0x00, 0),
    (0x3966, 0x00, 0),
    (0x3967, 0x00, 0),
    (0x3968, 0x01, 0),
    (0x3969, 0x01, 0),
    (0x396a, 0x01, 0),
    (0x396b, 0x01, 0),
    (0x396c, 0x10, 0),
    (0x396d, 0xf0, 0),
    (0x396e, 0x11, 0),
    (0x396f, 0x00, 0),
    (0x3970, 0x37, 0),
    (0x3971, 0x37, 0),
    (0x3972, 0x37, 0),
    (0x3973, 0x37, 0),
    (0x3974, 0x00, 0),
    (0x3975, 0x3c, 0),
    (0x3976, 0x3c, 0),
    (0x3977, 0x3c, 0),
    (0x3978, 0x3c, 0),
    
    # VFIFO
    (0x3c00, 0x0f, 0),
    (0x3c20, 0x01, 0),
    (0x3c21, 0x08, 0),
    
    # Format control
    (0x3f00, 0x8b, 0),
    (0x3f02, 0x0f, 0),
    
    # BLC control
    (0x4000, 0xc3, 0),
    (0x4001, 0xe0, 0),
    (0x4002, 0x00, 0),
    (0x4003, 0x40, 0),
    (0x4008, 0x04, 0),
    (0x4009, 0x23, 0),
    (0x400a, 0x04, 0),
    (0x400b, 0x01, 0),
    (0x4041, 0x20, 0),
    (0x4077, 0x06, 0),
    (0x4078, 0x00, 0),
    (0x4079, 0x1a, 0),
    (0x407a, 0x7f, 0),
    (0x407b, 0x01, 0),
    (0x4080, 0x03, 0),
    (0x4081, 0x84, 0),
    
    # VFIFO control
    (0x4308, 0x03, 0),
    (0x4309, 0xff, 0),
    (0x430d, 0x00, 0),
    
    # DVP control
    (0x4500, 0x07, 0),
    (0x4501, 0x00, 0),
    (0x4503, 0x00, 0),
    (0x450a, 0x04, 0),
    (0x450e, 0x00, 0),
    (0x450f, 0x00, 0),
    
    # MIPI control
    (0x4800, 0x64, 0),   # MIPI control (line sync enabled)
    (0x4806, 0x00, 0),
    (0x4813, 0x00, 0),
    (0x4815, 0x40, 0),
    (0x4816, 0x12, 0),
    (0x481f, 0x30, 0),
    (0x4837, 0x15, 0),   # MIPI PCLK period
    (0x4857, 0x05, 0),
    (0x4884, 0x04, 0),
    
    # ISP control
    (0x4900, 0x00, 0),
    (0x4901, 0x00, 0),
    (0x4902, 0x01, 0),
    
    # Temperature control
    (0x4d00, 0x03, 0),
    (0x4d01, 0xd8, 0),
    (0x4d02, 0xba, 0),
    (0x4d03, 0xa0, 0),
    (0x4d04, 0xb7, 0),
    (0x4d05, 0x34, 0),
    (0x4d0d, 0x00, 0),
    
    # ISP control
    (0x5000, 0xfd, 0),   # ISP enable
    (0x5001, 0x50, 0),
    (0x5006, 0x00, 0),
    (0x5080, 0x40, 0),
    
    # AWB control
    (0x5181, 0x2b, 0),
    (0x5202, 0xa3, 0),
    (0x5206, 0x01, 0),
    (0x5207, 0x00, 0),
    (0x520a, 0x01, 0),
    (0x520b, 0x00, 0),
    
    # Lens shading
    (0x4f00, 0x01, 0),
]


# Tables de gain pour OV02C10 (identiques à avant)
GAIN_VALUES = [
    # Gains 1x-2x (digital fine variation)
    1000, 1031, 1063, 1094, 1125, 1156, 1188, 1219,
    1250, 1281, 1313, 1344, 1375, 1406, 1438, 1469,
    1500, 1531, 1563, 1594, 1625, 1656, 1688, 1719,
    1750, 1781, 1813, 1844, 1875, 1906, 1938, 1969,
    # Gains 2x (analog 2x)
    2000, 2062, 2126, 2188, 2250, 2312, 2376, 2438,
    2500, 2562, 2626, 2688, 2750, 2812, 2876, 2938,
    3000, 3062, 3126, 3188, 3250, 3312, 3376, 3438,
    3500, 3562, 3626, 3688, 3750, 3812, 3876, 3938,
    # Gains 4x (analog 3x)
    4000, 4124, 4252, 4376, 4500, 4624, 4752, 4876,
    5000, 5124, 5252, 5376, 5500, 5624, 5752, 5876,
    6000, 6124, 6252, 6376, 6500, 6624, 6752, 6876,
    7000, 7124, 7252, 7376, 7500, 7624, 7752, 7876,
    # Gains 8x (analog 4x)
    8000, 8248, 8504, 8752, 9000, 9248, 9504, 9752,
    10000, 10248, 10504, 10752, 11000, 11248, 11504, 11752,
    12000, 12248, 12504, 12752, 13000, 13248, 13504, 13752,
    14000, 14248, 14504, 14752, 15000, 15248, 15504, 15752,
    # Gains 16x (analog 5x)
    16000, 16496, 17008, 17504, 18000, 18496, 19008, 19504,
    20000, 20496, 21008, 21504, 22000, 22496, 23008, 23504,
    24000, 24496, 25008, 25504, 26000, 26496, 27008, 27504,
    28000, 28496, 29008, 29504, 30000, 30496, 31008, 31504,
]

# Mapping des registres de gain (identiques à avant)
GAIN_REGISTERS = [
    # 1x analog, digital fine variation (0x80-0xFC)
    (0x80, 0x01, 0x01), (0x84, 0x01, 0x01), (0x88, 0x01, 0x01), (0x8c, 0x01, 0x01),
    (0x90, 0x01, 0x01), (0x94, 0x01, 0x01), (0x98, 0x01, 0x01), (0x9c, 0x01, 0x01),
    (0xa0, 0x01, 0x01), (0xa4, 0x01, 0x01), (0xa8, 0x01, 0x01), (0xac, 0x01, 0x01),
    (0xb0, 0x01, 0x01), (0xb4, 0x01, 0x01), (0xb8, 0x01, 0x01), (0xbc, 0x01, 0x01),
    (0xc0, 0x01, 0x01), (0xc4, 0x01, 0x01), (0xc8, 0x01, 0x01), (0xcc, 0x01, 0x01),
    (0xd0, 0x01, 0x01), (0xd4, 0x01, 0x01), (0xd8, 0x01, 0x01), (0xdc, 0x01, 0x01),
    (0xe0, 0x01, 0x01), (0xe4, 0x01, 0x01), (0xe8, 0x01, 0x01), (0xec, 0x01, 0x01),
    (0xf0, 0x01, 0x01), (0xf4, 0x01, 0x01), (0xf8, 0x01, 0x01), (0xfc, 0x01, 0x01),
    
    # 2x analog (0x02)
    (0x80, 0x01, 0x02), (0x84, 0x01, 0x02), (0x88, 0x01, 0x02), (0x8c, 0x01, 0x02),
    (0x90, 0x01, 0x02), (0x94, 0x01, 0x02), (0x98, 0x01, 0x02), (0x9c, 0x01, 0x02),
    (0xa0, 0x01, 0x02), (0xa4, 0x01, 0x02), (0xa8, 0x01, 0x02), (0xac, 0x01, 0x02),
    (0xb0, 0x01, 0x02), (0xb4, 0x01, 0x02), (0xb8, 0x01, 0x02), (0xbc, 0x01, 0x02),
    (0xc0, 0x01, 0x02), (0xc4, 0x01, 0x02), (0xc8, 0x01, 0x02), (0xcc, 0x01, 0x02),
    (0xd0, 0x01, 0x02), (0xd4, 0x01, 0x02), (0xd8, 0x01, 0x02), (0xdc, 0x01, 0x02),
    (0xe0, 0x01, 0x02), (0xe4, 0x01, 0x02), (0xe8, 0x01, 0x02), (0xec, 0x01, 0x02),
    (0xf0, 0x01, 0x02), (0xf4, 0x01, 0x02), (0xf8, 0x01, 0x02), (0xfc, 0x01, 0x02),
    
    # 4x analog (0x03)
    (0x80, 0x01, 0x03), (0x84, 0x01, 0x03), (0x88, 0x01, 0x03), (0x8c, 0x01, 0x03),
    (0x90, 0x01, 0x03), (0x94, 0x01, 0x03), (0x98, 0x01, 0x03), (0x9c, 0x01, 0x03),
    (0xa0, 0x01, 0x03), (0xa4, 0x01, 0x03), (0xa8, 0x01, 0x03), (0xac, 0x01, 0x03),
    (0xb0, 0x01, 0x03), (0xb4, 0x01, 0x03), (0xb8, 0x01, 0x03), (0xbc, 0x01, 0x03),
    (0xc0, 0x01, 0x03), (0xc4, 0x01, 0x03), (0xc8, 0x01, 0x03), (0xcc, 0x01, 0x03),
    (0xd0, 0x01, 0x03), (0xd4, 0x01, 0x03), (0xd8, 0x01, 0x03), (0xdc, 0x01, 0x03),
    (0xe0, 0x01, 0x03), (0xe4, 0x01, 0x03), (0xe8, 0x01, 0x03), (0xec, 0x01, 0x03),
    (0xf0, 0x01, 0x03), (0xf4, 0x01, 0x03), (0xf8, 0x01, 0x03), (0xfc, 0x01, 0x03),
    
    # 8x analog (0x04)
    (0x80, 0x01, 0x04), (0x84, 0x01, 0x04), (0x88, 0x01, 0x04), (0x8c, 0x01, 0x04),
    (0x90, 0x01, 0x04), (0x94, 0x01, 0x04), (0x98, 0x01, 0x04), (0x9c, 0x01, 0x04),
    (0xa0, 0x01, 0x04), (0xa4, 0x01, 0x04), (0xa8, 0x01, 0x04), (0xac, 0x01, 0x04),
    (0xb0, 0x01, 0x04), (0xb4, 0x01, 0x04), (0xb8, 0x01, 0x04), (0xbc, 0x01, 0x04),
    (0xc0, 0x01, 0x04), (0xc4, 0x01, 0x04), (0xc8, 0x01, 0x04), (0xcc, 0x01, 0x04),
    (0xd0, 0x01, 0x04), (0xd4, 0x01, 0x04), (0xd8, 0x01, 0x04), (0xdc, 0x01, 0x04),
    (0xe0, 0x01, 0x04), (0xe4, 0x01, 0x04), (0xe8, 0x01, 0x04), (0xec, 0x01, 0x04),
    (0xf0, 0x01, 0x04), (0xf4, 0x01, 0x04), (0xf8, 0x01, 0x04), (0xfc, 0x01, 0x04),
    
    # 16x analog (0x05)
    (0x80, 0x01, 0x05), (0x84, 0x01, 0x05), (0x88, 0x01, 0x05), (0x8c, 0x01, 0x05),
    (0x90, 0x01, 0x05), (0x94, 0x01, 0x05), (0x98, 0x01, 0x05), (0x9c, 0x01, 0x05),
    (0xa0, 0x01, 0x05), (0xa4, 0x01, 0x05), (0xa8, 0x01, 0x05), (0xac, 0x01, 0x05),
    (0xb0, 0x01, 0x05), (0xb4, 0x01, 0x05), (0xb8, 0x01, 0x05), (0xbc, 0x01, 0x05),
    (0xc0, 0x01, 0x05), (0xc4, 0x01, 0x05), (0xc8, 0x01, 0x05), (0xcc, 0x01, 0x05),
    (0xd0, 0x01, 0x05), (0xd4, 0x01, 0x05), (0xd8, 0x01, 0x05), (0xdc, 0x01, 0x05),
    (0xe0, 0x01, 0x05), (0xe4, 0x01, 0x05), (0xe8, 0x01, 0x05), (0xec, 0x01, 0x05),
    (0xf0, 0x01, 0x05), (0xf4, 0x01, 0x05), (0xf8, 0x01, 0x05), (0xfc, 0x01, 0x05),
]

def generate_driver_cpp():
    cpp_code = f'''
namespace esphome {{
namespace mipi_dsi_cam {{

namespace {SENSOR_INFO['name']}_regs {{
'''
    
    for name, addr in REGISTERS.items():
        cpp_code += f'    constexpr uint16_t {name.upper()} = 0x{addr:04X};\n'
    
    cpp_code += f'''
}}

struct {SENSOR_INFO['name'].upper()}InitRegister {{
    uint16_t addr;
    uint8_t value;
    uint16_t delay_ms;
}};

static const {SENSOR_INFO['name'].upper()}InitRegister {SENSOR_INFO['name']}_init_sequence[] = {{
'''
    
    for addr, value, delay in INIT_SEQUENCE:
        cpp_code += f'    {{0x{addr:04X}, 0x{value:02X}, {delay}}},\n'
    
    cpp_code += f'''
}};

struct {SENSOR_INFO['name'].upper()}GainRegisters {{
    uint8_t dgain_fine;
    uint8_t dgain_coarse;
    uint8_t analog_gain;
}};

static const {SENSOR_INFO['name'].upper()}GainRegisters {SENSOR_INFO['name']}_gain_map[] = {{
'''
    
    for fine, coarse, analog in GAIN_REGISTERS:
        cpp_code += f'    {{0x{fine:02X}, 0x{coarse:02X}, 0x{analog:02X}}},\n'
    
    cpp_code += f'''
}};

class {SENSOR_INFO['name'].upper()}Driver {{
public:
    {SENSOR_INFO['name'].upper()}Driver(esphome::i2c::I2CDevice* i2c) : i2c_(i2c) {{}}
    
    esp_err_t init() {{
        ESP_LOGI(TAG, "Init {SENSOR_INFO['name'].upper()} - 1288x728 @ 30fps, 1 lane (ORIGINAL)");
        
        for (size_t i = 0; i < sizeof({SENSOR_INFO['name']}_init_sequence) / sizeof({SENSOR_INFO['name'].upper()}InitRegister); i++) {{
            const auto& reg = {SENSOR_INFO['name']}_init_sequence[i];
            
            if (reg.delay_ms > 0) {{
                vTaskDelay(pdMS_TO_TICKS(reg.delay_ms));
            }}
            
            esp_err_t ret = write_register(reg.addr, reg.value);
            if (ret != ESP_OK) {{
                ESP_LOGE(TAG, "Init failed at reg 0x%04X", reg.addr);
                return ret;
            }}
        }}
        
        ESP_LOGI(TAG, "✅ {SENSOR_INFO['name'].upper()} initialized - 1 lane @ 400Mbps");
        return ESP_OK;
    }}
    
    esp_err_t read_id(uint16_t* pid) {{
        uint8_t pid_h, pid_l;
        
        ESP_LOGI(TAG, "Reading sensor ID...");
        
        // Utiliser la méthode avec repeated start
        esp_err_t ret = read_register({SENSOR_INFO['name']}_regs::SENSOR_ID_H, &pid_h);
        if (ret != ESP_OK) {{
            ESP_LOGE(TAG, "Failed to read PID high byte");
            return ret;
        }}
        
        ret = read_register({SENSOR_INFO['name']}_regs::SENSOR_ID_L, &pid_l);
        if (ret != ESP_OK) {{
            ESP_LOGE(TAG, "Failed to read PID low byte");
            return ret;
        }}
        
        *pid = (pid_h << 8) | pid_l;
        ESP_LOGI(TAG, "✅ Read PID: 0x%04X (H=0x%02X, L=0x%02X)", *pid, pid_h, pid_l);
        return ESP_OK;
    }}
    
    esp_err_t start_stream() {{
        return write_register({SENSOR_INFO['name']}_regs::STREAM_MODE, 0x01);
    }}
    
    esp_err_t stop_stream() {{
        return write_register({SENSOR_INFO['name']}_regs::STREAM_MODE, 0x00);
    }}
    
    esp_err_t set_gain(uint32_t gain_index) {{
        if (gain_index >= sizeof({SENSOR_INFO['name']}_gain_map) / sizeof({SENSOR_INFO['name'].upper()}GainRegisters)) {{
            gain_index = (sizeof({SENSOR_INFO['name']}_gain_map) / sizeof({SENSOR_INFO['name'].upper()}GainRegisters)) - 1;
        }}
        
        const auto& gain = {SENSOR_INFO['name']}_gain_map[gain_index];
        
        // Group hold start
        esp_err_t ret = write_register({SENSOR_INFO['name']}_regs::GROUP_HOLD, 0x00);
        if (ret != ESP_OK) return ret;
        
        // Write gain registers
        ret = write_register({SENSOR_INFO['name']}_regs::GAIN_DIG_FINE_H, gain.dgain_fine >> 2);
        if (ret != ESP_OK) return ret;
        
        ret = write_register({SENSOR_INFO['name']}_regs::GAIN_DIG_FINE_L, (gain.dgain_fine & 0x03) << 6);
        if (ret != ESP_OK) return ret;
        
        ret = write_register({SENSOR_INFO['name']}_regs::GAIN_DIG_COARSE, gain.dgain_coarse);
        if (ret != ESP_OK) return ret;
        
        ret = write_register({SENSOR_INFO['name']}_regs::GAIN_ANALOG, gain.analog_gain);
        if (ret != ESP_OK) return ret;
        
        // Group hold end
        ret = write_register({SENSOR_INFO['name']}_regs::GROUP_HOLD, 0x10);
        return ret;
    }}
    
    esp_err_t set_exposure(uint32_t exposure) {{
        // OV02C10 exposure format: 16-bit value in registers 0x3500-0x3502
        uint8_t exp_h = (exposure >> 12) & 0x0F;
        uint8_t exp_m = (exposure >> 4) & 0xFF;
        uint8_t exp_l = (exposure & 0x0F) << 4;
        
        // Group hold start
        esp_err_t ret = write_register({SENSOR_INFO['name']}_regs::GROUP_HOLD, 0x00);
        if (ret != ESP_OK) return ret;
        
        ret = write_register({SENSOR_INFO['name']}_regs::EXPOSURE_H, exp_h);
        if (ret != ESP_OK) return ret;
        
        ret = write_register({SENSOR_INFO['name']}_regs::EXPOSURE_M, exp_m);
        if (ret != ESP_OK) return ret;
        
        ret = write_register({SENSOR_INFO['name']}_regs::EXPOSURE_L, exp_l);
        if (ret != ESP_OK) return ret;
        
        // Group hold end
        ret = write_register({SENSOR_INFO['name']}_regs::GROUP_HOLD, 0x10);
        return ret;
    }}
    
    esp_err_t write_register(uint16_t reg, uint8_t value) {{
        uint8_t data[3] = {{
            static_cast<uint8_t>((reg >> 8) & 0xFF),
            static_cast<uint8_t>(reg & 0xFF),
            value
        }};
        
        auto err = i2c_->write(data, 3);
        if (err != esphome::i2c::ERROR_OK) {{
            ESP_LOGE(TAG, "Failed to write reg 0x%04X, error: %d", reg, err);
            return ESP_FAIL;
        }}
        return ESP_OK;
    }}
    
    esp_err_t read_register(uint16_t reg, uint8_t* value) {{
        uint8_t addr[2] = {{
            static_cast<uint8_t>((reg >> 8) & 0xFF),
            static_cast<uint8_t>(reg & 0xFF)
        }};
        
        // Écrire l'adresse du registre
        auto err = i2c_->write(addr, 2);
        if (err != esphome::i2c::ERROR_OK) {{
            ESP_LOGE(TAG, "Failed to write reg address 0x%04X", reg);
            return ESP_FAIL;
        }}
        
        // Lire la valeur
        err = i2c_->read(value, 1);
        if (err != esphome::i2c::ERROR_OK) {{
            ESP_LOGE(TAG, "Failed to read reg 0x%04X", reg);
            return ESP_FAIL;
        }}
        
        return ESP_OK;
    }}
    
private:
    esphome::i2c::I2CDevice* i2c_;
    static constexpr const char* TAG = "{SENSOR_INFO['name'].upper()}";
}};

class {SENSOR_INFO['name'].upper()}Adapter : public ISensorDriver {{
public:
    {SENSOR_INFO['name'].upper()}Adapter(i2c::I2CDevice* i2c) : driver_(i2c) {{}}
    
    const char* get_name() const override {{ return "{SENSOR_INFO['name']} (1288x728)"; }}
    uint16_t get_pid() const override {{ return 0x{SENSOR_INFO['pid']:04X}; }}
    uint8_t get_i2c_address() const override {{ return 0x{SENSOR_INFO['i2c_address']:02X}; }}
    uint8_t get_lane_count() const override {{ return {SENSOR_INFO['lane_count']}; }}
    uint8_t get_bayer_pattern() const override {{ return {SENSOR_INFO['bayer_pattern']}; }}
    uint16_t get_lane_bitrate_mbps() const override {{ return {SENSOR_INFO['lane_bitrate_mbps']}; }}
    uint16_t get_width() const override {{ return {SENSOR_INFO['width']}; }}
    uint16_t get_height() const override {{ return {SENSOR_INFO['height']}; }}
    uint8_t get_fps() const override {{ return {SENSOR_INFO['fps']}; }}
    
    esp_err_t init() override {{ return driver_.init(); }}
    esp_err_t read_id(uint16_t* pid) override {{ return driver_.read_id(pid); }}
    esp_err_t start_stream() override {{ return driver_.start_stream(); }}
    esp_err_t stop_stream() override {{ return driver_.stop_stream(); }}
    esp_err_t set_gain(uint32_t gain_index) override {{ return driver_.set_gain(gain_index); }}
    esp_err_t set_exposure(uint32_t exposure) override {{ return driver_.set_exposure(exposure); }}
    esp_err_t write_register(uint16_t reg, uint8_t value) override {{ return driver_.write_register(reg, value); }}
    esp_err_t read_register(uint16_t reg, uint8_t* value) override {{ return driver_.read_register(reg, value); }}
    
private:
    {SENSOR_INFO['name'].upper()}Driver driver_;
}};

}}
}}
'''
    
    return cpp_code

def get_sensor_info():
    return SENSOR_INFO

def get_driver_code():
    return generate_driver_cpp()
