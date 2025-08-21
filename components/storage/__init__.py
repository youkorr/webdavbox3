from __future__ import annotations

import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, image
from esphome import automation
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_PLATFORM,
    CONF_RESIZE,
    CONF_TYPE,
)
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

DOMAIN = "storage"
DEPENDENCIES = ["display"]

# Namespaces
storage_ns = cg.esphome_ns.namespace("storage")
sd_mmc_card_ns = cg.esphome_ns.namespace("sd_mmc_card")

# Classes
StorageComponent = storage_ns.class_("StorageComponent", cg.Component)
SdImageComponent = storage_ns.class_("SdImageComponent", cg.Component, image.Image_)
SdMmc = sd_mmc_card_ns.class_("SdMmc")

# Configuration keys
CONF_STORAGE_COMPONENT = "storage_component"
CONF_ROOT_PATH = "root_path"
CONF_OUTPUT_FORMAT = "format"
CONF_BYTE_ORDER = "byte_order"
CONF_SD_COMPONENT = "sd_component"
CONF_SD_IMAGES = "sd_images"
CONF_FILE_PATH = "file_path"
CONF_AUTO_LOAD = "auto_load"  # Uniquement pour sd_images, pas pour storage

# FIXED: Use simple string mappings instead of enums to avoid compilation issues
CONF_OUTPUT_IMAGE_FORMATS = {
    "RGB565": "RGB565",
    "RGB888": "RGB888", 
    "RGBA": "RGBA",
}

CONF_BYTE_ORDERS = {
    "LITTLE_ENDIAN": "LITTLE_ENDIAN",
    "BIG_ENDIAN": "BIG_ENDIAN",
}

# Actions - Using standard ESPHome automation framework
SdImageLoadAction = storage_ns.class_("SdImageLoadAction", automation.Action)
SdImageUnloadAction = storage_ns.class_("SdImageUnloadAction", automation.Action)

# Schema pour SdImageComponent - auto_load UNIQUEMENT ICI
SD_IMAGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdImageComponent),
        cv.Required(CONF_FILE_PATH): cv.string,
        cv.Optional(CONF_OUTPUT_FORMAT, default="RGB565"): cv.enum(CONF_OUTPUT_IMAGE_FORMATS, upper=True),
        cv.Optional(CONF_BYTE_ORDER, default="LITTLE_ENDIAN"): cv.enum(CONF_BYTE_ORDERS, upper=True),
        cv.Optional(CONF_RESIZE): cv.dimensions,
        cv.Optional(CONF_TYPE, default="SD_IMAGE"): cv.string,
        cv.Optional(CONF_AUTO_LOAD, default=True): cv.boolean,  # auto_load SEULEMENT pour les sd_images
    }
)

# Schema de validation pour StorageComponent - PAS d'auto_load ici
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageComponent),
        cv.Optional(CONF_PLATFORM, default="sd_direct"): cv.string,
        cv.Optional(CONF_SD_COMPONENT): cv.use_id(SdMmc),
        cv.Optional(CONF_ROOT_PATH, default="/"): cv.string,
        cv.Optional(CONF_SD_IMAGES, default=[]): cv.ensure_list(SD_IMAGE_SCHEMA),
        # PAS d'auto_load dans le schema principal - uniquement dans sd_images
    }
).extend(cv.COMPONENT_SCHEMA)

# Actions pour charger/décharger des images
LOAD_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SdImageComponent),
    cv.Optional(CONF_FILE_PATH): cv.templatable(cv.string),
})

UNLOAD_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SdImageComponent),
})

async def sd_image_load_action_to_code(config, action_id, template_arg, args):
    """Action pour charger une image depuis la SD"""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if CONF_FILE_PATH in config:
        template_ = await cg.templatable(config[CONF_FILE_PATH], args, cg.std_string)
        cg.add(var.set_file_path(template_))
    return var

async def sd_image_unload_action_to_code(config, action_id, template_arg, args):
    """Action pour décharger une image"""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var

# Enregistrer les actions
automation.register_action(
    "sd_image.load", 
    SdImageLoadAction, 
    LOAD_ACTION_SCHEMA
)(sd_image_load_action_to_code)

automation.register_action(
    "sd_image.unload", 
    SdImageUnloadAction, 
    UNLOAD_ACTION_SCHEMA
)(sd_image_unload_action_to_code)

