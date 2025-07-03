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

// Nouvelle fonction pour décoder les URL
std::string url_decode(const std::string &src) {
    std::string result;
    result.reserve(src.length());  // Pré-allouer pour éviter les réallocations

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
  // [Votre code existant]
  
  ESP_LOGI(TAG, "Diagnostic du système de fichiers");
  
  // 1. Vérifier si le répertoire racine est accessible  
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
          
          // Vérifier les permissions
          ESP_LOGI(TAG, "    Permissions: %c%c%c", 
                  (st.st_mode & S_IRUSR) ? 'r' : '-',
                  (st.st_mode & S_IWUSR) ? 'w' : '-',
                  (st.st_mode & S_IXUSR) ? 'x' : '-');
        } else {
          ESP_LOGE(TAG, "  - %s (erreur stat: %d)", entry->d_name, errno);
        }
      }
    }
    closedir(dir);
  }
  
  // 2. Essayer de créer un fichier test
  std::string test_file = root_path_;
  if (test_file.back() != '/') test_file += '/';
  test_file += "webdav_test.txt";
  
  FILE *f = fopen(test_file.c_str(), "w");
  if (f) {
    fputs("Test WebDAV access", f);
    fclose(f);
    ESP_LOGI(TAG, "Fichier test créé avec succès: %s", test_file.c_str());
  } else {
    ESP_LOGE(TAG, "Impossible de créer un fichier test (errno: %d)", errno);
  }
  
  // Continuer avec le reste du setup...
  this->configure_http_server();
  this->start_server();
}



void WebDAVBox3::loop() {
  // Rien pour le moment
}

void WebDAVBox3::configure_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Configuration de base
    config.server_port = port_;
    config.ctrl_port = port_ + 1000;
    config.max_uri_handlers = 16;
    
    // Paramètres de performance
    config.stack_size = 32768;
    config.core_id = tskNO_AFFINITY;
    //config.core_id = 0;  // Fixer sur le premier cœur
    config.task_priority = tskIDLE_PRIORITY + 10;  // Priorité plus élevée
    
    // Paramètres de délai et taille
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 60;  // 60 secondes d'attente max
    config.send_wait_timeout = 60;  // 60 secondes d'attente max
    
    // Pour les grandes requêtes
    config.max_resp_headers = 32;   // Plus d'en-têtes
    config.max_open_sockets = 7;    // Plus de sockets ouverts
    
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
  ESP_LOGI(TAG, "Serveur WebDAV démarré sur le port %d", port_);
  
  // Gestionnaire pour la racine
  httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handle_root,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &root_uri);
  
  // Gestionnaire OPTIONS pour les méthodes WebDAV
  httpd_uri_t options_uri = {
    .uri = "/*",
    .method = HTTP_OPTIONS,
    .handler = handle_webdav_options,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &options_uri);
  
  // Gestionnaires PROPFIND (pour la racine et tous les chemins)
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
  
  // Ajouter le support pour PROPPATCH
  httpd_uri_t proppatch_uri = {
    .uri = "/*",
    .method = HTTP_PROPPATCH,
    .handler = handle_webdav_proppatch,
    .user_ctx = this
  };
  httpd_register_uri_handler(server_, &proppatch_uri);
  
  // Autres gestionnaires WebDAV
