#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
extern "C" {
  #include "driver/ppa.h"
}
#endif

namespace esphome {
namespace lvgl_camera_display {

enum RotationAngle {
  ROTATION_0 = 0,
  ROTATION_90 = 90,
  ROTATION_180 = 180,
  ROTATION_270 = 270,
};

class LVGLCameraDisplay : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_camera(mipi_dsi_cam::MipiDsiCam *camera) { this->camera_ = camera; }
  void set_canvas_id(const std::string &canvas_id) { this->canvas_id_ = canvas_id; }
  void set_update_interval(uint32_t interval_ms) { 
    this->update_interval_ = interval_ms; 
  }
  
  // Nouvelles méthodes pour rotation et mirror
  void set_rotation(RotationAngle rotation) { this->rotation_ = rotation; }
  void set_mirror_x(bool enable) { this->mirror_x_ = enable; }
  void set_mirror_y(bool enable) { this->mirror_y_ = enable; }
  
  // Méthodes pour les encodeurs
  void set_jpeg_output(bool enable) { this->jpeg_output_ = enable; }
  void set_h264_output(bool enable) { this->h264_output_ = enable; }

  void configure_canvas(lv_obj_t *canvas);

  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  mipi_dsi_cam::MipiDsiCam *camera_{nullptr};
  lv_obj_t *canvas_obj_{nullptr};
  std::string canvas_id_{};

  uint32_t update_interval_{33};
  uint32_t last_update_{0};

  uint32_t frame_count_{0};
  bool first_update_{true};
  bool canvas_warning_shown_{false};

  uint32_t last_fps_time_{0};
  
  // Rotation et mirror
  RotationAngle rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  
  // Encodage
  bool jpeg_output_{false};
  bool h264_output_{false};
  
  // Suivi du pointeur de buffer
  uint8_t* last_buffer_ptr_{nullptr};
  
#ifdef USE_ESP32_VARIANT_ESP32P4
  // PPA pour rotation/mirror/scale
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};
  
  bool init_ppa_();
  void deinit_ppa_();
  bool transform_frame_(const uint8_t *src, uint8_t *dst);
#endif

  void update_canvas_();
};

}  // namespace lvgl_camera_display
}  // namespace esphome
