/*
 *  Board-specific setup code for the Lophilo Tabby Board
 *
 *  Based on drivers from Atmel Corporation.
 *
 *  Copyright (C) 2009 Lophilo 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/fb.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/atmel-mci.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/export.h> 


#include <mach/hardware.h>
#include <video/atmel_lcdc.h>
#include <media/soc_camera.h>
#include <media/atmel-isi.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/board.h>
#include <mach/at91sam9_smc.h>
#include <mach/at91_shdwc.h>
#include <mach/system_rev.h>


#include "soc.h"
#include "generic.h"
#include "clock.h"
#include "sam9_smc.h"

static void __init ek_init_early(void)
{
	/* Initialize processor: 12.000 MHz crystal */
	at91_initialize(12000000);

	/* DGBU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

/*
 * USB HS Host port (common to OHCI & EHCI)
 */
static struct at91_usbh_data __initdata ek_usbh_hs_data = {
	.ports		= 2,
	.vbus_pin	= {AT91_PIN_PD2, -EINVAL},
	.vbus_pin_active_low = {1, 1},
	//.overcurrent_pin= {-EINVAL, -EINVAL},
	.overcurrent_pin= {AT91_PIN_PD1, -EINVAL},
};

/*
 * USB HS Device port
 */
static struct usba_platform_data __initdata ek_usba_udc_data = {
	.vbus_pin	= AT91_PIN_PD4,
};

/*
 * MCI (SD/MMC)
 */
static struct mci_platform_data __initdata mci0_data = {
	.slot[0] = {
		.bus_width	= 4,
		.detect_pin	= AT91_PIN_PD27,
		.wp_pin		= -EINVAL,
	},
};

static struct mci_platform_data __initdata mci1_data = {
	.slot[0] = {
		.bus_width	= 4,
		.detect_pin	= AT91_PIN_PD5,
		.wp_pin		= AT91_PIN_PD16,
	},
};

/*
 * MACB Ethernet device
 */
static struct macb_platform_data __initdata ek_macb_data = {
	.phy_irq_pin	= AT91_PIN_PD15,
	.is_rmii	= 1,
};

/*
 *  ISI
 */
static struct isi_platform_data __initdata isi_data = {
	.frate			= ISI_CFG1_FRATE_CAPTURE_ALL,
	/* to use codec and preview path simultaneously */
	.full_mode		= 1,
	.data_width_flags	= ISI_DATAWIDTH_8 | ISI_DATAWIDTH_10,
	/* ISI_MCK is provided by programmable clock or external clock */
	.mck_hz			= 25000000,
};


/*
 * soc-camera OV2640
 */
#if defined(CONFIG_SOC_CAMERA_OV2640) || \
	defined(CONFIG_SOC_CAMERA_OV2640_MODULE)
static unsigned long isi_camera_query_bus_param(struct soc_camera_link *link)
{
	/* ISI board for ek using default 8-bits connection */
	return SOCAM_DATAWIDTH_8;
}

static int i2c_camera_power(struct device *dev, int on)
{
	/* enable or disable the camera */
	pr_debug("%s: %s the camera\n", __func__, on ? "ENABLE" : "DISABLE");
	at91_set_gpio_output(AT91_PIN_PD13, !on);

	if (!on)
		goto out;

	/* If enabled, give a reset impulse */
	at91_set_gpio_output(AT91_PIN_PD12, 0);
	msleep(20);
	at91_set_gpio_output(AT91_PIN_PD12, 1);
	msleep(100);

out:
	return 0;
}

static struct i2c_board_info i2c_camera = {
	I2C_BOARD_INFO("ov2640", 0x30),
};

static struct soc_camera_link iclink_ov2640 = {
	.bus_id			= 0,
	.board_info		= &i2c_camera,
	.i2c_adapter_id		= 0,
	.power			= i2c_camera_power,
	.query_bus_param	= isi_camera_query_bus_param,
};

static struct platform_device isi_ov2640 = {
	.name	= "soc-camera-pdrv",
	.id	= 0,
	.dev	= {
		.platform_data = &iclink_ov2640,
	},
};
#endif


/*
 * LCD Controller
 */
#if defined(CONFIG_FB_ATMEL) || defined(CONFIG_FB_ATMEL_MODULE)
static struct fb_videomode at91_tft_vga_modes[] = {
	{
		.name           = "Innolux",
		.refresh	= 60,
		.xres		= 480,		.yres		= 272,
		.pixclock	= KHZ2PICOS(9000),

