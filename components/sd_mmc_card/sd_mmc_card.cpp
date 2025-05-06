#include "sd_mmc_card.h"
#include "esp_task_wdt.h"

#include <algorithm>
#include <vector>
#include <cstdio>

#include "math.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int constexpr SD_OCR_SDHC_CAP = (1 << 30);  // value defined in esp-idf/components/sdmmc/include/sd_protocol_defs.h
#endif

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

// Taille optimale des buffers pour les transferts volumineux
static constexpr size_t OPTIMAL_BUFFER_SIZE = 64 * 1024;  // 64KB

#ifdef USE_ESP_IDF
static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const std::string MOUNT_POINT("/sdcard");

std::string build_path(const char *path) { return MOUNT_POINT + path; }
#endif

#ifdef USE_SENSOR
FileSizeSensor::FileSizeSensor(sensor::Sensor *sensor, std::string const &path) : sensor(sensor), path(path) {}
#endif

void SdMmc::loop() {
  // Mettre à jour les capteurs périodiquement si nécessaire
  static uint32_t last_update = 0;
  if (millis() - last_update > 60000) {  // Mise à jour toutes les 60 secondes
    update_sensors();
    last_update = millis();
  }
}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD MMC Component");
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

  if (!this->is_failed()) {
    const char *freq_unit = card_->real_freq_khz < 1000 ? "kHz" : "MHz";
    const float freq = card_->real_freq_khz < 1000 ? card_->real_freq_khz : card_->real_freq_khz / 1000.0;
    const char *max_freq_unit = card_->max_freq_khz < 1000 ? "kHz" : "MHz";
    const float max_freq = card_->max_freq_khz < 1000 ? card_->max_freq_khz : card_->max_freq_khz / 1000.0;
    ESP_LOGCONFIG(TAG, "  Card Speed:  %.2f %s (limit: %.2f %s)%s", freq, freq_unit, max_freq, max_freq_unit, 
                 card_->is_ddr ? ", DDR" : "");
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
  // Power control
  if (this->power_ctrl_pin_ != nullptr) {
    this->power_ctrl_pin_->setup();
    this->power_ctrl_pin_->digital_write(true);  // Active l'alimentation
    vTaskDelay(pdMS_TO_TICKS(100));  // Attendre que l'alimentation se stabilise
  }
  
  // Configuration optimisée pour les gros fichiers
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 16,
    .allocation_unit_size = 512 * 1024  // 512KB cluster size optimal pour gros fichiers
  };
  
  // Configuration de l'host SDMMC avec des paramètres optimaux
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  // 50MHz
  host.command_timeout_ms = 2000;  // Augmenter le timeout pour les opérations lentes
  host.io_timeout_ms = 60000;      // Timeout élevé pour les transferts volumineux 
  
  // Configurer le mode 4-bit (plus rapide) ou 1-bit selon la configuration
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = this->mode_1bit_ ? 1 : 4;
  
  // Configuration des flags avancés
  if (!this->mode_1bit_) {
    host.flags |= SDMMC_HOST_FLAG_4BIT;
    // Activer le mode DDR si possible (doublera la vitesse en 4-bit mode)
    // Certaines cartes SD peuvent ne pas supporter le mode DDR
    bool use_ddr = true; // Vous pouvez le rendre configurable
    if (use_ddr) {
      host.flags |= SDMMC_HOST_FLAG_DDR;
      ESP_LOGI(TAG, "DDR mode enabled for SD card");
    }
  }
  
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
  
  // Activer les pullups internes
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  
  // Tentatives multiples de montage avec délais
  esp_err_t ret = ESP_FAIL;
  for (int attempt = 1; attempt <= 3; attempt++) {
    ESP_LOGI(TAG, "Mounting SD Card (attempt %d/3)...", attempt);
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT.c_str(), &host, &slot_config, &mount_config, &this->card_);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "SD Card mounted successfully!");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(300)); // Délai plus long entre les tentatives
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
  
  // Diagnostic et information de la carte
  ESP_LOGI(TAG, "SD Card Info:");
  ESP_LOGI(TAG, "  Name: %s", this->card_->cid.name);
  ESP_LOGI(TAG, "  Type: %s", sd_card_type().c_str());
  ESP_LOGI(TAG, "  Speed: %d kHz (max: %d kHz)", this->card_->real_freq_khz, this->card_->max_freq_khz);
  ESP_LOGI(TAG, "  Size: %llu MB", ((uint64_t)this->card_->csd.capacity * this->card_->csd.sector_size) / (1024 * 1024));
  ESP_LOGI(TAG, "  CSD version: %d", this->card_->csd.csd_ver);
  ESP_LOGI(TAG, "  Sector size: %d bytes", this->card_->csd.sector_size);
  ESP_LOGI(TAG, "  DDR Mode: %s", this->card_->is_ddr ? "Enabled" : "Disabled");
  
  // Vérifier si la carte est en 1-bit mode alors qu'on a configuré en 4-bit
  if (!this->mode_1bit_ && !(host.flags & SDMMC_HOST_FLAG_4BIT)) {
    ESP_LOGW(TAG, "Card is operating in 1-bit mode despite 4-bit configuration");
  }
  
  #ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type());
  #endif
  
  update_sensors();
  
  // Optionnel: effectuer un test de vitesse d'écriture/lecture
  run_speed_test();
}