// Gestionnaire pour GET avec caractère générique
  httpd_uri_t get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = handle_webdav_get,
    .user_ctx = this,
    //.is_websocket = false,
    //.handle_ws_control_frames = false,
    //.supported_subprotocol = NULL
  };
  httpd_register_uri_handler(server_, &get_uri);

  
  httpd_uri_t head_uri = {
    .uri = "/*",
    .method = HTTP_HEAD,
    .handler = handle_webdav_get, // Utilisation de GET pour HEAD en attendant une implémentation spécifique
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
  
  // Gestionnaires pour LOCK et UNLOCK
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
  
  ESP_LOGI(TAG, "Tous les gestionnaires WebDAV ont été enregistrés");
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

esp_err_t WebDAVBox3::handle_webdav_lock(httpd_req_t *req) {
  // Implémentation minimale pour LOCK
  ESP_LOGD(TAG, "LOCK sur %s", req->uri);
  
  // Réponse minimaliste pour indiquer un verrouillage réussi
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
  // Implémentation minimale pour UNLOCK
  ESP_LOGD(TAG, "UNLOCK sur %s", req->uri);
  
  // Réponse simple indiquant que le déverrouillage a réussi
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

// Nouveau gestionnaire pour PROPPATCH
esp_err_t WebDAVBox3::handle_webdav_proppatch(httpd_req_t *req) {
  ESP_LOGD(TAG, "PROPPATCH sur %s", req->uri);
  
  // Réponse simple pour les requêtes PROPPATCH
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

std::string WebDAVBox3::get_file_path(httpd_req_t *req, const std::string &root_path) {
  std::string uri = req->uri;
  std::string path = root_path;
  
  // Log avant décodage
  ESP_LOGI(TAG, "URI brute: %s", uri.c_str());
  
  // Décoder l'URL
  uri = url_decode(uri);
  ESP_LOGI(TAG, "URI décodée: %s", uri.c_str());
  
  // Vérifier si le chemin racine se termine par un '/'
  if (path.back() != '/') {
    path += '/';
  }
  
  // Supprimer le premier '/' de l'URI s'il existe
  if (!uri.empty() && uri.front() == '/') {
    uri = uri.substr(1);
  }
  
  // Si c'est la racine, retourner le chemin racine sans ajouter de '/'
  if (uri.empty()) {
    ESP_LOGI(TAG, "Accès à la racine: %s", path.c_str());
    return path.substr(0, path.length() - 1); // Enlever le dernier '/'
  }
  
  // Normaliser les barres obliques en évitant les doubles barres
  path += uri;
  
  ESP_LOGI(TAG, "Mapped URI %s to path %s", req->uri, path.c_str());
  
  // Vérifier si le chemin existe
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

// Fonction utilitaire pour générer la réponse XML pour un fichier ou répertoire
std::string WebDAVBox3::generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size) {
  // Format RFC1123 préféré par de nombreux clients WebDAV
  char time_buf[50];
  struct tm *gmt = gmtime(&modified);
  strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
  
  // Extraire juste le nom pour displayname
  std::string display_name = href;
  if (href.back() == '/') {
    display_name = href.substr(0, href.length() - 1);  // Enlever le '/' final
  }
  size_t last_slash = display_name.find_last_of('/');
  if (last_slash != std::string::npos) {
    display_name = display_name.substr(last_slash + 1);
  }
  if (display_name.empty() && href == "/") {
    display_name = "Root";  // Nom spécial pour la racine
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
    
    // Identifier le type MIME en fonction de l'extension
    std::string content_type = "application/octet-stream";
    size_t dot_pos = href.find_last_of(".");
    if (dot_pos != std::string::npos) {
      std::string ext = href.substr(dot_pos + 1);
      // Convertir en minuscules
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
    }
    
    xml += "        <D:getcontenttype>" + content_type + "</D:getcontenttype>\n";
  }
  
  xml += "      </D:prop>\n";
  xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
  xml += "    </D:propstat>\n";
  xml += "  </D:response>\n";
  
  return xml;
}


// ========== HANDLERS ==========

esp_err_t WebDAVBox3::handle_root(httpd_req_t *req) {
  httpd_resp_send(req, "ESP32 WebDAV Server OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}
void WebDAVBox3::add_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", 
                      "GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", 
                      "Authorization, Content-Type, Depth, Destination, Overwrite");
}
void WebDAVBox3::register_handlers() {
    if (server_ == nullptr) {
        ESP_LOGE(TAG, "Server not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Registering WebDAV handlers");
    
    // Structure d'aide pour l'enregistrement
    struct {
        const char *uri;
        httpd_method_t method;
        esp_err_t (*handler)(httpd_req_t *req);
        const char *description;
    } handlers[] = {
        {"/*", HTTP_GET, handle_webdav_get, "GET"},
        {"/*", HTTP_HEAD, handle_webdav_get, "HEAD"},
        {"/*", HTTP_PUT, handle_webdav_put, "PUT"},
        {"/*", HTTP_DELETE, handle_webdav_delete, "DELETE"},
        {"/*", HTTP_MKCOL, handle_webdav_mkcol, "MKCOL"},
        {"/*", HTTP_PROPFIND, handle_webdav_propfind, "PROPFIND"},
        {"/*", HTTP_OPTIONS, handle_webdav_options, "OPTIONS"},
        // Ajoutez d'autres handlers au besoin
    };
    
    // Enregistrement avec gestion des erreurs
    for (const auto &h : handlers) {
        httpd_uri_t uri_handler = {
            .uri = h.uri,
            .method = h.method,
            .handler = h.handler,
            .user_ctx = this
        };
        
        esp_err_t ret = httpd_register_uri_handler(server_, &uri_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s handler: %d", h.description, ret);
        } else {
            ESP_LOGI(TAG, "Registered %s handler", h.description);
        }
    }
}

// Gestionnaire OPTIONS
esp_err_t WebDAVBox3::handle_webdav_options(httpd_req_t *req) {
    ESP_LOGI(TAG, "OPTIONS %s", req->uri);
    
    // CRUCIAL: Complete CORS headers for all requests
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", 
                     "GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", 
                     "Authorization, Content-Type, Depth, Destination, Overwrite");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "3600");
    
    // Standard WebDAV headers
    httpd_resp_set_hdr(req, "DAV", "1, 2");
    httpd_resp_set_hdr(req, "Allow", 
                     "GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, OPTIONS");
    httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");
    
    // Set the content type
    httpd_resp_set_type(req, "text/plain");
    
    // Send empty response
    return httpd_resp_sendstr(req, "");
}



esp_err_t WebDAVBox3::handle_webdav_propfind(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  // Ajouter plus de logs détaillés
  ESP_LOGI(TAG, "PROPFIND sur %s (URI: %s)", path.c_str(), req->uri);
  
  // Vérifier si le chemin existe
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Chemin non trouvé: %s (errno: %d)", path.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }
  
  // Lister le contenu du répertoire pour diagnostic
  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path.c_str());
    if (dir) {
      ESP_LOGI(TAG, "Contenu du répertoire %s:", path.c_str());
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
          ESP_LOGI(TAG, "  - %s", entry->d_name);
        }
      }
      closedir(dir);
    }
  }
  
  bool is_directory = S_ISDIR(st.st_mode);
  std::string depth_header = "0";  // Par défaut, profondeur 0
  
  // Récupérer l'en-tête Depth
  char depth_value[10] = {0};
  if (httpd_req_get_hdr_value_str(req, "Depth", depth_value, sizeof(depth_value)) == ESP_OK) {
    depth_header = depth_value;
    ESP_LOGI(TAG, "En-tête Depth: %s", depth_header.c_str());
  }
  
  // Construction de la réponse XML avec un format plus compatible
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                         "<D:multistatus xmlns:D=\"DAV:\">\n";
  
  // URI relatif pour le chemin actuel - s'assurer qu'il commence et se termine correctement
  std::string uri_path = req->uri;
  if (uri_path.empty() || uri_path == "/") uri_path = "/";
  // Assurer que les dossiers se terminent par '/'
  if (is_directory && uri_path.back() != '/') uri_path += '/';
  
  ESP_LOGI(TAG, "URI formatée pour la réponse: %s", uri_path.c_str());
  
  // Ajouter les propriétés pour le chemin actuel (avec format amélioré)
  response += generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);
  
  // Si c'est un répertoire et que la profondeur > 0, lister son contenu
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
        
        ESP_LOGI(TAG, "Ajout de %s à la réponse PROPFIND (est_dir: %d)", href.c_str(), is_file_dir);
        response += generate_prop_xml(href, is_file_dir, file_stat.st_mtime, file_stat.st_size);
      } else {
        ESP_LOGE(TAG, "Impossible d'obtenir le stat pour %s (errno: %d)", file_path.c_str(), errno);
      }
    }
  }
  
  response += "</D:multistatus>";
  
  // Ajouter des en-têtes CORS et autres en-têtes nécessaires
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, HEAD, PUT, OPTIONS, DELETE, PROPFIND, PROPPATCH, MKCOL");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization, Depth, Content-Type");
  httpd_resp_set_status(req, "207 Multi-Status");
  
  // Afficher la réponse XML pour débogage (mais pas en prod, c'est volumineux)
  ESP_LOGD(TAG, "Réponse XML: %s", response.c_str());
  
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}


