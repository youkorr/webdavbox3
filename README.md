# WEBDAV ,SD_CARD

SD MMC cards components for esphome.and webdav
```
## Config

```yaml
esp32:
  board: esp32-p4-evboard
  cpu_frequency: 360MHz
  flash_size: 16MB
  framework:
    type: esp-idf
    version: 5.4.2
    platform_version: 54.03.21
    advanced:
      enable_idf_experimental_features: yes
    sdkconfig_options:
      CONFIG_SPIRAM_RODATA: y
      CONFIG_FATFS_LFN_STACK: "y"
      CONFIG_LWIP_SO_RCVBUF: "y"
      CONFIG_LWIP_MAX_SOCKETS: "16"
      # Options expérimentales ou spécifiques au ESP32-P4
      CONFIG_IDF_TARGET_ESP32P4: "y"
      HAS_ESP32_P4_CAMERA: "y"
      CONFIG_CAMERA_MODULE_ENABLED: "y"
  

"important"  ESP-IDF Framework
By default long file name are not enabled, to change this behaviour CONFIG_FATFS_LFN_STACK or CONFIG_FATFS_LFN_HEAP should be set in the framework configuration. See the Espressif documentation for more detail.

sd_mmc_card:
  id: sd_mmc_card
  mode_1bit: false
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
  power_ctrl_pin: GPIO43  # Optionnel : GPIO pour contrôler l'alimentation de la carte SD
```

* **mode_1bit** (Optional, bool): spécifie si le mode 1 bit ou 4 bits est utilisé
* **clk_pin** : (Required, GPIO): broche d'horloge
* **cmd_pin** : (Required, GPIO): broche de commande
* **data0_pin**: (Required, GPIO): broche de données 0
* **data1_pin**: (Optional, GPIO): broche de données 1, utilisée uniquement en mode 4 bits
* **data2_pin**: (Optional, GPIO): broche de données 2, utilisée uniquement en mode 4 bits
* **data3_pin**: (Optional, GPIO): broche de données 3, utilisée uniquement en mode 4 bits
* **slot**: 



Sample configuration for the Tab5 m6stack:
```yaml
sd_mmc_card:
  id: sd_card
  clk_pin: GPIO43
  cmd_pin: GPIO44
  data0_pin: GPIO39
  data1_pin: GPIO40
  data2_pin: GPIO41
  data3_pin: GPIO42
  mode_1bit: false
  slot: 0   
``` yaml
webdavbox3:
  id: sd_cards
  root_path: "/sdcard"
  url_prefix: "/"
  port: 81
  username: ""
  password: ""

```
