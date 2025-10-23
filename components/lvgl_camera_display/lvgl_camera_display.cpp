#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (Enhanced with PPA)");

  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "❌ Camera not configured");
    this->mark_failed();
    return;
  }

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Initialiser PPA si rotation/mirror est activé
  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ PPA initialization failed");
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF", 
             this->mirror_y_ ? "ON" : "OFF");
  }
#endif

  ESP_LOGI(TAG, "✅ Display initialized");
  ESP_LOGI(TAG, "   Update interval: %u ms", this->update_interval_);
  
  if (this->jpeg_output_) {
    ESP_LOGI(TAG, "   JPEG output: ENABLED");
  }
  if (this->h264_output_) {
    ESP_LOGI(TAG, "   H264 output: ENABLED");
  }
}

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LVGLCameraDisplay::init_ppa_() {
  // Configurer le client PPA pour Scale-Rotate-Mirror
  ppa_client_config_t ppa_config = {
    .oper_type = PPA_OPERATION_SRM,
    .max_pending_trans_num = 1,
  };
  
  esp_err_t ret = ppa_register_client(&ppa_config, &this->ppa_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA register failed: 0x%x", ret);
    return false;
  }
  
  // Allouer le buffer de transformation
  uint16_t width = this->camera_->get_image_width();
  uint16_t height = this->camera_->get_image_height();
  
  // Si rotation 90° ou 270°, inverser width/height
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(width, height);
  }
  
  this->transform_buffer_size_ = width * height * 2; // RGB565
  this->transform_buffer_ = (uint8_t*)heap_caps_aligned_alloc(
    64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM
  );
  
  if (!this->transform_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate transform buffer");
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
    return false;
  }
  
  ESP_LOGI(TAG, "PPA transform buffer allocated: %ux%u", width, height);
  return true;
}

void LVGLCameraDisplay::deinit_ppa_() {
  if (this->transform_buffer_) {
    heap_caps_free(this->transform_buffer_);
    this->transform_buffer_ = nullptr;
  }
  
  if (this->ppa_handle_) {
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
  }
}

bool LVGLCameraDisplay::transform_frame_(const uint8_t *src, uint8_t *dst) {
  if (!this->ppa_handle_ || !src || !dst) {
    return false;
  }
  
  uint16_t src_width = this->camera_->get_image_width();
  uint16_t src_height = this->camera_->get_image_height();
  
  uint16_t dst_width = src_width;
  uint16_t dst_height = src_height;
  
  // Ajuster dimensions si rotation 90° ou 270°
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(dst_width, dst_height);
  }
  
  // Convertir l'angle de rotation
  ppa_srm_rotation_angle_t ppa_rotation;
  switch (this->rotation_) {
    case ROTATION_0:   ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
    case ROTATION_90:  ppa_rotation = PPA_SRM_ROTATION_ANGLE_90; break;
    case ROTATION_180: ppa_rotation = PPA_SRM_ROTATION_ANGLE_180; break;
    case ROTATION_270: ppa_rotation = PPA_SRM_ROTATION_ANGLE_270; break;
    default:           ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
  }
  
  // Configurer l'opération PPA
  ppa_srm_oper_config_t srm_config = {};
  
  // Input
  srm_config.in.buffer = const_cast<uint8_t*>(src);
  srm_config.in.pic_w = src_width;
  srm_config.in.pic_h = src_height;
  srm_config.in.block_w = src_width;
  srm_config.in.block_h = src_height;
  srm_config.in.block_offset_x = 0;
  srm_config.in.block_offset_y = 0;
  srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  // Output
  srm_config.out.buffer = dst;
  srm_config.out.buffer_size = this->transform_buffer_size_;
  srm_config.out.pic_w = dst_width;
  srm_config.out.pic_h = dst_height;
  srm_config.out.block_offset_x = 0;
  srm_config.out.block_offset_y = 0;
  srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  // Transformation
  srm_config.rotation_angle = ppa_rotation;
  srm_config.scale_x = 1.0f;
  srm_config.scale_y = 1.0f;
  srm_config.mirror_x = this->mirror_x_;
  srm_config.mirror_y = this->mirror_y_;
  srm_config.rgb_swap = false;
  srm_config.byte_swap = false;
  srm_config.mode = PPA_TRANS_MODE_BLOCKING;
  
  // Exécuter la transformation
  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transformation failed: 0x%x", ret);
    return false;
  }
  
  return true;
}
#endif

