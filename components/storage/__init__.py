import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from ..sd_mmc import CONF_SD_MMC_ID, SdMmc, sd_mmc_ns

DEPENDENCIES = ["sd_mmc"]

# Configuration constants
CONF_PATH_PREFIX = "path_prefix"

# Define the SD MMC Storage class
SdMmcStorage = sd_mmc_ns.class_("SdMmcStorage", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdMmcStorage),
        cv.Required(CONF_SD_MMC_ID): cv.use_id(SdMmc),
        cv.Optional(CONF_PATH_PREFIX, default=""): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Get the SD MMC component reference
    sd_mmc_component = await cg.get_variable(config[CONF_SD_MMC_ID])
    cg.add(var.set_sd_mmc(sd_mmc_component))
    
    # Set path prefix if specified
    if CONF_PATH_PREFIX in config and config[CONF_PATH_PREFIX]:
        cg.add(var.set_path_prefix(config[CONF_PATH_PREFIX]))
    
    return var

