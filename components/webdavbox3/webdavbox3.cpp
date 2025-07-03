#include "webdavbox3.h"
#include "esphome/core/log.h"
#include "esp_task_wdt.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <errno.h>
#include "esp_err.h"
#include "esp_netif.h"
#include <ctime>
#include <chrono>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "esp_timer.h"

namespace esphome {
namespace webdavbox3 {

static const char *const TAG = "webdavbox3";

bool create_directories_util(const std::string& path) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            struct stat st;
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, 0755) != 0) {
                    ESP_LOGE("WEBDAV", "Failed to create directory: %s (errno: %d)", tmp, errno);
                    return false;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                ESP_LOGE("WEBDAV", "Path exists but is not a directory: %s", tmp);
                return false;
            }
            *p = '/';
        }
    }

    struct stat st;
    if (stat(tmp, &st) != 0) {
        if (mkdir(tmp, 0755) != 0) {
            ESP_LOGE("WEBDAV", "Failed to create directory: %s (errno: %d)", tmp, errno);
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        ESP_LOGE("WEBDAV", "Path exists but is not a directory: %s", tmp);
        return false;
    }

    return true;
}

// Fonction pour décoder les URL
std::string url_decode(const std::string &src) {
    std::string result;
    result.reserve(src.length());

    const char* str = src.c_str();
    int i = 0;
    char ch;
    int j;
    
    while (str[i]) {
        if (str[i] == '%' && str[i+1] && str[i+2]) {
            if (sscanf(str + i + 1, "%2x", &j) == 1) {
                ch = static_cast<char>(j);
                result += ch;
                i += 3;
            } else {
                result += str[i++];
            }
        } else if (str[i] == '+') {
            result += ' ';
            i++;
        } else {
            result += str[i++];
        }
    }
    
    return result;
}

void WebDAVBox3::setup() {
    ESP_LOGI(TAG, "🚀 Démarrage WebDAV optimisé pour gros fichiers");
    
    // Diagnostic du système de fichiers
    ESP_LOGI(TAG, "Diagnostic du système de fichiers");
    
    DIR *dir = opendir(root_path_.c_str());
    if (!dir) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire racine (errno: %d)", errno);
    } else {
        ESP_LOGI(TAG, "Répertoire racine accessible");
        
        // Lister le contenu du répertoire racine
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                std::string full_path = root_path_;
                if (full_path.back() != '/') full_path += '/';
                full_path += entry->d_name;
                
                struct stat st;
                if (stat(full_path.c_str(), &st) == 0) {
                    ESP_LOGI(TAG, "  - %s (%s, taille: %ld)", 
                            entry->d_name,
                            S_ISDIR(st.st_mode) ? "dossier" : "fichier",
                            (long)st.st_size);
                }
            }
        }
        closedir(dir);
    }
    
    // Test mémoire disponible
    ESP_LOGI(TAG, "💾 Mémoire disponible - Interne: %zu KB, PSRAM: %zu KB", 
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    
    this->configure_http_server();
    this->start_server();
}

void WebDAVBox3::loop() {
    // Monitoring léger de la mémoire
    static uint32_t last_check = 0;
    uint32_t now = millis();
    
    if (now - last_check > 30000) { // Toutes les 30 secondes
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        
        if (free_internal < 50 * 1024 || free_psram < 100 * 1024) {
            ESP_LOGW(TAG, "⚠️ Mémoire faible - Interne: %zu KB, PSRAM: %zu KB", 
                free_internal / 1024, free_psram / 1024);
        }
        last_check = now;
    }
}

void WebDAVBox3::configure_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Configuration optimisée pour gros fichiers
    config.server_port = port_;
    config.ctrl_port = port_ + 1000;
    config.max_uri_handlers = 20;  // Plus de handlers
    
    // Paramètres de performance améliorés
    config.stack_size = 24576;     // 24KB de stack (au lieu de 16KB)
    config.core_id = tskNO_AFFINITY;
    config.task_priority = tskIDLE_PRIORITY + 6;  // Priorité plus élevée
    
    // Paramètres de délai optimisés pour gros fichiers
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 600;    // 10 minutes (au lieu de 60s)
    config.send_wait_timeout = 600;    // 10 minutes (au lieu de 60s)
    
    // Pour les grandes requêtes et réponses
    config.max_resp_headers = 40;      // Plus d'en-têtes
    config.max_open_sockets = 10;      // Plus de sockets simultanés
    
    // Obligatoire pour les URL avec wildcards
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Vérifier que le serveur n'est pas déjà démarré
    if (server_ != nullptr) {
        ESP_LOGW(TAG, "Server already started, stopping previous instance");
        httpd_stop(server_);
        server_ = nullptr;
    }
    
    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server on port %d", port_);
        server_ = nullptr;
        return;
    }
    ESP_LOGI(TAG, "🌐 Serveur WebDAV optimisé démarré sur le port %d", port_);
    
    this->register_handlers();
}

