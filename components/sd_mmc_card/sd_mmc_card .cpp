#include "sd_mmc_card.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

#include <algorithm>
#include <vector>
#include <cstdio>
#include <memory>

#include "math.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"

int constexpr SD_OCR_SDHC_CAP = (1 << 30);  // value defined in esp-idf/components/sdmmc/include/sd_protocol_defs.h
#endif

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

// Buffer size configuration based on file size
static constexpr size_t BUFFER_SIZE_STANDARD = 64 * 1024;      // 64KB for standard files
static constexpr size_t BUFFER_SIZE_LARGE = 128 * 1024;        // 128KB for large files (>50MB)
static constexpr size_t BUFFER_SIZE_VERY_LARGE = 256 * 1024;   // 256KB for very large files (>300MB)
static constexpr size_t FILE_SIZE_LARGE = 50 * 1024 * 1024;    // 50MB
static constexpr size_t FILE_SIZE_VERY_LARGE = 300 * 1024 * 1024; // 300MB

#ifdef USE_ESP_IDF
static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const std::string MOUNT_POINT("/sdcard");

std::string build_path(const char *path) { return MOUNT_POINT + path; }
#endif

#ifdef USE_SENSOR
FileSizeSensor::FileSizeSensor(sensor::Sensor *sensor, std::string const &path) : sensor(sensor), path(path) {}
#endif

void SdMmc::loop() {
  // Mise à jour périodique des capteurs (possible toutes les X secondes)
  static uint32_t last_update = 0;
  if (millis() - last_update > 30000) {  // Mise à jour toutes les 30 secondes
    this->update_sensors();
    last_update = millis();
  }
}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD MMC Component (Optimized with PSRAM)");
  ESP_LOGCONFIG(TAG, "  Mode 1 bit: %s", TRUEFALSE(this->mode_1bit_));
  ESP_LOGCONFIG(TAG, "  CLK Pin: %d", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: %d", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  DATA0 Pin: %d", this->data0_pin_);
  if (!this->mode_1bit_) {
    ESP_LOGCONFIG(TAG, "  DATA1 Pin: %d", this->data1_pin_);
    ESP_LOGCONFIG(TAG, "  DATA2 Pin: %d", this->data2_pin_);
    ESP_LOGCONFIG(TAG, "  DATA3 Pin: %d", this->data3_pin_);
  }
  if (this->power_ctrl_pin_ != nullptr) {
    LOG_PIN("  Power Ctrl Pin: ", this->power_ctrl_pin_);
  }

  // Affichage de l'utilisation de la mémoire
  ESP_LOGCONFIG(TAG, "  PSRAM disponible: %zu KB", esp_get_free_heap_size() / 1024);
  
  if (!this->is_failed()) {
    const char *freq_unit = card_->real_freq_khz < 1000 ? "kHz" : "MHz";
    const float freq = card_->real_freq_khz < 1000 ? card_->real_freq_khz : card_->real_freq_khz / 1000.0;
    const char *max_freq_unit = card_->max_freq_khz < 1000 ? "kHz" : "MHz";
    const float max_freq = card_->max_freq_khz < 1000 ? card_->max_freq_khz : card_->max_freq_khz / 1000.0;
    ESP_LOGCONFIG(TAG, "  Card Speed:  %.2f %s (limit: %.2f %s)%s", freq, freq_unit, max_freq, max_freq_unit, card_->is_ddr ? ", DDR" : "");
    ESP_LOGCONFIG(TAG, "  Buffer Sizes: Standard=%dKB, Large=%dKB, Very Large=%dKB", 
                  BUFFER_SIZE_STANDARD/1024, BUFFER_SIZE_LARGE/1024, BUFFER_SIZE_VERY_LARGE/1024);
  }

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Used space", this->used_space_sensor_);
  LOG_SENSOR("  ", "Total space", this->total_space_sensor_);
  LOG_SENSOR("  ", "Free space", this->free_space_sensor_);
  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      LOG_SENSOR("  ", "File size", sensor.sensor);
  }
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "SD Card Type", this->sd_card_type_text_sensor_);
#endif
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed : %s", SdMmc::error_code_to_string(this->init_error_).c_str());
    return;
  }
}

