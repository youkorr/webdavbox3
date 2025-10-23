#pragma once

#ifdef MIPI_DSI_CAM_ENABLE_JPEG
#ifdef MIPI_DSI_CAM_ENABLE_H264

#include "mipi_dsi_cam.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

extern "C" {
  #include "esp_video_device.h"
  #include "esp_video_buffer.h"
  #include <fcntl.h>
  #include <sys/ioctl.h>
}

namespace esphome {
namespace mipi_dsi_cam {

/**
 * @brief Encodeur JPEG pour mipi_dsi_cam
 */
class MipiDsiCamJPEGEncoder {
 public:
  MipiDsiCamJPEGEncoder(MipiDsiCam *camera);
  ~MipiDsiCamJPEGEncoder();
  
  esp_err_t init(uint8_t quality = 10);
  esp_err_t deinit();
  
  esp_err_t encode_frame(const uint8_t *rgb_data, size_t rgb_size, 
                         uint8_t **jpeg_data, size_t *jpeg_size);
  
  void set_quality(uint8_t quality);
  uint8_t get_quality() const { return quality_; }
  
 protected:
  MipiDsiCam *camera_;
  int jpeg_fd_{-1};
  uint8_t quality_{10};
  bool initialized_{false};
  
  uint8_t *jpeg_buffer_{nullptr};
  size_t jpeg_buffer_size_{0};
};

/**
 * @brief Encodeur H264 pour mipi_dsi_cam
 */
class MipiDsiCamH264Encoder {
 public:
  MipiDsiCamH264Encoder(MipiDsiCam *camera);
  ~MipiDsiCamH264Encoder();
  
  esp_err_t init(uint32_t bitrate = 2000000, uint32_t gop_size = 30);
  esp_err_t deinit();
  
  esp_err_t encode_frame(const uint8_t *rgb_data, size_t rgb_size,
                         uint8_t **h264_data, size_t *h264_size,
                         bool *is_keyframe);
  
  void set_bitrate(uint32_t bitrate);
  void set_gop_size(uint32_t gop_size);
  
  uint32_t get_bitrate() const { return bitrate_; }
  uint32_t get_gop_size() const { return gop_size_; }
  
 protected:
  MipiDsiCam *camera_;
  int h264_fd_{-1};
  uint32_t bitrate_{2000000};
  uint32_t gop_size_{30};
  uint32_t frame_count_{0};
  bool initialized_{false};
  
  uint8_t *h264_buffer_{nullptr};
  size_t h264_buffer_size_{0};
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_H264
#endif // MIPI_DSI_CAM_ENABLE_JPEG
