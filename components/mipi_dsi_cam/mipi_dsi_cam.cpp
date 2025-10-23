#include "mipi_dsi_cam.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "mipi_dsi_cam_drivers_generated.h"

#ifdef MIPI_DSI_CAM_ENABLE_V4L2
#include "mipi_dsi_cam_v4l2_adapter.h"
#endif

#ifdef MIPI_DSI_CAM_ENABLE_ISP_PIPELINE
#include "mipi_dsi_cam_isp_pipeline.h"
#endif

#ifdef USE_ESP32_VARIANT_ESP32P4

#include "driver/ledc.h"

namespace esphome {
namespace mipi_dsi_cam {

static const char *const TAG = "mipi_dsi_cam";

void MipiDsiCam::setup() {
  ESP_LOGI(TAG, "Init MIPI Camera");
  ESP_LOGI(TAG, "  Sensor type: %s", this->sensor_type_.c_str());
  
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(20);
  }
  
  if (!this->create_sensor_driver_()) {
    ESP_LOGE(TAG, "Driver creation failed");
    this->mark_failed();
    return;
  }
  
  if (!this->init_sensor_()) {
    ESP_LOGE(TAG, "Sensor init failed");
    this->mark_failed();
    return;
  }
  
  if (this->has_external_clock()) {
    if (!this->init_external_clock_()) {
      ESP_LOGE(TAG, "External clock init failed");
      this->mark_failed();
      return;
    }
  } else {
    ESP_LOGI(TAG, "No external clock configured - sensor must use internal clock");
  }
  
  if (!this->init_ldo_()) {
    ESP_LOGE(TAG, "LDO init failed");
    this->mark_failed();
    return;
  }
  
  if (!this->init_csi_()) {
    ESP_LOGE(TAG, "CSI init failed");
    this->mark_failed();
    return;
  }
  
  if (!this->init_isp_()) {
    ESP_LOGE(TAG, "ISP init failed");
    this->mark_failed();
    return;
  }
  
  if (!this->allocate_buffer_()) {
    ESP_LOGE(TAG, "Buffer alloc failed");
    this->mark_failed();
    return;
  }
  
  this->initialized_ = true;
  ESP_LOGI(TAG, "Camera ready (%ux%u) with Auto Exposure", this->width_, this->height_);
}

bool MipiDsiCam::create_sensor_driver_() {
  ESP_LOGI(TAG, "Creating driver for: %s", this->sensor_type_.c_str());
  
  this->sensor_driver_ = create_sensor_driver(this->sensor_type_, this);
  
  if (this->sensor_driver_ == nullptr) {
    ESP_LOGE(TAG, "Unknown or unavailable sensor: %s", this->sensor_type_.c_str());
    return false;
  }
  
  ESP_LOGI(TAG, "Driver created for: %s", this->sensor_driver_->get_name());
  return true;
}

bool MipiDsiCam::init_sensor_() {
  if (!this->sensor_driver_) {
    ESP_LOGE(TAG, "No sensor driver");
    return false;
  }
  
  ESP_LOGI(TAG, "Init sensor: %s", this->sensor_driver_->get_name());
  
  this->width_ = this->sensor_driver_->get_width();
  this->height_ = this->sensor_driver_->get_height();
  this->lane_count_ = this->sensor_driver_->get_lane_count();
  this->bayer_pattern_ = this->sensor_driver_->get_bayer_pattern();
  this->lane_bitrate_mbps_ = this->sensor_driver_->get_lane_bitrate_mbps();
  
  ESP_LOGI(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "  Lanes: %u", this->lane_count_);
  ESP_LOGI(TAG, "  Bayer: %u", this->bayer_pattern_);
  ESP_LOGI(TAG, "  Bitrate: %u Mbps", this->lane_bitrate_mbps_);
  
  uint16_t pid = 0;
  esp_err_t ret = this->sensor_driver_->read_id(&pid);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read sensor ID");
    return false;
  }
  
