#include "storage.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"  // For App.feed_wdt()
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <algorithm>

// Include yield function for ESP32/ESP8266
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define yield() taskYIELD()
#elif defined(ESP8266)
#include <Esp.h>
// yield() is already available on ESP8266
#else
// Fallback for other platforms
#define yield() delayMicroseconds(1)
#endif

namespace esphome {
namespace storage {

static const char *const TAG = "storage";
static const char *const TAG_IMAGE = "storage.image";

// Global decoder instances for callbacks
static SdImageComponent *current_image_component = nullptr;

// =====================================================
// StorageComponent Implementation
// =====================================================

void StorageComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Storage Component...");
  ESP_LOGCONFIG(TAG, "  Platform: %s", this->platform_.c_str());
  ESP_LOGCONFIG(TAG, "  Root path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  SD component: %s", this->sd_component_ ? "configured" : "not configured");
}

void StorageComponent::loop() {
  // Nothing to do in loop
}

void StorageComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Storage Component:");
  ESP_LOGCONFIG(TAG, "  Platform: %s", this->platform_.c_str());
  ESP_LOGCONFIG(TAG, "  Root path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  SD component: %s", this->sd_component_ ? "YES" : "NO");
}

bool StorageComponent::file_exists_direct(const std::string &path) {
  std::string full_path = this->root_path_ + path;
  struct stat st;
  return stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::vector<uint8_t> StorageComponent::read_file_direct(const std::string &path) {
  std::string full_path = this->root_path_ + path;
  FILE *file = fopen(full_path.c_str(), "rb");
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s (errno: %d)", full_path.c_str(), errno);
    return {};
  }
  
  // Get file size safely
  if (fseek(file, 0, SEEK_END) != 0) {
    ESP_LOGE(TAG, "Failed to seek to end of file: %s", full_path.c_str());
    fclose(file);
    return {};
  }
  
  long size = ftell(file);
  if (size < 0 || size > 10 * 1024 * 1024) { // 10MB limit
    ESP_LOGE(TAG, "Invalid file size: %ld bytes", size);
    fclose(file);
    return {};
  }
  
  if (fseek(file, 0, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to beginning of file: %s", full_path.c_str());
    fclose(file);
    return {};
  }
  
  std::vector<uint8_t> data(size);
  size_t read_size = fread(data.data(), 1, size, file);
  fclose(file);
  
  if (read_size != static_cast<size_t>(size)) {
    ESP_LOGE(TAG, "Failed to read complete file: expected %ld, got %zu", size, read_size);
    return {};
  }
  
  return data;
}

bool StorageComponent::write_file_direct(const std::string &path, const std::vector<uint8_t> &data) {
  std::string full_path = this->root_path_ + path;
  FILE *file = fopen(full_path.c_str(), "wb");
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to create file: %s", full_path.c_str());
    return false;
  }
  
  size_t written = fwrite(data.data(), 1, data.size(), file);
  fclose(file);
  
  return written == data.size();
}

size_t StorageComponent::get_file_size(const std::string &path) {
  std::string full_path = this->root_path_ + path;
  struct stat st;
  if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
    return st.st_size;
  }
  return 0;
}

// =====================================================
// SdImageComponent Implementation  
// =====================================================