void SdMmc::run_speed_test() {
  ESP_LOGI(TAG, "Running SD card speed test...");
  
  const size_t test_size = 4 * 1024 * 1024;  // 4MB
  size_t chunk_size = OPTIMAL_BUFFER_SIZE;   // 64KB
  std::vector<uint8_t> buffer(chunk_size, 0xAA);  // Remplir avec un motif
  const char *test_file = "/speed_test.bin";
  std::string test_path = build_path(test_file);
  float write_speed = 0, read_speed = 0;
  
  // Test d'écriture
  uint32_t start_time = millis();
  FILE *file = fopen(test_path.c_str(), "wb");
  if (file != nullptr) {
    size_t total_written = 0;
    while (total_written < test_size) {
      size_t written = fwrite(buffer.data(), 1, chunk_size, file);
      total_written += written;
      if (written != chunk_size) {
        ESP_LOGW(TAG, "Write test incomplete: %zu bytes written", total_written);
        break;
      }
    }
    fclose(file);
    uint32_t elapsed = millis() - start_time;
    write_speed = ((float)total_written / 1024.0f) / (elapsed / 1000.0f);  // KB/s
    ESP_LOGI(TAG, "Write speed: %.2f KB/s", write_speed);
  } else {
    ESP_LOGE(TAG, "Failed to open file for write test");
  }
  
  // Test de lecture
  start_time = millis();
  file = fopen(test_path.c_str(), "rb");
  if (file != nullptr) {
    size_t total_read = 0;
    while (total_read < test_size) {
      size_t read = fread(buffer.data(), 1, chunk_size, file);
      total_read += read;
      if (read != chunk_size && total_read < test_size) {
        ESP_LOGW(TAG, "Read test incomplete: %zu bytes read", total_read);
        break;
      }
    }
    fclose(file);
    uint32_t elapsed = millis() - start_time;
    read_speed = ((float)total_read / 1024.0f) / (elapsed / 1000.0f);  // KB/s
    ESP_LOGI(TAG, "Read speed: %.2f KB/s", read_speed);
  } else {
    ESP_LOGE(TAG, "Failed to open file for read test");
  }
  
  // Supprimer le fichier de test
  remove(test_path.c_str());
  
  ESP_LOGI(TAG, "Speed test completed - Write: %.2f KB/s, Read: %.2f KB/s", write_speed, read_speed);
}
#endif

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing to file: %s (%zu bytes)", path, len);
  this->write_file(path, buffer, len, "w");
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s (%zu bytes)", path, len);
  this->write_file(path, buffer, len, "a");
}

#ifdef USE_ESP_IDF
void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolut_path = build_path(path);
  FILE *file = nullptr;
  
  file = fopen(absolut_path.c_str(), mode);
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s (errno: %d)", path, errno);
    return;
  }
  
  // Pour les gros fichiers, procédons par blocs pour éviter les problèmes de mémoire et WDT
  const size_t chunk_size = OPTIMAL_BUFFER_SIZE;
  size_t total_written = 0;
  
  uint32_t start_time = millis();
  while (total_written < len) {
    size_t to_write = std::min(chunk_size, len - total_written);
    size_t written = fwrite(buffer + total_written, 1, to_write, file);
    
    if (written != to_write) {
      ESP_LOGE(TAG, "Failed to write chunk to file: %s (written %zu of %zu bytes, errno: %d)", 
               path, written, to_write, errno);
      break;
    }
    
    total_written += written;
    
    // Rafraichir watchdog et afficher progression périodiquement pour gros fichiers
    if (total_written % (1 * 1024 * 1024) == 0) {  // Tous les 1MB
      esp_task_wdt_reset();
      
      uint32_t elapsed = millis() - start_time;
      float speed = elapsed > 0 ? ((float)total_written / 1024.0f) / (elapsed / 1000.0f) : 0;  // KB/s
      ESP_LOGI(TAG, "Writing: %zu/%zu bytes (%.1f%%), %.2f KB/s", 
               total_written, len, (float)total_written * 100 / len, speed);
    }
  }
  
  // Synchroniser les données sur la carte
  fflush(file);
  fsync(fileno(file));
  fclose(file);
  
  uint32_t elapsed = millis() - start_time;
  if (elapsed > 0) {
    float speed = ((float)total_written / 1024.0f) / (elapsed / 1000.0f);  // KB/s
    ESP_LOGI(TAG, "File write completed: %zu bytes, %.2f KB/s", total_written, speed);
  }
  
  this->update_sensors();
}

