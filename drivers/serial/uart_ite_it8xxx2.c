/*
 * Copyright (c) 2021 ITE Corporation. All Rights Reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ite_it8xxx2_uart

#include <device.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <kernel.h>
#include <pm/device.h>
#include <soc.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(uart_ite_it8xxx2, LOG_LEVEL_ERR);

struct uart_it8xxx2_config {
	uint8_t port;
	/* GPIO cells */
	struct gpio_dt_spec gpio_wui;
};

enum uart_port_num {
	UART1 = 1,
	UART2,
};

#ifdef CONFIG_PM_DEVICE
__weak void uart1_wui_isr_late(void) { }
__weak void uart2_wui_isr_late(void) { }

void uart1_wui_isr(const struct device *gpio, struct gpio_callback *cb,
		   uint32_t pins)
{
	/* Disable interrupts on UART1 RX pin to avoid repeated interrupts. */
	(void)gpio_pin_interrupt_configure(gpio, (find_msb_set(pins) - 1),
					   GPIO_INT_DISABLE);

	if (uart1_wui_isr_late) {
		uart1_wui_isr_late();
	}
}

void uart2_wui_isr(const struct device *gpio, struct gpio_callback *cb,
		   uint32_t pins)
{
	/* Disable interrupts on UART2 RX pin to avoid repeated interrupts. */
	(void)gpio_pin_interrupt_configure(gpio, (find_msb_set(pins) - 1),
					   GPIO_INT_DISABLE);

	if (uart2_wui_isr_late) {
		uart2_wui_isr_late();
	}
}

static inline int uart_it8xxx2_pm_action(const struct device *dev,
					 enum pm_device_action action)
{
	const struct uart_it8xxx2_config *const config = dev->config;
	int ret = 0;

	switch (action) {
	/* Next device power state is in active. */
	case PM_DEVICE_ACTION_RESUME:
		/* Nothing to do. */
		break;
	/* Next device power state is deep doze mode */
	case PM_DEVICE_ACTION_SUSPEND:
		/* Enable UART WUI */
		ret = gpio_pin_interrupt_configure_dt(&config->gpio_wui,
						      GPIO_INT_TRIG_LOW);
		if (ret < 0) {
			LOG_ERR("Failed to configure UART%d WUI (ret %d)",
				config->port, ret);
			return ret;
		}

		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

static int uart_it8xxx2_init(const struct device *dev)
{
#ifdef CONFIG_PM_DEVICE
	const struct uart_it8xxx2_config *const config = dev->config;
	int ret = 0;

	/*
	 * When the system enters deep doze, all clocks are gated only the
	 * 32.768k clock is active. We need to wakeup EC by configuring
	 * UART Rx interrupt as a wakeup source. When the interrupt of UART
	 * Rx falling, EC will be woken.
	 */
	if (config->port == UART1) {
		static struct gpio_callback uart1_wui_cb;

		gpio_init_callback(&uart1_wui_cb, uart1_wui_isr,
				   BIT(config->gpio_wui.pin));

		ret = gpio_add_callback(config->gpio_wui.port, &uart1_wui_cb);
	} else if (config->port == UART2) {
		static struct gpio_callback uart2_wui_cb;

		gpio_init_callback(&uart2_wui_cb, uart2_wui_isr,
				   BIT(config->gpio_wui.pin));

		ret = gpio_add_callback(config->gpio_wui.port, &uart2_wui_cb);
	}

	if (ret < 0) {
		LOG_ERR("Failed to add UART%d callback (err %d)",
			config->port, ret);
		return ret;
	}
#endif /* CONFIG_PM_DEVICE */

	return 0;
}

#define UART_ITE_IT8XXX2_INIT(inst)                                            \
	static const struct uart_it8xxx2_config uart_it8xxx2_cfg_##inst = {    \
		.port = DT_INST_PROP(inst, port_num),                          \
		.gpio_wui = GPIO_DT_SPEC_INST_GET(inst, gpios),                \
	};                                                                     \
	DEVICE_DT_INST_DEFINE(inst, &uart_it8xxx2_init,                        \
			      uart_it8xxx2_pm_action,                          \
			      NULL, &uart_it8xxx2_cfg_##inst,                  \
			      PRE_KERNEL_1,                                    \
			      CONFIG_SERIAL_INIT_PRIORITY,                     \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(UART_ITE_IT8XXX2_INIT)