void LVGLCameraDisplay::loop() {
  if (!this->camera_->is_streaming()) {
    return;
  }
  
  // Capturer la frame
  if (this->camera_->capture_frame()) {
    this->update_canvas_();
    this->frame_count_++;

    // Logger FPS réel toutes les 100 frames
    if (this->frame_count_ % 100 == 0) {
      uint32_t now_time = millis();

      if (this->last_fps_time_ > 0) {
        float elapsed = (now_time - this->last_fps_time_) / 1000.0f;
        float fps = 100.0f / elapsed;
        ESP_LOGI(TAG, "🎞️  Display FPS: %.2f | %u frames total", fps, this->frame_count_);
      }
      this->last_fps_time_ = now_time;
    }
  }
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Mode: Event-driven (zero-copy)");
  ESP_LOGCONFIG(TAG, "  Canvas: %s", this->canvas_obj_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  JPEG Output: %s", this->jpeg_output_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  H264 Output: %s", this->h264_output_ ? "YES" : "NO");
}

void LVGLCameraDisplay::update_canvas_() {
  if (this->camera_ == nullptr || this->canvas_obj_ == nullptr) {
    if (!this->canvas_warning_shown_) {
      ESP_LOGW(TAG, "❌ Canvas null");
      this->canvas_warning_shown_ = true;
    }
    return;
  }

  uint8_t* img_data = this->camera_->get_image_data();
  uint16_t width = this->camera_->get_image_width();
  uint16_t height = this->camera_->get_image_height();

  if (img_data == nullptr) {
    return;
  }

  if (this->first_update_) {
    ESP_LOGI(TAG, "🖼️  First canvas update:");
    ESP_LOGI(TAG, "   Dimensions: %ux%u", width, height);
    ESP_LOGI(TAG, "   Buffer: %p", img_data);
    this->first_update_ = false;
  }

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Si transformation requise (rotation/mirror)
  if (this->ppa_handle_ && this->transform_buffer_) {
    if (this->transform_frame_(img_data, this->transform_buffer_)) {
      img_data = this->transform_buffer_;
      
      // Ajuster dimensions si rotation 90° ou 270°
      if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
        std::swap(width, height);
      }
    }
  }
#endif

  // Mettre à jour le buffer du canvas
  if (this->last_buffer_ptr_ != img_data) {
    // Compatibilité LVGL v8 et v9
    #if LV_VERSION_CHECK(9, 0, 0)
      lv_canvas_set_buffer(this->canvas_obj_, img_data, width, height, 
                           LV_COLOR_FORMAT_RGB565);
    #else
      // LVGL v8 utilise LV_IMG_CF_TRUE_COLOR pour RGB565
      lv_canvas_set_buffer(this->canvas_obj_, img_data, width, height, 
                           LV_IMG_CF_TRUE_COLOR);
    #endif
    this->last_buffer_ptr_ = img_data;
  }
  
  // Invalider pour forcer le rafraîchissement
  lv_obj_invalidate(this->canvas_obj_);
}

void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) { 
  this->canvas_obj_ = canvas;
  ESP_LOGI(TAG, "🎨 Canvas configured: %p", canvas);

  if (canvas != nullptr) {
    lv_coord_t w = lv_obj_get_width(canvas);
    lv_coord_t h = lv_obj_get_height(canvas);
    ESP_LOGI(TAG, "   Canvas size: %dx%d", w, h);
    
    // Désactiver le scrolling pour réduire la latence
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
  }
}

}  // namespace lvgl_camera_display
}  // namespace esphome
