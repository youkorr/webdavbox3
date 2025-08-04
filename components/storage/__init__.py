import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from ..sd_mmc import CONF_SD_MMC_ID, SdMmc, sd_mmc_ns

DEPENDENCIES = ["sd_mmc"]

# Define the storage schema directly instead of importing
CONF_STORAGE_ID = "storage_id"

sd_mmc_storage = sd_mmc_ns.class_("SD_MMC_Storage", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(sd_mmc_storage),
        cv.Required(CONF_SD_MMC_ID): cv.use_id(SdMmc),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    sd_mmc_component = await cg.get_variable(config[CONF_SD_MMC_ID])
    cg.add(var.set_sd_mmc(sd_mmc_component))

