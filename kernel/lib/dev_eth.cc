#include "dev_eth.h"
#include "printk.h"
#include "export.h"

#define ETH_DEV_DEBUG   1
#if ETH_DEV_DEBUG
#define ETH_DEV_TRACE(...) printdbg("eth_dev: " __VA_ARGS__)
#else
#define ETH_DEV_TRACE(...) ((void)0)
#endif

#define MAX_ETH_FACTORIES 16
static eth_dev_factory_t *eth_factories[MAX_ETH_FACTORIES];
static unsigned eth_factory_count;

#define MAX_ETH_DEVICES 16
static eth_dev_base_t *eth_devices[MAX_ETH_DEVICES];
static unsigned eth_device_count;

void register_eth_dev_factory(
        char const *name, eth_dev_factory_t *factory)
{
    (void)name;
    if (eth_factory_count < MAX_ETH_FACTORIES) {
        eth_factories[eth_factory_count++] = factory;
        ETH_DEV_TRACE("Registered %s device\n", name);
    } else {
        ETH_DEV_TRACE("Too many ethernet device factories,"
                      " dropped %s!\n", name);
    }
}

static void invoke_eth_dev_factories(void*)
{
    for (unsigned df = 0; df < eth_factory_count; ++df) {
        eth_dev_base_t **devices = nullptr;
        unsigned added_devices = eth_factories[df]->detect(&devices);
        for (unsigned i = 0; i < added_devices; ++i)
            eth_devices[eth_device_count++] = devices[i];
    }
}

REGISTER_CALLOUT(invoke_eth_dev_factories, nullptr,
                 callout_type_t::nic, "000");


