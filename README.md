# WEBDAV ,SD_CARD

SD MMC cards components for esphome.and webdav
```
## Config

```yaml
esp32:
  board: esp32-s3-devkitc-1
  flash_size: 16MB
  framework:
    type: esp-idf
    version: recommended
    sdkconfig_options:
      CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240: "y"
      CONFIG_ESP32S3_DATA_CACHE_64KB: "y"
      CONFIG_ESP32S3_DATA_CACHE_LINE_64B: "y"
      CONFIG_FATFS_LFN_HEAP: "y"    

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
* **power_ctrl_pin**: (Optional, GPIO): broche pour contrôler l'alimentation de la carte SD (par exemple, GPIO43 pour l'ESP32-S3-Box-3)

### Power Control (PWR_CTRL)

For devices like the ESP32-S3-Box-3, you can use the `power_ctrl_pin` to enable or disable SD card power. For example, on the ESP32-S3-Box-3, GPIO43 is often used to control power to the SD card reader.

Sample configuration for the ESP32-S3-Box-3:
```yaml
sd_mmc_card:
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  power_ctrl_pin: GPIO43  # Active l'alimentation du lecteur de carte SD
``` yaml
webdavbox3:
  id: sd_cards
  root_path: "/sdcard"
  url_prefix: "/"
  port: 81
  username: ""
  password: ""

```
