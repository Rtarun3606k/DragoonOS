#include "log.h"
#include "module.h"
#include "task.h"

int module_load_and_run(multiboot_info_t *mbi) {
  if (mbi == 0 || !(mbi->flags & MULTIBOOT_INFO_MODULES) ||
      mbi->mods_count != 1 || mbi->mods_addr == 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "No valid GRUB module found");
    return 0;
  }

  multiboot_module_t *module =
      (multiboot_module_t *)(mbi->mods_addr + 0xC0000000);

  if (module->mod_start == 0 || module->mod_end <= module->mod_start) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Invalid module memory range");
    return 0;
  }

  unsigned int size = module->mod_end - module->mod_start;
  const void *image = (const void *)(module->mod_start + 0xC0000000);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Loading ELF module (%u bytes) at phys 0x%x", size,
              module->mod_start);

  struct task *init;

  if (!task_create_from_image(image, size, 0, &init)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Failed to create init user task");
    return 0;
  }

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Starting init task (pid %d)",
              init->pid);

  task_start_first(init);

  return 1;
}