#ifdef USE_ESP_IDF
void SdMmc::setup() {
  // Vérifier la disponibilité de la PSRAM
  if (esp_psram_is_initialized()) {
    size_t psram_size = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM initialized, size: %zu bytes (%zu MB)", psram_size, psram_size / (1024 * 1024));
  } else {
    ESP_LOGW(TAG, "PSRAM not initialized or not available, using standard memory");
  }

  // Power control
  if (this->power_ctrl_pin_ != nullptr) {
    this->power_ctrl_pin_->setup();
    this->power_ctrl_pin_->digital_write(true);  // Activer l'alimentation
    delay(100);  // Attendre la stabilisation
  }
  
  // Configuration optimisée
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 16,
    .allocation_unit_size = 64 * 1024  // 64KB optimise l'écriture des fichiers
  };
  
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  
  // Optimisation de la vitesse - utiliser SDMMC_FREQ_HIGHSPEED (50MHz) ou SDMMC_FREQ_DEFAULT (40MHz) selon la stabilité
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  // 50MHz
  
  // Activer DDR seulement en mode 4 bits
  if (!this->mode_1bit_) {
    host.flags |= SDMMC_HOST_FLAG_DDR;
  } else {
    host.flags &= ~SDMMC_HOST_FLAG_DDR;
  }
  
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = this->mode_1bit_ ? 1 : 4;
  
  // Configuration des pins
  #ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);
  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }
  #endif
  
  // Enable internal pullups
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  
  // Logique de retry améliorée
  esp_err_t ret = ESP_FAIL;
  for (int attempt = 1; attempt <= 3; attempt++) {
    ESP_LOGI(TAG, "Mounting SD Card (attempt %d/3)...", attempt);
    
    // Réinitialiser l'alimentation entre les tentatives si nous avons un pin de contrôle
    if (attempt > 1 && this->power_ctrl_pin_ != nullptr) {
      this->power_ctrl_pin_->digital_write(false);
      delay(100);
      this->power_ctrl_pin_->digital_write(true);
      delay(200);  // Attendre que la carte se stabilise
    }
    
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT.c_str(), &host, &slot_config, &mount_config, &this->card_);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "SD Card mounted successfully!");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(300));  // Pause plus longue entre tentatives
  }
  
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      this->init_error_ = ErrorCode::ERR_MOUNT;
      ESP_LOGE(TAG, "Failed to mount filesystem on SD card");
    } else {
      this->init_error_ = ErrorCode::ERR_NO_CARD;
      ESP_LOGE(TAG, "No SD card detected");
    }
    mark_failed();
    return;
  }
  
  // Diagnostics de la carte
  ESP_LOGI(TAG, "SD Card Info:");
  ESP_LOGI(TAG, "  Name: %s", this->card_->cid.name);
  ESP_LOGI(TAG, "  Type: %s", sd_card_type().c_str());
  ESP_LOGI(TAG, "  Speed: %d kHz (max: %d kHz)", this->card_->real_freq_khz, this->card_->max_freq_khz);
  ESP_LOGI(TAG, "  Size: %llu MB", ((uint64_t)this->card_->csd.capacity * this->card_->csd.sector_size) / (1024 * 1024));
  ESP_LOGI(TAG, "  DDR Mode: %s", this->card_->is_ddr ? "Enabled" : "Disabled");
  
  #ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type());
  #endif
  
  update_sensors();
}
#endif

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing to file: %s", path);
  this->write_file(path, buffer, len, "w");
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s", path);
  this->write_file(path, buffer, len, "a");
}

#ifdef USE_ESP_IDF
void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolut_path = build_path(path);
  FILE *file = NULL;
  file = fopen(absolut_path.c_str(), mode);
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", strerror(errno));
    return;
  }
  
  size_t written = fwrite(buffer, 1, len, file);
  if (written != len) {
    ESP_LOGE(TAG, "Failed to write to file: %s", strerror(errno));
  }
  
  // Assurer que les données sont écrites sur le disque
  fflush(file);
  fsync(fileno(file));
  fclose(file);
  
  this->update_sensors();
}

