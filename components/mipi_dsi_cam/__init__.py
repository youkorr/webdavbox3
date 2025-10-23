import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_FREQUENCY,
    CONF_ADDRESS,
)
from esphome import pins

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["i2c", "esp32"]
MULTI_CONF = True

mipi_dsi_cam_ns = cg.esphome_ns.namespace("mipi_dsi_cam")
MipiDsiCam = mipi_dsi_cam_ns.class_("MipiDsiCam", cg.Component, i2c.I2CDevice)

CONF_EXTERNAL_CLOCK_PIN = "external_clock_pin"
CONF_RESET_PIN = "reset_pin"
CONF_SENSOR = "sensor"
CONF_LANE = "lane"
CONF_ADDRESS_SENSOR = "address_sensor"
CONF_RESOLUTION = "resolution"
CONF_PIXEL_FORMAT = "pixel_format"
CONF_FRAMERATE = "framerate"
CONF_JPEG_QUALITY = "jpeg_quality"
CONF_ENABLE_V4L2 = "enable_v4l2"
CONF_ENABLE_ISP_PIPELINE = "enable_isp_pipeline"
CONF_ENABLE_JPEG_ENCODER = "enable_jpeg_encoder"
CONF_ENABLE_H264_ENCODER = "enable_h264_encoder"

PixelFormat = mipi_dsi_cam_ns.enum("PixelFormat")
PIXEL_FORMAT_RGB565 = PixelFormat.PIXEL_FORMAT_RGB565
PIXEL_FORMAT_YUV422 = PixelFormat.PIXEL_FORMAT_YUV422
PIXEL_FORMAT_RAW8 = PixelFormat.PIXEL_FORMAT_RAW8

PIXEL_FORMATS = {
    "RGB565": PIXEL_FORMAT_RGB565,
    "YUV422": PIXEL_FORMAT_YUV422,
    "RAW8": PIXEL_FORMAT_RAW8,
}

RESOLUTIONS = {
    "720P": (1280, 720),
    "800x640": (800, 640),
    "800x480": (800, 480),
    "1280x800": (1280, 800),
}

AVAILABLE_SENSORS = {}

def load_sensors():
    import logging
    logger = logging.getLogger(__name__)
    
    try:
        from .sensor_mipi_csi_sc202cs import get_sensor_info, get_driver_code
        AVAILABLE_SENSORS['sc202cs'] = {
            'info': get_sensor_info(),
            'driver': get_driver_code
        }
        logger.info("SC202CS sensor loaded")
    except ImportError as e:
        logger.warning(f"SC202CS sensor not available: {e}")
    except Exception as e:
        logger.error(f"Error loading SC202CS: {e}")
    
    try:
        from .sensor_mipi_csi_ov5647 import get_sensor_info, get_driver_code
        AVAILABLE_SENSORS['ov5647'] = {
            'info': get_sensor_info(),
            'driver': get_driver_code
        }
        logger.info("OV5647 sensor loaded (800x640)")
    except ImportError as e:
        logger.warning(f"OV5647 sensor not available: {e}")
    except Exception as e:
        logger.error(f"Error loading OV5647: {e}")
    
    try:
        from .sensor_mipi_csi_ov02c10 import get_sensor_info, get_driver_code
        AVAILABLE_SENSORS['ov02c10'] = {
            'info': get_sensor_info(),
            'driver': get_driver_code
        }
        logger.info("OV02C10 sensor loaded (1280x800)")
    except ImportError as e:
        logger.warning(f"OV02C10 sensor not available: {e}")
    except Exception as e:
        logger.error(f"Error loading OV02C10: {e}")
    
    if not AVAILABLE_SENSORS:
        raise cv.Invalid(
            "Aucun sensor MIPI disponible. "
            "Assurez-vous que les fichiers sensor_mipi_csi_*.py sont dans components/mipi_dsi_cam/"
        )
    
    logger.info(f"Sensors disponibles: {', '.join(AVAILABLE_SENSORS.keys())}")

load_sensors()

def validate_sensor(value):
    if value not in AVAILABLE_SENSORS:
        available = ', '.join(AVAILABLE_SENSORS.keys())
        raise cv.Invalid(
            f"Sensor '{value}' non disponible. Disponibles: {available}"
        )
    return value

