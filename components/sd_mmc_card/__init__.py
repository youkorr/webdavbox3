import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import (
    CONF_ID,
    CONF_DATA,
    CONF_PATH,
    CONF_CLK_PIN,
    CONF_CMD_PIN,
    CONF_DATA0_PIN,
    CONF_DATA1_PIN,
    CONF_DATA2_PIN,
    CONF_DATA3_PIN,
    CONF_MODE_1BIT,
    CONF_POWER_CTRL_PIN,
)

# Namespace
sd_mmc_card_component_ns = cg.esphome_ns.namespace("sd_mmc_card")
SdMmc = sd_mmc_card_component_ns.class_("SdMmc", cg.Component)

# Actions
SdMmcWriteFileAction = sd_mmc_card_component_ns.class_("SdMmcWriteFileAction", automation.Action)
SdMmcAppendFileAction = sd_mmc_card_component_ns.class_("SdMmcAppendFileAction", automation.Action)
SdMmcCreateDirectoryAction = sd_mmc_card_component_ns.class_("SdMmcCreateDirectoryAction", automation.Action)
SdMmcRemoveDirectoryAction = sd_mmc_card_component_ns.class_("SdMmcRemoveDirectoryAction", automation.Action)
SdMmcDeleteFileAction = sd_mmc_card_component_ns.class_("SdMmcDeleteFileAction", automation.Action)

# Valide les données brutes
def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string wrapped in quotes or a list of bytes")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdMmc),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): cv.int_,  # Remplacé car l'ancien attribut a été supprimé
        cv.Optional(CONF_DATA1_PIN): cv.int_,
        cv.Optional(CONF_DATA2_PIN): cv.int_,
        cv.Optional(CONF_DATA3_PIN): cv.int_,
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
        cv.Optional(CONF_POWER_CTRL_PIN): pins.gpio_pin_schema({
            "output": True,
            "pullup": False,
            "pulldown": False,
        }),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    if not config[CONF_MODE_1BIT]:
        if CONF_DATA1_PIN in config:
            cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        if CONF_DATA2_PIN in config:
            cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        if CONF_DATA3_PIN in config:
            cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    if CONF_POWER_CTRL_PIN in config:
        power_ctrl = await cg.gpio_pin_expression(config[CONF_POWER_CTRL_PIN])
        cg.add(var.set_power_ctrl_pin(power_ctrl))

# Schemas d'actions
SD_MMC_PATH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdMmc),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
    }
)

SD_MMC_WRITE_FILE_ACTION_SCHEMA = SD_MMC_PATH_ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    }
)

@automation.register_action(
    "sd_mmc_card.write_file", SdMmcWriteFileAction, SD_MMC_WRITE_FILE_ACTION_SCHEMA
)
async def sd_mmc_write_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path))
    cg.add(var.set_data(data))
    return var

@automation.register_action(
    "sd_mmc_card.append_file", SdMmcAppendFileAction, SD_MMC_WRITE_FILE_ACTION_SCHEMA
)
async def sd_mmc_append_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path))
    cg.add(var.set_data(data))
    return var

@automation.register_action(
    "sd_mmc_card.create_directory", SdMmcCreateDirectoryAction, SD_MMC_PATH_ACTION_SCHEMA
)
async def sd_mmc_create_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path))
    return var

@automation.register_action(
    "sd_mmc_card.remove_directory", SdMmcRemoveDirectoryAction, SD_MMC_PATH_ACTION_SCHEMA
)
async def sd_mmc_remove_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path))
    return var

@automation.register_action(
    "sd_mmc_card.delete_file", SdMmcDeleteFileAction, SD_MMC_PATH_ACTION_SCHEMA
)
async def sd_mmc_delete_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path))
    return var