		.left_margin	= 1,		.right_margin	= 1,
		.upper_margin	= 40,		.lower_margin	= 1,
		.hsync_len	= 45,		.vsync_len	= 1,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
};

static struct fb_monspecs at91fb_default_monspecs = {
	.manufacturer	= "Inx",
	.monitor        = "AT043TN24",

	.modedb		= at91_tft_vga_modes,
	.modedb_len	= ARRAY_SIZE(at91_tft_vga_modes),
	.hfmin		= 15000,
	.hfmax		= 17640,
	.vfmin		= 57,
	.vfmax		= 67,
};

#define AT91SAM9G45_DEFAULT_LCDCON2 	(ATMEL_LCDC_MEMOR_LITTLE \
					| ATMEL_LCDC_DISTYPE_TFT \
					| ATMEL_LCDC_CLKMOD_ALWAYSACTIVE)

/* Driver datas */
static struct atmel_lcdfb_info __initdata ek_lcdc_data = {
	.lcdcon_is_backlight		= false,
	.default_bpp			= 32,
	.default_dmacon			= ATMEL_LCDC_DMAEN,
	.default_lcdcon2		= AT91SAM9G45_DEFAULT_LCDCON2,
	.default_monspecs		= &at91fb_default_monspecs,
	.guard_time			= 9,
	.lcd_wiring_mode		= ATMEL_LCDC_WIRING_RGB,
};

#else
static struct atmel_lcdfb_info __initdata ek_lcdc_data;
#endif


/*
 * AC97
 */
static struct ac97c_platform_data ek_ac97_data = {
	.reset_pin	= AT91_PIN_PD3,
	//.reset_pin	= AT91_PIN_PB6,
};

/*
 * PWM Leds
 */
static struct gpio_led ek_pwm_led[] = {
#if defined(CONFIG_LEDS_ATMEL_PWM) || defined(CONFIG_LEDS_ATMEL_PWM_MODULE)
	{
		.name			= "led-s0",
		.gpio			= 0,	/* is PWM channel number */
		.active_low		= 1,
		.default_trigger	= "heartbeat",
	},
	{
		.name			= "led-s1",
		.gpio			= 1,	/* is PWM channel number */
		.active_low		= 1,
		.default_trigger	= "mmc0",
	},
	{
		.name			= "led-s2",
		.gpio			= 2,	/* is PWM channel number */
		.active_low		= 1,
		.default_trigger	= "none",
	},
	{
		.name			= "led-s3",
		.gpio			= 3,	/* is PWM channel number */
		.active_low		= 1,
		.default_trigger	= "none",
	},
#endif
};

void __iomem *fpga_cs0_base = NULL;
void __iomem *fpga_cs1_base = NULL;
void __iomem *fpga_cs2_base = NULL;
void __iomem *fpga_cs3_base = NULL;

static struct sam9_smc_config __initdata lophilo_fpga_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 2,
	.ncs_write_setup	= 0,
	.nwe_setup		= 0,

	.ncs_read_pulse		= 0,
	.nrd_pulse		= 6,
	.ncs_write_pulse	= 0,
	.nwe_pulse		= 2,

	.read_cycle		= 10,
	.write_cycle		= 6,

	.mode			= ((0 << 28) + (0 << 24) + (1 << 20) + (8 << 16) + (2 << 12) + (0 << 8) + (2 << 4) + (1 << 1) + (1 << 0)),
	.tdf_cycles		= 8,
};

