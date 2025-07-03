#pragma once
#include "esphome/core/component.h"
#include <esp_http_server.h>
#include "esphome/core/helpers.h"
#include <string>
#include <vector>
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "../sd_mmc_card/sd_mmc_card.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"

namespace esphome {
namespace webdavbox3 {

class WebDAVBox3 : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }
  void set_root_path(const std::string &path) { root_path_ = path; }
  void set_url_prefix(const std::string &prefix) { url_prefix_ = prefix; }
  void set_port(uint16_t port) { port_ = port; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void enable_authentication(bool enabled) { auth_enabled_ = enabled; }
  void add_cors_headers(httpd_req_t *req);
  void register_handlers();
  float benchmark_sd_read(const std::string &filepath);
  
  bool mount_sd_card();  // Ajout de ta fonction publique

 protected:
  httpd_handle_t server_{nullptr};
  std::string root_path_{"/sdcard/"};
  std::string url_prefix_{"/"};
  uint16_t port_{81};
  std::string username_;
  std::string password_;
  bool auth_enabled_{false};

  bool sdcard_mounted_ = false;  // Ajout de ta variable privée

  // HTTP server configuration
  void configure_http_server();
  void start_server();
  void stop_server();
  
  // Authentication helpers
  bool authenticate(httpd_req_t *req);
  esp_err_t send_auth_required_response(httpd_req_t *req);
  
  // WebDAV path conversion
  std::string uri_to_filepath(const char* uri);

  esp_err_t handle_webdav_get_small_file(httpd_req_t *req, const std::string &path, size_t file_size);
  
  // WebDAV handler methods
  static esp_err_t handle_root(httpd_req_t *req);
  static esp_err_t handle_webdav_list(httpd_req_t *req);
  static esp_err_t handle_webdav_options(httpd_req_t *req);
  static esp_err_t handle_webdav_propfind(httpd_req_t *req);
  static esp_err_t handle_webdav_get(httpd_req_t *req);
  static esp_err_t handle_webdav_put(httpd_req_t *req);
  static esp_err_t handle_webdav_delete(httpd_req_t *req);
  static esp_err_t handle_webdav_mkcol(httpd_req_t *req);
  static esp_err_t handle_webdav_move(httpd_req_t *req);
  static esp_err_t handle_webdav_copy(httpd_req_t *req);
  static esp_err_t handle_webdav_lock(httpd_req_t *req);
  static esp_err_t handle_webdav_unlock(httpd_req_t *req);
  static esp_err_t handle_webdav_proppatch(httpd_req_t *req);
  static esp_err_t handle_post(httpd_req_t *req);
  static bool create_directories(const std::string& path);
  
  // *** NOUVELLES MÉTHODES AJOUTÉES ***
  // Range Requests pour le streaming audio
  static esp_err_t handle_range_request(httpd_req_t *req, const std::string &path, size_t file_size);
  
  // Helper methods
  static std::string get_file_path(httpd_req_t *req, const std::string &root_path);
  static bool is_dir(const std::string &path);
  static std::vector<std::string> list_dir(const std::string &path);
  static std::string generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size);
  
  // *** MÉTHODE CORS DÉPLACÉE DANS STATIC ***
  static void add_cors_headers_static(httpd_req_t *req);
};

}  // namespace webdavbox3
}  // namespace esphome








