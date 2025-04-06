#include <kernel/multiboot.h>
#include <kernel/printf.h>

void mod_install(void *mboot_info, uint32_t mboot_magic) {
    struct multiboot_tag *tag = mboot2_find_tag(mboot_info, MULTIBOOT_TAG_TYPE_MODULE);
    if (!tag) {
        printf("No modules found\n");
        return;
    }

    struct multiboot_tag_module *module = (struct multiboot_tag_module *)tag;
    char *module_name = (char *)module->string;
    void *module_start = (void *)(uint64_t)module->mod_start;
    void *module_end = (void *)(uint64_t)module->mod_end;

    printf("Module: %s (%p - %p)\n", module_name, module_start, module_end);

}