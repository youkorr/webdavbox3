#ifdef MIPI_DSI_CAM_ENABLE_JPEG
#ifdef MIPI_DSI_CAM_ENABLE_H264

#include "mipi_dsi_cam_encoders.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

namespace esphome {
namespace mipi_dsi_cam {

static const char *TAG_JPEG = "mipi_dsi_cam.jpeg";
static const char *TAG_H264 = "mipi_dsi_cam.h264";

// ========== JPEG Encoder ==========

MipiDsiCamJPEGEncoder::MipiDsiCamJPEGEncoder(MipiDsiCam *camera) 
  : camera_(camera) {
}

MipiDsiCamJPEGEncoder::~MipiDsiCamJPEGEncoder() {
  if (this->initialized_) {
    this->deinit();
  }
}

esp_err_t MipiDsiCamJPEGEncoder::init(uint8_t quality) {
  if (this->initialized_) {
    ESP_LOGW(TAG_JPEG, "JPEG encoder already initialized");
    return ESP_OK;
  }
  
  this->quality_ = quality;
  
  ESP_LOGI(TAG_JPEG, "🎨 Initializing JPEG encoder (quality=%u)...", quality);
  
  // Ouvrir le device JPEG
  this->jpeg_fd_ = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
  if (this->jpeg_fd_ < 0) {
    ESP_LOGE(TAG_JPEG, "Failed to open JPEG device");
    return ESP_FAIL;
  }
  
  // Allouer le buffer JPEG de sortie
  // Taille max: environ width * height (pour RGB565->JPEG, ratio ~1:1 au pire cas)
  this->jpeg_buffer_size_ = this->camera_->get_image_width() * 
                            this->camera_->get_image_height();
  
  this->jpeg_buffer_ = (uint8_t*)heap_caps_malloc(
    this->jpeg_buffer_size_, 
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );
  
  if (!this->jpeg_buffer_) {
    ESP_LOGE(TAG_JPEG, "Failed to allocate JPEG buffer");
    close(this->jpeg_fd_);
    this->jpeg_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }
  
  // Configurer le format d'entrée (RGB565)
  struct v4l2_format in_fmt = {};
  in_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  in_fmt.fmt.pix.width = this->camera_->get_image_width();
  in_fmt.fmt.pix.height = this->camera_->get_image_height();
  in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  in_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  
  if (ioctl(this->jpeg_fd_, VIDIOC_S_FMT, &in_fmt) < 0) {
    ESP_LOGE(TAG_JPEG, "Failed to set input format");
    heap_caps_free(this->jpeg_buffer_);
    close(this->jpeg_fd_);
    return ESP_FAIL;
  }
  
  // Configurer le format de sortie (JPEG)
  struct v4l2_format out_fmt = {};
  out_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_fmt.fmt.pix.width = this->camera_->get_image_width();
  out_fmt.fmt.pix.height = this->camera_->get_image_height();
  out_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  
  if (ioctl(this->jpeg_fd_, VIDIOC_S_FMT, &out_fmt) < 0) {
    ESP_LOGE(TAG_JPEG, "Failed to set output format");
    heap_caps_free(this->jpeg_buffer_);
    close(this->jpeg_fd_);
    return ESP_FAIL;
  }
  
  // Configurer la qualité JPEG
  struct v4l2_control ctrl = {};
  ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  ctrl.value = quality;
  
  if (ioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl) < 0) {
    ESP_LOGW(TAG_JPEG, "Failed to set JPEG quality, using default");
  }
  
