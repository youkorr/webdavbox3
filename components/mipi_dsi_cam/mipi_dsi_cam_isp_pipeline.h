#pragma once

#ifdef MIPI_DSI_CAM_ENABLE_ISP_PIPELINE

#include "mipi_dsi_cam.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

extern "C" {
  #include "driver/isp.h"
  #include "driver/isp_bf.h"
  #include "driver/isp_ccm.h"
  #include "driver/isp_sharpen.h"
  #include "driver/isp_gamma.h"
  #include "driver/isp_demosaic.h"
  #include "driver/isp_color.h"
}

namespace esphome {
namespace mipi_dsi_cam {

/**
 * @brief Pipeline ISP avancé pour mipi_dsi_cam
 * 
 * Cette classe gère un pipeline ISP complet avec :
 * - Bayer Filter (BF) pour réduction de bruit
 * - Color Correction Matrix (CCM) pour balance des blancs
 * - Demosaicing pour conversion Bayer -> RGB
 * - Sharpen pour amélioration de la netteté
 * - Gamma correction
 * - Color management (luminosité, contraste, saturation, teinte)
 * - Auto White Balance (AWB)
 * - Auto Exposure (AE)
 * - Histogramme
 */
class MipiDsiCamISPPipeline {
 public:
  MipiDsiCamISPPipeline(MipiDsiCam *camera);
  ~MipiDsiCamISPPipeline();
  
  /**
   * @brief Initialise le pipeline ISP complet
   * @return ESP_OK si réussi
   */
  esp_err_t init();
  
  /**
   * @brief Démarre le pipeline ISP
   * @return ESP_OK si réussi
   */
  esp_err_t start();
  
  /**
   * @brief Arrête le pipeline ISP
   * @return ESP_OK si réussi
   */
  esp_err_t stop();
  
  /**
   * @brief Libère les ressources ISP
   * @return ESP_OK si réussi
   */
  esp_err_t deinit();
  
  // ===== Configuration Bayer Filter =====
  
  /**
   * @brief Active/désactive le filtre Bayer (réduction de bruit)
   */
  esp_err_t enable_bayer_filter(bool enable);
  
  /**
   * @brief Configure le niveau de débruitage (0-20)
   */
  esp_err_t set_denoising_level(uint8_t level);
  
  // ===== Configuration CCM (Color Correction Matrix) =====
  
  /**
   * @brief Active/désactive la correction colorimétrique
   */
  esp_err_t enable_ccm(bool enable);
  
  /**
   * @brief Configure la matrice CCM 3x3
   * @param matrix Matrice 3x3 de correction des couleurs
   */
  esp_err_t set_ccm_matrix(float matrix[3][3]);
  
  /**
   * @brief Configure la balance des blancs manuelle
   * @param red_gain Gain rouge (0.5 - 4.0)
   * @param green_gain Gain vert (0.5 - 4.0)
   * @param blue_gain Gain bleu (0.5 - 4.0)
   */
  esp_err_t set_white_balance(float red_gain, float green_gain, float blue_gain);
  
  // ===== Configuration Sharpen =====
  
  /**
   * @brief Active/désactive l'amélioration de la netteté
   */
  esp_err_t enable_sharpen(bool enable);
  
  /**
   * @brief Configure les paramètres de netteté
   * @param h_thresh Seuil hautes fréquences (0-255)
   * @param l_thresh Seuil basses fréquences (0-255)
   * @param h_coeff Coefficient hautes fréquences (0.0-1.0)
   * @param m_coeff Coefficient moyennes fréquences (0.0-1.0)
   */
  esp_err_t set_sharpen_params(uint8_t h_thresh, uint8_t l_thresh, 
                                float h_coeff, float m_coeff);
  
  // ===== Configuration Gamma =====
  
  /**
   * @brief Active/désactive la correction gamma
   */
  esp_err_t enable_gamma(bool enable);
  
  /**
   * @brief Configure la courbe gamma
   * @param gamma Valeur gamma (0.1 - 5.0), 1.0 = linéaire, <1.0 = plus lumineux, >1.0 = plus sombre
   */
  esp_err_t set_gamma_curve(float gamma);
  
  // ===== Configuration Demosaic =====
  
  /**
   * @brief Active/désactive le demosaicing
   */
  esp_err_t enable_demosaic(bool enable);
  
  /**
   * @brief Configure le ratio de gradient pour le demosaicing
   * @param ratio Ratio de gradient (0.0 - 1.0)
   */
  esp_err_t set_demosaic_gradient_ratio(float ratio);
  
  // ===== Configuration Color Management =====
  
