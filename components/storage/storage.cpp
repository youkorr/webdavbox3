#include "storage.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"

namespace esphome {
namespace storage {

static const char *const TAG = "storage";
FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {
  this->read_offset = 0;
}

FileInfo::FileInfo() : path(), size(), is_directory() { this->read_offset = 0; }

std::vector<FileInfo> Storage::list_directory(const std::string &path) { return this->direct_list_directory(path); }

FileInfo Storage::get_file_info(const std::string &path) { return this->direct_get_file_info(path); }

void Storage::set_file(FileInfo *file) {
  if (this->current_file_ != file) {
    this->current_file_ = file;
    this->direct_set_file(this->current_file_->path);
  }
}

uint8_t Storage::read() {
  uint8_t data;
  data = this->direct_read_byte(this->current_file_->read_offset);
  this->update_offset(1);
  return data;
}

bool Storage::write(uint8_t data) {
  //    if(this->buffer_ != nullptr) {
  //       this->buffer_[this->current_offset_] = data;
  //    } else {
  ESP_LOGW(TAG, "Buffer not available using direct access instead");
  return this->direct_write_byte(data);
  //    }
}

bool Storage::append(uint8_t data) { return this->direct_append_byte(data); }

size_t Storage::read_array(uint8_t *data, size_t data_length) {
  size_t num_bytes_read = this->direct_read_byte_array(this->current_file_->read_offset, data, data_length);
  this->update_offset(num_bytes_read);
  return num_bytes_read;
}

bool Storage::write_array(uint8_t *data, size_t data_length) {
  return this->direct_write_byte_array(data, data_length);
}

bool Storage::append_array(uint8_t *data, size_t data_length) {
  return this->direct_write_byte_array(data, data_length);
}
// void Storage::allocate_buffer(uint32_t buffer_size) {
//    if(buffer_size) {
//       if(this->buffer_ == nullptr) {
//          RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::NONE);
//          this->buffer_ = allocator.allocate(buffer_size);
//       }
//       if(this->buffer_ == nullptr) {
//          ESP_LOGE(TAG, "Unable to Allocate memory for storage buffer");
//       }
//    }
// }

void Storage::update_offset(size_t value) {
  this->current_file_->read_offset += value;
  //    if (this->current_offset_ > this->buffer_size_){
  //       if(this->max_offset_ && (this->base_offset_ + this->current_offset_ > this->max_offset_)) {
  //          if(this->buffer_offset_ != this->base_offset_){
  //             this->load_buffer(base_offset_,0,this->buffer_size_);
  //          }
  //       } else {
  //          uint32_t new_buffer_offset = this->buffer_offset_+this->current_offset_;
  //          if(this->max_offset_ && (new_buffer_offset+this->buffer_size_-1 > this->max_offset_)) {
  //             this->load_buffer(this->buffer_offset_+this->current_offset_,0,this->max_offset_-new_buffer_offset+1);
  //          } else {
  //             this->load_buffer(this->buffer_offset_+this->current_offset_,0,this->buffer_size_);
  //          }
  //       }
  //       this->current_offset_ = 0;
  //    }
}

std::vector<FileInfo> StorageClient::list_directory(const std::string &path) {
  int prefix_end = path.find("://");
  if (prefix_end < 0) {
    ESP_LOGE(TAG, "Invalid path. Must start with a valid prefix");
    return std::vector<FileInfo>();
  }
  std::string prefix = path.substr(0, prefix_end);
  auto nstorage = storages.find(prefix);
  if (nstorage == storages.end()) {
    ESP_LOGE(TAG, "storage %s prefix does not exist", prefix);
    return std::vector<FileInfo>();
  }
  std::vector<FileInfo> result = nstorage->second->list_directory(path.substr(prefix_end + 3));
  for (auto i = result.begin(); i != result.end(); i++) {
    i->path = prefix + "://" + i->path;
  }
  return result;
}

FileInfo StorageClient::get_file_info(const std::string &path) {
  int prefix_end = path.find("://");
  if (prefix_end < 0) {
    ESP_LOGE(TAG, "Invalid path. Must start with a valid prefix");
    return FileInfo();
  }
  std::string prefix = path.substr(0, prefix_end);
  auto nstorage = storages.find(prefix);
  if (nstorage == storages.end()) {
    ESP_LOGE(TAG, "storage %s prefix does not exist", prefix);
    return FileInfo();
  }
  FileInfo result = nstorage->second->get_file_info(path.substr(prefix_end + 3));
  result.path = prefix + "://" + result.path;
  return result;
}

void StorageClient::set_file(const std::string &path) {
  int prefix_end = path.find("://");
  if (prefix_end < 0) {
    ESP_LOGE(TAG, "Invalid path. Must start with a valid prefix");
    return;
  }
  std::string prefix = path.substr(0, prefix_end);
  auto nstorage = storages.find(prefix);
  if (nstorage == storages.end()) {
    ESP_LOGE(TAG, "storage %s prefix does not exist", prefix);
    return;
  }
  this->current_storage_ = nstorage->second;
  this->current_file_ = this->current_storage_->get_file_info(path.substr(prefix_end + 3));
  this->current_storage_->set_file(&(this->current_file_));
  ESP_LOGVV(TAG, "Current File Set to %s", this->current_file_.path.c_str());
}

uint8_t StorageClient::read() {
  if (this->current_storage_) {
    ESP_LOGVV(TAG, "Reading File: %s", this->current_file_.path.c_str());
    this->current_storage_->set_file(&(this->current_file_));
    return current_storage_->read();
  } else {
    ESP_LOGE(TAG, "File has not been set");
    return 0;
  }
}

void StorageClient::set_read_offset(size_t offset) {
  if (this->current_storage_) {
    this->current_file_.read_offset = offset;
  } else {
    ESP_LOGE(TAG, "File has not been set");
  }
}

bool StorageClient::write(uint8_t data) {
  if (current_storage_) {
    this->current_storage_->set_file(&(this->current_file_));
    return current_storage_->write(data);
  } else {
    ESP_LOGE(TAG, "File has not been set");
    return 0;
  }
}

bool StorageClient::append(uint8_t data) {
  if (current_storage_) {
    this->current_storage_->set_file(&(this->current_file_));
    return current_storage_->append(data);
  } else {
    ESP_LOGE(TAG, "File has not been set");
    return 0;
  }
}

size_t StorageClient::read_array(uint8_t *data, size_t data_length) {
  if (current_storage_) {
    ESP_LOGVV(TAG, "Reading File: %s", this->current_file_.path.c_str());
    this->current_storage_->set_file(&(this->current_file_));
    return current_storage_->read_array(data, data_length);
  } else {
    ESP_LOGE(TAG, "File has not been set");
    return 0;
  }
}

bool StorageClient::write_array(uint8_t *data, size_t data_length) {
  if (current_storage_) {
    this->current_storage_->set_file(&(this->current_file_));
    return current_storage_->write_array(data, data_length);
  } else {
    ESP_LOGE(TAG, "File has not been set");
    return 0;
  }
}

bool StorageClient::append_array(uint8_t *data, size_t data_length) {
  if (current_storage_) {
    this->current_storage_->set_file(&(this->current_file_));
    return current_storage_->append_array(data, data_length);
  } else {
    ESP_LOGE(TAG, "File has not been set");
    return 0;
  }
}

std::map<std::string, Storage *> StorageClient::storages = {};

void StorageClient::add_storage(Storage *storage_inst, std::string prefix) {
  StorageClient::storages[prefix] = storage_inst;
}

}  // namespace storage
}  // namespace esphome