  if (pid != this->sensor_driver_->get_pid()) {
    ESP_LOGE(TAG, "Wrong PID: 0x%04X (expected 0x%04X)", 
             pid, this->sensor_driver_->get_pid());
    return false;
  }
  
  ESP_LOGI(TAG, "Sensor ID: 0x%04X", pid);
  
  ret = this->sensor_driver_->init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Sensor init failed: %d", ret);
    return false;
  }
  
  ESP_LOGI(TAG, "Sensor initialized");
  
  delay(200);
  ESP_LOGI(TAG, "Sensor stabilized");
  
  return true;
}

bool MipiDsiCam::init_external_clock_() {
  ESP_LOGI(TAG, "Init external clock on GPIO%d @ %u Hz", 
           this->external_clock_pin_, this->external_clock_frequency_);
  
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_1_BIT;
  ledc_timer.timer_num = LEDC_TIMER_0;
  ledc_timer.freq_hz = this->external_clock_frequency_;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  
  esp_err_t ret = ledc_timer_config(&ledc_timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "LEDC timer config failed: %d", ret);
    return false;
  }
  
  ledc_channel_config_t ledc_channel = {};
  ledc_channel.gpio_num = this->external_clock_pin_;
  ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel = LEDC_CHANNEL_0;
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.timer_sel = LEDC_TIMER_0;
  ledc_channel.duty = 1;
  ledc_channel.hpoint = 0;
  
  ret = ledc_channel_config(&ledc_channel);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "LEDC channel config failed: %d", ret);
    return false;
  }
  
  ESP_LOGI(TAG, "External clock initialized");
  return true;
}

bool MipiDsiCam::init_ldo_() {
  ESP_LOGI(TAG, "Init LDO MIPI");
  
  esp_ldo_channel_config_t ldo_config = {
    .chan_id = 3,
    .voltage_mv = 2500,
  };
  
  esp_err_t ret = esp_ldo_acquire_channel(&ldo_config, &this->ldo_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "LDO failed: %d", ret);
    return false;
  }
  
  ESP_LOGI(TAG, "LDO OK (2.5V)");
  return true;
}

bool MipiDsiCam::init_csi_() {
  ESP_LOGI(TAG, "Init MIPI-CSI");
  
  esp_cam_ctlr_csi_config_t csi_config = {};
  csi_config.ctlr_id = 0;
  csi_config.clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT;
  csi_config.h_res = this->width_;
  csi_config.v_res = this->height_;
  csi_config.lane_bit_rate_mbps = this->lane_bitrate_mbps_;
  csi_config.input_data_color_type = CAM_CTLR_COLOR_RAW8;
  csi_config.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  csi_config.data_lane_num = this->lane_count_;
  csi_config.byte_swap_en = false;
  csi_config.queue_items = 10;
  
  esp_err_t ret = esp_cam_new_csi_ctlr(&csi_config, &this->csi_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "CSI failed: %d", ret);
    return false;
  }
  
  esp_cam_ctlr_evt_cbs_t callbacks = {
    .on_get_new_trans = MipiDsiCam::on_csi_new_frame_,
    .on_trans_finished = MipiDsiCam::on_csi_frame_done_,
  };
  
  ret = esp_cam_ctlr_register_event_callbacks(this->csi_handle_, &callbacks, this);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Callbacks failed: %d", ret);
    return false;
  }
  
  ret = esp_cam_ctlr_enable(this->csi_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Enable CSI failed: %d", ret);
    return false;
  }
  
  ESP_LOGI(TAG, "CSI OK");
  return true;
}

