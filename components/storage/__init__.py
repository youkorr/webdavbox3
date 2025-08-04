import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM
from esphome.core import CORE

CODEOWNERS = ["@youkorr"]

# Define the base storage namespace
storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.Component)

# Configuration constants
CONF_PATH_PREFIX = "path_prefix"

def storage_schema(storage_class):
    """Create a storage schema for a specific storage implementation"""
    return cv.Schema({
        cv.GenerateID(): cv.declare_id(storage_class),
        cv.Optional(CONF_PATH_PREFIX, default=""): cv.string,
    }).extend(cv.COMPONENT_SCHEMA)

# Base schema for storage components
BASE_STORAGE_SCHEMA = cv.Schema({
    cv.Required(CONF_PLATFORM): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

# This will be extended by platform-specific schemas
CONFIG_SCHEMA = cv.All(BASE_STORAGE_SCHEMA)

async def storage_to_code(config):
    """Base storage setup code"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    if CONF_PATH_PREFIX in config:
        cg.add(var.set_path_prefix(config[CONF_PATH_PREFIX]))
    
    return var

