import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["lvgl", "mipi_dsi_cam"]
AUTO_LOAD = ["mipi_dsi_cam"]

CONF_CAMERA_ID = "camera_id"
CONF_CANVAS_ID = "canvas_id"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_ROTATION = "rotation"
CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"
CONF_JPEG_OUTPUT = "jpeg_output"
CONF_H264_OUTPUT = "h264_output"

lvgl_camera_display_ns = cg.esphome_ns.namespace("lvgl_camera_display")
LVGLCameraDisplay = lvgl_camera_display_ns.class_("LVGLCameraDisplay", cg.Component)

RotationAngle = lvgl_camera_display_ns.enum("RotationAngle")
ROTATION_ANGLES = {
    0: RotationAngle.ROTATION_0,
    90: RotationAngle.ROTATION_90,
    180: RotationAngle.ROTATION_180,
    270: RotationAngle.ROTATION_270,
}

# Import du namespace mipi_dsi_cam
mipi_dsi_cam_ns = cg.esphome_ns.namespace("mipi_dsi_cam")
MipiDsiCam = mipi_dsi_cam_ns.class_("MipiDsiCam")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(LVGLCameraDisplay),
    cv.Required(CONF_CAMERA_ID): cv.use_id(MipiDsiCam),
    cv.Required(CONF_CANVAS_ID): cv.string,
    cv.Optional(CONF_UPDATE_INTERVAL, default="33ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_ROTATION, default=0): cv.enum(ROTATION_ANGLES, int=True),
    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
    cv.Optional(CONF_JPEG_OUTPUT, default=False): cv.boolean,
    cv.Optional(CONF_H264_OUTPUT, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Lier à la caméra
    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))
    
    # Définir l'intervalle de mise à jour
    update_interval_ms = config[CONF_UPDATE_INTERVAL].total_milliseconds
    cg.add(var.set_update_interval(int(update_interval_ms)))
    
    # Configuration de rotation et mirror
    cg.add(var.set_rotation(config[CONF_ROTATION]))
    cg.add(var.set_mirror_x(config[CONF_MIRROR_X]))
    cg.add(var.set_mirror_y(config[CONF_MIRROR_Y]))
    
    # Configuration des encodeurs
    cg.add(var.set_jpeg_output(config[CONF_JPEG_OUTPUT]))
    cg.add(var.set_h264_output(config[CONF_H264_OUTPUT]))
    
    # Logger la configuration
    rotation_str = f"{config[CONF_ROTATION]}°"
    mirror_x_str = "ON" if config[CONF_MIRROR_X] else "OFF"
    mirror_y_str = "ON" if config[CONF_MIRROR_Y] else "OFF"
    jpeg_str = "enabled" if config[CONF_JPEG_OUTPUT] else "disabled"
    h264_str = "enabled" if config[CONF_H264_OUTPUT] else "disabled"
    
    cg.add(cg.RawExpression(f'''
        ESP_LOGI("compile", "LVGL Camera Display configuration:");
        ESP_LOGI("compile", "  Update interval: {int(update_interval_ms)} ms");
        ESP_LOGI("compile", "  Rotation: {rotation_str}");
        ESP_LOGI("compile", "  Mirror X: {mirror_x_str}");
        ESP_LOGI("compile", "  Mirror Y: {mirror_y_str}");
        ESP_LOGI("compile", "  JPEG output: {jpeg_str}");
        ESP_LOGI("compile", "  H264 output: {h264_str}");
    '''))
