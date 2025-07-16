#ifndef ETHERNET_CONFIG_H
#define ETHERNET_CONFIG_H

#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#endif

#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR  0
#endif

#ifndef ETH_PHY_MDC
#define ETH_PHY_MDC   23
#endif

#ifndef ETH_PHY_MDIO
#define ETH_PHY_MDIO  18
#endif

#ifndef ETH_PHY_POWER
#define ETH_PHY_POWER -1
#endif

#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE  ETH_CLOCK_GPIO17_OUT
#endif

#endif // ETHERNET_CONFIG_H