esp_err_t WebDAVBox3::handle_webdav_get(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);
    
    ESP_LOGI(TAG, "GET %s (URI: %s)", path.c_str(), req->uri);
    
    // Vérifier si le fichier existe
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "Fichier non trouvé: %s (errno: %d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    }
    
    // Vérifier si c'est un répertoire
    if (S_ISDIR(st.st_mode)) {
        return handle_webdav_propfind(req);
    }
    
    // Ouvrir le fichier
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le fichier: %s (errno: %d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    }
    
    // Configurer la connexion pour un transfert optimal
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd >= 0) {
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
        
        // Augmenter considérablement la taille du tampon d'envoi pour les gros fichiers
        int send_buf = 256 * 1024;  // 256KB pour maximiser le débit
        setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_buf, sizeof(send_buf));
        
        // Timeouts beaucoup plus longs pour les gros fichiers
        struct timeval timeout;
        timeout.tv_sec = 300;  // 5 minutes pour les très gros fichiers
        timeout.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        // Désactiver l'algorithme de Nagle pour améliorer la vitesse
        opt = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }
    
    // Ajouter des en-têtes CORS et caching appropriés
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, HEAD");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");  // Cache d'une heure
    
    // Déterminer le type de contenu
    const char* content_type = "application/octet-stream";
    const char* ext = strrchr(path.c_str(), '.');
    if (ext) {
        ext++; // Avancer après le point
        if (strcasecmp(ext, "mp3") == 0) content_type = "audio/mpeg";
        else if (strcasecmp(ext, "mp4") == 0) content_type = "video/mp4";
        else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) content_type = "image/jpeg";
        else if (strcasecmp(ext, "png") == 0) content_type = "image/png";
        else if (strcasecmp(ext, "flac") == 0) content_type = "audio/flac";    
        else if (strcasecmp(ext, "gif") == 0) content_type = "image/gif";
        else if (strcasecmp(ext, "pdf") == 0) content_type = "application/pdf";
        else if (strcasecmp(ext, "txt") == 0) content_type = "text/plain";
        else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) content_type = "text/html";
    }
    
    // Configurer les en-têtes de la réponse
    httpd_resp_set_type(req, content_type);
    //httpd_resp_set_hdr(req, "Content-Length", std::to_string(st.st_size).c_str());
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    
    ESP_LOGI(TAG, "Envoi du fichier %s (%zu octets, type: %s)", path.c_str(), (size_t)st.st_size, content_type);
    
    // Stratégie optimisée pour les fichiers volumineux - utiliser des buffer plus grands grâce à la PSRAM
    // Utiliser la PSRAM si disponible pour allouer de grands buffers
    const size_t CHUNK_SIZE = (st.st_size > 300 * 1024 * 1024) ? 262144 :  // 256K pour très grands fichiers 
                             (st.st_size > 50 * 1024 * 1024) ? 131072 :   // 128K pour grands fichiers
                             65536;                                       // 64K pour fichiers moyens/petits
    
    ESP_LOGI(TAG, "Utilisation d'un buffer de taille %zu pour un fichier de %zu octets", 
             CHUNK_SIZE, (size_t)st.st_size);
    
    // Vérifier si la PSRAM est disponible
    bool using_psram = false;
    char *buffer = nullptr;
    
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > CHUNK_SIZE) {
        // Utiliser la PSRAM pour le buffer
        buffer = (char*)heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_SPIRAM);
        using_psram = true;
        ESP_LOGI(TAG, "Buffer alloué en PSRAM");
    } else {
        // Fallback sur la mémoire interne
        buffer = (char*)heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_8BIT);
        ESP_LOGW(TAG, "PSRAM non disponible ou insuffisante, utilisation de la mémoire interne");
    }
    
    if (!buffer) {
        ESP_LOGE(TAG, "Impossible d'allouer le buffer pour l'envoi");
        fclose(file);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server Error");
    }
    
    // Afficher les infos mémoire avant le transfert
    ESP_LOGI(TAG, "Mémoire avant transfert - Heap interne: %zu, PSRAM: %zu", 
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // Optimisation: positionnez le fichier au début pour garantir un départ frais
    fseek(file, 0, SEEK_SET);
    
    // Désactiver les logs trop fréquents pour les gros fichiers
    bool is_large_file = (st.st_size > 50 * 1024 * 1024);
    size_t log_interval = is_large_file ? (10 * 1024 * 1024) : (1 * 1024 * 1024);  // 10MB ou 1MB
    
    size_t read_bytes;
    size_t total_sent = 0;
    esp_err_t err = ESP_OK;
    
    // Commencer la lecture et l'envoi du fichier par chunks
    unsigned long start_time = esp_timer_get_time() / 1000;  // Temps en ms
    unsigned long last_log_time = start_time;
    
    while ((read_bytes = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        err = httpd_resp_send_chunk(req, buffer, read_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Erreur d'envoi du chunk (%zu bytes): %d", read_bytes, err);
            break;
        }
        
        total_sent += read_bytes;
        
        // Logs moins fréquents pour les gros fichiers
        unsigned long current_time = esp_timer_get_time() / 1000;
        if (total_sent % log_interval == 0 || current_time - last_log_time > 5000) {
            float elapsed_sec = (current_time - start_time) / 1000.0f;
            float speed_kbps = (total_sent / 1024.0f) / elapsed_sec;
            
            ESP_LOGI(TAG, "Envoyé: %zu/%zu octets (%d%%), %.2f KB/s", 
                    total_sent, (size_t)st.st_size, 
                    (int)((total_sent * 100) / st.st_size),
                    speed_kbps);
            
            last_log_time = current_time;
        }
        
        // Pour les très gros fichiers (>300MB), pause minimale seulement tous les 10MB
        if (st.st_size > 300 * 1024 * 1024 && total_sent % (10 * 1024 * 1024) == 0) {
            taskYIELD();  // Cède juste le CPU sans délai
        }
    }
    
    // Libérer le buffer
    heap_caps_free(buffer);
    fclose(file);
    
    unsigned long end_time = esp_timer_get_time() / 1000;
    float total_time = (end_time - start_time) / 1000.0f;
    float avg_speed = (total_sent / 1024.0f / 1024.0f) / total_time;  // MB/s
    
    // Afficher les infos mémoire après le transfert
    ESP_LOGI(TAG, "Mémoire après transfert - Heap interne: %zu, PSRAM: %zu", 
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    if (err == ESP_OK) {
        // Fermer la réponse avec un chunk vide
        err = httpd_resp_send_chunk(req, NULL, 0);
        ESP_LOGI(TAG, "Fichier envoyé avec succès: %zu octets en %.2f secondes (%.2f MB/s, buffer en %s)", 
                total_sent, total_time, avg_speed, using_psram ? "PSRAM" : "RAM interne");
    } else {
        ESP_LOGE(TAG, "Erreur lors de l'envoi du fichier: %d (total envoyé: %zu/%zu octets, %.2f MB/s)",
                err, total_sent, (size_t)st.st_size, avg_speed);
    }
    
    return err;
}

float WebDAVBox3::benchmark_sd_read(const std::string &filepath) {
    FILE *file = fopen(filepath.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Erreur ouverture %s", filepath.c_str());
        return 0.0;
    }

    // Utiliser un buffer plus grand grâce à la PSRAM
    const size_t CHUNK = 65536;  // 64K au lieu de 8K
    
    // Essayer d'allouer en PSRAM d'abord
    char *buf = NULL;
    bool using_psram = false;
    
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > CHUNK) {
        buf = (char*)heap_caps_malloc(CHUNK, MALLOC_CAP_SPIRAM);
        using_psram = true;
    }
    
    // Si pas de PSRAM disponible, fallback sur la mémoire interne
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

    ESP_LOGI(TAG, "Benchmark SD: %.2f MB lus en %.2f s (%.2f MB/s) avec buffer en %s", 
             total / 1048576.0f, elapsed, mbps, using_psram ? "PSRAM" : "RAM interne");
    return mbps;
}
esp_err_t WebDAVBox3::handle_webdav_get_small_file(httpd_req_t *req, const std::string &path, size_t file_size) {
    // Cette méthode est pour les fichiers jusqu'à 8MB
    const size_t MAX_FILE_SIZE = 8 * 1024 * 1024; // 8 Mo

    if (file_size > MAX_FILE_SIZE) {
        return ESP_FAIL;  // Trop grand, utiliser la méthode standard
    }
    
    // Allouer un buffer pour tout le fichier
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        ESP_LOGE(TAG, "Impossible d'allouer de la mémoire pour le fichier (%zu octets)", file_size);
        return ESP_FAIL;
    }
    
    // Ouvrir et lire le fichier
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        heap_caps_free(buffer);
        return ESP_FAIL;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != file_size) {
        ESP_LOGE(TAG, "Échec de lecture du fichier complet: %zu/%zu", bytes_read, file_size);
        heap_caps_free(buffer);
        return ESP_FAIL;
    }
    
    // Envoyer le contenu en une seule fois
    esp_err_t ret = httpd_resp_send(req, (const char*)buffer, bytes_read);
    heap_caps_free(buffer);
    
    ESP_LOGI(TAG, "Fichier envoyé en une fois: %zu octets, résultat: %d", bytes_read, ret);
    return ret;
}