void SdImageComponent::setup() {
  ESP_LOGCONFIG(TAG_IMAGE, "Setting up SD Image Component...");
  ESP_LOGCONFIG(TAG_IMAGE, "  File path: %s", this->file_path_.c_str());
  ESP_LOGCONFIG(TAG_IMAGE, "  Resize: %dx%d", this->resize_width_, this->resize_height_);
  ESP_LOGCONFIG(TAG_IMAGE, "  Format: %s", this->format_to_string().c_str());
  ESP_LOGCONFIG(TAG_IMAGE, "  Auto load: %s", this->auto_load_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG_IMAGE, "  Storage component: %s", this->storage_component_ ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG_IMAGE, "  Decoders: JPEG available");
  
  // CRITIQUE: Chargement automatique avec plus de vérifications
  if (this->auto_load_) {
    if (this->file_path_.empty()) {
      ESP_LOGW(TAG_IMAGE, "Auto-load enabled but no file path configured!");
      return;
    }
    
    if (!this->storage_component_) {
      ESP_LOGW(TAG_IMAGE, "Auto-load enabled but no storage component configured!");
      return;
    }
    
    // Attendre un peu que la carte SD soit prête
    ESP_LOGI(TAG_IMAGE, "Waiting for SD card to be ready...");
    delay(500); // Attendre 500ms
    
    ESP_LOGI(TAG_IMAGE, "Auto-loading image from: %s", this->file_path_.c_str());
    if (this->load_image()) {
      ESP_LOGI(TAG_IMAGE, "Image auto-loaded successfully!");
    } else {
      ESP_LOGW(TAG_IMAGE, "Failed to auto-load image, will retry later");
      // Programmer un retry dans la loop()
      this->retry_load_ = true;
    }
  }
}

void SdImageComponent::loop() {
  // Nothing to do in loop
}

void SdImageComponent::dump_config() {
  ESP_LOGCONFIG(TAG_IMAGE, "SD Image Component:");
  ESP_LOGCONFIG(TAG_IMAGE, "  File: %s", this->file_path_.c_str());
  ESP_LOGCONFIG(TAG_IMAGE, "  Dimensions: %dx%d", this->image_width_, this->image_height_);
  ESP_LOGCONFIG(TAG_IMAGE, "  Format: %s", this->format_to_string().c_str());
  ESP_LOGCONFIG(TAG_IMAGE, "  Loaded: %s", this->image_loaded_ ? "YES" : "NO");
  if (this->image_loaded_) {
    ESP_LOGCONFIG(TAG_IMAGE, "  Buffer size: %zu bytes", this->image_buffer_.size());
    ESP_LOGCONFIG(TAG_IMAGE, "  Base Image - W:%d H:%d Type:%d Data:%p", 
                  this->width_, this->height_, this->type_, this->data_start_);
  }
}

// Compatibility methods for YAML configuration
void SdImageComponent::set_output_format_string(const std::string &format) {
  if (format == "RGB565") {
    this->format_ = ImageFormat::RGB565;
  } else if (format == "RGB888") {
    this->format_ = ImageFormat::RGB888;
  } else if (format == "RGBA") {
    this->format_ = ImageFormat::RGBA;
  } else {
    ESP_LOGW(TAG_IMAGE, "Unknown format: %s, using RGB565", format.c_str());
    this->format_ = ImageFormat::RGB565;
  }
}

void SdImageComponent::set_byte_order_string(const std::string &byte_order) {
  ESP_LOGD(TAG_IMAGE, "Byte order set to: %s", byte_order.c_str());
}

// Implementation de la méthode draw() selon le code source ESPHome
void SdImageComponent::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  if (!this->image_loaded_ || this->image_buffer_.empty()) {
    ESP_LOGW(TAG_IMAGE, "Cannot draw: image not loaded");
    return;
  }
  
  ESP_LOGD(TAG_IMAGE, "Drawing SD image %dx%d at position %d,%d (Base: W:%d H:%d Data:%p)", 
           this->get_current_width(), this->get_current_height(), x, y,
           this->width_, this->height_, this->data_start_);
  
  // Si les données de base sont correctes, utiliser la méthode optimisée d'ESPHome
  if (this->data_start_ && this->width_ > 0 && this->height_ > 0) {
    ESP_LOGD(TAG_IMAGE, "Using ESPHome base image draw method");
    // Appeler la méthode de base qui gère le clipping et l'optimisation
    image::Image::draw(x, y, display, color_on, color_off);
  } else {
    ESP_LOGD(TAG_IMAGE, "Using fallback pixel-by-pixel drawing");
    // Fallback: dessiner pixel par pixel
    this->draw_pixels_directly(x, y, display, color_on, color_off);
  }
}

// Loading methods
bool SdImageComponent::load_image() {
  return this->load_image_from_path(this->file_path_);
}

