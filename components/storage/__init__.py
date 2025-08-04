import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Import du module officiel sans créer de boucle
from esphome.components import storage as base_storage

# Import des constantes et classes du composant sd_mmc
from ..sd_mmc import CONF_SD_MMC_ID, SdMmc, sd_mmc_ns

# Dépendances explicites
DEPENDENCIES = ["sd_mmc", "storage"]

# Déclaration de classe pour ce composant personnalisé
sd_mmc_storage_ns = cg.esphome_ns.namespace("sd_storage")
SdMmcStorage = sd_mmc_storage_ns.class_("SdMmcStorage", cg.Component)

# Schéma de configuration
CONFIG_SCHEMA = base_storage.storage_schema(SdMmcStorage).extend(
    {
        cv.GenerateID(): cv.declare_id(SdMmcStorage),
        cv.Required(CONF_SD_MMC_ID): cv.use_id(SdMmc),
    }
)

# Code de génération
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sd_mmc_component = await cg.get_variable(config[CONF_SD_MMC_ID])
    cg.add(var.set_sd_mmc(sd_mmc_component))

    await base_storage.storage_to_code(config)