// Corrected PUT handler with chunked transfer support


esp_err_t WebDAVBox3::handle_webdav_put(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);

    ESP_LOGI(TAG, "===== PUT REQUEST HEADERS =====");

    char expect_hdr[32];
    if (httpd_req_get_hdr_value_str(req, "Expect", expect_hdr, sizeof(expect_hdr)) == ESP_OK) {
        if (strcasecmp(expect_hdr, "100-continue") == 0) {
            ESP_LOGI(TAG, "Client expects 100-continue");
            httpd_resp_set_status(req, "100 Continue");
            httpd_resp_send(req, NULL, 0);  // Send empty body
        }
    }

    // Vérification Expect: 100-continue
    char expect_val[64] = {0};
    if (httpd_req_get_hdr_value_len(req, "Expect") > 0) {
        httpd_req_get_hdr_value_str(req, "Expect", expect_val, sizeof(expect_val));
        if (strcasecmp(expect_val, "100-continue") == 0) {
            ESP_LOGI(TAG, "Sending 100-continue response");
            httpd_resp_set_status(req, "100 Continue");
            httpd_resp_send(req, "", 0);
            // continue quand même l’exécution
        }
    }

    // Détection du mode chunked
    bool is_chunked = false;
    char transfer_encoding[64] = {0};
    if (httpd_req_get_hdr_value_len(req, "Transfer-Encoding") > 0) {
        httpd_req_get_hdr_value_str(req, "Transfer-Encoding", transfer_encoding, sizeof(transfer_encoding));
        if (strcasecmp(transfer_encoding, "chunked") == 0) {
            is_chunked = true;
        }
    }

    ESP_LOGI(TAG, "PUT %s (URI: %s)", path.c_str(), req->uri);
    ESP_LOGI(TAG, "Content length: %d bytes", req->content_len);

    // Ne pas écraser un dossier
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Cannot overwrite directory");
    }

    // Création récursive du dossier parent
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

    // Ouverture du fichier en écriture
    FILE *file = fopen(path.c_str(), "wb");
    if (!file) {
        ESP_LOGE(TAG, "Cannot open file: %s (errno=%d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
    }

    // Lecture des données
    char buffer[4096];
    int total_received = 0;
    int timeout_count = 0;

    while (true) {
        int received = httpd_req_recv(req, buffer, sizeof(buffer));

        if (received < 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                if (++timeout_count >= 5) {
                    ESP_LOGE(TAG, "Too many timeouts, aborting");
                    fclose(file);
                    return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Timeout");
                }
                continue;
            } else {
                ESP_LOGE(TAG, "Socket error: %d", received);
                fclose(file);
                unlink(path.c_str());
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Socket error");
            }
        }

        if (received == 0) {
            ESP_LOGI(TAG, "End of data stream");
            break;
        }

        size_t written = fwrite(buffer, 1, received, file);
        if (written != received) {
            ESP_LOGE(TAG, "Write error: wrote %zu / %d", written, received);
            fclose(file);
            unlink(path.c_str());
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
        }

        total_received += received;
    }

    fclose(file);
    ESP_LOGI(TAG, "✅ Upload complete: %s (%d bytes)", path.c_str(), total_received);

    // Réponse HTTP
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "DAV", "1,2");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_status(req, total_received > 0 ? "201 Created" : "200 OK");
    return httpd_resp_sendstr(req, "");
}

