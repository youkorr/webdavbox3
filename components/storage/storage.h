#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstring>
#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/optional.h"
#include "esphome/components/image/image.h"
#include "esphome/components/display/display.h"
#include "../sd_mmc_card/sd_mmc_card.h"

// Image decoder configuration for ESP-IDF
#ifdef ESP_IDF_VERSION
  #define USE_JPEGDEC
#else
  #define USE_JPEGDEC
#endif

// Image decoders - only JPEG for now
#ifdef USE_JPEGDEC
#include <JPEGDEC.h>
#endif

namespace esphome {
namespace storage {

// Forward declarations
class StorageComponent;
class SdImageComponent;

// Image format enums
enum class ImageFormat {
  RGB565,
  RGB888,
  RGBA
};

enum class SdByteOrder {
  LITTLE_ENDIAN_SD,
  BIG_ENDIAN_SD
};

// =====================================================
// StorageComponent - Main Storage Class
// =====================================================
class StorageComponent : public Component {
 public:
  StorageComponent() = default;
  
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  
  // Configuration
  void set_platform(const std::string &platform) { this->platform_ = platform; }
  void set_sd_component(sd_mmc_card::SdMmc *sd_component) { this->sd_component_ = sd_component; }
  void set_root_path(const std::string &root_path) { this->root_path_ = root_path; }
  
  // File methods
  bool file_exists_direct(const std::string &path);
  std::vector<uint8_t> read_file_direct(const std::string &path);
  bool write_file_direct(const std::string &path, const std::vector<uint8_t> &data);
  size_t get_file_size(const std::string &path);
  
  // Getters
  const std::string &get_platform() const { return this->platform_; }
  const std::string &get_root_path() const { return this->root_path_; }
  sd_mmc_card::SdMmc *get_sd_component() const { return this->sd_component_; }
  
 private:
  std::string platform_;
  std::string root_path_{"/"}; 
  sd_mmc_card::SdMmc *sd_component_{nullptr};
};

// =====================================================
// SdImageComponent - Version corrigée pour ESPHome/LVGL
// =====================================================
class SdImageComponent : public Component, public image::Image {
 public:
  // Constructeur CRITIQUE - doit initialiser la classe de base avec des données valides
  SdImageComponent() : Component(), 
                       image::Image(nullptr, 0, 0, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE) {
    // Initialisation de base
  }

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  
  // Configuration setters
  void set_file_path(const std::string &path) { this->file_path_ = path; }
  void set_storage_component(StorageComponent *storage) { this->storage_component_ = storage; }
  void set_resize(int width, int height) { 
    this->resize_width_ = width; 
    this->resize_height_ = height; 
  }
  void set_format(ImageFormat format) { this->format_ = format; }
  void set_auto_load(bool auto_load) { this->auto_load_ = auto_load; }
  
  // Compatibility methods for YAML configuration
  void set_output_format_string(const std::string &format);
  void set_byte_order_string(const std::string &byte_order);
  
  // CRITIQUE: Override des méthodes Image avec la signature exacte du code source ESPHome
  void draw(int x, int y, display::Display *display, Color color_on, Color color_off) override;
  int get_width() const override { return this->get_current_width(); }
  int get_height() const override { return this->get_current_height(); }
  
  // Loading/unloading
  bool load_image();
  bool load_image_from_path(const std::string &path);
  void unload_image();
  bool reload_image();
  
  // CRITIQUE: Méthode pour finaliser le chargement
  void finalize_image_load();
  
  // Status
  bool is_loaded() const { return this->image_loaded_; }
  const std::string &get_file_path() const { return this->file_path_; }
  
  // CRITIQUE: Accès au buffer d'image pour LVGL
  const std::vector<uint8_t> &get_image_buffer() const { return this->image_buffer_; }
  uint8_t* get_image_data() { return this->image_buffer_.empty() ? nullptr : this->image_buffer_.data(); }
  size_t get_image_data_size() const { return this->image_buffer_.size(); }
  
  // Debug info
  std::string get_debug_info() const;

 protected:
  // Image state
  std::string file_path_;
  StorageComponent *storage_component_{nullptr};
  std::vector<uint8_t> image_buffer_;
  bool image_loaded_{false};
  bool auto_load_{true};
  
  // Image properties - locales
  int image_width_{0};
  int image_height_{0};
  int resize_width_{0};
  int resize_height_{0};
  ImageFormat format_{ImageFormat::RGB565};

 private:
  // Retry logic for image loading
  bool retry_load_{false};
  uint32_t last_retry_attempt_{0};
  static const uint32_t RETRY_INTERVAL_MS = 2000; // Retry toutes les 2 secondes
  
  // File type detection
  enum class FileType {
    UNKNOWN,
    JPEG
  };
  
  FileType detect_file_type(const std::vector<uint8_t> &data) const;
  bool is_jpeg_data(const std::vector<uint8_t> &data) const;
  
  // Image decoding - JPEG only for now
  bool decode_image(const std::vector<uint8_t> &data);
  bool decode_jpeg_image(const std::vector<uint8_t> &jpeg_data);
  
  // JPEG decoder callbacks
#ifdef USE_JPEGDEC
  static int jpeg_decode_callback(JPEGDRAW *draw);
  JPEGDEC *jpeg_decoder_{nullptr};
  bool jpeg_decode_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
#endif

  // Image processing
  bool allocate_image_buffer();
  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
  size_t get_pixel_size() const;
  size_t get_buffer_size() const;
  
  // CRITIQUE: Mise à jour des propriétés de la classe de base selon le code source ESPHome
  void update_base_image_properties();
  
  int get_current_width() const;
  int get_current_height() const;
  image::ImageType get_esphome_image_type() const;
  
  void draw_pixels_directly(int x, int y, display::Display *display, Color color_on, Color color_off);
  void draw_pixel_at(display::Display *display, int screen_x, int screen_y, int img_x, int img_y);
  Color get_pixel_color(int x, int y) const;
  
  // Utility methods
  void list_directory_contents(const std::string &dir_path);
  bool extract_jpeg_dimensions(const std::vector<uint8_t> &data, int &width, int &height) const;
  
  // Format helpers
  std::string format_to_string() const;

};

// =====================================================
// Action classes following ESPHome pattern
// =====================================================
template<typename... Ts> 
class SdImageLoadAction : public Action<Ts...> {
 public:
  explicit SdImageLoadAction(SdImageComponent *parent) : parent_(parent) {}
  
  TEMPLATABLE_VALUE(std::string, file_path)
  
  void play(Ts... x) override {
    if (this->parent_ == nullptr) return;
    
    if (this->file_path_.has_value()) {
      std::string path = this->file_path_.value(x...);
      if (!path.empty()) {
        this->parent_->load_image_from_path(path);
        return;
      }
    }
    
    this->parent_->load_image();
  }

 private:
  SdImageComponent *parent_;
};

template<typename... Ts> 
class SdImageUnloadAction : public Action<Ts...> {
 public:
  explicit SdImageUnloadAction(SdImageComponent *parent) : parent_(parent) {}
  
  void play(Ts... x) override {
    if (this->parent_ != nullptr) {
      this->parent_->unload_image();
    }
  }

 private:
  SdImageComponent *parent_;
};

}  // namespace storage
}  // namespace esphome







