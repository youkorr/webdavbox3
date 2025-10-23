#ifdef MIPI_DSI_CAM_ENABLE_V4L2

#include "mipi_dsi_cam_v4l2_adapter.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

// Note: Le vrai esp_video.h serait ici, mais pour la démo nous utilisons une version simplifiée
// #include "esp_video.h"
// #include "esp_video_device.h"

namespace esphome {
namespace mipi_dsi_cam {

static const char *TAG = "mipi_dsi_cam.v4l2";

// Structure simplifiée pour la démo - dans un vrai système, utiliser esp_video_ops
struct esp_video_ops {
  esp_err_t (*init)(void *video);
  esp_err_t (*deinit)(void *video);
  esp_err_t (*start)(void *video, uint32_t type);
  esp_err_t (*stop)(void *video, uint32_t type);
  esp_err_t (*enum_format)(void *video, uint32_t type, uint32_t index, uint32_t *pixel_format);
  esp_err_t (*set_format)(void *video, const struct v4l2_format *format);
  esp_err_t (*get_format)(void *video, struct v4l2_format *format);
  esp_err_t (*notify)(void *video, int event, void *arg);
  esp_err_t (*set_ext_ctrl)(void *video, const struct v4l2_ext_controls *ctrls);
  esp_err_t (*get_ext_ctrl)(void *video, struct v4l2_ext_controls *ctrls);
  esp_err_t (*query_ext_ctrl)(void *video, struct v4l2_query_ext_ctrl *qctrl);
  esp_err_t (*set_sensor_format)(void *video, const void *format);
  esp_err_t (*get_sensor_format)(void *video, void *format);
};

// Table des opérations V4L2
static const struct esp_video_ops s_mipi_dsi_cam_v4l2_ops = {
  .init = MipiDsiCamV4L2Adapter::v4l2_init,
  .deinit = MipiDsiCamV4L2Adapter::v4l2_deinit,
  .start = MipiDsiCamV4L2Adapter::v4l2_start,
  .stop = MipiDsiCamV4L2Adapter::v4l2_stop,
  .enum_format = MipiDsiCamV4L2Adapter::v4l2_enum_format,
  .set_format = MipiDsiCamV4L2Adapter::v4l2_set_format,
  .get_format = MipiDsiCamV4L2Adapter::v4l2_get_format,
  .notify = MipiDsiCamV4L2Adapter::v4l2_notify,
  .set_ext_ctrl = MipiDsiCamV4L2Adapter::v4l2_set_ext_ctrl,
  .get_ext_ctrl = MipiDsiCamV4L2Adapter::v4l2_get_ext_ctrl,
  .query_ext_ctrl = MipiDsiCamV4L2Adapter::v4l2_query_ext_ctrl,
  .set_sensor_format = MipiDsiCamV4L2Adapter::v4l2_set_sensor_format,
  .get_sensor_format = MipiDsiCamV4L2Adapter::v4l2_get_sensor_format,
};

MipiDsiCamV4L2Adapter::MipiDsiCamV4L2Adapter(MipiDsiCam *camera) {
  this->context_.camera = camera;
  this->context_.video_device = nullptr;
}

MipiDsiCamV4L2Adapter::~MipiDsiCamV4L2Adapter() {
  if (this->context_.video_device) {
    this->deinit();
  }
}

esp_err_t MipiDsiCamV4L2Adapter::init() {
  if (this->context_.video_device) {
    ESP_LOGW(TAG, "V4L2 adapter already initialized");
    return ESP_OK;
  }
  
  ESP_LOGI(TAG, "🎬 Initializing V4L2 adapter...");
  
  // Dans un vrai système, appeler esp_video_create():
  // this->context_.video_device = esp_video_create(
  //   "MIPI-CSI",
  //   ESP_VIDEO_MIPI_CSI_DEVICE_ID,
  //   &s_mipi_dsi_cam_v4l2_ops,
  //   &this->context_,
  //   V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING,
  //   V4L2_CAP_DEVICE_CAPS
  // );
  
  // Pour la démo, créer une structure minimale
  this->context_.video_device = (void*)0x12345678; // Placeholder
  
  if (!this->context_.video_device) {
    ESP_LOGE(TAG, "❌ Failed to create V4L2 video device");
    return ESP_FAIL;
  }
  
  ESP_LOGI(TAG, "✅ V4L2 adapter initialized successfully");
  ESP_LOGI(TAG, "   Camera: %s", this->context_.camera->get_name().c_str());
  ESP_LOGI(TAG, "   Resolution: %ux%u", 
           this->context_.camera->get_image_width(),
           this->context_.camera->get_image_height());
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::deinit() {
  if (this->context_.video_device) {
    // Dans un vrai système: esp_video_destroy(this->context_.video_device);
    this->context_.video_device = nullptr;
    ESP_LOGI(TAG, "V4L2 adapter deinitialized");
  }
  return ESP_OK;
}

// ===== Implémentation des callbacks V4L2 =====

esp_err_t MipiDsiCamV4L2Adapter::v4l2_init(void *video) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  ESP_LOGI(TAG, "V4L2 init callback");
  ESP_LOGI(TAG, "  Resolution: %ux%u", cam->get_image_width(), cam->get_image_height());
  ESP_LOGI(TAG, "  Format: RGB565");
  
  // La caméra est déjà initialisée par ESPHome setup()
  // Ici on configure juste les buffers V4L2 si nécessaire
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_deinit(void *video) {
  ESP_LOGI(TAG, "V4L2 deinit callback");
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_start(void *video, uint32_t type) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  ESP_LOGI(TAG, "V4L2 start streaming (type=%u)", type);
  
  if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    if (!cam->is_streaming()) {
      bool success = cam->start_streaming();
      if (success) {
        ESP_LOGI(TAG, "✅ Streaming started via V4L2");
        return ESP_OK;
      } else {
        ESP_LOGE(TAG, "❌ Failed to start streaming");
        return ESP_FAIL;
      }
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_stop(void *video, uint32_t type) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  ESP_LOGI(TAG, "V4L2 stop streaming (type=%u)", type);
  
  if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    if (cam->is_streaming()) {
      bool success = cam->stop_streaming();
      if (success) {
        ESP_LOGI(TAG, "✅ Streaming stopped via V4L2");
        return ESP_OK;
      } else {
        ESP_LOGE(TAG, "❌ Failed to stop streaming");
        return ESP_FAIL;
      }
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_enum_format(void *video, uint32_t type, 
                                                   uint32_t index, uint32_t *pixel_format) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  
  if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  // Formats supportés par notre caméra
  static const uint32_t formats[] = {
    V4L2_PIX_FMT_RGB565,   // Format principal
    V4L2_PIX_FMT_SBGGR8,   // RAW8 si besoin
    V4L2_PIX_FMT_YUV422P,  // YUV422 si supporté
  };
  
  if (index >= sizeof(formats) / sizeof(formats[0])) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  *pixel_format = formats[index];
  
  ESP_LOGD(TAG, "V4L2 enum_format[%u] = 0x%08X", index, *pixel_format);
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_set_format(void *video, const struct v4l2_format *format) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  if (format->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    ESP_LOGE(TAG, "Unsupported buffer type: %u", format->type);
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  const struct v4l2_pix_format *pix = &format->fmt.pix;
  
  ESP_LOGI(TAG, "V4L2 set_format:");
  ESP_LOGI(TAG, "  Resolution: %ux%u", pix->width, pix->height);
  ESP_LOGI(TAG, "  Format: 0x%08X", pix->pixelformat);
  
  // Vérifier que le format demandé correspond au capteur
  if (pix->width != cam->get_image_width() || 
      pix->height != cam->get_image_height()) {
    ESP_LOGE(TAG, "❌ Format mismatch: requested %ux%u, camera is %ux%u",
             pix->width, pix->height, 
             cam->get_image_width(), cam->get_image_height());
    return ESP_ERR_INVALID_ARG;
  }
  
  // Vérifier le format de pixel
  if (pix->pixelformat != V4L2_PIX_FMT_RGB565 &&
      pix->pixelformat != V4L2_PIX_FMT_SBGGR8 &&
      pix->pixelformat != V4L2_PIX_FMT_YUV422P) {
    ESP_LOGE(TAG, "❌ Unsupported pixel format: 0x%08X", pix->pixelformat);
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  ESP_LOGI(TAG, "✅ Format accepted");
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_get_format(void *video, struct v4l2_format *format) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  if (format->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  struct v4l2_pix_format *pix = &format->fmt.pix;
  
  pix->width = cam->get_image_width();
  pix->height = cam->get_image_height();
  pix->pixelformat = V4L2_PIX_FMT_RGB565;
  pix->field = V4L2_FIELD_NONE;
  pix->bytesperline = cam->get_image_width() * 2; // RGB565 = 2 bytes/pixel
  pix->sizeimage = cam->get_image_size();
  pix->colorspace = V4L2_COLORSPACE_SRGB;
  
  ESP_LOGD(TAG, "V4L2 get_format: %ux%u, format=0x%08X", 
           pix->width, pix->height, pix->pixelformat);
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_notify(void *video, int event, void *arg) {
  ESP_LOGD(TAG, "V4L2 notify: event=%d", event);
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_set_ext_ctrl(void *video, 
                                                    const struct v4l2_ext_controls *ctrls) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  ESP_LOGI(TAG, "V4L2 set_ext_ctrl: %u controls", ctrls->count);
  
  for (uint32_t i = 0; i < ctrls->count; i++) {
    const struct v4l2_ext_control *ctrl = &ctrls->controls[i];
    
    switch (ctrl->id) {
      case V4L2_CID_EXPOSURE:
        ESP_LOGI(TAG, "  Setting exposure: %d", ctrl->value);
        cam->adjust_exposure(ctrl->value);
        break;
        
      case V4L2_CID_GAIN:
        ESP_LOGI(TAG, "  Setting gain: %d", ctrl->value);
        cam->adjust_gain(ctrl->value);
        break;
        
      case V4L2_CID_BRIGHTNESS:
        ESP_LOGI(TAG, "  Setting brightness: %d", ctrl->value);
        // Scale 0-255 to 0-10
        cam->set_brightness_level(ctrl->value / 25);
        break;
        
      case V4L2_CID_AUTO_EXPOSURE_BIAS:
        ESP_LOGI(TAG, "  Setting AE target: %d", ctrl->value);
        cam->set_ae_target_brightness(ctrl->value);
        break;
        
      case V4L2_CID_RED_BALANCE:
        ESP_LOGI(TAG, "  Setting red balance: %d", ctrl->value);
        // Implémenter si nécessaire
        break;
        
      case V4L2_CID_BLUE_BALANCE:
        ESP_LOGI(TAG, "  Setting blue balance: %d", ctrl->value);
        // Implémenter si nécessaire
        break;
        
      default:
        ESP_LOGW(TAG, "  Unsupported control ID: 0x%08X", ctrl->id);
        return ESP_ERR_NOT_SUPPORTED;
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_get_ext_ctrl(void *video, 
                                                    struct v4l2_ext_controls *ctrls) {
  MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
  MipiDsiCam *cam = ctx->camera;
  
  ESP_LOGI(TAG, "V4L2 get_ext_ctrl: %u controls", ctrls->count);
  
  for (uint32_t i = 0; i < ctrls->count; i++) {
    struct v4l2_ext_control *ctrl = &ctrls->controls[i];
    
    switch (ctrl->id) {
      case V4L2_CID_EXPOSURE:
        // Pas de getter dans le code actuel
        ctrl->value = 0x9C0;
        ESP_LOGD(TAG, "  Getting exposure: %d", ctrl->value);
        break;
        
      case V4L2_CID_GAIN:
        ctrl->value = 20;
        ESP_LOGD(TAG, "  Getting gain: %d", ctrl->value);
        break;
        
      default:
        ESP_LOGW(TAG, "  Unsupported control ID for get: 0x%08X", ctrl->id);
        return ESP_ERR_NOT_SUPPORTED;
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_query_ext_ctrl(void *video, 
                                                      struct v4l2_query_ext_ctrl *qctrl) {
  ESP_LOGD(TAG, "V4L2 query_ext_ctrl: id=0x%08X", qctrl->id);
  
  // Définir les contrôles supportés
  switch (qctrl->id) {
    case V4L2_CID_EXPOSURE:
      qctrl->type = V4L2_CTRL_TYPE_INTEGER;
      qctrl->minimum = 0x200;
      qctrl->maximum = 0xF00;
      qctrl->step = 0x40;
      qctrl->default_value = 0x9C0;
      qctrl->flags = 0;
      strncpy(qctrl->name, "Exposure", sizeof(qctrl->name));
      return ESP_OK;
      
    case V4L2_CID_GAIN:
      qctrl->type = V4L2_CTRL_TYPE_INTEGER;
      qctrl->minimum = 0;
      qctrl->maximum = 120;
      qctrl->step = 1;
      qctrl->default_value = 20;
      qctrl->flags = 0;
      strncpy(qctrl->name, "Gain", sizeof(qctrl->name));
      return ESP_OK;
      
    case V4L2_CID_BRIGHTNESS:
      qctrl->type = V4L2_CTRL_TYPE_INTEGER;
      qctrl->minimum = 0;
      qctrl->maximum = 255;
      qctrl->step = 25;
      qctrl->default_value = 128;
      qctrl->flags = 0;
      strncpy(qctrl->name, "Brightness", sizeof(qctrl->name));
      return ESP_OK;
      
    case V4L2_CID_AUTO_EXPOSURE_BIAS:
      qctrl->type = V4L2_CTRL_TYPE_INTEGER;
      qctrl->minimum = 64;
      qctrl->maximum = 192;
      qctrl->step = 8;
      qctrl->default_value = 128;
      qctrl->flags = 0;
      strncpy(qctrl->name, "AE Target", sizeof(qctrl->name));
      return ESP_OK;
      
    default:
      ESP_LOGW(TAG, "Unknown control ID: 0x%08X", qctrl->id);
      return ESP_ERR_NOT_SUPPORTED;
  }
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_set_sensor_format(void *video, const void *format) {
  ESP_LOGI(TAG, "V4L2 set_sensor_format");
  // Pour l'instant, la résolution est fixée au setup
  // Pourrait être étendu pour changer dynamiquement la résolution
  return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_get_sensor_format(void *video, void *format) {
  ESP_LOGI(TAG, "V4L2 get_sensor_format");
  return ESP_OK;
}

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_V4L2