bool SdImageComponent::load_image_from_path(const std::string &path) {
  ESP_LOGI(TAG_IMAGE, "Loading image from: %s", path.c_str());
  
  if (!this->storage_component_) {
    ESP_LOGE(TAG_IMAGE, "Storage component not available");
    return false;
  }
  
  // Unload previous image
  this->unload_image();
  
  // Check file existence
  if (!this->storage_component_->file_exists_direct(path)) {
    ESP_LOGE(TAG_IMAGE, "Image file not found: %s", path.c_str());
    
    // List directory for debugging
    std::string dir_path = path.substr(0, path.find_last_of("/"));
    if (dir_path.empty()) dir_path = "/";
    this->list_directory_contents(dir_path);
    
    return false;
  }
  
  // Read file data
  std::vector<uint8_t> file_data = this->storage_component_->read_file_direct(path);
  if (file_data.empty()) {
    ESP_LOGE(TAG_IMAGE, "Failed to read image file: %s", path.c_str());
    return false;
  }
  
  ESP_LOGI(TAG_IMAGE, "Read %zu bytes from file", file_data.size());
  
  // Show first few bytes for debugging
  if (file_data.size() >= 16) {
    ESP_LOGI(TAG_IMAGE, "First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
             file_data[0], file_data[1], file_data[2], file_data[3],
             file_data[4], file_data[5], file_data[6], file_data[7],
             file_data[8], file_data[9], file_data[10], file_data[11],
             file_data[12], file_data[13], file_data[14], file_data[15]);
  }
  
  // Decode image
  if (!this->decode_image(file_data)) {
    ESP_LOGE(TAG_IMAGE, "Failed to decode image: %s", path.c_str());
    return false;
  }
  
  this->file_path_ = path;
  this->image_loaded_ = true;
  
  // Finaliser le chargement en mettant à jour les propriétés de base
  this->finalize_image_load();
  
  ESP_LOGI(TAG_IMAGE, "Image loaded successfully: %dx%d, %zu bytes", 
           this->image_width_, this->image_height_, this->image_buffer_.size());
  
  return true;
}

void SdImageComponent::unload_image() {
  this->image_buffer_.clear();
  this->image_buffer_.shrink_to_fit();
  this->image_loaded_ = false;
  this->image_width_ = 0;
  this->image_height_ = 0;
  
  // Réinitialiser aussi les propriétés de la classe de base
  this->width_ = 0;
  this->height_ = 0;
  this->data_start_ = nullptr;
  this->bpp_ = 0;
}

bool SdImageComponent::reload_image() {
  std::string path = this->file_path_;
  this->unload_image();
  return this->load_image_from_path(path);
}

// Implémentation de finalize_image_load()
void SdImageComponent::finalize_image_load() {
  if (this->image_loaded_) {
    this->update_base_image_properties();
    ESP_LOGI(TAG_IMAGE, "Image properties updated - W:%d H:%d Type:%d Data:%p BPP:%d", 
             this->width_, this->height_, this->type_, this->data_start_, this->bpp_);
  }
}

// Mise à jour des propriétés de la classe de base selon le code source ESPHome
void SdImageComponent::update_base_image_properties() {
  // Mettre à jour les membres de la classe de base
  this->width_ = this->get_current_width();
  this->height_ = this->get_current_height();
  this->type_ = this->get_esphome_image_type();
  
  if (!this->image_buffer_.empty()) {
    this->data_start_ = this->image_buffer_.data();
    
    // Calculer bpp selon le code source ESPHome
    switch (this->type_) {
      case image::IMAGE_TYPE_BINARY:
        this->bpp_ = 1;
        break;
      case image::IMAGE_TYPE_GRAYSCALE:
        this->bpp_ = 8;
        break;
      case image::IMAGE_TYPE_RGB565:
        this->bpp_ = 16;
        break;
      case image::IMAGE_TYPE_RGB:
        this->bpp_ = 24;
        break;
      default:
        this->bpp_ = 16;
        break;
    }
  } else {
    this->data_start_ = nullptr;
    this->bpp_ = 0;
  }
}

