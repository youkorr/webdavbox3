# WEBDAV ,SD_CARD for Tab5

SD MMC cards components for esphome.and webdav for esp32P4 Tab5 m5stack
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
      CONFIG_SPIRAM_USE_CAPS_ALLOC: y
      CONFIG_SPIRAM_USE_MALLOC: y
      CONFIG_ESP_MAIN_TASK_STACK_SIZE: "16384"  
      CONFIG_ESP_TIMER_TASK_STACK_SIZE: "4096"
      CONFIG_ESP_TASK_WDT_TIMEOUT_S: "60"       
      CONFIG_ESP_TASK_LOOP_STACK_SIZE: "16384"   
      DCONFIG_ESP_JPG_DECODE_ENABLE: "y"
  
```

* **mode_1bit** (Optional, bool): spécifie si le mode 1 bit ou 4 bits est utilisé
* **clk_pin** : (Required, GPIO): broche d'horloge
* **cmd_pin** : (Required, GPIO): broche de commande
* **data0_pin**: (Required, GPIO): broche de données 0
* **data1_pin**: (Optional, GPIO): broche de données 1, utilisée uniquement en mode 4 bits
* **data2_pin**: (Optional, GPIO): broche de données 2, utilisée uniquement en mode 4 bits
* **data3_pin**: (Optional, GPIO): broche de données 3, utilisée uniquement en mode 4 bits
* **slot**: 



Sample configuration for the  esp32P4 Tab5 m5stack:
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

storage:
    platform: sd_direct
    id: my_storage
    sd_component: sd_card
    root_path: "/sdcard" 
    sd_images:
      - id: testree
        file_path: "/img/sanctuary.jpg"
        resize: 1280x720
        format: rgb565
        byte_order: little_endian 
        auto_load: true

```
