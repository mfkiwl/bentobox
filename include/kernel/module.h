#pragma once

struct Module {
    const char *name;
    int (*init)();
    // TODO: add fini
};