int SdImageComponent::get_current_width() const {
  return this->resize_width_ > 0 ? this->resize_width_ : this->image_width_;
}

int SdImageComponent::get_current_height() const {
  return this->resize_height_ > 0 ? this->resize_height_ : this->image_height_;
}

image::ImageType SdImageComponent::get_esphome_image_type() const {
  switch (this->format_) {
    case ImageFormat::RGB565: return image::IMAGE_TYPE_RGB565;
    case ImageFormat::RGB888: return image::IMAGE_TYPE_RGB;
    case ImageFormat::RGBA: return image::IMAGE_TYPE_RGB; // ESPHome n'a pas de RGBA natif
    default: return image::IMAGE_TYPE_RGB565;
  }
}

void SdImageComponent::draw_pixels_directly(int x, int y, display::Display *display, Color color_on, Color color_off) {
  // Méthode de fallback pour dessiner directement
  ESP_LOGD(TAG_IMAGE, "Drawing %dx%d pixels directly", this->get_current_width(), this->get_current_height());
  
  for (int img_y = 0; img_y < this->get_current_height(); img_y++) {
    for (int img_x = 0; img_x < this->get_current_width(); img_x++) {
      Color pixel_color = this->get_pixel_color(img_x, img_y);
      display->draw_pixel_at(x + img_x, y + img_y, pixel_color);
    }
    
    // Yield périodique pour éviter le watchdog
    if (img_y % 32 == 0) {
      App.feed_wdt();
      yield();
    }
  }
}

void SdImageComponent::draw_pixel_at(display::Display *display, int screen_x, int screen_y, int img_x, int img_y) {
  Color pixel_color = this->get_pixel_color(img_x, img_y);
  display->draw_pixel_at(screen_x, screen_y, pixel_color);
}

Color SdImageComponent::get_pixel_color(int x, int y) const {
  if (x < 0 || x >= this->get_current_width() || y < 0 || y >= this->get_current_height()) {
    return Color::BLACK;
  }
  
  size_t offset = (y * this->get_current_width() + x) * this->get_pixel_size();
  
  if (offset + this->get_pixel_size() > this->image_buffer_.size()) {
    return Color::BLACK;
  }
  
  switch (this->format_) {
    case ImageFormat::RGB565: {
      uint16_t rgb565 = this->image_buffer_[offset] | (this->image_buffer_[offset + 1] << 8);
      uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
      uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
      uint8_t b = (rgb565 & 0x1F) << 3;
      return Color(r, g, b);
    }
    case ImageFormat::RGB888:
      return Color(this->image_buffer_[offset], 
                  this->image_buffer_[offset + 1], 
                  this->image_buffer_[offset + 2]);
    case ImageFormat::RGBA:
      return Color(this->image_buffer_[offset], 
                  this->image_buffer_[offset + 1], 
                  this->image_buffer_[offset + 2], 
                  this->image_buffer_[offset + 3]);
    default:
      return Color::BLACK;
  }
}

// File type detection
SdImageComponent::FileType SdImageComponent::detect_file_type(const std::vector<uint8_t> &data) const {
  if (this->is_jpeg_data(data)) return FileType::JPEG;
  return FileType::UNKNOWN;
}