async def to_code(config):
    """Génère le code C++ pour le composant storage"""
    
    # Créer le composant principal - PAS d'auto_load ici
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Configuration du composant principal
    cg.add(var.set_platform(config[CONF_PLATFORM]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
    
    if CONF_SD_COMPONENT in config:
        sd_comp = await cg.get_variable(config[CONF_SD_COMPONENT])
        cg.add(var.set_sd_component(sd_comp))
    
    # Configuration des images SD - auto_load configuré ici
    if CONF_SD_IMAGES in config:
        for img_config in config[CONF_SD_IMAGES]:
            await setup_sd_image_component(img_config, var)

async def setup_sd_image_component(config, parent_storage):
    """Configure un SdImageComponent"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Lier au composant storage parent
    cg.add(var.set_storage_component(parent_storage))
    
    # FIXED: Pass strings directly instead of enum values
    cg.add(var.set_file_path(config[CONF_FILE_PATH]))
    
    # Get the string values from the config (they're already strings due to our mapping)
    output_format_str = config[CONF_OUTPUT_FORMAT]  # This is already a string like "RGB565"
    byte_order_str = config[CONF_BYTE_ORDER]        # This is already a string like "LITTLE_ENDIAN"
    
    cg.add(var.set_output_format_string(output_format_str))
    cg.add(var.set_byte_order_string(byte_order_str))
    
    # Configuration auto_load - SEULEMENT pour les sd_images
    cg.add(var.set_auto_load(config[CONF_AUTO_LOAD]))
    
    if CONF_RESIZE in config:
        cg.add(var.set_resize(config[CONF_RESIZE][0], config[CONF_RESIZE][1]))
    
    return var

# Encodeur personnalisé pour les images SD
class SdImageEncoder(image.ImageEncoder):
    """Encodeur d'image pour les images stockées sur carte SD"""
    
    allow_config = {image.CONF_ALPHA_CHANNEL, image.CONF_CHROMA_KEY, image.CONF_OPAQUE}

    def __init__(self, width, height, transparency, dither, invert_alpha, storage_component, file_path, output_format="RGB565"):
        super().__init__(width, height, transparency, dither, invert_alpha)
        self.storage_component = storage_component
        self.file_path = file_path
        self.output_format = output_format
        
    @staticmethod
    def validate(value):
        # Validation spécifique pour SD image
        if not value.get(CONF_FILE):
            raise cv.Invalid("File path is required for SD images")
        return value

    def convert(self, image_data, path):
        # Pour les images SD, on ne fait pas de conversion ici
        # La conversion se fait côté C++ lors du chargement depuis la SD
        return image_data

    def encode(self, pixel):
        # L'encodage se fait côté C++
        pass

# Ajouter le type SD_IMAGE au système d'images d'ESPHome
def register_sd_image_type():
    """Enregistre le type d'image SD dans le système ESPHome"""
    # Ajouter à la liste des types d'images disponibles
    if hasattr(image, 'IMAGE_TYPE'):
        image.IMAGE_TYPE["SD_IMAGE"] = SdImageEncoder
    
    # Ajouter aussi aux formats d'image reconnus pour la compatibilité LVGL
    if hasattr(image, 'IMAGE_TYPE_SCHEMA'):
        # Étendre le schéma des types d'images
        image.IMAGE_TYPE_SCHEMA = cv.one_of(
            *list(image.IMAGE_TYPE_SCHEMA.validators), 
            "SD_IMAGE", 
            upper=True
        )

# Intégration avec le système d'images ESPHome
def validate_sd_image(value):
    """Validation pour les images SD dans la configuration image"""
    if value.get(CONF_TYPE) == "SD_IMAGE":
        # Validation spécifique aux images SD
        if not value.get(CONF_FILE):
            raise cv.Invalid("file is required for SD_IMAGE type")
        if not value.get(CONF_STORAGE_COMPONENT):
            raise cv.Invalid("storage_component is required for SD_IMAGE type")
    return value

# Hook pour modifier le comportement des images ESPHome
async def image_to_code_hook(config):
    """Hook appelé lors de la génération du code pour les images"""
    if config.get(CONF_TYPE) == "SD_IMAGE":
        # Générer un SdImageComponent au lieu d'une Image standard
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        
        cg.add(var.set_file_path(config[CONF_FILE]))
        
        if CONF_STORAGE_COMPONENT in config:
            storage = await cg.get_variable(config[CONF_STORAGE_COMPONENT])
            cg.add(var.set_storage_component(storage))
        
        # Use string methods with proper string values
        format_str = config.get(CONF_OUTPUT_FORMAT, "RGB565")
        byte_order_str = config.get(CONF_BYTE_ORDER, "LITTLE_ENDIAN")
        
        cg.add(var.set_output_format_string(format_str))
        cg.add(var.set_byte_order_string(byte_order_str))
        
        # Configuration auto_load dans le hook aussi - SEULEMENT si présent
        auto_load = config.get(CONF_AUTO_LOAD, True)
        cg.add(var.set_auto_load(auto_load))
        
        return var
    
    return None

# Configuration d'initialisation
def setup_hooks():
    """Configure les hooks d'intégration avec ESPHome"""
    # Enregistrer le type d'image
    register_sd_image_type()
    
    # Ajouter le hook pour les images si disponible
    if hasattr(image, 'register_image_hook'):
        image.register_image_hook(image_to_code_hook)

# Appeler setup lors de l'import
try:
    setup_hooks()
except Exception as e:
    _LOGGER.warning(f"Failed to setup hooks: {e}")