void SdMmc::write_file_chunked(const char *path, const uint8_t *buffer, size_t len, size_t chunk_size) {
  std::string absolut_path = build_path(path);
  FILE *file = nullptr;
  
  file = fopen(absolut_path.c_str(), "a");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for chunked writing: %s", path);
    return;
  }
  
  size_t total_written = 0;
  uint32_t start_time = millis();
  
  while (total_written < len) {
    // Limiter la taille maximale du chunk pour éviter problèmes de mémoire
    size_t to_write = std::min(chunk_size, len - total_written);
    
    // Écrire le chunk actuel
    size_t written = fwrite(buffer + total_written, 1, to_write, file);
    if (written != to_write) {
      ESP_LOGE(TAG, "Failed to write chunk: %zu of %zu bytes written", written, to_write);
      break;
    }
    
    total_written += written;
    
    // Rafraichir watchdog et afficher progression toutes les 4MB
    if (total_written % (4 * 1024 * 1024) == 0) { 
      esp_task_wdt_reset();
      fflush(file);  // Périodiquement synchroniser les données
      
      uint32_t elapsed = millis() - start_time;
      float speed = elapsed > 0 ? ((float)total_written / 1024.0f) / (elapsed / 1000.0f) : 0;
      ESP_LOGI(TAG, "Chunked writing: %zu/%zu bytes (%.1f%%), %.2f KB/s", 
               total_written, len, (float)total_written * 100 / len, speed);
    }
  }
  
  fflush(file);
  fsync(fileno(file));
  fclose(file);
  
  uint32_t elapsed = millis() - start_time;
  if (elapsed > 0) {
    float speed = ((float)total_written / 1024.0f) / (elapsed / 1000.0f);  // KB/s
    ESP_LOGI(TAG, "Chunked file write completed: %zu bytes, %.2f KB/s", total_written, speed);
  }
  
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
  list.resize(infos.size());
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
  ESP_LOGV(TAG, "Listing directory file info: %s (depth: %d)", path, depth);
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory '%s': %s", path, strerror(errno));
    return list;
  }
  
  char entry_absolut_path[FILE_PATH_MAX];
  char entry_path[FILE_PATH_MAX];
  const size_t dirpath_len = MOUNT_POINT.size();
  size_t entry_path_len = strlen(path);
  
  strlcpy(entry_path, path, sizeof(entry_path));
  if (entry_path_len > 0 && entry_path[entry_path_len-1] != '/') {
    strlcpy(entry_path + entry_path_len, "/", sizeof(entry_path) - entry_path_len);
    entry_path_len++;
  }

  strlcpy(entry_absolut_path, MOUNT_POINT.c_str(), sizeof(entry_absolut_path));
  struct dirent *entry;
  
  uint32_t count = 0;
  uint32_t start_time = millis();
  
  // Reset WDT au début pour éviter timeout pendant la liste
  esp_task_wdt_reset();
  
  while ((entry = readdir(dir)) != nullptr) {
    count++;
    
    // Reset WDT périodiquement pour les dossiers contenant beaucoup de fichiers
    if (count % 100 == 0) {
      esp_task_wdt_reset();
      ESP_LOGD(TAG, "Processed %u entries in directory '%s'", count, path);
    }
    
    size_t file_size = 0;
    strlcpy(entry_path + entry_path_len, entry->d_name, sizeof(entry_path) - entry_path_len);
    strlcpy(entry_absolut_path + dirpath_len, entry_path, sizeof(entry_absolut_path) - dirpath_len);
    
    if (entry->d_type != DT_DIR) {
      struct stat info;
      if (stat(entry_absolut_path, &info) < 0) {
        ESP_LOGW(TAG, "Failed to stat file: '%s' - %s", entry_path, strerror(errno));
      } else {
        file_size = info.st_size;
      }
    }
    
    list.emplace_back(entry_path, file_size, entry->d_type == DT_DIR);
    
    // Pour les répertoires, récursivement lister le contenu si profondeur > 0
    if (entry->d_type == DT_DIR && depth > 0) {
      // Éviter les dossiers "." et ".."
      if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
        list_directory_file_info_rec(entry_path, depth - 1, list);
      }
    }
  }
  
  closedir(dir);
  
  uint32_t elapsed = millis() - start_time;
  ESP_LOGI(TAG, "Listed %u items in directory '%s' in %u ms", count, path, elapsed);
  
  return list;
}