bool SdImageComponent::is_jpeg_data(const std::vector<uint8_t> &data) const {
  return data.size() >= 4 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

// Image decoding
bool SdImageComponent::decode_image(const std::vector<uint8_t> &data) {
  FileType type = this->detect_file_type(data);
  
  switch (type) {
    case FileType::JPEG:
      ESP_LOGI(TAG_IMAGE, "Decoding JPEG image");
      return this->decode_jpeg_image(data);
      
    default:
      ESP_LOGE(TAG_IMAGE, "Unsupported image format (only JPEG supported in this build)");
      return false;
  }
}

// =====================================================
// JPEG Decoder Implementation (ESPHome style)
// =====================================================

bool SdImageComponent::decode_jpeg_image(const std::vector<uint8_t> &jpeg_data) {
  ESP_LOGD(TAG_IMAGE, "Using JPEGDEC decoder");
  
  // Set current component for callback
  current_image_component = this;
  
  // Create decoder
  this->jpeg_decoder_ = new JPEGDEC();
  if (!this->jpeg_decoder_) {
    ESP_LOGE(TAG_IMAGE, "Failed to allocate JPEG decoder");
    current_image_component = nullptr;
    return false;
  }
  
  // Configuration correcte du décodeur JPEGDEC
  // Forcer le format RGB565 directement dans JPEGDEC
  this->format_ = ImageFormat::RGB565;
  
  // Open JPEG avec validation
  int result = this->jpeg_decoder_->openRAM((uint8_t*)jpeg_data.data(), jpeg_data.size(), 
                                           SdImageComponent::jpeg_decode_callback);
  if (result != 1) {
    ESP_LOGE(TAG_IMAGE, "Failed to open JPEG data: %d", result);
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
    current_image_component = nullptr;
    return false;
  }
  
  // Get image dimensions
  int orig_width = this->jpeg_decoder_->getWidth();
  int orig_height = this->jpeg_decoder_->getHeight();
  
  ESP_LOGI(TAG_IMAGE, "JPEG original dimensions: %dx%d", orig_width, orig_height);
  
  // Validate dimensions
  if (orig_width <= 0 || orig_height <= 0 || 
      orig_width > 2048 || orig_height > 2048) {
    ESP_LOGE(TAG_IMAGE, "Invalid JPEG dimensions: %dx%d", orig_width, orig_height);
    this->jpeg_decoder_->close();
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
    current_image_component = nullptr;
    return false;
  }
  
  // Gestion correcte du redimensionnement
  if (this->resize_width_ > 0 && this->resize_height_ > 0) {
    this->image_width_ = this->resize_width_;
    this->image_height_ = this->resize_height_;
    ESP_LOGI(TAG_IMAGE, "Will resize to: %dx%d", this->image_width_, this->image_height_);
  } else {
    this->image_width_ = orig_width;
    this->image_height_ = orig_height;
  }
  
  // Allocate buffer
  if (!this->allocate_image_buffer()) {
    this->jpeg_decoder_->close();
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
    current_image_component = nullptr;
    return false;
  }
  
  // Initialisation propre du buffer
  std::fill(this->image_buffer_.begin(), this->image_buffer_.end(), 0);
  
  ESP_LOGI(TAG_IMAGE, "Starting JPEG decode to RGB565...");
  
  // Paramètres de décodage optimisés
  // decode(x, y, flags)
  int decode_flags = 0;
  
  result = this->jpeg_decoder_->decode(0, 0, decode_flags);
  
  // Cleanup
  this->jpeg_decoder_->close();
  delete this->jpeg_decoder_;
  this->jpeg_decoder_ = nullptr;
  current_image_component = nullptr;
  
  if (result != 1) {
    ESP_LOGE(TAG_IMAGE, "Failed to decode JPEG: %d", result);
    return false;
  }
  
  ESP_LOGI(TAG_IMAGE, "JPEG decoded successfully to RGB565 format");
  
  // Validation finale
  if (this->image_buffer_.empty()) {
    ESP_LOGE(TAG_IMAGE, "Image buffer is empty after decoding");
    return false;
  }
  
  // Log amélioré pour debug
  if (this->image_buffer_.size() >= 8) {
    uint16_t pixel1 = (this->image_buffer_[1] << 8) | this->image_buffer_[0];
    uint16_t pixel2 = (this->image_buffer_[3] << 8) | this->image_buffer_[2];
    uint16_t pixel3 = (this->image_buffer_[5] << 8) | this->image_buffer_[4];
    uint16_t pixel4 = (this->image_buffer_[7] << 8) | this->image_buffer_[6];
    
    ESP_LOGD(TAG_IMAGE, "First 4 RGB565 pixels: 0x%04X 0x%04X 0x%04X 0x%04X", 
             pixel1, pixel2, pixel3, pixel4);
    
    // Décoder les couleurs pour verification
    uint8_t r1 = ((pixel1 >> 11) & 0x1F) << 3;
    uint8_t g1 = ((pixel1 >> 5) & 0x3F) << 2;
    uint8_t b1 = (pixel1 & 0x1F) << 3;
    ESP_LOGD(TAG_IMAGE, "First pixel RGB: R=%d G=%d B=%d", r1, g1, b1);
  }
  
  return true;
}

// =====================================================
// JPEG Decoder Callback Implementation
// =====================================================

int SdImageComponent::jpeg_decode_callback(JPEGDRAW *pDraw) {
  // Get the current component instance
  if (!current_image_component) {
    ESP_LOGE(TAG_IMAGE, "No current image component in callback");
    return 0; // Stop decoding
  }
  
  SdImageComponent *component = current_image_component;
  
  // Validate draw parameters
  if (!pDraw || !pDraw->pPixels) {
    ESP_LOGE(TAG_IMAGE, "Invalid draw parameters in callback");
    return 0;
  }
  
  // Log progress occasionally
  static int callback_count = 0;
  if (++callback_count % 100 == 0) {
    ESP_LOGD(TAG_IMAGE, "JPEG callback: %d,%d %dx%d", 
             pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  }
  
  // Process pixels - JPEGDEC provides RGB565 pixels directly
  uint16_t *pixels = (uint16_t *)pDraw->pPixels;
  
  for (int py = 0; py < pDraw->iHeight; py++) {
    for (int px = 0; px < pDraw->iWidth; px++) {
      int img_x = pDraw->x + px;
      int img_y = pDraw->y + py;
      
      // Apply resize scaling if needed
      if (component->resize_width_ > 0 && component->resize_height_ > 0) {
        int orig_width = component->jpeg_decoder_->getWidth();
        int orig_height = component->jpeg_decoder_->getHeight();
        
        if (orig_width > 0 && orig_height > 0) {
          img_x = (img_x * component->resize_width_) / orig_width;
          img_y = (img_y * component->resize_height_) / orig_height;
        }
      }
      
      // Bounds check
      if (img_x >= 0 && img_x < component->image_width_ && 
          img_y >= 0 && img_y < component->image_height_) {
        
        // Get RGB565 pixel
        uint16_t rgb565 = pixels[py * pDraw->iWidth + px];
        
        // Store directly as RGB565 in buffer
        size_t offset = (img_y * component->image_width_ + img_x) * 2;
        if (offset + 1 < component->image_buffer_.size()) {
          component->image_buffer_[offset] = rgb565 & 0xFF;
          component->image_buffer_[offset + 1] = (rgb565 >> 8) & 0xFF;
        }
      }
    }
    
    // Yield periodically to prevent watchdog timeout
    if (py % 16 == 0) {
      App.feed_wdt();
      yield();
    }
  }
  
  return 1; // Continue decoding
}

bool SdImageComponent::jpeg_decode_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  // Apply resize scaling if needed
  if (this->resize_width_ > 0 && this->resize_height_ > 0) {
    int orig_width = this->jpeg_decoder_->getWidth();
    int orig_height = this->jpeg_decoder_->getHeight();
    x = (x * this->resize_width_) / orig_width;
    y = (y * this->resize_height_) / orig_height;
  }
  
  // Bounds check
  if (x < 0 || x >= this->image_width_ || y < 0 || y >= this->image_height_) {
    return false;
  }
  
  this->set_pixel(x, y, r, g, b);
  return true;
}

// =====================================================
// Helper Methods
// =====================================================

bool SdImageComponent::allocate_image_buffer() {
  size_t buffer_size = this->get_buffer_size();
  
  if (buffer_size == 0 || buffer_size > 3 * 1024 * 1024) { // 3MB limit for ESP32P4
    ESP_LOGE(TAG_IMAGE, "Invalid buffer size: %zu bytes", buffer_size);
    return false;
  }
  
  this->image_buffer_.clear();
  this->image_buffer_.resize(buffer_size, 0);
  ESP_LOGD(TAG_IMAGE, "Allocated image buffer: %zu bytes", buffer_size);
  return true;
}

void SdImageComponent::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (x < 0 || x >= this->image_width_ || y < 0 || y >= this->image_height_) {
    return;
  }
  
  size_t offset = (y * this->image_width_ + x) * this->get_pixel_size();
  
  if (offset + this->get_pixel_size() > this->image_buffer_.size()) {
    return;
  }
  
  switch (this->format_) {
    case ImageFormat::RGB565: {
      uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
      this->image_buffer_[offset] = rgb565 & 0xFF;
      this->image_buffer_[offset + 1] = (rgb565 >> 8) & 0xFF;
      break;
    }
    case ImageFormat::RGB888:
      this->image_buffer_[offset] = r;
      this->image_buffer_[offset + 1] = g;
      this->image_buffer_[offset + 2] = b;
      break;
    case ImageFormat::RGBA:
      this->image_buffer_[offset] = r;
      this->image_buffer_[offset + 1] = g;
      this->image_buffer_[offset + 2] = b;
      this->image_buffer_[offset + 3] = a;
      break;
  }
}