  /**
   * @brief Configure la luminosité
   * @param brightness Luminosité (-128 à +127, 0 = neutre)
   */
  esp_err_t set_brightness(int8_t brightness);
  
  /**
   * @brief Configure le contraste
   * @param contrast Contraste (0-255, 128 = neutre)
   */
  esp_err_t set_contrast(uint8_t contrast);
  
  /**
   * @brief Configure la saturation
   * @param saturation Saturation (0-255, 128 = neutre)
   */
  esp_err_t set_saturation(uint8_t saturation);
  
  /**
   * @brief Configure la teinte
   * @param hue Teinte (0-360 degrés)
   */
  esp_err_t set_hue(uint16_t hue);
  
  // ===== Configuration Auto White Balance =====
  
  /**
   * @brief Active/désactive l'Auto White Balance
   */
  esp_err_t enable_awb(bool enable);
  
  // ===== Configuration Auto Exposure =====
  
  /**
   * @brief Active/désactive l'Auto Exposure
   */
  esp_err_t enable_ae(bool enable);
  
  // ===== Configuration Histogram =====
  
  /**
   * @brief Active/désactive l'histogramme
   */
  esp_err_t enable_histogram(bool enable);
  
 protected:
  MipiDsiCam *camera_;
  
  // États du pipeline
  bool pipeline_initialized_{false};
  bool pipeline_started_{false};
  
  // États des modules ISP
  bool bf_enabled_{true};
  bool ccm_enabled_{true};
  bool sharpen_enabled_{true};
  bool gamma_enabled_{true};
  bool demosaic_enabled_{true};
  bool awb_enabled_{false};
  bool ae_enabled_{false};
  bool histogram_enabled_{false};
  
  // Paramètres Bayer Filter
  uint8_t denoising_level_{8};
  
  // Paramètres CCM
  float ccm_matrix_[3][3];
  float red_balance_{1.0f};
  float green_balance_{1.0f};
  float blue_balance_{1.0f};
  
  // Paramètres Sharpen
  uint8_t sharpen_h_thresh_{100};
  uint8_t sharpen_l_thresh_{50};
  float sharpen_h_coeff_{0.8f};
  float sharpen_m_coeff_{0.5f};
  
  // Paramètres Gamma
  float gamma_value_{2.2f};
  
  // Paramètres Demosaic
  float demosaic_gradient_ratio_{0.5f};
  
  // Paramètres Color
  int8_t brightness_{0};
  uint8_t contrast_{128};
  uint8_t saturation_{128};
  uint16_t hue_{0};
  
  // Contrôleurs ISP
  isp_awb_ctlr_t awb_ctlr_{nullptr};
  isp_ae_ctlr_t ae_ctlr_{nullptr};
  isp_hist_ctlr_t hist_ctlr_{nullptr};
  
  // Méthodes d'initialisation des modules
  esp_err_t init_bayer_filter_();
  esp_err_t init_ccm_();
  esp_err_t init_sharpen_();
  esp_err_t init_gamma_();
  esp_err_t init_demosaic_();
  esp_err_t init_color_();
  esp_err_t init_awb_();
  esp_err_t init_ae_();
  esp_err_t init_histogram_();
  
  // Méthodes de démarrage des modules
  esp_err_t start_bayer_filter_();
  esp_err_t start_ccm_();
  esp_err_t start_sharpen_();
  esp_err_t start_gamma_();
  esp_err_t start_demosaic_();
  esp_err_t start_color_();
  esp_err_t start_awb_();
  esp_err_t start_ae_();
  esp_err_t start_histogram_();
  
  // Méthodes d'arrêt des modules
  esp_err_t stop_bayer_filter_();
  esp_err_t stop_ccm_();
  esp_err_t stop_sharpen_();
  esp_err_t stop_gamma_();
  esp_err_t stop_demosaic_();
  esp_err_t stop_color_();
  esp_err_t stop_awb_();
  esp_err_t stop_ae_();
  esp_err_t stop_histogram_();
  
  // Callbacks ISP
  static bool awb_stats_callback(isp_awb_ctlr_t awb_ctlr,
                                  const esp_isp_awb_evt_data_t *edata,
                                  void *user_data);
  
  static bool ae_stats_callback(isp_ae_ctlr_t ae_ctlr,
                                 const esp_isp_ae_env_detector_evt_data_t *edata,
                                 void *user_data);
  
  static bool hist_stats_callback(isp_hist_ctlr_t hist_ctlr,
                                   const esp_isp_hist_evt_data_t *edata,
                                   void *user_data);
  
  // Méthodes utilitaires
  void init_default_ccm_matrix_();
  void generate_gamma_curve_(float gamma, isp_gamma_curve_points_t *curve);
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_ISP_PIPELINE
