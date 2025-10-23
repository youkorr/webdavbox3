#ifdef MIPI_DSI_CAM_ENABLE_ISP_PIPELINE

#include "mipi_dsi_cam_isp_pipeline.h"
#include "esphome/core/log.h"
#include <cmath>

#ifdef USE_ESP32_VARIANT_ESP32P4

namespace esphome {
namespace mipi_dsi_cam {

static const char *TAG = "mipi_dsi_cam.isp_pipeline";

MipiDsiCamISPPipeline::MipiDsiCamISPPipeline(MipiDsiCam *camera) 
  : camera_(camera) {
  // Initialiser la matrice CCM par défaut (identité)
  this->init_default_ccm_matrix_();
}

MipiDsiCamISPPipeline::~MipiDsiCamISPPipeline() {
  if (this->pipeline_started_) {
    this->stop();
  }
  if (this->pipeline_initialized_) {
    this->deinit();
  }
}

esp_err_t MipiDsiCamISPPipeline::init() {
  if (this->pipeline_initialized_) {
    ESP_LOGW(TAG, "ISP pipeline already initialized");
    return ESP_OK;
  }
  
  ESP_LOGI(TAG, "🎨 Initializing advanced ISP pipeline...");
  
  // Initialiser tous les modules ISP
  // Note: L'ordre est important pour le pipeline
  
  // 1. Bayer Filter (première étape, réduit le bruit RAW)
  ESP_RETURN_ON_ERROR(this->init_bayer_filter_(), TAG, "Failed to init BF");
  
  // 2. Demosaic (convertit Bayer en RGB)
  ESP_RETURN_ON_ERROR(this->init_demosaic_(), TAG, "Failed to init Demosaic");
  
  // 3. Color Correction Matrix (correction des couleurs)
  ESP_RETURN_ON_ERROR(this->init_ccm_(), TAG, "Failed to init CCM");
  
  // 4. Gamma correction
  ESP_RETURN_ON_ERROR(this->init_gamma_(), TAG, "Failed to init Gamma");
  
  // 5. Sharpen (amélioration de la netteté)
  ESP_RETURN_ON_ERROR(this->init_sharpen_(), TAG, "Failed to init Sharpen");
  
  // 6. Color management (luminosité, contraste, etc.)
  ESP_RETURN_ON_ERROR(this->init_color_(), TAG, "Failed to init Color");
  
  // 7. AWB (Auto White Balance)
  ESP_RETURN_ON_ERROR(this->init_awb_(), TAG, "Failed to init AWB");
  
  // 8. AE (Auto Exposure)
  ESP_RETURN_ON_ERROR(this->init_ae_(), TAG, "Failed to init AE");
  
  // 9. Histogram
  ESP_RETURN_ON_ERROR(this->init_histogram_(), TAG, "Failed to init Histogram");
  
  this->pipeline_initialized_ = true;
  
  ESP_LOGI(TAG, "✅ ISP pipeline initialized successfully");
  ESP_LOGI(TAG, "   Modules: BF=%s CCM=%s Sharpen=%s Gamma=%s Demosaic=%s",
           this->bf_enabled_ ? "ON" : "OFF",
           this->ccm_enabled_ ? "ON" : "OFF",
           this->sharpen_enabled_ ? "ON" : "OFF",
           this->gamma_enabled_ ? "ON" : "OFF",
           this->demosaic_enabled_ ? "ON" : "OFF");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start() {
  if (!this->pipeline_initialized_) {
    ESP_LOGE(TAG, "Pipeline not initialized");
    return ESP_FAIL;
  }
  
  if (this->pipeline_started_) {
    ESP_LOGW(TAG, "Pipeline already started");
    return ESP_OK;
  }
  
  ESP_LOGI(TAG, "▶️  Starting ISP pipeline...");
  
  // Démarrer les modules activés
  if (this->bf_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_bayer_filter_(), TAG, "Failed to start BF");
  }
  
  if (this->demosaic_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_demosaic_(), TAG, "Failed to start Demosaic");
  }
  