size_t SdImageComponent::get_pixel_size() const {
  switch (this->format_) {
    case ImageFormat::RGB565: return 2;
    case ImageFormat::RGB888: return 3;
    case ImageFormat::RGBA: return 4;
    default: return 2;
  }
}

size_t SdImageComponent::get_buffer_size() const {
  return this->image_width_ * this->image_height_ * this->get_pixel_size();
}

std::string SdImageComponent::format_to_string() const {
  switch (this->format_) {
    case ImageFormat::RGB565: return "RGB565";
    case ImageFormat::RGB888: return "RGB888";
    case ImageFormat::RGBA: return "RGBA";
    default: return "Unknown";
  }
}

std::string SdImageComponent::get_debug_info() const {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), 
    "SdImage[%s]: %dx%d, %s, loaded=%s, size=%zu bytes",
    this->file_path_.c_str(),
    this->image_width_, this->image_height_,
    this->format_to_string().c_str(),
    this->image_loaded_ ? "yes" : "no",
    this->image_buffer_.size()
  );
  return std::string(buffer);
}

void SdImageComponent::list_directory_contents(const std::string &dir_path) {
  ESP_LOGI(TAG_IMAGE, "Directory listing for: %s", dir_path.c_str());
  
  DIR *dir = opendir(dir_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG_IMAGE, "Cannot open directory: %s (errno: %d)", dir_path.c_str(), errno);
    return;
  }
  
  struct dirent *entry;
  int file_count = 0;
  
  while ((entry = readdir(dir)) != nullptr) {
    std::string full_path = dir_path;
    if (full_path.back() != '/') full_path += "/";
    full_path += entry->d_name;
    
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
      if (S_ISREG(st.st_mode)) {
        ESP_LOGI(TAG_IMAGE, "  📄 %s (%ld bytes)", entry->d_name, (long)st.st_size);
        file_count++;
      } else if (S_ISDIR(st.st_mode)) {
        ESP_LOGI(TAG_IMAGE, "  📁 %s/", entry->d_name);
      }
    }
  }
  
  closedir(dir);
  ESP_LOGI(TAG_IMAGE, "Total files: %d", file_count);
}

bool SdImageComponent::extract_jpeg_dimensions(const std::vector<uint8_t> &data, int &width, int &height) const {
  for (size_t i = 0; i < data.size() - 10; i++) {
    if (data[i] == 0xFF) {
      uint8_t marker = data[i + 1];
      if (marker >= 0xC0 && marker <= 0xC3) {
        if (i + 9 < data.size()) {
          height = (data[i + 5] << 8) | data[i + 6];
          width = (data[i + 7] << 8) | data[i + 8];
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace storage
}  // namespace esphome