bool SdMmc::is_directory(const char *path) {
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (dir) {
    closedir(dir);
    return true;
  }
  return false;
}

size_t SdMmc::file_size(const char *path) {
  std::string absolut_path = build_path(path);
  struct stat info;
  if (stat(absolut_path.c_str(), &info) < 0) {
    ESP_LOGE(TAG, "Failed to stat file '%s': %s", path, strerror(errno));
    return 0;
  }
  return info.st_size;
}

std::string SdMmc::sd_card_type() const {
  if (this->card_ == nullptr)
    return "UNKNOWN";
    
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->card_ == nullptr)
    return;

  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;
  uint64_t total_bytes = 0, free_bytes = 0, used_bytes = 0;
  auto res = f_getfree(MOUNT_POINT.c_str(), &fre_clust, &fs);
  if (!res) {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    total_bytes = static_cast<uint64_t>(tot_sect) * FF_SS_SDCARD;
    free_bytes = static_cast<uint64_t>(fre_sect) * FF_SS_SDCARD;
    used_bytes = total_bytes - free_bytes;
    
    ESP_LOGD(TAG, "SD Card space: %llu MB total, %llu MB used, %llu MB free", 
             total_bytes / (1024 * 1024), used_bytes / (1024 * 1024), free_bytes / (1024 * 1024));
  } else {
    ESP_LOGW(TAG, "Failed to get filesystem info: error %d", res);
  }

  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(free_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
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
  if (rmdir(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory '%s': %s", path, strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Cannot delete '%s': it is a directory", path);
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

std::vector<uint8_t> SdMmc::read_file(const char *path) {
  ESP_LOGV(TAG, "Read File: %s", path);

  std::string absolut_path = build_path(path);
  size_t file_size = this->file_size(path);
  
  if (file_size == 0) {
    ESP_LOGE(TAG, "File '%s' is empty or does not exist", path);
    return {};
  }
  
  // Pour les fichiers volumineux (>10MB), utiliser un autre approche
  if (file_size > 10 * 1024 * 1024) {
    ESP_LOGW(TAG, "File '%s' is very large (%zu bytes), consider using read_file_stream instead", 
             path, file_size);
  }
  
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
    return {};
  }

  std::vector<uint8_t> res(file_size);
  size_t total_read = 0;
  uint32_t start_time = millis();
  
  // Lire le fichier par chunks pour éviter les problèmes de watchdog
  const size_t chunk_size = OPTIMAL_BUFFER_SIZE;
  
  while (total_read < file_size) {
    size_t to_read = std::min(chunk_size, file_size - total_read);
    size_t read_len = fread(res.data() + total_read, 1, to_read, file);
    
    if (read_len != to_read) {
      ESP_LOGE(TAG, "Read error: expected %zu bytes, got %zu (%s)", 
               to_read, read_len, strerror(errno));
      fclose(file);
      return {};
    }
    
    total_read += read_len;
    
    // Reset watchdog périodiquement pour gros fichiers
    if (total_read % (4 * 1024 * 1024) == 0) {  // Tous les 4MB
      esp_task_wdt_reset();
      
      uint32_t elapsed = millis() - start_time;
      float speed = elapsed > 0 ? ((float)total_read / 1024.0f) / (elapsed / 1000.0f) : 0;
      ESP_LOGI(TAG, "Reading: %zu/%zu bytes (%.1f%%), %.2f KB/s", 
               total_read, file_size, (float)total_read * 100 / file_size, speed);
    }
  }
  
  fclose(file);
  
  uint32_t elapsed = millis() - start_time;
  if (elapsed > 0) {
    float speed = ((float)total_read / 1024.0f) / (elapsed / 1000.0f);  // KB/s
    ESP_LOGI(TAG, "File read completed: %zu bytes, %.2f KB/s", total_read, speed);
  }
  
  return res;
}

// Version optimisée pour lire des fichiers par morceaux
std::vector<uint8_t> SdMmc::read_file_chunked(const char *path, size_t offset, size_t chunk_size) {
  ESP_LOGV(TAG, "Reading file chunked: %s (offset: %zu, size: %zu)", path, offset, chunk_size);
  
  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
    return {};
  }
  
  // Se positionner au bon offset
  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to position %zu in file: %s", offset, path);
    fclose(file);
    return {};
  }
  
  // Vérifier la taille réelle disponible à partir de l'offset
  size_t file_size = this->file_size(path);
  if (offset >= file_size) {
    ESP_LOGE(TAG, "Offset %zu is beyond file size %zu", offset, file_size);
    fclose(file);
    return {};
  }
  
  size_t available_size = file_size - offset;
  size_t read_size = std::min(chunk_size, available_size);
  
  // Préparer le buffer de retour
  std::vector<uint8_t> buffer(read_size);
  
  // Lire les données
  size_t bytes_read = fread(buffer.data(), 1, read_size, file);
  fclose(file);
  
  if (bytes_read != read_size) {
    ESP_LOGW(TAG, "Read incomplete: expected %zu bytes, got %zu", read_size, bytes_read);
    buffer.resize(bytes_read);  // Ajuster la taille du buffer
  }
  
  return buffer;
}