void WebDAVBox3::register_handlers() {
    if (server_ == nullptr) {
        ESP_LOGE(TAG, "Server not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "📋 Enregistrement des gestionnaires WebDAV");
    
    // Gestionnaire pour la racine
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &root_uri);
    
    // Gestionnaire OPTIONS 
    httpd_uri_t options_uri = {
        .uri = "/*",
        .method = HTTP_OPTIONS,
        .handler = handle_webdav_options,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &options_uri);
    
    // Gestionnaires PROPFIND
    httpd_uri_t propfind_uri = {
        .uri = "/",
        .method = HTTP_PROPFIND,
        .handler = handle_webdav_propfind,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &propfind_uri);
    
    httpd_uri_t propfind_wildcard_uri = {
        .uri = "/*",
        .method = HTTP_PROPFIND,
        .handler = handle_webdav_propfind,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &propfind_wildcard_uri);
    
    // Gestionnaire PROPPATCH
    httpd_uri_t proppatch_uri = {
        .uri = "/*",
        .method = HTTP_PROPPATCH,
        .handler = handle_webdav_proppatch,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &proppatch_uri);
    
    // Gestionnaire GET optimisé
    httpd_uri_t get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = handle_webdav_get,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &get_uri);
    
    // Autres gestionnaires WebDAV
    httpd_uri_t head_uri = {
        .uri = "/*",
        .method = HTTP_HEAD,
        .handler = handle_webdav_get,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &head_uri);
    
    httpd_uri_t put_uri = {
        .uri = "/*",
        .method = HTTP_PUT,
        .handler = handle_webdav_put,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &put_uri);
    
    httpd_uri_t delete_uri = {
        .uri = "/*",
        .method = HTTP_DELETE,
        .handler = handle_webdav_delete,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &delete_uri);
    
    httpd_uri_t mkcol_uri = {
        .uri = "/*",
        .method = HTTP_MKCOL,
        .handler = handle_webdav_mkcol,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &mkcol_uri);
    
    httpd_uri_t move_uri = {
        .uri = "/*",
        .method = HTTP_MOVE,
        .handler = handle_webdav_move,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &move_uri);
    
    httpd_uri_t copy_uri = {
        .uri = "/*",
        .method = HTTP_COPY,
        .handler = handle_webdav_copy,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &copy_uri);
    
    // Gestionnaires LOCK/UNLOCK
    httpd_uri_t lock_uri = {
        .uri = "/*",
        .method = HTTP_LOCK,
        .handler = handle_webdav_lock,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &lock_uri);
    
    httpd_uri_t unlock_uri = {
        .uri = "/*",
        .method = HTTP_UNLOCK,
        .handler = handle_webdav_unlock,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &unlock_uri);
    
    ESP_LOGI(TAG, "✅ Tous les gestionnaires WebDAV ont été enregistrés");
}

void WebDAVBox3::start_server() {
    if (server_ != nullptr)
        return;
    configure_http_server();
}

void WebDAVBox3::stop_server() {
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
    std::string uri = req->uri;
    std::string path = root_path;
    
    ESP_LOGI(TAG, "URI brute: %s", uri.c_str());
    
    // Décoder l'URL
    uri = url_decode(uri);
    ESP_LOGI(TAG, "URI décodée: %s", uri.c_str());
    
    if (path.back() != '/') {
        path += '/';
    }
    
    if (!uri.empty() && uri.front() == '/') {
        uri = uri.substr(1);
    }
    
    if (uri.empty()) {
        ESP_LOGI(TAG, "Accès à la racine: %s", path.c_str());
        return path.substr(0, path.length() - 1);
    }
    
    path += uri;
    
    ESP_LOGI(TAG, "Mapped URI %s to path %s", req->uri, path.c_str());
    
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        ESP_LOGI(TAG, "Chemin existe: %s", path.c_str());
    } else {
        ESP_LOGI(TAG, "⚠️ Chemin n'existe PAS: %s (errno: %d)", path.c_str(), errno);
    }
    
    return path;
}

