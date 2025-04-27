#include <stdint.h>
#include "hardware/gpio.h"
#include "tusb.h"

#define JPORT1		0
#define JPORT2		2
#define JPORT3		4
#define JPORT4		6
#define JPORT5		VCC
#define JPORT6		1
#define JPORT7		3
#define JPORT8		5
#define JPORT9		GND

#define GPIO_SOC	JPORT8
#define GPIO_CLK	JPORT6
#define GPIO_DAT	JPORT1
#define GPIO_TRG	JPORT2
#define GPIO_LED	25

#define DIV_MIN		2
#define DIV_MAX		10

#define POS_MIN		151 /* left end of arkanoid2 */
#define POS_MAX		309 /* right end of arkanoid */

static volatile int32_t pos = 236; /* center */

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
	if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_MOUSE) {
		return;
	}

	gpio_put(GPIO_LED, 1);
	tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
	if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_MOUSE) {
		return;
	}

	gpio_put(GPIO_LED, 0);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
	static uint8_t mid = 0;
	static uint8_t div = 6;
	int32_t tmp;

	if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_MOUSE) {
		return;
	}

	if ((mid == 0) && (((hid_mouse_report_t const *)report)->buttons & 0b100)) {
		div -= 2;
		if (div < DIV_MIN) {
			div = DIV_MAX;
		}
	}
	mid = ((hid_mouse_report_t const *)report)->buttons & 0b100;

	gpio_set_dir(GPIO_TRG, ((hid_mouse_report_t const *)report)->buttons & 0b011);

	tmp = pos + ((hid_mouse_report_t const *)report)->x / div;
	tmp = MAX(tmp, POS_MIN);
	tmp = MIN(tmp, POS_MAX);
	pos = tmp;

	tuh_hid_receive_report(dev_addr, instance);
}

static void gpio_callback(uint gpio, uint32_t events)
{
	static int32_t reg = 0;

	if (gpio == GPIO_SOC) {
		reg = pos;
	} else { /* GPIO_CLK */
		reg <<= 1;
	}
	gpio_set_dir(GPIO_DAT, !(reg & (1 << 8)));
}

int main(void)
{
	gpio_init(JPORT1);
	gpio_init(JPORT2);
	gpio_init(JPORT3);
	gpio_init(JPORT4);
	gpio_init(JPORT6);
	gpio_init(JPORT7);
	gpio_init(JPORT8);

	gpio_pull_up(JPORT1);
	gpio_pull_up(JPORT2);
	gpio_pull_up(JPORT3);
	gpio_pull_up(JPORT4);
	gpio_pull_up(JPORT6);
	gpio_pull_up(JPORT7);
	gpio_pull_up(JPORT8);

	gpio_put(GPIO_DAT, 0);
	gpio_put(GPIO_TRG, 0);

	gpio_init(GPIO_LED);
	gpio_set_dir(GPIO_LED, 1);

	gpio_set_irq_enabled_with_callback(GPIO_SOC, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
	gpio_set_irq_enabled_with_callback(GPIO_CLK, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

	tuh_init(0);

	while (1) {
		tuh_task();
	}
}