void SdMmc::write_file_chunked(const char *path, const uint8_t *buffer, size_t len, size_t chunk_size) {
  std::string absolut_path = build_path(path);
  FILE *file = NULL;
  
  // Déterminer la taille optimale du buffer en fonction de la taille du fichier
  size_t optimal_chunk_size = chunk_size;
  if (len > FILE_SIZE_VERY_LARGE) {
    optimal_chunk_size = BUFFER_SIZE_VERY_LARGE;
  } else if (len > FILE_SIZE_LARGE) {
    optimal_chunk_size = BUFFER_SIZE_LARGE;
  } else {
    optimal_chunk_size = BUFFER_SIZE_STANDARD;
  }
  
  ESP_LOGI(TAG, "Writing file chunked with buffer size: %zu KB", optimal_chunk_size / 1024);
  
  file = fopen(absolut_path.c_str(), "a");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for chunked writing: %s", strerror(errno));
    return;
  }

  size_t written = 0;
  uint32_t start_time = millis();
  
  while (written < len) {
    size_t to_write = std::min(optimal_chunk_size, len - written);
    size_t bytes_written = fwrite(buffer + written, 1, to_write, file);
    
    if (bytes_written != to_write) {
      ESP_LOGE(TAG, "Failed to write chunk to file: %s (wrote %zu of %zu)", 
               strerror(errno), bytes_written, to_write);
      break;
    }
    
    written += bytes_written;
    
    // Reset WDT periodically
    if (written % (1024 * 1024) == 0) {  // Reset every 1MB
      esp_task_wdt_reset();
      
      // Log progress for large files
      if (len > 10 * 1024 * 1024) {  // 10MB
        float progress = (float)written / len * 100.0f;
        float speed_kbps = written / (float)(millis() - start_time) * 1000.0f / 1024.0f;
        ESP_LOGI(TAG, "Writing progress: %.1f%% (%.2f KB/s)", progress, speed_kbps);
      }
      
      // Flush périodiquement pour éviter la perte de données en cas de crash
      fflush(file);
    }
  }
  
  // Log final statistics
  uint32_t elapsed_time = millis() - start_time;
  float speed_kbps = written / (float)elapsed_time * 1000.0f / 1024.0f;
  ESP_LOGI(TAG, "File write complete: %zu bytes in %u ms (%.2f KB/s)", 
           written, elapsed_time, speed_kbps);
  
  // Assurer que les données sont écrites sur le disque
  fflush(file);
  fsync(fileno(file));
  fclose(file);
  
  this->update_sensors();
}
#else
void SdMmc::write_file_chunked(const char *path, const uint8_t *buffer, size_t len, size_t chunk_size) {
  ESP_LOGV(TAG, "Writing chunked to file: %s", path);
  size_t written = 0;
  while (written < len) {
    size_t to_write = std::min(chunk_size, len - written);
    this->write_file(path, buffer + written, to_write, "a");
    written += to_write;
  }
}
#endif

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  std::vector<FileInfo> infos = list_directory_file_info(path, depth);
  list.resize(infos.size());  // Préallouer pour éviter les réallocations
  std::transform(infos.cbegin(), infos.cend(), list.begin(), [](FileInfo const &info) { return info.path; });
  return list;
}

std::vector<std::string> SdMmc::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> list;
  list_directory_file_info_rec(path, depth, list);
  return list;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(std::string path, uint8_t depth) {
  return this->list_directory_file_info(path.c_str(), depth);
}