bool WebDAVBox3::is_dir(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode);
    return false;
}

std::vector<std::string> WebDAVBox3::list_dir(const std::string &path) {
    std::vector<std::string> files;
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                files.push_back(entry->d_name);
            }
        }
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire: %s (errno: %d)", path.c_str(), errno);
    }
    return files;
}

std::string WebDAVBox3::generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size) {
    char time_buf[50];
    struct tm *gmt = gmtime(&modified);
    strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    
    std::string display_name = href;
    if (href.back() == '/') {
        display_name = href.substr(0, href.length() - 1);
    }
    size_t last_slash = display_name.find_last_of('/');
    if (last_slash != std::string::npos) {
        display_name = display_name.substr(last_slash + 1);
    }
    if (display_name.empty() && href == "/") {
        display_name = "Root";
    }
    
    std::string xml = "  <D:response>\n";
    xml += "    <D:href>" + href + "</D:href>\n";
    xml += "    <D:propstat>\n";
    xml += "      <D:prop>\n";
    xml += "        <D:resourcetype>";
    if (is_directory) {
        xml += "<D:collection/>";
    }
    xml += "</D:resourcetype>\n";
    xml += "        <D:getlastmodified>" + std::string(time_buf) + "</D:getlastmodified>\n";
    xml += "        <D:creationdate>" + std::string(time_buf) + "</D:creationdate>\n";
    xml += "        <D:displayname>" + display_name + "</D:displayname>\n";
    
    if (!is_directory) {
        xml += "        <D:getcontentlength>" + std::to_string(size) + "</D:getcontentlength>\n";
        
        std::string content_type = "application/octet-stream";
        size_t dot_pos = href.find_last_of(".");
        if (dot_pos != std::string::npos) {
            std::string ext = href.substr(dot_pos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), 
                         [](unsigned char c){ return std::tolower(c); });
            
            if (ext == "txt") content_type = "text/plain";
            else if (ext == "html" || ext == "htm") content_type = "text/html";
            else if (ext == "css") content_type = "text/css";
            else if (ext == "js") content_type = "application/javascript";
            else if (ext == "jpg" || ext == "jpeg") content_type = "image/jpeg";
            else if (ext == "png") content_type = "image/png";
            else if (ext == "gif") content_type = "image/gif";
            else if (ext == "mp3") content_type = "audio/mpeg";
            else if (ext == "mp4") content_type = "video/mp4";
            else if (ext == "pdf") content_type = "application/pdf";
            else if (ext == "flac") content_type = "audio/flac";
            else if (ext == "wav") content_type = "audio/wav";
            else if (ext == "m4a") content_type = "audio/mp4";
        }
        
        xml += "        <D:getcontenttype>" + content_type + "</D:getcontenttype>\n";
    }
    
    xml += "      </D:prop>\n";
    xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
    xml += "    </D:propstat>\n";
    xml += "  </D:response>\n";
    
    return xml;
}

// ========== HANDLERS OPTIMISÉS ==========

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
    const char* response = "🎵 ESP32 WebDAV Server - Optimisé pour fichiers audio volumineux";
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void WebDAVBox3::add_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", 
                      "GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", 
                      "Authorization, Content-Type, Depth, Destination, Overwrite, Range");
}

esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
    ESP_LOGI(TAG, "OPTIONS %s", req->uri);
    
    // En-têtes CORS complets
    add_cors_headers(req);
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "3600");
    
    // En-têtes WebDAV standards
    httpd_resp_set_hdr(req, "DAV", "1, 2");
    httpd_resp_set_hdr(req, "Allow", 
                     "GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, OPTIONS");
    httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
    
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "");
}