bool MipiDsiCam::init_isp_() {
  ESP_LOGI(TAG, "Init ISP");
  
  uint32_t isp_clock_hz = 120000000;
  
  esp_isp_processor_cfg_t isp_config = {};
  isp_config.clk_src = ISP_CLK_SRC_DEFAULT;
  isp_config.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
  isp_config.input_data_color_type = ISP_COLOR_RAW8;
  isp_config.output_data_color_type = ISP_COLOR_RGB565;
  isp_config.h_res = this->width_;
  isp_config.v_res = this->height_;
  isp_config.has_line_start_packet = false;
  isp_config.has_line_end_packet = false;
  isp_config.clk_hz = isp_clock_hz;
  isp_config.bayer_order = (color_raw_element_order_t)this->bayer_pattern_;
  
  esp_err_t ret = esp_isp_new_processor(&isp_config, &this->isp_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ISP creation failed: 0x%x", ret);
    return false;
  }
  
  ret = esp_isp_enable(this->isp_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ISP enable failed: 0x%x", ret);
    esp_isp_del_processor(this->isp_handle_);
    this->isp_handle_ = nullptr;
    return false;
  }
  
  // Configure AWB si supporté par le capteur
  this->configure_white_balance_();
  
  ESP_LOGI(TAG, "ISP OK");
  return true;
}

void MipiDsiCam::configure_white_balance_() {
  if (!this->isp_handle_) return;
  
  // OV5647 et SC202CS ont des problèmes avec AWB matériel sur ESP32-P4
  if (this->sensor_type_ == "ov5647" || this->sensor_type_ == "sc202cs") {
    ESP_LOGI(TAG, "%s détecté - AWB matériel désactivé (correction logicielle uniquement)", 
             this->sensor_type_.c_str());
    return;
  }
  
  // Tentative de configuration AWB pour les autres capteurs
  esp_isp_awb_config_t awb_config = {};
  awb_config.sample_point = ISP_AWB_SAMPLE_POINT_AFTER_CCM;
  
  // Configuration de la window (noms corrects pour ESP32-P4)
  awb_config.window.top_left.x = this->width_ / 4;
  awb_config.window.top_left.y = this->height_ / 4;
  awb_config.window.btm_right.x = (this->width_ * 3) / 4;
  awb_config.window.btm_right.y = (this->height_ * 3) / 4;
  
  esp_err_t ret = esp_isp_new_awb_controller(this->isp_handle_, &awb_config, &this->awb_ctlr_);
  
  if (ret == ESP_OK && this->awb_ctlr_ != nullptr) {
    esp_isp_awb_controller_enable(this->awb_ctlr_);
    ESP_LOGI(TAG, "✅ AWB matériel activé (auto-correction couleurs)");
  } else {
    ESP_LOGW(TAG, "AWB matériel non disponible (0x%x), utilisation ISP par défaut", ret);
  }
}

bool MipiDsiCam::allocate_buffer_() {
  this->frame_buffer_size_ = this->width_ * this->height_ * 2;
  
  this->frame_buffers_[0] = (uint8_t*)heap_caps_aligned_alloc(
    64, this->frame_buffer_size_, MALLOC_CAP_SPIRAM
  );
  
  this->frame_buffers_[1] = (uint8_t*)heap_caps_aligned_alloc(
    64, this->frame_buffer_size_, MALLOC_CAP_SPIRAM
  );
  
  if (!this->frame_buffers_[0] || !this->frame_buffers_[1]) {
    ESP_LOGE(TAG, "Buffer alloc failed");
    return false;
  }
  
  this->current_frame_buffer_ = this->frame_buffers_[0];
  
  ESP_LOGI(TAG, "Buffers: 2x%u bytes", this->frame_buffer_size_);
  return true;
}

bool IRAM_ATTR MipiDsiCam::on_csi_new_frame_(
  esp_cam_ctlr_handle_t handle,
  esp_cam_ctlr_trans_t *trans,
  void *user_data
) {
  MipiDsiCam *cam = (MipiDsiCam*)user_data;
  trans->buffer = cam->frame_buffers_[cam->buffer_index_];
  trans->buflen = cam->frame_buffer_size_;
  return false;
}

bool IRAM_ATTR MipiDsiCam::on_csi_frame_done_(
  esp_cam_ctlr_handle_t handle,
  esp_cam_ctlr_trans_t *trans,
  void *user_data
) {
  MipiDsiCam *cam = (MipiDsiCam*)user_data;
  
  if (trans->received_size > 0) {
    cam->frame_ready_ = true;
    cam->buffer_index_ = (cam->buffer_index_ + 1) % 2;
    cam->total_frames_received_++;
  }
  
  return false;
}