#ifdef USE_ESP_IDF
std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory: %s", path);
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
    return list;
  }
  
  // Préallocation pour éviter les réallocations fréquentes
  list.reserve(list.size() + 50);  // Réserver de l'espace pour 50 entrées supplémentaires
  
  char entry_absolut_path[FILE_PATH_MAX];
  char entry_path[FILE_PATH_MAX];
  const size_t dirpath_len = MOUNT_POINT.size();
  size_t entry_path_len = strlen(path);
  strlcpy(entry_path, path, sizeof(entry_path));
  strlcpy(entry_path + entry_path_len, "/", sizeof(entry_path) - entry_path_len);
  entry_path_len = strlen(entry_path);

  strlcpy(entry_absolut_path, MOUNT_POINT.c_str(), sizeof(entry_absolut_path));
  struct dirent *entry;
  
  uint32_t entry_count = 0;
  uint32_t start_time = millis();
  
  while ((entry = readdir(dir)) != nullptr) {
    entry_count++;
    
    // Reset le WDT tous les 50 fichiers pour éviter le timeout
    if (entry_count % 50 == 0) {
      esp_task_wdt_reset();
    }
    
    size_t file_size = 0;
    strlcpy(entry_path + entry_path_len, entry->d_name, sizeof(entry_path) - entry_path_len);
    strlcpy(entry_absolut_path + dirpath_len, entry_path, sizeof(entry_absolut_path) - dirpath_len);
    
    if (entry->d_type != DT_DIR) {
      struct stat info;
      if (stat(entry_absolut_path, &info) < 0) {
        ESP_LOGE(TAG, "Failed to stat file: %s '%s' %s", strerror(errno), entry->d_name, entry_absolut_path);
      } else {
        file_size = info.st_size;
      }
    }
    list.emplace_back(entry_path, file_size, entry->d_type == DT_DIR);
    
    if (entry->d_type == DT_DIR && depth)
      list_directory_file_info_rec(entry_path, depth - 1, list);
  }
  
  closedir(dir);
  
  if (entry_count > 100) {  // Log seulement pour les grands répertoires
    ESP_LOGI(TAG, "Listed %u entries in %u ms", entry_count, millis() - start_time);
  }
  
  return list;
}

bool SdMmc::is_directory(const char *path) {
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (dir) {
    closedir(dir);
  }
  return dir != nullptr;
}

size_t SdMmc::file_size(const char *path) {
  std::string absolut_path = build_path(path);
  struct stat info;
  if (stat(absolut_path.c_str(), &info) < 0) {
    ESP_LOGE(TAG, "Failed to stat file '%s': %s", path, strerror(errno));
    return -1;
  }
  return info.st_size;
}

std::string SdMmc::sd_card_type() const {
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
  return "UNKNOWN";
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->card_ == nullptr)
    return;

  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;
  uint64_t total_bytes = -1, free_bytes = -1, used_bytes = -1;
  auto res = f_getfree(MOUNT_POINT.c_str(), &fre_clust, &fs);
  if (!res) {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    total_bytes = static_cast<uint64_t>(tot_sect) * FF_SS_SDCARD;
    free_bytes = static_cast<uint64_t>(fre_sect) * FF_SS_SDCARD;
    used_bytes = total_bytes - free_bytes;
  }

  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(free_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path.c_str()));
  }
#endif
}

bool SdMmc::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  std::string absolut_path = build_path(path);
  if (mkdir(absolut_path.c_str(), 0777) < 0) {
    ESP_LOGE(TAG, "Failed to create directory '%s': %s", path, strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a directory: %s", path);
    return false;
  }
  std::string absolut_path = build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory '%s': %s", path, strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a file: %s", path);
    return false;
  }
  std::string absolut_path = build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove file '%s': %s", path, strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