// NOUVEAU: Gestionnaire pour les Range Requests (streaming audio)
esp_err_t WebDAVBox3::handle_range_request(httpd_req_t *req, const std::string &path, size_t file_size) {
    char range_header[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Range", range_header, sizeof(range_header)) != ESP_OK) {
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "🎯 Range request détecté: %s", range_header);
    
    // Parser "bytes=start-end"
    if (strncmp(range_header, "bytes=", 6) != 0) return ESP_FAIL;
    
    char *range_str = range_header + 6;
    char *dash = strchr(range_str, '-');
    if (!dash) return ESP_FAIL;
    
    size_t start = 0, end = file_size - 1;
    
    // Parse start
    if (dash != range_str) {
        *dash = '\0';
        start = strtoul(range_str, nullptr, 10);
    }
    
    // Parse end  
    if (*(dash + 1) != '\0') {
        end = strtoul(dash + 1, nullptr, 10);
    }
    
    // Validation des ranges
    if (start >= file_size || end >= file_size || start > end) {
        ESP_LOGE(TAG, "Range invalide: %zu-%zu pour fichier de %zu octets", start, end, file_size);
        return ESP_FAIL;
    }
    
    size_t content_length = end - start + 1;
    
    ESP_LOGI(TAG, "📦 Range request: bytes %zu-%zu/%zu (%zu octets à envoyer)", 
             start, end, file_size, content_length);
    
    // Déterminer le type de contenu
    const char* content_type = "application/octet-stream";
    const char* ext = strrchr(path.c_str(), '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "mp3") == 0) content_type = "audio/mpeg";
        else if (strcasecmp(ext, "mp4") == 0) content_type = "video/mp4";
        else if (strcasecmp(ext, "flac") == 0) content_type = "audio/flac";
        else if (strcasecmp(ext, "wav") == 0) content_type = "audio/wav";
        else if (strcasecmp(ext, "m4a") == 0) content_type = "audio/mp4";
    }
    
    // En-têtes pour partial content
    httpd_resp_set_status(req, "206 Partial Content");
    httpd_resp_set_type(req, content_type);
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    httpd_resp_set_hdr(req, "Content-Length", std::to_string(content_length).c_str());
    
    char content_range[128];
    snprintf(content_range, sizeof(content_range), "bytes %zu-%zu/%zu", start, end, file_size);
    httpd_resp_set_hdr(req, "Content-Range", content_range);
    
    // CORS pour streaming
    add_cors_headers(req);
    
    // Ouvrir et positionner le fichier
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le fichier pour range request");
        return ESP_FAIL;
    }
    
    if (fseek(file, start, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Impossible de se positionner à l'offset %zu", start);
        fclose(file);
        return ESP_FAIL;
    }
    
    // Optimisation du buffer pour les range requests (streaming)
    const size_t RANGE_CHUNK_SIZE = 32768; // 32K optimal pour streaming
    
    char *buffer = (char*)heap_caps_malloc(RANGE_CHUNK_SIZE, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        buffer = (char*)heap_caps_malloc(RANGE_CHUNK_SIZE, MALLOC_CAP_8BIT);
    }
    
    if (!buffer) {
        ESP_LOGE(TAG, "Impossible d'allouer le buffer pour range request");
        fclose(file);
        return ESP_FAIL;
    }
    
    size_t remaining = content_length;
    size_t sent = 0;
    esp_err_t result = ESP_OK;
    
    unsigned long start_time = esp_timer_get_time() / 1000;
    
    while (remaining > 0 && sent < content_length) {
        size_t to_read = (remaining < RANGE_CHUNK_SIZE) ? remaining : RANGE_CHUNK_SIZE;
        size_t read_bytes = fread(buffer, 1, to_read, file);
        
        if (read_bytes == 0) {
            ESP_LOGW(TAG, "Fin prématurée du fichier lors du range request");
            break;
        }
        
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            ESP_LOGE(TAG, "Erreur d'envoi de chunk pour range request");
            result = ESP_FAIL;
            break;
        }
        
        remaining -= read_bytes;
        sent += read_bytes;
        
        // Pas de délai pour le streaming
    }
    
    // Terminer la réponse
    if (result == ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    
    unsigned long end_time = esp_timer_get_time() / 1000;
    float elapsed = (end_time - start_time) / 1000.0f;
    float speed = (sent / 1024.0f / 1024.0f) / elapsed;
    
    ESP_LOGI(TAG, "🎵 Range request terminé: %zu/%zu octets envoyés en %.2f s (%.2f MB/s)", 
             sent, content_length, elapsed, speed);
    
    heap_caps_free(buffer);
    fclose(file);
    
    return result;
}

esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);
    
    ESP_LOGI(TAG, "🎵 GET %s (URI: %s)", path.c_str(), req->uri);
    
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "Fichier non trouvé: %s", path.c_str());
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    }
    
    if (S_ISDIR(st.st_mode)) {
        return handle_webdav_propfind(req);
    }
    
    // *** NOUVEAU: Support des Range Requests pour streaming audio ***
    if (handle_range_request(req, path, st.st_size) == ESP_OK) {
        return ESP_OK;
    }
    
    // Optimisation de la connexion socket pour gros fichiers
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd >= 0) {
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
        
        // Buffer d'envoi plus grand pour gros fichiers
        int send_buf = 512 * 1024;  // 512KB
        setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_buf, sizeof(send_buf));
        
        // Timeouts très longs pour gros fichiers
        struct timeval timeout;
        timeout.tv_sec = 600;  // 10 minutes
        timeout.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        // Désactiver Nagle pour améliorer la latence  
        opt = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }
    
    // Headers optimisés
    add_cors_headers(req);
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    httpd_resp_set_hdr(req, "Content-Length", std::to_string(st.st_size).c_str());
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
    
    // Déterminer le type de contenu
    const char* content_type = "application/octet-stream";
    const char* ext = strrchr(path.c_str(), '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "mp3") == 0) content_type = "audio/mpeg";
        else if (strcasecmp(ext, "mp4") == 0) content_type = "video/mp4";
        else if (strcasecmp(ext, "flac") == 0) content_type = "audio/flac";
        else if (strcasecmp(ext, "wav") == 0) content_type = "audio/wav";
        else if (strcasecmp(ext, "m4a") == 0) content_type = "audio/mp4";
        else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) content_type = "image/jpeg";
        else if (strcasecmp(ext, "png") == 0) content_type = "image/png";
        else if (strcasecmp(ext, "txt") == 0) content_type = "text/plain";
    }
    
    httpd_resp_set_type(req, content_type);
    
    // Ouvrir le fichier
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le fichier: %s", path.c_str());
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open file");
    }
    
    ESP_LOGI(TAG, "📤 Envoi du fichier %s (%zu octets, type: %s)", 
             path.c_str(), (size_t)st.st_size, content_type);
    
    // *** Stratégie de buffer adaptatif selon la mémoire disponible ***
    size_t available_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t available_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    
    size_t CHUNK_SIZE;
    if (available_psram > 2 * 1024 * 1024 && st.st_size > 10 * 1024 * 1024) {
        CHUNK_SIZE = 128 * 1024;  // 128KB pour gros fichiers avec beaucoup de PSRAM
    } else if (available_psram > 1024 * 1024) {
        CHUNK_SIZE = 64 * 1024;   // 64KB pour fichiers moyens
    } else if (available_internal > 100 * 1024) {
        CHUNK_SIZE = 32 * 1024;   // 32KB pour mémoire limitée
    } else {
        CHUNK_SIZE = 16 * 1024;   // 16KB mode sécurité
    }
    
    ESP_LOGI(TAG, "💾 Mémoire - PSRAM: %zu KB, Interne: %zu KB, Buffer choisi: %zu KB", 
             available_psram / 1024, available_internal / 1024, CHUNK_SIZE / 1024);
    
    // Allocation du buffer avec préférence PSRAM
    char *buffer = nullptr;
    bool using_psram = false;
    
    if (available_psram > CHUNK_SIZE * 2) {
        buffer = (char*)heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_SPIRAM);
        using_psram = true;
        ESP_LOGI(TAG, "📦 Buffer alloué en PSRAM");
    }
    
    if (!buffer) {
        buffer = (char*)heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_8BIT);
        using_psram = false;
        ESP_LOGW(TAG, "📦 Buffer alloué en mémoire interne (fallback)");
    }
    
    if (!buffer) {
        ESP_LOGE(TAG, "❌ Impossible d'allouer le buffer de %zu octets", CHUNK_SIZE);
        fclose(file);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }
    
    // Transfert optimisé avec monitoring
    fseek(file, 0, SEEK_SET);
    
    size_t read_bytes;
    size_t total_sent = 0;
    esp_err_t err = ESP_OK;
    
    unsigned long start_time = esp_timer_get_time() / 1000;
    unsigned long last_log_time = start_time;
    
    // Log adaptatif selon la taille du fichier
    bool is_large_file = (st.st_size > 20 * 1024 * 1024);  // 20MB
    size_t log_interval = is_large_file ? (5 * 1024 * 1024) : (1 * 1024 * 1024);
    
    while ((read_bytes = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        err = httpd_resp_send_chunk(req, buffer, read_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Erreur d'envoi chunk: %d (data: %zu bytes)", err, read_bytes);
            break;
        }
        
        total_sent += read_bytes;
        
        // Monitoring de progression
        unsigned long current_time = esp_timer_get_time() / 1000;
        if (total_sent % log_interval == 0 || (current_time - last_log_time > 3000)) {
            float elapsed_sec = (current_time - start_time) / 1000.0f;
            float speed_mbps = (total_sent / 1024.0f / 1024.0f) / elapsed_sec;
            int progress = (int)((total_sent * 100) / st.st_size);
            
            ESP_LOGI(TAG, "📊 Progression: %d%% (%zu/%zu), %.2f MB/s", 
                    progress, total_sent, (size_t)st.st_size, speed_mbps);
            
            last_log_time = current_time;
        }
        
        // Yield CPU seulement pour très gros fichiers et éviter les timeouts
        if (is_large_file && (total_sent % (2 * 1024 * 1024)) == 0) {
            taskYIELD();
        }
    }
    
    // Finalisation
    heap_caps_free(buffer);
    fclose(file);
    
    unsigned long end_time = esp_timer_get_time() / 1000;
    float total_time = (end_time - start_time) / 1000.0f;
    float avg_speed = (total_sent / 1024.0f / 1024.0f) / total_time;
    
    if (err == ESP_OK) {
        err = httpd_resp_send_chunk(req, NULL, 0);  // Chunk de fin
        ESP_LOGI(TAG, "✅ Fichier envoyé avec succès: %.2f MB en %.2f s (%.2f MB/s, %s)", 
                total_sent / 1048576.0f, total_time, avg_speed, 
                using_psram ? "PSRAM" : "RAM interne");
    } else {
        ESP_LOGE(TAG, "❌ Erreur transfert: %d (envoyé: %.2f/%.2f MB)", 
                err, total_sent / 1048576.0f, st.st_size / 1048576.0f);
    }
    
    return err;
}