esp_err_t WebDAVBox3::handle_webdav_delete(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string path = get_file_path(req, inst->root_path_);

  ESP_LOGD(TAG, "DELETE %s", path.c_str());
  
  // Vérifier si c'est un répertoire ou un fichier
  if (is_dir(path)) {
    // Supprimer le répertoire (doit être vide)
    if (rmdir(path.c_str()) == 0) {
      ESP_LOGI(TAG, "Répertoire supprimé: %s", path.c_str());
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Erreur lors de la suppression du répertoire: %s (errno: %d)", path.c_str(), errno);
    }
  } else {
    // Supprimer le fichier
    if (remove(path.c_str()) == 0) {
      ESP_LOGI(TAG, "Fichier supprimé: %s", path.c_str());
      httpd_resp_set_status(req, "204 No Content");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Erreur lors de la suppression du fichier: %s (errno: %d)", path.c_str(), errno);
    }
  }

  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
}

esp_err_t WebDAVBox3::handle_webdav_mkcol(httpd_req_t *req) {
    auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
    std::string path = get_file_path(req, inst->root_path_);
    
    ESP_LOGI(TAG, "MKCOL %s (URI: %s)", path.c_str(), req->uri);
    
    // Vérifier si le chemin existe déjà
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        ESP_LOGE(TAG, "Le chemin existe déjà: %s", path.c_str());
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
    }
    
    // Créer le dossier
    if (mkdir(path.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Échec de la création du dossier: %s (errno: %d)", path.c_str(), errno);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
    }
    
    ESP_LOGI(TAG, "Dossier créé avec succès: %s", path.c_str());
    
    // En-têtes de réponse
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}