static void __init start_grid_fpga(void)
{
	//unsigned int i = 0;
	struct clk *pck;

	/* Reset FPGA (Grid) */
	at91_set_gpio_output(AT91_PIN_PA27, 0);
	
	/*Enable PCK0 output */
	at91_set_B_periph(AT91_PIN_PD12, 0);

	pck = clk_get(NULL, "pck0");
	clk_round_rate(pck, 66666666);
	clk_enable(pck);
	clk_put(pck);

	/*Enable PCK1 output */
	at91_set_B_periph(AT91_PIN_PE31, 0);
	
	pck = clk_get(NULL, "pck1");
	clk_round_rate(pck, 66666666);
	clk_enable(pck);
	clk_put(pck);

	at91_set_A_periph(AT91_PIN_PC2, 0);
	at91_set_A_periph(AT91_PIN_PC3, 0);
	at91_set_A_periph(AT91_PIN_PC4, 0);
	at91_set_A_periph(AT91_PIN_PC5, 0);
	at91_set_A_periph(AT91_PIN_PC6, 0);
	at91_set_A_periph(AT91_PIN_PC13, 0);
	at91_set_A_periph(AT91_PIN_PC14, 0);
	at91_set_A_periph(AT91_PIN_PC15, 0);
	at91_set_A_periph(AT91_PIN_PC16, 0);
	at91_set_A_periph(AT91_PIN_PC17, 0);
	at91_set_A_periph(AT91_PIN_PC18, 0);
	at91_set_A_periph(AT91_PIN_PC19, 0);
	at91_set_A_periph(AT91_PIN_PC20, 0);
	at91_set_A_periph(AT91_PIN_PC21, 0);
	at91_set_A_periph(AT91_PIN_PC22, 0);
	at91_set_A_periph(AT91_PIN_PC23, 0);
	at91_set_A_periph(AT91_PIN_PC24, 0);
	at91_set_A_periph(AT91_PIN_PC25, 0);
	at91_set_A_periph(AT91_PIN_PC26, 0);
	at91_set_A_periph(AT91_PIN_PC27, 0);
	at91_set_A_periph(AT91_PIN_PC28, 0);
	at91_set_A_periph(AT91_PIN_PC29, 0);
	at91_set_A_periph(AT91_PIN_PC30, 0);
	at91_set_A_periph(AT91_PIN_PC31, 0);

	sam9_smc_configure(0, 0, &lophilo_fpga_smc_config);
	sam9_smc_configure(0, 1, &lophilo_fpga_smc_config);
	sam9_smc_configure(0, 2, &lophilo_fpga_smc_config);
	sam9_smc_configure(0, 3, &lophilo_fpga_smc_config);

	fpga_cs0_base = ioremap((0x10000000), (0x1000000));
	printk(KERN_INFO "grid: CS0 (0x10000000) ioremap to 0x%08X", (int)fpga_cs0_base);
	fpga_cs1_base = ioremap((0x20000000), (0x1000000));
	printk(KERN_INFO "grid: CS1 (0x20000000) ioremap to 0x%08X", (int)fpga_cs1_base);
	fpga_cs2_base = ioremap((0x30000000), (0x1000000));
	printk(KERN_INFO "grid: CS2 (0x30000000) ioremap to 0x%08X", (int)fpga_cs2_base);
	fpga_cs3_base = ioremap((0x40000000), (0x1000000));
	printk(KERN_INFO "grid: CS3 (0x40000000) ioremap to 0x%08X", (int)fpga_cs3_base);

	msleep(50);
	/* Release the Grid's reset pin */
	at91_set_gpio_output(AT91_PIN_PA27, 1);

	iowrite32(0x581F, (fpga_cs0_base+0x100));
	iowrite32(0x581F, (fpga_cs0_base+0x104));
	iowrite32(0x581F, (fpga_cs0_base+0x108));
	iowrite32(0x581F, (fpga_cs0_base+0x10C));

	printk(KERN_INFO "grid: Lophilo FPGA (grid) initialized, SysID: 0x%08X", (int)(ioread32(fpga_cs0_base)));
}

static struct platform_device *devices[] __initdata = {
};

static void __init ek_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* FPGA */
	start_grid_fpga();
	/* USB HS Host */
	at91_add_device_usbh_ohci(&ek_usbh_hs_data);
	at91_add_device_usbh_ehci(&ek_usbh_hs_data);
	/* USB HS Device */
	at91_add_device_usba(&ek_usba_udc_data);
	/* MMC */
	at91_add_device_mci(0, &mci0_data);
	at91_add_device_mci(1, &mci1_data);
	/* Ethernet */
	at91_add_device_eth(&ek_macb_data);
	/* ISI, using programmable clock as ISI_MCK */
	at91_add_device_isi(&isi_data, true);
	/* LCD Controller */
	at91_add_device_lcdc(&ek_lcdc_data);
	/* AC97 */
	at91_add_device_ac97(&ek_ac97_data);
	/* LEDs */
	at91_pwm_leds(ek_pwm_led, ARRAY_SIZE(ek_pwm_led));
	/* Other platform devices */
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(AT91SAM9M10G45EK, "Lophilo Tabby")
	/* Maintainer: Atmel */
	.timer		= &at91sam926x_timer,
	.map_io		= at91_map_io,
	.init_early	= ek_init_early,
	.init_irq	= at91_init_irq_default,
	.init_machine	= ek_board_init,
MACHINE_END

EXPORT_SYMBOL(fpga_cs0_base);
EXPORT_SYMBOL(fpga_cs1_base);
EXPORT_SYMBOL(fpga_cs2_base);
EXPORT_SYMBOL(fpga_cs3_base);
