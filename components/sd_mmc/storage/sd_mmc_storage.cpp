#include "sd_mmc_storage.h"

namespace esphome {
namespace sd_mmc {

static const char *TAG = "SD_MMC_STORAGE";

uint8_t SD_MMC_Storage::direct_read_byte(size_t offset) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "file %s is a directory", this->current_file_);
    return 0;
  }
  if (this->sd_ref_ != nullptr) {
    uint8_t value;
    size_t read = this->sd_ref_->read_file_chunk("/" + this->current_file_.path, offset, &value, (size_t) 1);
    if (read == 1) {
      return value;
    }
  }
  ESP_LOGE(TAG, "Unable to properly read value from sd card");
  return 0;
}

size_t SD_MMC_Storage::direct_read_byte_array(size_t offset, uint8_t *data, size_t data_length) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "file %s is a directory", this->current_file_);
    return 0;
  }
  if (this->sd_ref_ != nullptr) {
    return this->sd_ref_->read_file_chunk("/" + this->current_file_.path, offset, data, data_length);
  }
  ESP_LOGE(TAG, "Unable to properly read value from sd card");
  return 0;
}

bool SD_MMC_Storage::direct_write_byte(uint8_t data) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "file %s is a directory", this->current_file_);
    return false;
  }
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->write_file(("/" + this->current_file_.path).c_str(), &data, 1);
    return true;
  }
  return false;
}

bool SD_MMC_Storage::direct_write_byte_array(uint8_t *data, size_t data_length) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "file %s is a directory", this->current_file_);
    return false;
  }
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->write_file(("/" + this->current_file_.path).c_str(), data, data_length);
    return true;
  }
  return false;
}

bool SD_MMC_Storage::direct_append_byte(uint8_t data) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "file %s is a directory", this->current_file_);
    return false;
  }
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->append_file(("/" + this->current_file_.path).c_str(), &data, 1);
    return true;
  }
  return false;
}

bool SD_MMC_Storage::direct_append_byte_array(uint8_t *data, size_t data_length) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "file %s is a directory", this->current_file_);
    return false;
  }
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->append_file(("/" + this->current_file_.path).c_str(), data, data_length);
    return true;
  }
  return false;
}

void SD_MMC_Storage::direct_set_file(const std::string &path) {
  if (this->current_file_.is_directory) {
    ESP_LOGE(TAG, "File %s is actually a directory", this->current_file_.path);
    return;
  }
  this->current_file_ = direct_get_file_info(path);
}

storage::FileInfo SD_MMC_Storage::direct_get_file_info(const std::string &path) {
  if (this->sd_ref_ != nullptr) {
    return this->sd_ref_->file_info("/" + path);
  }
  return storage::FileInfo("", 0, false);
}

std::vector<storage::FileInfo> SD_MMC_Storage::direct_list_directory(const std::string &path) {
  if (this->sd_ref_ != nullptr) {
    return this->sd_ref_->list_directory_file_info("/" + path, 0);
  }
  return std::vector<storage::FileInfo>();
}

}  // namespace sd_mmc
}  // namespace esphome