def validate_resolution(value):
    if isinstance(value, str):
        value_upper = value.upper()
        if value_upper == "720P":
            return RESOLUTIONS["720P"]
        elif value_upper in ["800X640", "800x640"]:
            return RESOLUTIONS["800x640"]
        elif value_upper in ["800X480", "800x480"]:
            return RESOLUTIONS["800x480"]
        elif value_upper in ["1280X800", "1280x800"]:
            return RESOLUTIONS["1280x800"]
        else:
            raise cv.Invalid(
                f"Résolution '{value}' non supportée. Disponibles: 720P, 800x640, 800x480, 1280x800"
            )
    raise cv.Invalid("Le format de résolution doit être '720P', '800x640', '800x480' ou '1280x800'")

NO_CLOCK = "__NO_EXTERNAL_CLOCK__"

def validate_external_clock_pin(value):
    if value is None:
        return NO_CLOCK
    if value is False:
        return NO_CLOCK
    if isinstance(value, str):
        lower_val = value.lower()
        if lower_val in ["none", "false", "null"]:
            return NO_CLOCK
    if isinstance(value, int):
        return cv.int_range(min=0, max=50)(value)
    return pins.internal_gpio_output_pin_schema(value)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MipiDsiCam),
        cv.Optional(CONF_NAME, default="MIPI Camera"): cv.string,
        cv.Optional(CONF_EXTERNAL_CLOCK_PIN): validate_external_clock_pin,
        cv.Optional(CONF_FREQUENCY, default=24000000): cv.int_range(min=6000000, max=40000000),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_SENSOR): validate_sensor,
        cv.Optional(CONF_LANE): cv.int_range(min=1, max=4),
        cv.Optional(CONF_ADDRESS_SENSOR): cv.i2c_address,
        cv.Optional(CONF_RESOLUTION): validate_resolution,
        cv.Optional(CONF_PIXEL_FORMAT, default="RGB565"): cv.enum(PIXEL_FORMATS, upper=True),
        cv.Optional(CONF_FRAMERATE): cv.int_range(min=1, max=60),
        cv.Optional(CONF_JPEG_QUALITY, default=10): cv.int_range(min=1, max=63),
        
        # Options V4L2 et encoders
        cv.Optional(CONF_ENABLE_V4L2, default=False): cv.boolean,
        cv.Optional(CONF_ENABLE_ISP_PIPELINE, default=False): cv.boolean,
        cv.Optional(CONF_ENABLE_JPEG_ENCODER, default=False): cv.boolean,
        cv.Optional(CONF_ENABLE_H264_ENCODER, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x36))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    
    cg.add(var.set_name(config[CONF_NAME]))
    
    # Gérer external_clock_pin
    if CONF_EXTERNAL_CLOCK_PIN in config:
        ext_clock_value = config[CONF_EXTERNAL_CLOCK_PIN]
        
        if ext_clock_value == NO_CLOCK:
            cg.add(var.set_external_clock_pin(-1))
        elif isinstance(ext_clock_value, int):
            cg.add(var.set_external_clock_pin(ext_clock_value))
        elif isinstance(ext_clock_value, dict):
            pin_num = ext_clock_value[pins.CONF_NUMBER]
            cg.add(var.set_external_clock_pin(pin_num))
        else:
            cg.add(var.set_external_clock_pin(-1))
    else:
        cg.add(var.set_external_clock_pin(-1))
    
    cg.add(var.set_external_clock_frequency(config[CONF_FREQUENCY]))
    
    # Récupérer les infos du capteur
    sensor_name = config[CONF_SENSOR]
    sensor_info = AVAILABLE_SENSORS[sensor_name]['info']
    
    # Résolution
    if CONF_RESOLUTION in config:
        width, height = config[CONF_RESOLUTION]
        resolution_source = "configured"
    else:
        width = sensor_info['width']
        height = sensor_info['height']
        resolution_source = "native"
    
    # Paramètres du capteur
    lane_count = config.get(CONF_LANE, sensor_info['lane_count'])
    sensor_address = config.get(CONF_ADDRESS_SENSOR, sensor_info['i2c_address'])
    framerate = config.get(CONF_FRAMERATE, sensor_info['fps'])
    
    cg.add(var.set_sensor_type(sensor_name))
    cg.add(var.set_sensor_address(sensor_address))
    cg.add(var.set_lane_count(lane_count))
    cg.add(var.set_resolution(width, height))
    cg.add(var.set_bayer_pattern(sensor_info['bayer_pattern']))
    cg.add(var.set_lane_bitrate(sensor_info['lane_bitrate_mbps']))
    
    cg.add(var.set_pixel_format(config[CONF_PIXEL_FORMAT]))
    cg.add(var.set_jpeg_quality(config[CONF_JPEG_QUALITY]))
    cg.add(var.set_framerate(framerate))
    
    if CONF_RESET_PIN in config:
        reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset_pin))
    
    # Activer les fonctionnalités V4L2
    enable_v4l2 = config.get(CONF_ENABLE_V4L2, False)
    enable_isp = config.get(CONF_ENABLE_ISP_PIPELINE, False)
    enable_jpeg = config.get(CONF_ENABLE_JPEG_ENCODER, False)
    enable_h264 = config.get(CONF_ENABLE_H264_ENCODER, False)
    
    if enable_v4l2:
        cg.add_define("MIPI_DSI_CAM_ENABLE_V4L2")
        cg.add(var.enable_v4l2_adapter())
    
    if enable_isp:
        cg.add_define("MIPI_DSI_CAM_ENABLE_ISP_PIPELINE")
        cg.add(var.enable_isp_pipeline())
    
    if enable_jpeg:
        cg.add_define("MIPI_DSI_CAM_ENABLE_JPEG")
    
    if enable_h264:
        cg.add_define("MIPI_DSI_CAM_ENABLE_H264")
    
    # Générer le code des drivers
    import os
    
    all_drivers_code = ""
    
    for sensor_id, sensor_data in AVAILABLE_SENSORS.items():
        driver_code_func = sensor_data['driver']
        all_drivers_code += driver_code_func() + "\n\n"
    
    factory_code = f'''
namespace esphome {{
namespace mipi_dsi_cam {{

inline ISensorDriver* create_sensor_driver(const std::string& sensor_type, i2c::I2CDevice* i2c) {{
'''
    
    for sensor_id in AVAILABLE_SENSORS.keys():
        sensor_upper = sensor_id.upper()
        factory_code += f'''
    if (sensor_type == "{sensor_id}") {{
        return new {sensor_upper}Adapter(i2c);
    }}
'''
    
    factory_code += f'''
    
    ESP_LOGE("mipi_dsi_cam", "Unknown sensor type: %s", sensor_type.c_str());
    return nullptr;
}}

}}
}}
'''
    
    complete_code = all_drivers_code + factory_code
    
    component_dir = os.path.dirname(__file__)
    generated_file_path = os.path.join(component_dir, "mipi_dsi_cam_drivers_generated.h")
    
    with open(generated_file_path, 'w') as f:
        f.write("#ifndef MIPI_DSI_CAM_DRIVERS_GENERATED_H\n")
        f.write("#define MIPI_DSI_CAM_DRIVERS_GENERATED_H\n\n")
        f.write(complete_code)
        f.write("\n#endif\n")
    
    cg.add_build_flag("-DBOARD_HAS_PSRAM")
    cg.add_build_flag("-DCONFIG_CAMERA_CORE0=1")
    cg.add_build_flag("-DUSE_ESP32_VARIANT_ESP32P4")
    
    # Message de log pour la configuration
    has_ext_clock = CONF_EXTERNAL_CLOCK_PIN in config and config[CONF_EXTERNAL_CLOCK_PIN] != NO_CLOCK
    ext_clock_msg = "enabled" if has_ext_clock else "disabled"
    
    v4l2_msg = "enabled" if enable_v4l2 else "disabled"
    isp_msg = "enabled" if enable_isp else "disabled"
    jpeg_msg = "enabled" if enable_jpeg else "disabled"
    h264_msg = "enabled" if enable_h264 else "disabled"
    
    cg.add(cg.RawExpression(f'''
        ESP_LOGI("compile", "Camera configuration:");
        ESP_LOGI("compile", "  Sensor: {sensor_name}");
        ESP_LOGI("compile", "  Resolution: {width}x{height} ({resolution_source})");
        ESP_LOGI("compile", "  Lanes: {lane_count}");
        ESP_LOGI("compile", "  Address: 0x{sensor_address:02X}");
        ESP_LOGI("compile", "  Format: {config[CONF_PIXEL_FORMAT]}");
        ESP_LOGI("compile", "  FPS: {framerate}");
        ESP_LOGI("compile", "  External Clock: {ext_clock_msg}");
        ESP_LOGI("compile", "  V4L2 Interface: {v4l2_msg}");
        ESP_LOGI("compile", "  ISP Pipeline: {isp_msg}");
        ESP_LOGI("compile", "  JPEG Encoder: {jpeg_msg}");
        ESP_LOGI("compile", "  H264 Encoder: {h264_msg}");
    '''))