esp_err_t WebDAVBox3::handle_webdav_move(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    ESP_LOGD(TAG, "Destination brute: %s", dest_uri);
    
    // Extraire la partie URI du chemin de destination
    const char *uri_part = strstr(dest_uri, "//");
    if (uri_part) {
      // Sauter le protocole et le nom d'hôte
      uri_part = strchr(uri_part + 2, '/');
    } else {
      uri_part = strchr(dest_uri, '/');
    }
    
    if (!uri_part) {
      ESP_LOGE(TAG, "Format d'URI de destination invalide: %s", dest_uri);
      return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid destination URI");
    }
    
    // Construire directement le chemin de destination
    std::string dst = inst->root_path_;
    
    // S'assurer que le chemin se termine par un '/'
    if (dst.back() != '/') {
      dst += '/';
    }
    
    // Supprimer le premier '/' de l'URI si présent
    std::string uri_str = uri_part;
    if (!uri_str.empty() && uri_str.front() == '/') {
      uri_str = uri_str.substr(1);
    }
    
    // Ajouter l'URI décodée au chemin racine
    dst += url_decode(uri_str);
    
    ESP_LOGD(TAG, "MOVE de %s vers %s", src.c_str(), dst.c_str());
    
    // Créer le répertoire parent si nécessaire
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      ESP_LOGI(TAG, "Création du répertoire parent: %s", parent_dir.c_str());
      if (mkdir(parent_dir.c_str(), 0755) != 0) {
        ESP_LOGE(TAG, "Impossible de créer le répertoire parent: %s (errno: %d)", parent_dir.c_str(), errno);
      }
    }
    
    if (rename(src.c_str(), dst.c_str()) == 0) {
      ESP_LOGI(TAG, "Déplacement réussi: %s -> %s", src.c_str(), dst.c_str());
      httpd_resp_set_status(req, "201 Created");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Erreur de déplacement: %s -> %s (errno: %d)", src.c_str(), dst.c_str(), errno);
    }
  } else {
    ESP_LOGE(TAG, "En-tête Destination manquant pour MOVE");
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Move failed");
}

esp_err_t WebDAVBox3::handle_webdav_copy(httpd_req_t *req) {
  auto *inst = static_cast<WebDAVBox3 *>(req->user_ctx);
  std::string src = get_file_path(req, inst->root_path_);

  char dest_uri[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_uri, sizeof(dest_uri)) == ESP_OK) {
    // Traitement similaire à MOVE pour obtenir le chemin de destination
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
    
    ESP_LOGD(TAG, "COPY de %s vers %s", src.c_str(), dst.c_str());
    
    // Créer le répertoire parent si nécessaire
    std::string parent_dir = dst.substr(0, dst.find_last_of('/'));
    if (!parent_dir.empty() && !is_dir(parent_dir)) {
      mkdir(parent_dir.c_str(), 0777);
    }
    
    // Pour les répertoires, il faudrait une copie récursive (non implémentée ici)
    if (is_dir(src)) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
    }
    
    // Copie de fichier
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);

    if (!in || !out)
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");

    out << in.rdbuf();
    
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }

  return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Copy failed");
}

}  // namespace webdavbox3
}  // namespace esphome






































































































































































