// Lecture optimisée utilisant PSRAM si disponible
std::vector<uint8_t> SdMmc::read_file(const char *path) {
  ESP_LOGV(TAG, "Read File: %s", path);
  std::string absolut_path = build_path(path);
  
  // Vérifier d'abord la taille du fichier
  size_t file_size = this->file_size(path);
  if (file_size == (size_t)-1) {
    ESP_LOGE(TAG, "Failed to get file size for '%s'", path);
    return {};
  }
  
  // Log pour les gros fichiers
  if (file_size > 1024 * 1024) {
    ESP_LOGI(TAG, "Reading large file: %s (%zu MB)", path, file_size / (1024 * 1024));
  }
  
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", absolut_path.c_str());
    return {};
  }

  std::unique_ptr<FILE, decltype(&fclose)> file_guard(file, fclose);
  
  // Utiliser PSRAM si disponible et si le fichier est suffisamment grand
  std::vector<uint8_t> res;
  
  // Essayer d'utiliser PSRAM pour les gros fichiers
  if (file_size > 1024 * 1024 && esp_psram_is_initialized()) {
    // Allouer un buffer dans la PSRAM
    uint8_t* psram_buffer = (uint8_t*)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (psram_buffer != nullptr) {
      ESP_LOGI(TAG, "Using PSRAM for file buffer");
      size_t read_len = fread(psram_buffer, 1, file_size, file);
      
      if (read_len != file_size) {
        ESP_LOGE(TAG, "Read incomplete: expected %zu bytes, got %zu (%s)", 
                 file_size, read_len, strerror(errno));
        heap_caps_free(psram_buffer);
        return {};
      }
      
      // Copier depuis PSRAM vers le vecteur résultat
      res.assign(psram_buffer, psram_buffer + file_size);
      heap_caps_free(psram_buffer);
      return res;
    } else {
      ESP_LOGW(TAG, "Failed to allocate PSRAM buffer, using standard memory");
    }
  }
  
  // Si PSRAM n'est pas disponible ou l'allocation a échoué, utiliser la méthode standard
  res.resize(file_size);
  size_t read_len = fread(res.data(), 1, file_size, file);

  if (read_len != file_size) {
    ESP_LOGE(TAG, "Read incomplete: expected %zu bytes, got %zu (%s)", 
             file_size, read_len, strerror(errno));
    return {};
  }

  return res;
}

// Lecture en streaming par chunks avec reset du WDT
void SdMmc::read_file_stream(const char *path, size_t offset, size_t chunk_size,
                             std::function<void(const uint8_t*, size_t)> callback) {
  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", absolut_path.c_str());
    return;
  }

  std::unique_ptr<FILE, decltype(&fclose)> file_guard(file, fclose);

  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to position %zu in file: %s (errno: %d)", offset, absolut_path.c_str(), errno);
    return;
  }

  std::vector<uint8_t> buffer(chunk_size);
  size_t read = 0;
  size_t bytes_since_reset = 0;

  while ((read = fread(buffer.data(), 1, chunk_size, file)) > 0) {
    callback(buffer.data(), read);
    bytes_since_reset += read;

    if (bytes_since_reset >= 32 * 1024) {
      esp_task_wdt_reset();
      bytes_since_reset = 0;
    }
  }

  if (ferror(file)) {
    ESP_LOGE(TAG, "Error reading file: %s", absolut_path.c_str());
  }
}


#endif
size_t SdMmc::file_size(std::string const &path) { return this->file_size(path.c_str()); }

bool SdMmc::is_directory(std::string const &path) { return this->is_directory(path.c_str()); }

bool SdMmc::delete_file(std::string const &path) { return this->delete_file(path.c_str()); }

std::vector<uint8_t> SdMmc::read_file(std::string const &path) { return this->read_file(path.c_str()); }

std::vector<uint8_t> SdMmc::read_file_chunked(std::string const &path, size_t offset, size_t chunk_size) {
  return this->read_file_chunked(path.c_str(), offset, chunk_size);
}

#ifdef USE_SENSOR
void SdMmc::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

void SdMmc::set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }

void SdMmc::set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }

void SdMmc::set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }

void SdMmc::set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }

void SdMmc::set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }

void SdMmc::set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }

void SdMmc::set_mode_1bit(bool b) { this->mode_1bit_ = b; }

void SdMmc::set_power_ctrl_pin(GPIOPin *pin) { this->power_ctrl_pin_ = pin; }

std::string SdMmc::error_code_to_string(SdMmc::ErrorCode code) {
  switch (code) {
    case ErrorCode::ERR_PIN_SETUP:
      return "Failed to set pins";
    case ErrorCode::ERR_MOUNT:
      return "Failed to mount card";
    case ErrorCode::ERR_NO_CARD:
      return "No card found";
    default:
      return "Unknown error";
  }
}

long double convertBytes(uint64_t value, MemoryUnits unit) {
  return value * 1.0 / pow(1024, static_cast<uint64_t>(unit));
}

FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {}

}  // namespace sd_mmc_card
}  // namespace esphome