bool MipiDsiCam::start_streaming() {
  if (!this->initialized_ || this->streaming_) {
    return false;
  }
  
  ESP_LOGI(TAG, "Start streaming");
  
  this->total_frames_received_ = 0;
  this->last_frame_log_time_ = millis();
  
  if (this->sensor_driver_) {
    esp_err_t ret = this->sensor_driver_->start_stream();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Sensor start failed: %d", ret);
      return false;
    }
    delay(100);
  }
  
  esp_err_t ret = esp_cam_ctlr_start(this->csi_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "CSI start failed: %d", ret);
    return false;
  }
  
  this->streaming_ = true;
  ESP_LOGI(TAG, "Streaming active avec Auto Exposure");
  return true;
}

bool MipiDsiCam::stop_streaming() {
  if (!this->streaming_) {
    return true;
  }
  
  esp_cam_ctlr_stop(this->csi_handle_);
  
  if (this->sensor_driver_) {
    this->sensor_driver_->stop_stream();
  }
  
  this->streaming_ = false;
  ESP_LOGI(TAG, "Streaming stopped");
  return true;
}

bool MipiDsiCam::capture_frame() {
  if (!this->streaming_) {
    return false;
  }
  
  bool was_ready = this->frame_ready_;
  if (was_ready) {
    this->frame_ready_ = false;
    uint8_t last_buffer = (this->buffer_index_ + 1) % 2;
    this->current_frame_buffer_ = this->frame_buffers_[last_buffer];
  }
  
  return was_ready;
}

void MipiDsiCam::update_auto_exposure_() {
  if (!this->auto_exposure_enabled_ || !this->sensor_driver_) {
    return;
  }
  
  uint32_t now = millis();
  if (now - this->last_ae_update_ < 100) {
    return;
  }
  this->last_ae_update_ = now;
  
  uint32_t avg_brightness = this->calculate_brightness_();
  
  int32_t brightness_error = (int32_t)this->ae_target_brightness_ - (int32_t)avg_brightness;
  
  if (abs(brightness_error) > 10) {
    
    if (brightness_error > 0) {
      // Image trop sombre
      if (this->current_exposure_ < 0xF00) {
        this->current_exposure_ += 0x40;
      } else if (this->current_gain_index_ < 120) {
        this->current_gain_index_ += 2;
      }
    } else {
      // Image trop lumineuse
      if (this->current_exposure_ > 0x200) {
        this->current_exposure_ -= 0x40;
      } else if (this->current_gain_index_ > 0) {
        this->current_gain_index_ -= 2;
      }
    }
    
    this->sensor_driver_->set_exposure(this->current_exposure_);
    this->sensor_driver_->set_gain(this->current_gain_index_);
    
    ESP_LOGV(TAG, "🔆 AE: brightness=%u target=%u → exp=0x%04X gain=%u",
             avg_brightness, this->ae_target_brightness_,
             this->current_exposure_, this->current_gain_index_);
  }
}

uint32_t MipiDsiCam::calculate_brightness_() {
  if (!this->current_frame_buffer_) {
    return 128;
  }
  
  uint32_t sum = 0;
  uint32_t sample_count = 0;
  
  uint32_t center_offset = (this->height_ / 2) * this->width_ * 2 + (this->width_ / 2) * 2;
  
  for (int i = 0; i < 100; i++) {
    uint32_t offset = center_offset + (i * 200);
    if (offset + 1 < this->frame_buffer_size_) {
      uint16_t pixel = (this->current_frame_buffer_[offset + 1] << 8) | 
                       this->current_frame_buffer_[offset];
      
      uint8_t r = (pixel >> 11) & 0x1F;
      uint8_t g = (pixel >> 5) & 0x3F;
      uint8_t b = pixel & 0x1F;
      
      uint32_t brightness = (r * 8 * 299 + g * 4 * 587 + b * 8 * 114) / 1000;
      
      sum += brightness;
      sample_count++;
    }
  }
  
  return sample_count > 0 ? (sum / sample_count) : 128;
}