// Lecture en streaming pour gros fichiers avec callback
void SdMmc::read_file_stream(const char *path, size_t offset, size_t chunk_size,
                            std::function<void(const uint8_t*, size_t)> callback) {
  ESP_LOGV(TAG, "Streaming file: %s (offset: %zu, chunk: %zu)", path, offset, chunk_size);

  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for streaming: %s", path);
    return;
  }

  // Vérifier taille et position
  size_t file_size = this->file_size(path);
  if (offset >= file_size) {
    ESP_LOGE(TAG, "Offset %zu is beyond file size %zu", offset, file_size);
    fclose(file);
    return;
  }
  
  // Se positionner au bon offset
  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to position %zu", offset);
    fclose(file);
    return;
  }

  // Allouer un buffer optimisé pour DMA si possible
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
  if (buffer == nullptr) {
    // Fallback à un buffer standard si DMA n'est pas disponible
    buffer = (uint8_t*)malloc(chunk_size);
    if (buffer == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate buffer for streaming");
      fclose(file);
      return;
    }
  }

  size_t total_read = 0;
  size_t bytes_since_wdt = 0;
  size_t bytes_since_log = 0;
  uint32_t start_time = millis();
  size_t available_size = file_size - offset;
  
  while (true) {
    size_t read = fread(buffer, 1, chunk_size, file);
    if (read == 0) break;
    
    total_read += read;
    bytes_since_wdt += read;
    bytes_since_log += read;
    
    // Appeler le callback avec les données lues
    callback(buffer, read);
    
    // Reset WDT périodiquement
    if (bytes_since_wdt >= 4 * 1024 * 1024) {  // 4MB
      esp_task_wdt_reset();
      bytes_since_wdt = 0;
    }
    
    // Afficher progression périodiquement
    if (bytes_since_log >= 8 * 1024 * 1024) {  // 8MB
      uint32_t elapsed = millis() - start_time;
      float speed = elapsed > 0 ? ((float)total_read / 1024.0f) / (elapsed / 1000.0f) : 0;
      float percent = available_size > 0 ? ((float)total_read * 100.0f / available_size) : 0;
      ESP_LOGI(TAG, "Streaming: %zu/%zu bytes (%.1f%%), %.2f KB/s", 
               total_read, available_size, percent, speed);
      bytes_since_log = 0;
    }
  }
  
  // Libérer ressources
  free(buffer);
  fclose(file);
  
  uint32_t elapsed = millis() - start_time;
  if (elapsed > 0) {
    float speed = ((float)total_read / 1024.0f) / (elapsed / 1000.0f);  // KB/s
    ESP_LOGI(TAG, "File streaming completed: %zu bytes, %.2f KB/s", total_read, speed);
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

void SdMmc::read_file_stream(std::string const &path, size_t offset, size_t chunk_size,
                            std::function<void(const uint8_t*, size_t)> callback) {
  this->read_file_stream(path.c_str(), offset, chunk_size, callback);
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

