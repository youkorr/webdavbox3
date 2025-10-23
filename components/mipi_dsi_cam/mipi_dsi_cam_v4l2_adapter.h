#pragma once

#ifdef MIPI_DSI_CAM_ENABLE_V4L2

#include "mipi_dsi_cam.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

// Inclure les headers V4L2 d'ESP-IDF
extern "C" {
  #include "videodev2.h"
}

namespace esphome {
namespace mipi_dsi_cam {

// Structure pour passer le contexte entre le video device et notre camera
struct MipiCameraV4L2Context {
  MipiDsiCam *camera;
  void *video_device;
};

/**
 * @brief Adaptateur V4L2 pour mipi_dsi_cam
 * 
 * Cette classe crée un pont entre le driver bas niveau mipi_dsi_cam
 * et l'interface V4L2 standard d'ESP-IDF pour permettre l'utilisation
 * avec les pipelines vidéo avancés (JPEG, H.264, ISP avancé, etc.)
 */
class MipiDsiCamV4L2Adapter {
 public:
  MipiDsiCamV4L2Adapter(MipiDsiCam *camera);
  ~MipiDsiCamV4L2Adapter();
  
  /**
   * @brief Initialise l'interface V4L2
   * @return ESP_OK si réussi
   */
  esp_err_t init();
  
  /**
   * @brief Libère les ressources V4L2
   * @return ESP_OK si réussi
   */
  esp_err_t deinit();
  
  /**
   * @brief Retourne le pointeur vers le device V4L2
   * @return Pointeur vers la structure esp_video
   */
  void* get_video_device() { return this->context_.video_device; }
  
  /**
   * @brief Vérifie si l'adaptateur est initialisé
   */
  bool is_initialized() const { return this->context_.video_device != nullptr; }
  
 protected:
  MipiCameraV4L2Context context_;
  
  // ===== Callbacks V4L2 pour le driver =====
  // Ces fonctions sont appelées par le framework V4L2 d'ESP-IDF
  
  /**
   * @brief Initialise le device V4L2
   */
  static esp_err_t v4l2_init(void *video);
  
  /**
   * @brief Dé-initialise le device V4L2
   */
  static esp_err_t v4l2_deinit(void *video);
  
  /**
   * @brief Démarre le streaming vidéo
   * @param type Type de buffer (V4L2_BUF_TYPE_VIDEO_CAPTURE)
   */
  static esp_err_t v4l2_start(void *video, uint32_t type);
  
  /**
   * @brief Arrête le streaming vidéo
   * @param type Type de buffer (V4L2_BUF_TYPE_VIDEO_CAPTURE)
   */
  static esp_err_t v4l2_stop(void *video, uint32_t type);
  
  /**
   * @brief Énumère les formats disponibles
   * @param type Type de buffer
   * @param index Index du format à énumérer
   * @param pixel_format Format retourné (fourcc code V4L2)
   */
  static esp_err_t v4l2_enum_format(void *video, uint32_t type, 
                                     uint32_t index, uint32_t *pixel_format);
  
  /**
   * @brief Configure le format vidéo
   * @param format Structure v4l2_format contenant le format souhaité
   */
  static esp_err_t v4l2_set_format(void *video, const struct v4l2_format *format);
  
  /**
   * @brief Récupère le format vidéo actuel
   * @param format Structure v4l2_format à remplir
   */
  static esp_err_t v4l2_get_format(void *video, struct v4l2_format *format);
  
  /**
   * @brief Notification d'événements
   * @param event Type d'événement
   * @param arg Argument de l'événement
   */
  static esp_err_t v4l2_notify(void *video, int event, void *arg);
  
  /**
   * @brief Configure des contrôles étendus (exposition, gain, etc.)
   * @param ctrls Structure contenant les contrôles à configurer
   */
  static esp_err_t v4l2_set_ext_ctrl(void *video, const struct v4l2_ext_controls *ctrls);
  
  /**
   * @brief Récupère les valeurs des contrôles étendus
   * @param ctrls Structure contenant les contrôles à lire
   */
  static esp_err_t v4l2_get_ext_ctrl(void *video, struct v4l2_ext_controls *ctrls);
  
  /**
   * @brief Interroge les propriétés d'un contrôle
   * @param qctrl Structure à remplir avec les infos du contrôle
   */
  static esp_err_t v4l2_query_ext_ctrl(void *video, struct v4l2_query_ext_ctrl *qctrl);
  
  /**
   * @brief Configure les paramètres du capteur (résolution, fps, etc.)
   */
  static esp_err_t v4l2_set_sensor_format(void *video, const void *format);
  
  /**
   * @brief Récupère les paramètres du capteur
   */
  static esp_err_t v4l2_get_sensor_format(void *video, void *format);
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_V4L2
