import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObjClass

storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.EntityBase)
StorageClient = storage_ns.class_("StorageClient", cg.EntityBase)
StorageClientStatic = storage_ns.namespace("StorageClient")

IS_PLATFORM_COMPONENT = True
CONF_PREFIX = "path_prefix"

STORAGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Storage), 
        cv.Required(CONF_PREFIX): cv.string
    }
)

def storage_schema(
    class_: MockObjClass = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}
    if class_ is not cv.UNDEFINED:
        # Not cv.optional
        schema[cv.GenerateID()] = cv.declare_id(class_)
    return STORAGE_SCHEMA.extend(schema)

async def storage_to_code(config):
    storage = await cg.get_variable(config[CONF_ID])
    prefix = config[CONF_PREFIX]
    cg.add(StorageClientStatic.add_storage(storage, prefix))
