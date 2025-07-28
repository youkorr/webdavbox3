#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include <vector>
#include <map>

namespace esphome {
namespace storage {

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;
  size_t read_offset;
  FileInfo(std::string const &path, size_t size, bool is_directory);
  FileInfo();
};

class Storage : public EntityBase {
 public:
  // direct functions
  virtual uint8_t direct_read_byte(size_t offset) = 0;
  virtual bool direct_write_byte(uint8_t data) = 0;
  virtual bool direct_append_byte(uint8_t data) = 0;
  virtual size_t direct_read_byte_array(size_t offset, uint8_t *data, size_t data_length) = 0;
  virtual bool direct_write_byte_array(uint8_t *data, size_t data_length) = 0;
  virtual bool direct_append_byte_array(uint8_t *data, size_t data_length) = 0;
  std::vector<FileInfo> list_directory(const std::string &path);
  FileInfo get_file_info(const std::string &path);
  void set_file(FileInfo *file);
  uint8_t read();
  bool write(uint8_t data);
  bool append(uint8_t data);
  size_t read_array(uint8_t *data, size_t data_length);
  bool write_array(uint8_t *data, size_t data_length);
  bool append_array(uint8_t *data, size_t data_length);

  // void write_buffer();
  // void refresh_buffer(uint32_t offset = 0);
  // uint8_t & operator[] (size_t index);
  // uint32_t get_buffer_size();
  // void write_on_shutdown(bool value);

 protected:
  virtual void direct_set_file(const std::string &file) = 0;
  virtual FileInfo direct_get_file_info(const std::string &path) = 0;
  virtual std::vector<FileInfo> direct_list_directory(const std::string &path) = 0;
  // void load_buffer (uint32_t offset, uint32_t buffer_offset, uint32_t length);
  // void write_buffer (uint32_t offset, uint32_t buffer_offset, uint32_t length);
  // void allocate_buffer(uint32_t buffer_size);
  void update_offset(size_t value);
  // uint8_t * buffer_;
  // uint32_t buffer_size_;
  // uint32_t buffer_offset_;
  FileInfo *current_file_;
  // uint32_t base_offset_;
  // uint32_t max_offset_;
  // bool write_on_shutdown_;
};

class StorageClient : public EntityBase {
 public:
  std::vector<FileInfo> list_directory(const std::string &path);
  FileInfo get_file_info(const std::string &path);
  void set_file(const std::string &path);
  uint8_t read();
  void set_read_offset(size_t offset);
  bool write(uint8_t data);
  bool append(uint8_t data);
  size_t read_array(uint8_t *data, size_t data_length);
  bool write_array(uint8_t *data, size_t data_length);
  bool append_array(uint8_t *data, size_t data_length);

  static void add_storage(Storage *storage_inst, std::string prefix);

 protected:
  static std::map<std::string, Storage *> storages;
  Storage *current_storage_;
  FileInfo current_file_;
};

}  // namespace storage
}  // namespace esphome
