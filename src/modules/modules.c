#include "module.h"
#include "log.h"

int module_load_and_run(multiboot_info_t *mbi) {
    if (mbi == 0) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "Missing Multiboot information");
        return 0;
    }

    if (!(mbi->flags & MULTIBOOT_INFO_MODULES)) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "GRUB provided no module information");
        return 0;
    }

    if (mbi->mods_count != 1 || mbi->mods_addr == 0) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "Expected one valid module descriptor");
        return 0;
    }

    multiboot_module_t *module =
        (multiboot_module_t *)mbi->mods_addr;

    if (module->mod_start == 0 ||
        module->mod_end <= module->mod_start) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "Invalid module memory range");
        return 0;
    }

    unsigned int size = module->mod_end - module->mod_start;

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Module start: 0x%x", module->mod_start);
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Module end: 0x%x", module->mod_end);
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Module size: %u bytes", size);
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Executing module at 0x%x", module->mod_start);

    typedef unsigned int (*module_entry_t)(void);

    module_entry_t entry = (module_entry_t)module->mod_start;
    unsigned int result = entry();

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Module returned: 0x%x", result);

    return 1;
}
