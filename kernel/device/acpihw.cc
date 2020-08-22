#include "acpihw.h"
#include "irq.h"
#include "callout.h"

class acpi_hw_t {
public:
    static void init(void *);

private:
    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void irq_handler();

    static acpi_hw_t *instance;
};

acpi_hw_t *acpi_hw_t::instance;

REGISTER_CALLOUT(&acpi_hw_t::init, nullptr,
                 callout_type_t::acpi_ready, "000");

void acpi_hw_t::init(void *)
{

}

isr_context_t *acpi_hw_t::irq_handler(int irq, isr_context_t *ctx)
{
    instance->irq_handler();
    return ctx;
}

void acpi_hw_t::irq_handler()
{

}
