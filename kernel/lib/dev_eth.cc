#include "dev_eth.h"
#include "printk.h"

#define ETH_DEV_DEBUG   1
#if ETH_DEV_DEBUG
#define ETH_DEV_TRACE(...) printdbg("eth_dev: " __VA_ARGS__)
#else
#define ETH_DEV_TRACE(...) ((void)0)
#endif

#define MAX_ETH_DEVICES 16
static eth_dev_base_t *eth_devices[MAX_ETH_DEVICES];
static unsigned eth_device_count;

void register_eth_dev_device(const char *name, eth_factory_t *dev)
{
    (void)name;
    ETH_DEV_TRACE("Registered %s device\n", name);

    eth_dev_base_t **devices = 0;
    unsigned added_devices = dev->detect(&devices);

    for (unsigned i = 0; i < added_devices; ++i)
        eth_devices[eth_device_count++] = devices[i];
}
