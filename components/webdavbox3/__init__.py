import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_USERNAME, CONF_PASSWORD, CONF_PORT

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["sd_mmc_card"]
MULTI_CONF = False  # Si tu prévois un seul composant, sinon mets True si c'est une liste

webdavbox_ns = cg.esphome_ns.namespace("webdavbox3")
WebDAVBox3 = webdavbox_ns.class_("WebDAVBox3", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(WebDAVBox3),
    cv.Optional("root_path", default="/sdcard/"): cv.string,
    cv.Optional("url_prefix", default="/"): cv.string,
    cv.Optional(CONF_PORT, default=81): cv.port,
    cv.Optional(CONF_USERNAME, default=""): cv.string,
    cv.Optional(CONF_PASSWORD, default=""): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_root_path(config["root_path"]))
    cg.add(var.set_url_prefix(config["url_prefix"]))
    cg.add(var.set_port(config[CONF_PORT]))
    
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))
    
    return var