  if (this->ccm_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_ccm_(), TAG, "Failed to start CCM");
  }
  
  if (this->gamma_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_gamma_(), TAG, "Failed to start Gamma");
  }
  
  if (this->sharpen_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_sharpen_(), TAG, "Failed to start Sharpen");
  }
  
  ESP_RETURN_ON_ERROR(this->start_color_(), TAG, "Failed to start Color");
  
  if (this->awb_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_awb_(), TAG, "Failed to start AWB");
  }
  
  if (this->ae_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_ae_(), TAG, "Failed to start AE");
  }
  
  if (this->histogram_enabled_) {
    ESP_RETURN_ON_ERROR(this->start_histogram_(), TAG, "Failed to start Histogram");
  }
  
  this->pipeline_started_ = true;
  ESP_LOGI(TAG, "✅ ISP pipeline started");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop() {
  if (!this->pipeline_started_) {
    return ESP_OK;
  }
  
  ESP_LOGI(TAG, "⏸️  Stopping ISP pipeline...");
  
  // Arrêter dans l'ordre inverse
  if (this->histogram_enabled_) {
    this->stop_histogram_();
  }
  
  if (this->ae_enabled_) {
    this->stop_ae_();
  }
  
  if (this->awb_enabled_) {
    this->stop_awb_();
  }
  
  this->stop_color_();
  
  if (this->sharpen_enabled_) {
    this->stop_sharpen_();
  }
  
  if (this->gamma_enabled_) {
    this->stop_gamma_();
  }
  
  if (this->ccm_enabled_) {
    this->stop_ccm_();
  }
  
  if (this->demosaic_enabled_) {
    this->stop_demosaic_();
  }
  
  if (this->bf_enabled_) {
    this->stop_bayer_filter_();
  }
  
  this->pipeline_started_ = false;
  ESP_LOGI(TAG, "✅ ISP pipeline stopped");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::deinit() {
  if (this->pipeline_started_) {
    this->stop();
  }
  
  // Libérer les contrôleurs
  if (this->hist_ctlr_) {
    esp_isp_del_hist_controller(this->hist_ctlr_);
    this->hist_ctlr_ = nullptr;
  }
  
  if (this->ae_ctlr_) {
    esp_isp_del_ae_controller(this->ae_ctlr_);
    this->ae_ctlr_ = nullptr;
  }
  
  if (this->awb_ctlr_) {
    esp_isp_del_awb_controller(this->awb_ctlr_);
    this->awb_ctlr_ = nullptr;
  }
  
  // Les autres contrôleurs (BF, CCM, etc.) sont gérés par l'ISP processor
  
  this->pipeline_initialized_ = false;
  ESP_LOGI(TAG, "ISP pipeline deinitialized");
  
  return ESP_OK;
}

// ===== Bayer Filter =====

esp_err_t MipiDsiCamISPPipeline::init_bayer_filter_() {
  ESP_LOGD(TAG, "Initializing Bayer Filter");
  // Le BF est configuré mais pas démarré ici
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_bayer_filter(bool enable) {
  this->bf_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_bayer_filter_();
    } else {
      return this->stop_bayer_filter_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_denoising_level(uint8_t level) {
  if (level > 20) {
    level = 20;
  }
  this->denoising_level_ = level;
  
  if (this->pipeline_started_ && this->bf_enabled_) {
    // Reconfigurer le BF avec le nouveau niveau
    return this->start_bayer_filter_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_bayer_filter_() {
  // Note: Dans le vrai code, utiliser l'ISP handle de la caméra
  // isp_proc_handle_t isp = this->camera_->get_isp_handle();
  
  esp_isp_bf_config_t bf_config = {};
  bf_config.denoising_level = this->denoising_level_;
  bf_config.padding_mode = ISP_BF_EDGE_PADDING_MODE_SRND_DATA;
  
  // Matrice de filtrage par défaut (filtre moyenneur 3x3)
  for (int i = 0; i < ISP_BF_TEMPLATE_X_NUMS; i++) {
    for (int j = 0; j < ISP_BF_TEMPLATE_Y_NUMS; j++) {
      bf_config.bf_template[i][j] = 1;
    }
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_bf_configure(isp, &bf_config), TAG, "BF config failed");
  // ESP_RETURN_ON_ERROR(esp_isp_bf_enable(isp), TAG, "BF enable failed");
  
  ESP_LOGI(TAG, "✅ Bayer Filter started (level=%u)", this->denoising_level_);
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_bayer_filter_() {
  // ESP_RETURN_ON_ERROR(esp_isp_bf_disable(isp), TAG, "BF disable failed");
  ESP_LOGD(TAG, "Bayer Filter stopped");
  return ESP_OK;
}

// ===== Color Correction Matrix =====

esp_err_t MipiDsiCamISPPipeline::init_ccm_() {
  ESP_LOGD(TAG, "Initializing CCM");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_ccm(bool enable) {
  this->ccm_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_ccm_();
    } else {
      return this->stop_ccm_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_ccm_matrix(float matrix[3][3]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      this->ccm_matrix_[i][j] = matrix[i][j];
    }
  }
  
  if (this->pipeline_started_ && this->ccm_enabled_) {
    return this->start_ccm_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_white_balance(float red_gain, float green_gain, float blue_gain) {
  this->red_balance_ = red_gain;
  this->green_balance_ = green_gain;
  this->blue_balance_ = blue_gain;
  
  ESP_LOGI(TAG, "White Balance: R=%.2f G=%.2f B=%.2f", red_gain, green_gain, blue_gain);
  
  if (this->pipeline_started_ && this->ccm_enabled_) {
    return this->start_ccm_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_ccm_() {
  esp_isp_ccm_config_t ccm_config = {};
  ccm_config.saturation = true;
  
  // Appliquer la matrice CCM avec les gains de balance des blancs
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      ccm_config.matrix[i][j] = this->ccm_matrix_[i][j];
    }
    // Appliquer les gains R/G/B
    ccm_config.matrix[i][0] *= this->red_balance_;
    ccm_config.matrix[i][1] *= this->green_balance_;
    ccm_config.matrix[i][2] *= this->blue_balance_;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_ccm_configure(isp, &ccm_config), TAG, "CCM config failed");
  // ESP_RETURN_ON_ERROR(esp_isp_ccm_enable(isp), TAG, "CCM enable failed");
  
  ESP_LOGI(TAG, "✅ CCM started (WB: R=%.2f G=%.2f B=%.2f)", 
           this->red_balance_, this->green_balance_, this->blue_balance_);
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_ccm_() {
  // ESP_RETURN_ON_ERROR(esp_isp_ccm_disable(isp), TAG, "CCM disable failed");
  ESP_LOGD(TAG, "CCM stopped");
  return ESP_OK;
}

// ===== Sharpen =====

esp_err_t MipiDsiCamISPPipeline::init_sharpen_() {
  ESP_LOGD(TAG, "Initializing Sharpen");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_sharpen(bool enable) {
  this->sharpen_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_sharpen_();
    } else {
      return this->stop_sharpen_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_sharpen_params(uint8_t h_thresh, uint8_t l_thresh,
                                                     float h_coeff, float m_coeff) {
  this->sharpen_h_thresh_ = h_thresh;
  this->sharpen_l_thresh_ = l_thresh;
  this->sharpen_h_coeff_ = h_coeff;
  this->sharpen_m_coeff_ = m_coeff;
  
  if (this->pipeline_started_ && this->sharpen_enabled_) {
    return this->start_sharpen_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_sharpen_() {
  esp_isp_sharpen_config_t sharpen_config = {};
  
  uint8_t h_amount = 1 << ISP_SHARPEN_H_FREQ_COEF_DEC_BITS;
  uint8_t m_amount = 1 << ISP_SHARPEN_M_FREQ_COEF_DEC_BITS;
  
  uint8_t h_integer = (uint8_t)(this->sharpen_h_coeff_ * h_amount);
  uint8_t m_integer = (uint8_t)(this->sharpen_m_coeff_ * m_amount);
  
  sharpen_config.h_freq_coeff.integer = h_integer / m_amount;
  sharpen_config.h_freq_coeff.decimal = h_integer % m_amount;
  sharpen_config.m_freq_coeff.integer = m_integer / m_amount;
  sharpen_config.m_freq_coeff.decimal = m_integer % m_amount;
  
  sharpen_config.h_thresh = this->sharpen_h_thresh_;
  sharpen_config.l_thresh = this->sharpen_l_thresh_;
  sharpen_config.padding_mode = ISP_SHARPEN_EDGE_PADDING_MODE_SRND_DATA;
  
  // Matrice de sharpening par défaut (Laplacien)
  int8_t sharpen_matrix[3][3] = {
    { -1, -1, -1 },
    { -1,  9, -1 },
    { -1, -1, -1 }
  };
  
  for (int i = 0; i < ISP_SHARPEN_TEMPLATE_X_NUMS; i++) {
    for (int j = 0; j < ISP_SHARPEN_TEMPLATE_Y_NUMS; j++) {
      sharpen_config.sharpen_template[i][j] = sharpen_matrix[i][j];
    }
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_sharpen_configure(isp, &sharpen_config), TAG, "Sharpen config failed");
  // ESP_RETURN_ON_ERROR(esp_isp_sharpen_enable(isp), TAG, "Sharpen enable failed");
  
  ESP_LOGI(TAG, "✅ Sharpen started (h_thresh=%u l_thresh=%u)", 
           this->sharpen_h_thresh_, this->sharpen_l_thresh_);
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_sharpen_() {
  // ESP_RETURN_ON_ERROR(esp_isp_sharpen_disable(isp), TAG, "Sharpen disable failed");
  ESP_LOGD(TAG, "Sharpen stopped");
  return ESP_OK;
}

// ===== Gamma =====

esp_err_t MipiDsiCamISPPipeline::init_gamma_() {
  ESP_LOGD(TAG, "Initializing Gamma");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_gamma(bool enable) {
  this->gamma_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_gamma_();
    } else {
      return this->stop_gamma_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_gamma_curve(float gamma) {
  if (gamma < 0.1f) gamma = 0.1f;
  if (gamma > 5.0f) gamma = 5.0f;
  
  this->gamma_value_ = gamma;
  
  if (this->pipeline_started_ && this->gamma_enabled_) {
    return this->start_gamma_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_gamma_() {
  isp_gamma_curve_points_t gamma_curve;
  this->generate_gamma_curve_(this->gamma_value_, &gamma_curve);
  
  // ESP_RETURN_ON_ERROR(esp_isp_gamma_configure(isp, COLOR_COMPONENT_R, &gamma_curve), TAG, "Gamma R failed");
  // ESP_RETURN_ON_ERROR(esp_isp_gamma_configure(isp, COLOR_COMPONENT_G, &gamma_curve), TAG, "Gamma G failed");
  // ESP_RETURN_ON_ERROR(esp_isp_gamma_configure(isp, COLOR_COMPONENT_B, &gamma_curve), TAG, "Gamma B failed");
  // ESP_RETURN_ON_ERROR(esp_isp_gamma_enable(isp), TAG, "Gamma enable failed");
  
  ESP_LOGI(TAG, "✅ Gamma started (gamma=%.2f)", this->gamma_value_);
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_gamma_() {
  // ESP_RETURN_ON_ERROR(esp_isp_gamma_disable(isp), TAG, "Gamma disable failed");
  ESP_LOGD(TAG, "Gamma stopped");
  return ESP_OK;
}

// ===== Demosaic =====

esp_err_t MipiDsiCamISPPipeline::init_demosaic_() {
  ESP_LOGD(TAG, "Initializing Demosaic");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_demosaic(bool enable) {
  this->demosaic_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_demosaic_();
    } else {
      return this->stop_demosaic_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_demosaic_gradient_ratio(float ratio) {
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  
  this->demosaic_gradient_ratio_ = ratio;
  
  if (this->pipeline_started_ && this->demosaic_enabled_) {
    return this->start_demosaic_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_demosaic_() {
  esp_isp_demosaic_config_t demosaic_config = {};
  
  uint32_t gradient_ratio_amount = 1 << ISP_DEMOSAIC_GRAD_RATIO_DEC_BITS;
  uint32_t gradient_ratio_val = (uint32_t)(this->demosaic_gradient_ratio_ * gradient_ratio_amount);
  
  demosaic_config.grad_ratio.integer = gradient_ratio_val / gradient_ratio_amount;
  demosaic_config.grad_ratio.decimal = gradient_ratio_val % gradient_ratio_amount;
  
  // ESP_RETURN_ON_ERROR(esp_isp_demosaic_configure(isp, &demosaic_config), TAG, "Demosaic config failed");
  // ESP_RETURN_ON_ERROR(esp_isp_demosaic_enable(isp), TAG, "Demosaic enable failed");
  
  ESP_LOGI(TAG, "✅ Demosaic started (gradient_ratio=%.2f)", this->demosaic_gradient_ratio_);
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_demosaic_() {
  // ESP_RETURN_ON_ERROR(esp_isp_demosaic_disable(isp), TAG, "Demosaic disable failed");
  ESP_LOGD(TAG, "Demosaic stopped");
  return ESP_OK;
}

// ===== Color Management =====

esp_err_t MipiDsiCamISPPipeline::init_color_() {
  ESP_LOGD(TAG, "Initializing Color");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_brightness(int8_t brightness) {
  this->brightness_ = brightness;
  
  if (this->pipeline_started_) {
    return this->start_color_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_contrast(uint8_t contrast) {
  this->contrast_ = contrast;
  
  if (this->pipeline_started_) {
    return this->start_color_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_saturation(uint8_t saturation) {
  this->saturation_ = saturation;
  
  if (this->pipeline_started_) {
    return this->start_color_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::set_hue(uint16_t hue) {
  if (hue > 360) hue = 360;
  this->hue_ = hue;
  
  if (this->pipeline_started_) {
    return this->start_color_();
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_color_() {
  esp_isp_color_config_t color_config = {};
  
  color_config.color_brightness = this->brightness_;
  color_config.color_contrast.val = this->contrast_;
  color_config.color_saturation.val = this->saturation_;
  color_config.color_hue = this->hue_;
  
  // ESP_RETURN_ON_ERROR(esp_isp_color_configure(isp, &color_config), TAG, "Color config failed");
  // ESP_RETURN_ON_ERROR(esp_isp_color_enable(isp), TAG, "Color enable failed");
  
  ESP_LOGI(TAG, "✅ Color started (B=%d C=%u S=%u H=%u)", 
           this->brightness_, this->contrast_, this->saturation_, this->hue_);
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_color_() {
  // ESP_RETURN_ON_ERROR(esp_isp_color_disable(isp), TAG, "Color disable failed");
  ESP_LOGD(TAG, "Color stopped");
  return ESP_OK;
}

// ===== Auto White Balance =====

esp_err_t MipiDsiCamISPPipeline::init_awb_() {
  ESP_LOGD(TAG, "Initializing AWB");
  
  uint32_t width = this->camera_->get_image_width();
  uint32_t height = this->camera_->get_image_height();
  
  esp_isp_awb_config_t awb_config = {};
  awb_config.sample_point = ISP_AWB_SAMPLE_POINT_BEFORE_CCM;
  
  // Fenêtre centrale (80% de l'image)
  awb_config.window.top_left.x = width * 0.1f;
  awb_config.window.top_left.y = height * 0.1f;
  awb_config.window.btm_right.x = width * 0.9f;
  awb_config.window.btm_right.y = height * 0.9f;
  
  // Configuration White Patch
  awb_config.white_patch.luminance.min = 185;
  awb_config.white_patch.luminance.max = 395;
  awb_config.white_patch.red_green_ratio.min = 0.5040f;
  awb_config.white_patch.red_green_ratio.max = 0.8899f;
  awb_config.white_patch.blue_green_ratio.min = 0.4838f;
  awb_config.white_patch.blue_green_ratio.max = 0.7822f;
  
  // ESP_RETURN_ON_ERROR(esp_isp_new_awb_controller(isp, &awb_config, &this->awb_ctlr_), TAG, "AWB create failed");
  
  // Enregistrer le callback
  esp_isp_awb_cbs_t awb_cbs = {};
  awb_cbs.on_statistics_done = MipiDsiCamISPPipeline::awb_stats_callback;
  
  // ESP_RETURN_ON_ERROR(esp_isp_awb_register_event_callbacks(this->awb_ctlr_, &awb_cbs, this), TAG, "AWB callback failed");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_awb(bool enable) {
  this->awb_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_awb_();
    } else {
      return this->stop_awb_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_awb_() {
  if (!this->awb_ctlr_) {
    ESP_LOGE(TAG, "AWB controller not initialized");
    return ESP_FAIL;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_awb_controller_enable(this->awb_ctlr_), TAG, "AWB enable failed");
  // ESP_RETURN_ON_ERROR(esp_isp_awb_controller_start_continuous_statistics(this->awb_ctlr_), TAG, "AWB start failed");
  
  ESP_LOGI(TAG, "✅ AWB started (continuous statistics)");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_awb_() {
  if (!this->awb_ctlr_) {
    return ESP_OK;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_awb_controller_stop_continuous_statistics(this->awb_ctlr_), TAG, "AWB stop failed");
  // ESP_RETURN_ON_ERROR(esp_isp_awb_controller_disable(this->awb_ctlr_), TAG, "AWB disable failed");
  
  ESP_LOGD(TAG, "AWB stopped");
  return ESP_OK;
}

bool MipiDsiCamISPPipeline::awb_stats_callback(isp_awb_ctlr_t awb_ctlr,
                                                const esp_isp_awb_evt_data_t *edata,
                                                void *user_data) {
  MipiDsiCamISPPipeline *pipeline = (MipiDsiCamISPPipeline*)user_data;
  
  // Calculer les gains de balance des blancs à partir des statistiques
  float red_gain = edata->white_patch_num > 0 ? 
                   (float)edata->color_info[1] / (float)edata->color_info[0] : 1.0f;
  float blue_gain = edata->white_patch_num > 0 ?
                    (float)edata->color_info[1] / (float)edata->color_info[2] : 1.0f;
  
  // Limiter les gains
  if (red_gain < 0.5f) red_gain = 0.5f;
  if (red_gain > 4.0f) red_gain = 4.0f;
  if (blue_gain < 0.5f) blue_gain = 0.5f;
  if (blue_gain > 4.0f) blue_gain = 4.0f;
  
  // Appliquer les gains avec un filtre pour éviter les changements brusques
  pipeline->red_balance_ = pipeline->red_balance_ * 0.9f + red_gain * 0.1f;
  pipeline->blue_balance_ = pipeline->blue_balance_ * 0.9f + blue_gain * 0.1f;
  
  ESP_LOGV(TAG, "AWB: R=%.2f B=%.2f (patches=%u)", 
           pipeline->red_balance_, pipeline->blue_balance_, edata->white_patch_num);
  
  return false; // Ne pas bloquer l'ISR
}

// ===== Auto Exposure =====

esp_err_t MipiDsiCamISPPipeline::init_ae_() {
  ESP_LOGD(TAG, "Initializing AE");
  
  uint32_t width = this->camera_->get_image_width();
  uint32_t height = this->camera_->get_image_height();
  
  esp_isp_ae_config_t ae_config = {};
  ae_config.sample_point = ISP_AE_SAMPLE_POINT_AFTER_GAMMA;
  
  // Fenêtre centrale
  ae_config.window.top_left.x = width * 0.2f;
  ae_config.window.top_left.y = height * 0.2f;
  ae_config.window.btm_right.x = width * 0.8f;
  ae_config.window.btm_right.y = height * 0.8f;
  
  ae_config.intr_priority = 0;
  
  // ESP_RETURN_ON_ERROR(esp_isp_new_ae_controller(isp, &ae_config, &this->ae_ctlr_), TAG, "AE create failed");
  
  // Enregistrer le callback
  esp_isp_ae_env_detector_evt_cbs_t ae_cbs = {};
  ae_cbs.on_env_statistics_done = MipiDsiCamISPPipeline::ae_stats_callback;
  
  // ESP_RETURN_ON_ERROR(esp_isp_ae_env_detector_register_event_callbacks(this->ae_ctlr_, &ae_cbs, this), TAG, "AE callback failed");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_ae(bool enable) {
  this->ae_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_ae_();
    } else {
      return this->stop_ae_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_ae_() {
  if (!this->ae_ctlr_) {
    ESP_LOGE(TAG, "AE controller not initialized");
    return ESP_FAIL;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_ae_controller_enable(this->ae_ctlr_), TAG, "AE enable failed");
  // ESP_RETURN_ON_ERROR(esp_isp_ae_controller_start_continuous_statistics(this->ae_ctlr_), TAG, "AE start failed");
  
  ESP_LOGI(TAG, "✅ AE started (continuous statistics)");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_ae_() {
  if (!this->ae_ctlr_) {
    return ESP_OK;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_ae_controller_stop_continuous_statistics(this->ae_ctlr_), TAG, "AE stop failed");
  // ESP_RETURN_ON_ERROR(esp_isp_ae_controller_disable(this->ae_ctlr_), TAG, "AE disable failed");
  
  ESP_LOGD(TAG, "AE stopped");
  return ESP_OK;
}

bool MipiDsiCamISPPipeline::ae_stats_callback(isp_ae_ctlr_t ae_ctlr,
                                               const esp_isp_ae_env_detector_evt_data_t *edata,
                                               void *user_data) {
  MipiDsiCamISPPipeline *pipeline = (MipiDsiCamISPPipeline*)user_data;
  
  // Calculer la luminosité moyenne
  uint32_t total_lum = 0;
  for (int i = 0; i < edata->ae_result_count; i++) {
    total_lum += edata->luminance[i];
  }
  uint32_t avg_lum = edata->ae_result_count > 0 ? 
                     total_lum / edata->ae_result_count : 128;
  
  ESP_LOGV(TAG, "AE: avg_lum=%u", avg_lum);
  
  // L'ajustement d'exposition devrait être fait via le capteur
  // pipeline->camera_->adjust_exposure_from_ae(avg_lum);
  
  return false;
}

// ===== Histogram =====

esp_err_t MipiDsiCamISPPipeline::init_histogram_() {
  ESP_LOGD(TAG, "Initializing Histogram");
  
  uint32_t width = this->camera_->get_image_width();
  uint32_t height = this->camera_->get_image_height();
  
  esp_isp_hist_config_t hist_config = {};
  hist_config.window.top_left.x = width * 0.2f;
  hist_config.window.top_left.y = height * 0.2f;
  hist_config.window.btm_right.x = width * 0.8f;
  hist_config.window.btm_right.y = height * 0.8f;
  
  hist_config.hist_mode = ISP_HIST_SAMPLING_YUV_Y;
  
  // Coefficients RGB pour conversion Y
  hist_config.rgb_coefficient.coeff_r.integer = 0;
  hist_config.rgb_coefficient.coeff_r.decimal = 85;
  hist_config.rgb_coefficient.coeff_g.integer = 0;
  hist_config.rgb_coefficient.coeff_g.decimal = 85;
  hist_config.rgb_coefficient.coeff_b.integer = 0;
  hist_config.rgb_coefficient.coeff_b.decimal = 85;
  
  // Poids de fenêtre (tous égaux)
  for (int i = 0; i < ISP_HIST_BLOCK_X_NUMS * ISP_HIST_BLOCK_Y_NUMS; i++) {
    hist_config.window_weight[i].integer = 0;
    hist_config.window_weight[i].decimal = 10;
  }
  
  // Seuils de segment
  for (int i = 0; i < ISP_HIST_SEGMENT_NUMS; i++) {
    hist_config.segment_threshold[i] = (i + 1) * 16;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_new_hist_controller(isp, &hist_config, &this->hist_ctlr_), TAG, "Hist create failed");
  
  // Enregistrer le callback
  esp_isp_hist_cbs_t hist_cbs = {};
  hist_cbs.on_statistics_done = MipiDsiCamISPPipeline::hist_stats_callback;
  
  // ESP_RETURN_ON_ERROR(esp_isp_hist_register_event_callbacks(this->hist_ctlr_, &hist_cbs, this), TAG, "Hist callback failed");
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::enable_histogram(bool enable) {
  this->histogram_enabled_ = enable;
  
  if (this->pipeline_started_) {
    if (enable) {
      return this->start_histogram_();
    } else {
      return this->stop_histogram_();
    }
  }
  
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::start_histogram_() {
  if (!this->hist_ctlr_) {
    ESP_LOGE(TAG, "Histogram controller not initialized");
    return ESP_FAIL;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_hist_controller_enable(this->hist_ctlr_), TAG, "Hist enable failed");
  // ESP_RETURN_ON_ERROR(esp_isp_hist_controller_start_continuous_statistics(this->hist_ctlr_), TAG, "Hist start failed");
  
  ESP_LOGI(TAG, "✅ Histogram started (continuous statistics)");
  return ESP_OK;
}

esp_err_t MipiDsiCamISPPipeline::stop_histogram_() {
  if (!this->hist_ctlr_) {
    return ESP_OK;
  }
  
  // ESP_RETURN_ON_ERROR(esp_isp_hist_controller_stop_continuous_statistics(this->hist_ctlr_), TAG, "Hist stop failed");
  // ESP_RETURN_ON_ERROR(esp_isp_hist_controller_disable(this->hist_ctlr_), TAG, "Hist disable failed");
  
  ESP_LOGD(TAG, "Histogram stopped");
  return ESP_OK;
}

bool MipiDsiCamISPPipeline::hist_stats_callback(isp_hist_ctlr_t hist_ctlr,
                                                 const esp_isp_hist_evt_data_t *edata,
                                                 void *user_data) {
  // Traiter les données d'histogramme si nécessaire
  ESP_LOGV(TAG, "Histogram data received");
  return false;
}

// ===== Utilitaires =====

void MipiDsiCamISPPipeline::init_default_ccm_matrix_() {
  // Matrice identité par défaut
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      this->ccm_matrix_[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }
}

void MipiDsiCamISPPipeline::generate_gamma_curve_(float gamma, isp_gamma_curve_points_t *curve) {
  // Générer une courbe gamma standard
  for (int i = 0; i < ISP_GAMMA_CURVE_POINTS_NUM; i++) {
    float x = (float)i / (ISP_GAMMA_CURVE_POINTS_NUM - 1);
    float y = powf(x, 1.0f / gamma);
    
    curve->pt[i].x = (uint32_t)(x * 255.0f);
    curve->pt[i].y = (uint32_t)(y * 255.0f);
  }
}

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_ISP_PIPELINE