void MipiDsiCam::loop() {
  if (this->streaming_) {
    // Mise à jour Auto Exposure
    this->update_auto_exposure_();
    
    static uint32_t ready_count = 0;
    static uint32_t not_ready_count = 0;
    
    if (this->frame_ready_) {
      ready_count++;
    } else {
      not_ready_count++;
    }
    
    uint32_t now = millis();
    if (now - this->last_frame_log_time_ >= 3000) {
      float sensor_fps = this->total_frames_received_ / 3.0f;
      float ready_rate = (float)ready_count / (float)(ready_count + not_ready_count) * 100.0f;
      
      ESP_LOGI(TAG, "📸 FPS: %.1f | frame_ready: %.1f%% | exp:0x%04X gain:%u", 
               sensor_fps, ready_rate, this->current_exposure_, this->current_gain_index_);
      
      this->total_frames_received_ = 0;
      this->last_frame_log_time_ = now;
      ready_count = 0;
      not_ready_count = 0;
    }
  }
}

void MipiDsiCam::dump_config() {
  ESP_LOGCONFIG(TAG, "MIPI Camera:");
  if (this->sensor_driver_) {
    ESP_LOGCONFIG(TAG, "  Sensor: %s", this->sensor_driver_->get_name());
    ESP_LOGCONFIG(TAG, "  PID: 0x%04X", this->sensor_driver_->get_pid());
  } else {
    ESP_LOGCONFIG(TAG, "  Sensor: %s (driver not loaded)", this->sensor_type_.c_str());
  }
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Format: RGB565");
  ESP_LOGCONFIG(TAG, "  Lanes: %u", this->lane_count_);
  ESP_LOGCONFIG(TAG, "  Bayer: %u", this->bayer_pattern_);
  
  if (this->has_external_clock()) {
    ESP_LOGCONFIG(TAG, "  External Clock: GPIO%d @ %u Hz", 
                  this->external_clock_pin_, this->external_clock_frequency_);
  } else {
    ESP_LOGCONFIG(TAG, "  External Clock: None (using internal clock)");
  }
  
  ESP_LOGCONFIG(TAG, "  Auto Exposure: %s", this->auto_exposure_enabled_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  AE Target: %u", this->ae_target_brightness_);
  ESP_LOGCONFIG(TAG, "  Streaming: %s", this->streaming_ ? "YES" : "NO");
}

// Méthodes publiques pour contrôle
void MipiDsiCam::set_auto_exposure(bool enabled) {
  this->auto_exposure_enabled_ = enabled;
  ESP_LOGI(TAG, "Auto Exposure: %s", enabled ? "ENABLED" : "DISABLED");
}

void MipiDsiCam::set_ae_target_brightness(uint8_t target) {
  this->ae_target_brightness_ = target;
  ESP_LOGI(TAG, "AE target brightness: %u", target);
}

void MipiDsiCam::set_manual_exposure(uint16_t exposure) {
  this->current_exposure_ = exposure;
  if (this->sensor_driver_) {
    this->sensor_driver_->set_exposure(exposure);
    ESP_LOGI(TAG, "Manual exposure: 0x%04X", exposure);
  }
}

void MipiDsiCam::set_manual_gain(uint8_t gain_index) {
  this->current_gain_index_ = gain_index;
  if (this->sensor_driver_) {
    this->sensor_driver_->set_gain(gain_index);
    ESP_LOGI(TAG, "Manual gain: %u", gain_index);
  }
}

void MipiDsiCam::set_white_balance_gains(float red, float green, float blue) {
  this->wb_red_gain_ = red;
  this->wb_green_gain_ = green;
  this->wb_blue_gain_ = blue;
  
  // Pour les capteurs supportant la balance des blancs au niveau registre
  if (this->sensor_driver_ && (this->sensor_type_ == "sc202cs" || this->sensor_type_ == "sc2336")) {
    // Appeler la méthode du driver si disponible
    ESP_LOGI(TAG, "WB gains: R=%.2f G=%.2f B=%.2f (logiciel uniquement)", red, green, blue);
  } else {
    ESP_LOGI(TAG, "WB gains: R=%.2f G=%.2f B=%.2f", red, green, blue);
  }
}

void MipiDsiCam::adjust_exposure(uint16_t exposure_value) {
  if (!this->sensor_driver_) {
    ESP_LOGE(TAG, "No sensor driver");
    return;
  }
  
  ESP_LOGI(TAG, "Adjusting exposure to: 0x%04X", exposure_value);
  esp_err_t ret = this->sensor_driver_->set_exposure(exposure_value);
  
  if (ret == ESP_OK) {
    this->current_exposure_ = exposure_value;
    ESP_LOGI(TAG, "✅ Exposure adjusted successfully");
  } else {
    ESP_LOGE(TAG, "❌ Failed to adjust exposure");
  }
}

void MipiDsiCam::adjust_gain(uint8_t gain_index) {
  if (!this->sensor_driver_) {
    ESP_LOGE(TAG, "No sensor driver");
    return;
  }
  
  ESP_LOGI(TAG, "Adjusting gain to index: %u", gain_index);
  esp_err_t ret = this->sensor_driver_->set_gain(gain_index);
  
  if (ret == ESP_OK) {
    this->current_gain_index_ = gain_index;
    ESP_LOGI(TAG, "✅ Gain adjusted successfully");
  } else {
    ESP_LOGE(TAG, "❌ Failed to adjust gain");
  }
}

void MipiDsiCam::set_brightness_level(uint8_t level) {
  if (level > 10) level = 10;
  
  uint16_t exposure = 0x400 + (level * 0x0B0);
  uint8_t gain = level * 6;
  
  ESP_LOGI(TAG, "🔆 Setting brightness level %u: exposure=0x%04X, gain=%u", 
           level, exposure, gain);
  
  adjust_exposure(exposure);
  delay(50);
  adjust_gain(gain);
}

void MipiDsiCam::enable_v4l2_adapter() {
#ifdef MIPI_DSI_CAM_ENABLE_V4L2
  if (this->v4l2_adapter_ != nullptr) {
    ESP_LOGW(TAG, "V4L2 adapter already enabled");
    return;
  }
  
  ESP_LOGI(TAG, "Enabling V4L2 adapter...");
  this->v4l2_adapter_ = new MipiDsiCamV4L2Adapter(this);
  
  esp_err_t ret = this->v4l2_adapter_->init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize V4L2 adapter: 0x%x", ret);
    delete this->v4l2_adapter_;
    this->v4l2_adapter_ = nullptr;
  } else {
    ESP_LOGI(TAG, "✅ V4L2 adapter enabled");
  }
#else
  ESP_LOGW(TAG, "V4L2 adapter not compiled in. Enable enable_v4l2: true in your configuration.");
  // Ne rien faire avec v4l2_adapter_ car c'est un void* dans ce cas
#endif
}

void MipiDsiCam::enable_isp_pipeline() {
#ifdef MIPI_DSI_CAM_ENABLE_ISP_PIPELINE
  if (this->isp_pipeline_ != nullptr) {
    ESP_LOGW(TAG, "ISP pipeline already enabled");
    return;
  }
  
  ESP_LOGI(TAG, "Enabling ISP pipeline...");
  this->isp_pipeline_ = new MipiDsiCamISPPipeline(this);
  
  esp_err_t ret = this->isp_pipeline_->init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize ISP pipeline: 0x%x", ret);
    delete this->isp_pipeline_;
    this->isp_pipeline_ = nullptr;
  } else {
    ESP_LOGI(TAG, "✅ ISP pipeline enabled");
  }
#else
  ESP_LOGW(TAG, "ISP pipeline not compiled in. Enable enable_isp_pipeline: true in your configuration.");
  // Ne rien faire avec isp_pipeline_ car c'est un void* dans ce cas
#endif
}

}  // namespace mipi_dsi_cam
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32P4