float WebDAVBox3::benchmark_sd_read(const std::string &filepath) {
    FILE *file = fopen(filepath.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Erreur ouverture %s", filepath.c_str());
        return 0.0;
    }

    const size_t CHUNK = 65536;  // 64K
    
    char *buf = NULL;
    bool using_psram = false;
    
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > CHUNK) {
        buf = (char*)heap_caps_malloc(CHUNK, MALLOC_CAP_SPIRAM);
        using_psram = true;
    }
    
    if (!buf) {
        buf = (char*)heap_caps_malloc(CHUNK, MALLOC_CAP_8BIT);
        using_psram = false;
    }
    
    if (!buf) {
        fclose(file);
        ESP_LOGE(TAG, "Erreur allocation mémoire");
        return 0.0;
    }

    size_t total = 0;
    size_t r = 0;
    unsigned long start = esp_timer_get_time();

    while ((r = fread(buf, 1, CHUNK, file)) > 0) {
        total += r;
    }

    unsigned long end = esp_timer_get_time();
    heap_caps_free(buf);
    fclose(file);

    float elapsed = (end - start) / 1e6f;
    float mbps = (total / 1024.0f / 1024.0f) / elapsed;

    ESP_LOGI(TAG, "📊 Benchmark SD: %.2f MB lus en %.2f s (%.2f MB/s) avec buffer en %s", 
             total / 1048576.0f, elapsed, mbps, using_psram ? "PSRAM" : "RAM interne");
    return mbps;
}

// ========== AUTRES HANDLERS (inchangés mais avec logs améliorés) ==========

esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);

    ESP_LOGI(TAG, "PROPFIND sur %s (URI: %s)", path.c_str(), req->uri);
    
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "Chemin non trouvé: %s (errno: %d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
    }
    
    bool is_directory = S_ISDIR(st.st_mode);
    std::string depth_header = "0";
    
    char depth_value[10] = {0};
    if (httpd_req_get_hdr_value_str(req, "Depth", depth_value, sizeof(depth_value)) == ESP_OK) {
        depth_header = depth_value;
        ESP_LOGI(TAG, "En-tête Depth: %s", depth_header.c_str());
    }
    
    std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                           "<D:multistatus xmlns:D=\"DAV:\">\n";
    
    std::string uri_path = req->uri;
    if (uri_path.empty() || uri_path == "/") uri_path = "/";
    if (is_directory && uri_path.back() != '/') uri_path += '/';
    
    ESP_LOGI(TAG, "URI formatée pour la réponse: %s", uri_path.c_str());
    
    response += generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);
    
    if (is_directory && (depth_header == "1" || depth_header == "infinity")) {
        auto files = list_dir(path);
        ESP_LOGI(TAG, "Trouvé %d fichiers/dossiers dans %s", files.size(), path.c_str());
        
        for (const auto &file_name : files) {
            std::string file_path = path;
            if (file_path.back() != '/') file_path += '/';
            file_path += file_name;
            
            struct stat file_stat;
            if (stat(file_path.c_str(), &file_stat) == 0) {
                bool is_file_dir = S_ISDIR(file_stat.st_mode);
                std::string href = uri_path;
                if (href.back() != '/') href += '/';
                href += file_name;
                if (is_file_dir) href += '/';
                
                response += generate_prop_xml(href, is_file_dir, file_stat.st_mtime, file_stat.st_size);
            }
        }
    }
    
    response += "</D:multistatus>";
    
    httpd_resp_set_type(req, "application/xml; charset=utf-8");
    add_cors_headers(req);
    httpd_resp_set_status(req, "207 Multi-Status");
    
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);

    ESP_LOGI(TAG, "PUT %s (URI: %s)", path.c_str(), req->uri);
    ESP_LOGI(TAG, "Content length: %d bytes", req->content_len);

    // Gestion Expect: 100-continue
    char expect_hdr[32];
    if (httpd_req_get_hdr_value_str(req, "Expect", expect_hdr, sizeof(expect_hdr)) == ESP_OK) {
        if (strcasecmp(expect_hdr, "100-continue") == 0) {
            ESP_LOGI(TAG, "Envoi réponse 100-continue");
            httpd_resp_set_status(req, "100 Continue");
            httpd_resp_send(req, NULL, 0);
        }
    }

    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Cannot overwrite directory");
    }

    // Créer le dossier parent si nécessaire
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string dir_path = path.substr(0, last_slash);
        struct stat dir_stat;
        if (stat(dir_path.c_str(), &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode)) {
            if (!create_directories_util(dir_path)) {
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create parent directory");
            }
        }
    }

    FILE *file = fopen(path.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Cannot open file: %s (errno=%d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
    }

    char buffer[8192];  // Buffer plus petit pour PUT
    int total_received = 0;
    int timeout_count = 0;

    while (true) {
        int received = httpd_req_recv(req, buffer, sizeof(buffer));

        if (received < 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                if (++timeout_count >= 10) {  // Plus de tolérance
                    ESP_LOGE(TAG, "Trop de timeouts, abandon");
                    fclose(file);
                    return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Timeout");
                }
                continue;
            } else {
                ESP_LOGE(TAG, "Erreur socket: %d", received);
                fclose(file);
                unlink(path.c_str());
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Socket error");
            }
        }

        if (received == 0) {
            ESP_LOGI(TAG, "Fin du flux de données");
            break;
        }

        size_t written = fwrite(buffer, 1, received, file);
        if (written != received) {
            ESP_LOGE(TAG, "Erreur d'écriture: écrit %zu / %d", written, received);
            fclose(file);
            unlink(path.c_str());
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
        }

        total_received += received;
        
        if (total_received % (100 * 1024) == 0) {  // Log tous les 100KB
            ESP_LOGI(TAG, "📤 Upload en cours: %d octets reçus", total_received);
        }
    }

    fclose(file);
    ESP_LOGI(TAG, "✅ Upload terminé: %s (%d octets)", path.c_str(), total_received);

    add_cors_headers(req);
    httpd_resp_set_hdr(req, "DAV", "1,2");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_status(req, total_received > 0 ? "201 Created" : "200 OK");
    return httpd_resp_sendstr(req, "");
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);

    ESP_LOGI(TAG, "DELETE %s", path.c_str());
    
    if (is_dir(path)) {
        if (rmdir(path.c_str()) == 0) {
            ESP_LOGI(TAG, "✅ Répertoire supprimé: %s", path.c_str());
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    } else {
        if (remove(path.c_str()) == 0) {
            ESP_LOGI(TAG, "✅ Fichier supprimé: %s", path.c_str());
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "❌ Erreur suppression: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);
    
    ESP_LOGI(TAG, "MKCOL %s", path.c_str());
    
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Path already exists");
    }
    
    if (mkdir(path.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "❌ Echec création dossier: %s (errno: %d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
    }
    
    ESP_LOGI(TAG, "✅ Dossier créé: %s", path.c_str());
    
    add_cors_headers(req);
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string src = get_file_path(req, inst->root_path_);

    char dest_uri[512];
    if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
        const char *uri_part = strstr(dest_uri, "//");
        if (uri_part) {
            uri_part = strchr(uri_part + 2, '/');
        } else {
            uri_part = strchr(dest_uri, '/');
        }
        
        if (!uri_part) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
        }
        
        std::string dst = inst->root_path_;
        if (dst.back() != '/') dst += '/';
        
        std::string uri_str = uri_part;
        if (!uri_str.empty() && uri_str.front() == '/') {
            uri_str = uri_str.substr(1);
        }
        
        dst += url_decode(uri_str);
        
        ESP_LOGI(TAG, "MOVE de %s vers %s", src.c_str(), dst.c_str());
        
        std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
        if (!parent_dir.empty() && !is_dir(parent_dir)) {
            if (mkdir(parent_dir.c_str(), 0755) != 0) {
                ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s", parent_dir.c_str());
            }
        }
        
        if (rename(src.c_str(), dst.c_str()) == 0) {
            ESP_LOGI(TAG, "✅ Déplacement réussi: %s -> %s", src.c_str(), dst.c_str());
            httpd_resp_set_status(req, "201 Created");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }

    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string src = get_file_path(req, inst->root_path_);

    char dest_uri[512];
    if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
        const char *path_start = strstr(dest_uri, inst->root_path_.c_str());
        std::string dst;
        
        if (path_start) {
            dst = path_start;
        } else {
            dst = inst->root_path_;
            if (dst.back() != '/' && dest_uri[0] != '/') dst += '/';
            if (dst.back() == '/' && dest_uri[0] == '/') dst += (dest_uri + 1);
            else dst += dest_uri;
        }
        
        ESP_LOGI(TAG, "COPY de %s vers %s", src.c_str(), dst.c_str());
        
        std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
        if (!parent_dir.empty() && !is_dir(parent_dir)) {
            mkdir(parent_dir.c_str(), 0777);
        }
        
        if (is_dir(src)) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
        }
        
        std::ifstream in(src, std::ios::binary);
        std::ofstream out(dst, std::ios::binary);

        if (!in || !out) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
        }

        out << in.rdbuf();
        
        ESP_LOGI(TAG, "✅ Copie réussie: %s -> %s", src.c_str(), dst.c_str());
        httpd_resp_set_status(req, "201 Created");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
}

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
    ESP_LOGD(TAG, "LOCK sur %s", req->uri);
    
    std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
                           "<D:prop xmlns:D=\"DAV:\">\n"
                           "  <D:lockdiscovery>\n"
                           "    <D:activelock>\n"
                           "      <D:locktype><D:write/></D:locktype>\n"
                           "      <D:lockscope><D:exclusive/></D:lockscope>\n"
                           "      <D:depth>0</D:depth>\n"
                           "      <D:timeout>Second-600</D:timeout>\n"
                           "    </D:activelock>\n"
                           "  </D:lockdiscovery>\n"
                           "</D:prop>";
    
    httpd_resp_set_type(req, "application/xml");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_unlock(httpd_req_t *req) {
    ESP_LOGD(TAG, "UNLOCK sur %s", req->uri);
    
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
    ESP_LOGD(TAG, "PROPPATCH sur %s", req->uri);
    
    std::string response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<D:multistatus xmlns:D=\"DAV:\">\n"
                           "  <D:response>\n"
                           "    <D:href>" + std::string(req->uri) + "</D:href>\n"
                           "    <D:propstat>\n"
                           "      <D:prop></D:prop>\n"
                           "      <D:status>HTTP/1.1 200 OK</D:status>\n"
                           "    </D:propstat>\n"
                           "  </D:response>\n"
                           "</D:multistatus>";
    
    httpd_resp_set_type(req, "application/xml");
    httpd_resp_set_status(req, "207 Multi-Status");
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

}  // namespace webdavbox3
}  // namespace esphome



































































































