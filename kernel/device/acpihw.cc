#include "acpihw.h"
#include "irq.h"

class acpi_pm_tmr_t {
public:
    static bool init()
private:
    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void irq_handler();
};