  this->initialized_ = true;
  ESP_LOGI(TAG_JPEG, "✅ JPEG encoder initialized");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamJPEGEncoder::deinit() {
  if (!this->initialized_) {
    return ESP_OK;
  }
  
  if (this->jpeg_buffer_) {
    heap_caps_free(this->jpeg_buffer_);
    this->jpeg_buffer_ = nullptr;
  }
  
  if (this->jpeg_fd_ >= 0) {
    close(this->jpeg_fd_);
    this->jpeg_fd_ = -1;
  }
  
  this->initialized_ = false;
  ESP_LOGI(TAG_JPEG, "JPEG encoder deinitialized");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamJPEGEncoder::encode_frame(const uint8_t *rgb_data, 
                                               size_t rgb_size,
                                               uint8_t **jpeg_data, 
                                               size_t *jpeg_size) {
  if (!this->initialized_) {
    ESP_LOGE(TAG_JPEG, "Encoder not initialized");
    return ESP_FAIL;
  }
  
  if (!rgb_data || !jpeg_data || !jpeg_size) {
    return ESP_ERR_INVALID_ARG;
  }
  
  // Note: Implémentation simplifiée
  // Dans un vrai système, utiliser V4L2 pour l'encodage matériel
  // Pour l'instant, on retourne juste un placeholder
  
  ESP_LOGW(TAG_JPEG, "JPEG encoding not fully implemented yet");
  *jpeg_data = nullptr;
  *jpeg_size = 0;
  
  return ESP_ERR_NOT_SUPPORTED;
}

void MipiDsiCamJPEGEncoder::set_quality(uint8_t quality) {
  if (quality < 1) quality = 1;
  if (quality > 100) quality = 100;
  
  this->quality_ = quality;
  
  if (this->initialized_ && this->jpeg_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    ctrl.value = quality;
    
    if (ioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl) < 0) {
      ESP_LOGW(TAG_JPEG, "Failed to update JPEG quality");
    } else {
      ESP_LOGI(TAG_JPEG, "JPEG quality updated to %u", quality);
    }
  }
}

// ========== H264 Encoder ==========

MipiDsiCamH264Encoder::MipiDsiCamH264Encoder(MipiDsiCam *camera)
  : camera_(camera) {
}

MipiDsiCamH264Encoder::~MipiDsiCamH264Encoder() {
  if (this->initialized_) {
    this->deinit();
  }
}

esp_err_t MipiDsiCamH264Encoder::init(uint32_t bitrate, uint32_t gop_size) {
  if (this->initialized_) {
    ESP_LOGW(TAG_H264, "H264 encoder already initialized");
    return ESP_OK;
  }
  
  this->bitrate_ = bitrate;
  this->gop_size_ = gop_size;
  
  ESP_LOGI(TAG_H264, "🎬 Initializing H264 encoder (bitrate=%u, gop=%u)...", 
           bitrate, gop_size);
  
  // Ouvrir le device H264
  this->h264_fd_ = open(ESP_VIDEO_H264_DEVICE_NAME, O_RDWR);
  if (this->h264_fd_ < 0) {
    ESP_LOGE(TAG_H264, "Failed to open H264 device");
    return ESP_FAIL;
  }
  
  // Allouer le buffer H264 de sortie
  // Taille max: environ width * height / 2 pour du H264
  this->h264_buffer_size_ = (this->camera_->get_image_width() * 
                             this->camera_->get_image_height()) / 2;
  
  this->h264_buffer_ = (uint8_t*)heap_caps_malloc(
    this->h264_buffer_size_,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );
  
  if (!this->h264_buffer_) {
    ESP_LOGE(TAG_H264, "Failed to allocate H264 buffer");
    close(this->h264_fd_);
    this->h264_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }
  
  // Configurer le format d'entrée (RGB565)
  struct v4l2_format in_fmt = {};
  in_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  in_fmt.fmt.pix.width = this->camera_->get_image_width();
  in_fmt.fmt.pix.height = this->camera_->get_image_height();
  in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  in_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  
  if (ioctl(this->h264_fd_, VIDIOC_S_FMT, &in_fmt) < 0) {
    ESP_LOGE(TAG_H264, "Failed to set input format");
    heap_caps_free(this->h264_buffer_);
    close(this->h264_fd_);
    return ESP_FAIL;
  }
  
  // Configurer le format de sortie (H264)
  struct v4l2_format out_fmt = {};
  out_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_fmt.fmt.pix.width = this->camera_->get_image_width();
  out_fmt.fmt.pix.height = this->camera_->get_image_height();
  out_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  
  if (ioctl(this->h264_fd_, VIDIOC_S_FMT, &out_fmt) < 0) {
    ESP_LOGE(TAG_H264, "Failed to set output format");
    heap_caps_free(this->h264_buffer_);
    close(this->h264_fd_);
    return ESP_FAIL;
  }
  
  // Configurer le bitrate
  struct v4l2_control ctrl = {};
  ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
  ctrl.value = bitrate;
  
  if (ioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl) < 0) {
    ESP_LOGW(TAG_H264, "Failed to set bitrate");
  }
  
  // Configurer le GOP size
  ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
  ctrl.value = gop_size;
  
  if (ioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl) < 0) {
    ESP_LOGW(TAG_H264, "Failed to set GOP size");
  }
  
  this->initialized_ = true;
  ESP_LOGI(TAG_H264, "✅ H264 encoder initialized");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamH264Encoder::deinit() {
  if (!this->initialized_) {
    return ESP_OK;
  }
  
  if (this->h264_buffer_) {
    heap_caps_free(this->h264_buffer_);
    this->h264_buffer_ = nullptr;
  }
  
  if (this->h264_fd_ >= 0) {
    close(this->h264_fd_);
    this->h264_fd_ = -1;
  }
  
  this->initialized_ = false;
  ESP_LOGI(TAG_H264, "H264 encoder deinitialized");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamH264Encoder::encode_frame(const uint8_t *rgb_data,
                                               size_t rgb_size,
                                               uint8_t **h264_data,
                                               size_t *h264_size,
                                               bool *is_keyframe) {
  if (!this->initialized_) {
    ESP_LOGE(TAG_H264, "Encoder not initialized");
    return ESP_FAIL;
  }
  
  if (!rgb_data || !h264_data || !h264_size) {
    return ESP_ERR_INVALID_ARG;
  }
  
  // Déterminer si c'est une keyframe (I-frame)
  bool is_i_frame = (this->frame_count_ % this->gop_size_) == 0;
  if (is_keyframe) {
    *is_keyframe = is_i_frame;
  }
  
  this->frame_count_++;
  
  // Note: Implémentation simplifiée
  // Dans un vrai système, utiliser V4L2 pour l'encodage matériel
  ESP_LOGW(TAG_H264, "H264 encoding not fully implemented yet");
  *h264_data = nullptr;
  *h264_size = 0;
  
  return ESP_ERR_NOT_SUPPORTED;
}

void MipiDsiCamH264Encoder::set_bitrate(uint32_t bitrate) {
  this->bitrate_ = bitrate;
  
  if (this->initialized_ && this->h264_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ctrl.value = bitrate;
    
    if (ioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl) < 0) {
      ESP_LOGW(TAG_H264, "Failed to update bitrate");
    } else {
      ESP_LOGI(TAG_H264, "Bitrate updated to %u", bitrate);
    }
  }
}

void MipiDsiCamH264Encoder::set_gop_size(uint32_t gop_size) {
  this->gop_size_ = gop_size;
  
  if (this->initialized_ && this->h264_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
    ctrl.value = gop_size;
    
    if (ioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl) < 0) {
      ESP_LOGW(TAG_H264, "Failed to update GOP size");
    } else {
      ESP_LOGI(TAG_H264, "GOP size updated to %u", gop_size);
    }
  }
}

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_H264
#endif // MIPI_DSI_CAM_ENABLE_JPEG
