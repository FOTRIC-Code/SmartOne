#ifndef NET_APP_LED_LED_MANAGER_H
#define NET_APP_LED_LED_MANAGER_H

typedef enum net_led_state {
	NET_LED_WAIT_PROVISION,
	NET_LED_PROVISIONING,
	NET_LED_ONLINE,
	NET_LED_OFFLINE,
	NET_LED_FAILED
} net_led_state_t;

/* Keep old name as alias for backward compatibility */
#define NET_LED_AUTHENTICATED NET_LED_PROVISIONING

int net_led_init(void);
void net_led_deinit(void);
int net_led_set_state(net_led_state_t state);

#endif
