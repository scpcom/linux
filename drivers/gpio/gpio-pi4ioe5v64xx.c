// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for the Pericom PI4IOE5V6416 GPIO Expander.
 * https://www.diodes.com/assets/Datasheets/PI4IOE5V6416.pdf
 *
 * Copyright (C) 2020 Tesla Motors, Inc.
 */
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
// #include <linux/pinctrl/pinctrl.h>
// #include <linux/pinctrl/pinconf.h>
// #include <linux/pinctrl/pinconf-generic.h>
// Registers
#define PI4IO16_INPUT_PORT0	0x00
#define PI4IO16_INPUT_PORT1	0x01
#define PI4IO16_OUTPUT_PORT0	0x02
#define PI4IO16_OUTPUT_PORT1	0x03
#define PI4IO16_POLARITY0	0x04
#define PI4IO16_POLARITY1	0x05
#define PI4IO16_CONFIG_PORT0	0x06
#define PI4IO16_CONFIG_PORT1	0x07
#define PI4IO16_OUTPUT_DRIVE0_0 0x40
#define PI4IO16_OUTPUT_DRIVE0_1 0x41
#define PI4IO16_OUTPUT_DRIVE1_0 0x42
#define PI4IO16_OUTPUT_DRIVE1_1 0x43
#define PI4IO16_INPUT_LATCH0	0x44
#define PI4IO16_INPUT_LATCH1	0x45
#define PI4IO16_PULLUP_ENB0	0x46
#define PI4IO16_PULLUP_ENB1	0x47
#define PI4IO16_PULLUP_SEL0	0x48
#define PI4IO16_PULLUP_SEL1	0x49
#define PI4IO16_INTMASK_REG0	0x4A
#define PI4IO16_INTMASK_REG1	0x4B
#define PI4IO16_INT_STATUS0	0x4C
#define PI4IO16_INT_STATUS1	0x4D
#define PI4IO16_OUTPUT_CONFIG	0x4F

#define PI4IO16_N_GPIO 16




#define PI4IO08_CHIP_ID_VAL 0xA0
#define PI4IO08_CHIP_ID_MASK 0xFC
#define PI4IO08_CHIP_ID 	0x1
#define PI4IO08_INPUT_STATUS	0x0F
#define PI4IO08_OUTPUT	0x05
#define PI4IO08_IO_DIRECTION	0x03
#define PI4IO08_OUTPUT_HI_IMPEDANCE 0x07
#define PI4IO08_INPUT_DEFAULT	0x09
#define PI4IO08_PULLUP_ENB	0x0B
#define PI4IO08_PULLUP_SEL	0x0D
#define PI4IO08_INTMASK_REG	0x11
#define PI4IO08_INTERRUPT_STATUS	0x13

#define PI4IO08_DIRECTION_TO_GPIOD(x) ((x) ? GPIOF_DIR_OUT : GPIOF_DIR_IN)
#define GPIOD_DIRECTION_TO_PI4IO08(x) ((x) == GPIOF_DIR_OUT ? 1 : 0)

#define GPIO_OUT_LOW 0
#define GPIO_OUT_HIGH 1

#define PI4IO08_N_GPIO 8


/*
 * The datasheet calls for a minimum of 30s pulse, and 600ns recovery time.
 * to safe, round to 1ms.
 */
#define PI4IO16_RESET_DELAY_MS 1

/*
 * Robustify the link since gpio is critical. Retry multiple times on failures.
 */
#define I2C_READ_RETRIES 5

/*
 * 0<->GPIOF_DIR_OUT
 * 1<->GPIOF_DIR_IN
 */
#define PI4IO16_DIRECTION_TO_GPIOD(x) ((x) ? GPIOF_DIR_IN : GPIOF_DIR_OUT)
#define GPIOD_DIRECTION_TO_PI4IO16(x) ((x) == GPIOF_DIR_OUT ? 0 : 1)

//#define PI4IO16_IRQ /* TODO: Enable irqchip */


struct pi4ioxx_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct gpio_chip gpio;
	struct gpio_desc *reset_gpio;
//
//	struct pinctrl_dev	*pctldev;
//	struct pinctrl_desc	pinctrl_desc;
#ifdef CONFIG_GPIO_PI4IOE5V64XX_IRQ
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	uint16_t irq_mask;
#endif
};


static bool pi4ioxx_readable_reg(struct device *dev, unsigned int reg)
{
	struct pi4ioxx_priv *pi4io = (struct pi4ioxx_priv *)dev->driver_data;
	if(pi4io->gpio.ngpio == 16)
	{
		// registers are readable.
		switch (reg) {
		case PI4IO16_INPUT_PORT0 ... PI4IO16_CONFIG_PORT1:
		case PI4IO16_OUTPUT_DRIVE0_0 ... PI4IO16_OUTPUT_CONFIG:
			return true;
		default:
			return false;
		}
		return false;
	}
	else
	{
		// registers are readable.
		// All readable registers are odd-numbered.
		return (reg % 2) == 1;
	}

}

static bool pi4ioxx_writeable_reg(struct device *dev, unsigned int reg)
{
	struct pi4ioxx_priv *pi4io = (struct pi4ioxx_priv *)dev->driver_data;
	if(pi4io->gpio.ngpio == 16)
	{
		switch (reg) {
		case PI4IO16_OUTPUT_PORT0 ... PI4IO16_CONFIG_PORT1:
		case PI4IO16_OUTPUT_DRIVE0_0 ... PI4IO16_INTMASK_REG1:
		case PI4IO16_OUTPUT_CONFIG:
			return true;
		default:
			return false;
		}
		return false;
	}
	else
	{
		// All odd-numbered registers are writable except for 0xF.
		if ((reg % 2) == 1) {
			if (reg != PI4IO08_INPUT_STATUS) {
				return true;
			}
		}
		return false;
	}

}

static bool pi4ioxx_volatile_reg(struct device *dev, unsigned int reg)
{
	struct pi4ioxx_priv *pi4io = (struct pi4ioxx_priv *)dev->driver_data;
	if(pi4io->gpio.ngpio == 16)
	{
		switch (reg) {
		case PI4IO16_INPUT_PORT0:
		case PI4IO16_INPUT_PORT1:
		case PI4IO16_INPUT_LATCH0:
		case PI4IO16_INPUT_LATCH1:
		case PI4IO16_INT_STATUS0:
		case PI4IO16_INT_STATUS1:
			return true;
		default:
			return false;
		}
		return false;
	}
	else
	{
		if (reg == PI4IO08_INPUT_STATUS || reg == PI4IO08_INTERRUPT_STATUS) {
			return true;
		}
		return false;
	}

}

// static int pi4io_pinctrl_get_groups_count(struct pinctrl_dev *pctldev)
// {
// 	return 0;
// }
// 
// static const char *pi4io_pinctrl_get_group_name(struct pinctrl_dev *pctldev,
// 						unsigned int group)
// {
// 	return NULL;
// }
// 
// static int pi4io_pinctrl_get_group_pins(struct pinctrl_dev *pctldev,
// 					unsigned int group,
// 					const unsigned int **pins,
// 					unsigned int *num_pins)
// {
// 	return -ENOTSUPP;
// }
// 
// static int pi4io_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin,
// 			      unsigned long *config)
// {
// 	struct pi4ioxx_priv *pi4io = (struct pi4ioxx_priv *)pinctrl_dev_get_drvdata(pctldev);
// 	struct i2c_client *client = pi4io->i2c;
// 	struct device *dev = &client->dev;
// 	enum pin_config_param param = pinconf_to_config_param(*config);
// 	unsigned int data, status;
// 	int ret;
// 
// 	switch (param) {
// 	case PIN_CONFIG_BIAS_PULL_UP:
// 		ret = regmap_read(pi4io->regmap, PI4IO08_PULLUP_ENB, &data);
// 		if (ret < 0)
// 			return ret;
// 		status = (data & BIT(pin)) ? 1 : 0;
// 		break;
// 	case PIN_CONFIG_BIAS_PULL_DOWN:
// 		ret = regmap_read(pi4io->regmap, PI4IO08_PULLUP_ENB, &data);
// 		if (ret < 0)
// 			return ret;
// 		status = ((~data) & BIT(pin)) ? 1 : 0;
// 		break;
// 	default:
// 		dev_err(dev, "Invalid config param %04x\n", param);
// 		return -ENOTSUPP;
// 	}
// 
// 	*config = 0;
// 
// 	return status ? 0 : -EINVAL;
// }
// 
// static int pi4io_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
// 			      unsigned long *configs, unsigned int num_configs)
// {
// 	struct pi4ioxx_priv *pi4io = (struct pi4ioxx_priv *)pinctrl_dev_get_drvdata(pctldev);
// 	struct i2c_client *client = pi4io->i2c;
// 	struct device *dev = &client->dev;
// 	enum pin_config_param param;
// 	u32 arg;
// 	int ret = 0;
// 	int i;
// 
// 	for (i = 0; i < num_configs; i++) {
// 		param = pinconf_to_config_param(configs[i]);
// 		arg = pinconf_to_config_argument(configs[i]);
// 		u16 mask = BIT(pin);
// 
// 		switch (param) {
// 		case PIN_CONFIG_BIAS_DISABLE:
// 		{
// 			ret = regmap_update_bits(pi4io->regmap, PI4IO08_PULLUP_ENB, mask, 0x0000);
// 		}
// 			break;
// 		case PIN_CONFIG_BIAS_PULL_UP:
// 		{
// 			ret = regmap_update_bits(pi4io->regmap, PI4IO08_PULLUP_ENB, mask, 0xffff);
// 			ret = regmap_update_bits(pi4io->regmap, PI4IO08_PULLUP_SEL, mask, 0xffff);
// 		}
// 			break;
// 		case PIN_CONFIG_BIAS_PULL_DOWN:
// 		{
// 			ret = regmap_update_bits(pi4io->regmap, PI4IO08_PULLUP_ENB, mask, 0xffff);
// 			ret = regmap_update_bits(pi4io->regmap, PI4IO08_PULLUP_SEL, mask, 0x0000);
// 		}
// 			break;
// 		default:
// 			dev_err(dev, "Invalid config param %04x\n", param);
// 			return -ENOTSUPP;
// 		}
// 	}
// 
// 	return ret;
// }

static const struct regmap_config pi4io16_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PI4IO16_OUTPUT_CONFIG,
	.writeable_reg = pi4ioxx_writeable_reg,
	.readable_reg = pi4ioxx_readable_reg,
	.volatile_reg = pi4ioxx_volatile_reg,
};

static const struct regmap_config pi4io08_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PI4IO08_INTERRUPT_STATUS,
	.writeable_reg = pi4ioxx_writeable_reg,
	.readable_reg = pi4ioxx_readable_reg,
	.volatile_reg = pi4ioxx_volatile_reg,
};

// static const struct pinctrl_ops pi4io_pinctrl_ops = {
// 	.get_groups_count = pi4io_pinctrl_get_groups_count,
// 	.get_group_name = pi4io_pinctrl_get_group_name,
// 	.get_group_pins = pi4io_pinctrl_get_group_pins,
// #ifdef CONFIG_OF
// 	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
// 	.dt_free_map = pinconf_generic_dt_free_map,
// #endif
// };
// 
// static const struct pinconf_ops pi4io_pinconf_ops = {
// 	.pin_config_get = pi4io_pinconf_get,
// 	.pin_config_set = pi4io_pinconf_set,
// 	.is_generic = true,
// };
// 
// static const struct pinctrl_pin_desc pi4iox08_pins[] = {
// 	PINCTRL_PIN(0, "gpio0"),
// 	PINCTRL_PIN(1, "gpio1"),
// 	PINCTRL_PIN(2, "gpio2"),
// 	PINCTRL_PIN(3, "gpio3"),
// 	PINCTRL_PIN(4, "gpio4"),
// 	PINCTRL_PIN(5, "gpio5"),
// 	PINCTRL_PIN(6, "gpio6"),
// 	PINCTRL_PIN(7, "gpio7"),
// };
// 
// static const struct pinctrl_pin_desc pi4iox16_pins[] = {
// 	PINCTRL_PIN(0, "gpio0"),
// 	PINCTRL_PIN(1, "gpio1"),
// 	PINCTRL_PIN(2, "gpio2"),
// 	PINCTRL_PIN(3, "gpio3"),
// 	PINCTRL_PIN(4, "gpio4"),
// 	PINCTRL_PIN(5, "gpio5"),
// 	PINCTRL_PIN(6, "gpio6"),
// 	PINCTRL_PIN(7, "gpio7"),
// 	PINCTRL_PIN(8, "gpio8"),
// 	PINCTRL_PIN(9, "gpio9"),
// 	PINCTRL_PIN(10, "gpio10"),
// 	PINCTRL_PIN(11, "gpio11"),
// 	PINCTRL_PIN(12, "gpio12"),
// 	PINCTRL_PIN(13, "gpio13"),
// 	PINCTRL_PIN(14, "gpio14"),
// 	PINCTRL_PIN(15, "gpio15"),
// };


static int pi4ioxx_byte_reg_read(void *context, unsigned int reg,
				      unsigned int *val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret;
	int retries = I2C_READ_RETRIES;

	if (reg > 0xff)
		return -EINVAL;

	do {
		ret = i2c_smbus_read_byte_data(i2c, reg);
	} while (ret < 0 && retries-- > 0);

	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}

static int pi4ioxx_byte_reg_write(void *context, unsigned int reg,
				unsigned int val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret;
	int retries = I2C_READ_RETRIES;

	if (val > 0xff || reg > 0xff)
		return -EINVAL;

	do {
		ret = i2c_smbus_write_byte_data(i2c, reg, val);
	} while (ret != 0 && retries-- > 0);

	return ret;
}

static struct regmap_bus pi4io_regmap_bus = {
	.reg_write = pi4ioxx_byte_reg_write,
	.reg_read = pi4ioxx_byte_reg_read,
};

static struct regmap* pi4ioxx_setup_regmap(struct i2c_client *i2c, int pi4io_dev_id)
{
	if (!i2c_check_functionality(i2c->adapter,
				    I2C_FUNC_SMBUS_BYTE_DATA)) {
		return ERR_PTR(-ENOTSUPP);
	}
	if(pi4io_dev_id){
		return devm_regmap_init(&i2c->dev, &pi4io_regmap_bus, &i2c->dev,
					&pi4io08_regmap_config);
	}else{
		return devm_regmap_init(&i2c->dev, &pi4io_regmap_bus, &i2c->dev,
					&pi4io16_regmap_config);
	}
}

static int pi4ioxx_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	int ret, io_dir, direction;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;
	if(pi4io->gpio.ngpio == 16)
	{
		/*
		* The Configuration registers configure the direction of the I/O pins.
		* If a bit in these regsiters is set to 1 the corresponding port pin is
		* enabled as a high-impedence input if a bit in these registers is cleared
		* to 0, the corresponding port pin is enabled as an output.
		*/
		/* GPIO pins 0-7 -> PORT0, pins 8->15 PORT1 */
		if (offset < 8) {
			ret = regmap_read(pi4io->regmap, PI4IO16_CONFIG_PORT0, &io_dir);
		} else {
			ret = regmap_read(pi4io->regmap, PI4IO16_CONFIG_PORT1, &io_dir);
			offset = offset % 8;
		}

		if (ret) {
			dev_err(dev, "Failed to read I/O direction: %d", ret);
			return ret;
		}

		direction = PI4IO16_DIRECTION_TO_GPIOD((io_dir >> offset) & 1);
		dev_dbg(dev, "get_direction : offset=%u, direction=%s, reg=0x%X",
			offset, (direction == GPIOF_DIR_IN) ? "input" : "output",
			io_dir);

		return direction;
	}
	else
	{
		ret = regmap_read(pi4io->regmap, PI4IO08_IO_DIRECTION, &io_dir);
		if (ret) {
			dev_err(dev, "Failed to read I/O direction: %d", ret);
			return ret;
		}

		return PI4IO08_DIRECTION_TO_GPIOD((io_dir >> offset) & 1);
	}
}

static int pi4ioxx_gpio_set_direction(struct gpio_chip *chip, unsigned offset,
				      int direction)
{
	int ret, reg;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	dev_dbg(dev, "set_direction : offset=%u, direction=%s", offset,
		(direction == GPIOF_DIR_IN) ? "input" : "output");

	if(pi4io->gpio.ngpio == 16){
		if (offset < 8) {
			reg = PI4IO16_CONFIG_PORT0;
		} else {
			reg = PI4IO16_CONFIG_PORT1;
			offset = offset % 8;
		}
		ret = regmap_update_bits(pi4io->regmap, reg, 1 << offset,
					GPIOD_DIRECTION_TO_PI4IO16(direction) <<
					offset);

		if (ret) {
			dev_err(dev, "Failed to set direction: %d", ret);
			return ret;
		}

		return ret;
	}else{
		ret = regmap_update_bits(pi4io->regmap, PI4IO08_IO_DIRECTION, 1 << offset,
			GPIOD_DIRECTION_TO_PI4IO08(direction) << offset);
		if (ret) {
			dev_err(dev, "Failed to set direction: %d", ret);
			return ret;
		}

		// We desire the hi-impedance state to track the output state.
		ret = regmap_update_bits(pi4io->regmap, PI4IO08_OUTPUT_HI_IMPEDANCE,
			1 << offset, direction << offset);

		return ret;
	}
}

static int pi4ioxx_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	int ret, out, reg;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;
	if(pi4io->gpio.ngpio == 16){
		if (offset < 8) {
			reg = PI4IO16_INPUT_PORT0;
		} else {
			reg = PI4IO16_INPUT_PORT1;
			offset = offset % 8;
		}
	}else{
		reg = PI4IO08_OUTPUT;
	}

	ret = regmap_read(pi4io->regmap, reg, &out);
	if (ret) {
		dev_err(dev, "Failed to read output: %d", ret);
		return ret;
	}

	dev_dbg(dev, "gpio_get : offset=%u, val=%s reg=0x%X", offset,
		out & (1 << (offset % 8)) ? "1" : "0", out);

	if (out & (1 << (offset % 8))) {
		return 1;
	}
	return 0;
}

static void pi4ioxx_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	int ret, reg;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	dev_dbg(dev, "gpio_set : offset=%u, val=%s", offset, value ? "1" : "0");

	if(pi4io->gpio.ngpio == 16){
		if (offset < 8) {
			reg = PI4IO16_OUTPUT_PORT0;
		} else {
			reg = PI4IO16_OUTPUT_PORT1;
			offset = offset % 8;
		}
	}else{
		reg = PI4IO08_OUTPUT;
	}

	ret = regmap_update_bits(pi4io->regmap, reg, 1 << offset,
				value << offset);
	if (ret) {
		dev_err(dev, "Failed to write output: %d", ret);
	}
}

static int pi4ioxx_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{	
	return pi4ioxx_gpio_set_direction(chip, offset, GPIOF_DIR_IN);
}

static int pi4ioxx_gpio_direction_output(struct gpio_chip *chip,
					 unsigned offset, int value)
{
	int ret;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	ret = pi4ioxx_gpio_set_direction(chip, offset, GPIOF_DIR_OUT);
	if (ret) {
		dev_err(dev, "Failed to set direction: %d", ret);
		return ret;
	}

	pi4ioxx_gpio_set(chip, offset, value);
	return 0;
}

static int pi4ioxx_gpio_configure_input_latch(struct gpio_chip *chip,
					      uint16_t irq_mask)
{
	int ret;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;

	if(pi4io->gpio.ngpio == 8){
		return ret;
	}

	ret = regmap_update_bits(pi4io->regmap, PI4IO16_INPUT_LATCH0,
				 0xFF, irq_mask & 0xFF);
	if (ret) {
		dev_err(dev, "Failed to configure INPUT_LATCH0: %d", ret);
		return ret;
	}

	ret = regmap_update_bits(pi4io->regmap, PI4IO16_INPUT_LATCH1,
				0xFF, (irq_mask >> 8) & 0xFF);
	if (ret) {
		dev_err(dev, "Failed to configure INPUT_LATCH1: %d", ret);
		return ret;
	}
	return ret;
}

static int pi4ioxx_gpio_unmask_interrupts(struct gpio_chip *chip,
					  uint16_t irq_mask)
{
	int ret, reg;
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(chip);
	struct device *dev = &pi4io->i2c->dev;
	if(pi4io->gpio.ngpio == 16){
		reg = PI4IO16_INTMASK_REG0;
	}else{
		reg = PI4IO08_INTMASK_REG;
	}
	ret = regmap_update_bits(pi4io->regmap, reg,
				 0xFF, ~(irq_mask & 0xFF));
	if (ret) {
		dev_err(dev, "Failed to configure INTMASK_REG0: %d", ret);
		return ret;
	}
	if(pi4io->gpio.ngpio == 16){
		ret = regmap_update_bits(pi4io->regmap, PI4IO16_INTMASK_REG1,
					0xFF, ~((irq_mask >> 8) & 0xFF));
		if (ret) {
			dev_err(dev, "Failed to configure INTMASK_REG1: %d", ret);
			return ret;
		}
	}
	return ret;
}

static int pi4ioxx_gpio_setup(struct pi4ioxx_priv *pi4io, int pi4io_dev_id)
{
	int ret;
	struct device *dev = &pi4io->i2c->dev;
	struct gpio_chip *gc = &pi4io->gpio;

	gc->ngpio = pi4io_dev_id ? PI4IO08_N_GPIO : PI4IO16_N_GPIO;
	gc->label = pi4io->i2c->name;
	gc->parent = &pi4io->i2c->dev;
	gc->owner = THIS_MODULE;
	gc->base = -1;
	gc->can_sleep = true;

	gc->get_direction = pi4ioxx_gpio_get_direction;
	gc->direction_input = pi4ioxx_gpio_direction_input;
	gc->direction_output = pi4ioxx_gpio_direction_output;
	gc->get = pi4ioxx_gpio_get;
	gc->set = pi4ioxx_gpio_set;

	ret = devm_gpiochip_add_data(dev, gc, pi4io);
	if (ret) {
		dev_err(dev, "devm_gpiochip_add_data failed: %d", ret);
		return ret;
	}
	return 0;
}

static int pi4ioxx_reset_setup(struct pi4ioxx_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	struct gpio_desc *gpio;

	gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	if (gpio) {
		dev_info(dev, "Reset pin=%d\n", desc_to_gpio(gpio));
		pi4io->reset_gpio = gpio;
	}

	return 0;
}

static void pi4ioxx_reset(struct pi4ioxx_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;

	if (pi4io->reset_gpio) {
		dev_info(dev, "Resetting\n");
		gpiod_set_value(pi4io->reset_gpio, 0);
		msleep(PI4IO16_RESET_DELAY_MS);
		gpiod_set_value(pi4io->reset_gpio, 1);
		msleep(PI4IO16_RESET_DELAY_MS);
		gpiod_set_value(pi4io->reset_gpio, 0);
		msleep(PI4IO16_RESET_DELAY_MS);
	}
}

#ifdef CONFIG_GPIO_PI4IOE5V64XX_IRQ
static void pi4ioxx_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;

	dev_dbg(dev, "update irq_mask=0x%X & ~%lX\n", pi4io->irq_mask,
		d->hwirq);

	pi4io->irq_mask &= ~(1 << d->hwirq);
}

static void pi4ioxx_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;

	dev_dbg(dev, "update irq_mask=0x%X | %lX\n", pi4io->irq_mask, d->hwirq);

	pi4io->irq_mask |= (1 << d->hwirq);
}

static void pi4ioxx_irq_bus_lock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(gc);

	mutex_lock(&pi4io->irq_lock);
}

static void pi4ioxx_irq_bus_sync_unlock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	uint16_t new_irqs;
	int level;

	new_irqs = pi4io->irq_mask;

	dev_dbg(dev, "syncing update new_mask=0x%X\n", new_irqs);

	/* Enable input latch on newly masked registers */
	pi4ioxx_gpio_configure_input_latch(&pi4io->gpio, pi4io->irq_mask);

	/* for every new irq that got re-configured set to an input */
	while (new_irqs) {
		level = __ffs(new_irqs);
		pi4ioxx_gpio_direction_input(&pi4io->gpio, level);
		new_irqs &= ~(1 << level);
	}

	/* unmask the interrupts */
	pi4ioxx_gpio_unmask_interrupts(&pi4io->gpio, pi4io->irq_mask);

	mutex_unlock(&pi4io->irq_lock);
}

static int pi4ioxx_irq_set_type(struct irq_data *d, unsigned int type)
{
	int ret;
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pi4ioxx_priv *pi4io = gpiochip_get_data(gc);
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	uint16_t irq = BIT(d->hwirq);

	dev_dbg(dev, "set_type irq=%d, type=%d\n", irq, type);

	if(pi4io->gpio.ngpio == 16){
		/* The pi4io16 only supports generating interrupts when value changes
		* from its default state.
		*/
		if (!(type & IRQ_TYPE_EDGE_BOTH)) {
			dev_err(dev, "irq %d: unsupported type %d\n", d->irq, type);
			return -EINVAL;
		}
	}else{
		switch (type)
		{
		case IRQ_TYPE_LEVEL_HIGH:
		{
			ret = regmap_update_bits(pi4io->regmap, PI4IO08_INPUT_DEFAULT, irq, 0x00);
			if (ret) {
				dev_err(dev, "Failed to write output: %d", ret);
			}
		}
			break;
		case IRQ_TYPE_LEVEL_LOW:
		{
			ret = regmap_update_bits(pi4io->regmap, PI4IO08_INPUT_DEFAULT, irq, 0xff);
			if (ret) {
				dev_err(dev, "Failed to write output: %d", ret);
			}
		}
			break;
		default:
			dev_err(dev, "irq %d: unsupported type %d\n", d->irq, type);
			return -EINVAL;
			break;
		}
	}
	return 0;
}

static uint16_t pi4ioxx_irq_pending(struct pi4ioxx_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	uint16_t irq_status;
	int irq_status_reg;
	int ret, reg;
	if(pi4io->gpio.ngpio == 16){
		reg = PI4IO16_INT_STATUS0;
	}else{
		reg = PI4IO08_INTERRUPT_STATUS;
	}
	/* Interrupt status from both banks */
	ret = regmap_read(pi4io->regmap, reg, &irq_status_reg);
	if (ret < 0) {
		dev_err(dev, "Failed to read INT_STATUS0 rc=%d\n", ret);
		return 0;
	}

	irq_status = irq_status_reg;

	if(pi4io->gpio.ngpio == 16){
		ret = regmap_read(pi4io->regmap, PI4IO16_INT_STATUS1, &irq_status_reg);
		if (ret < 0) {
			dev_err(dev, "Failed to read INT_STATUS1 rc=%d\n", ret);
			return 0;
		}

		irq_status |= (irq_status_reg << 8);
	}

	/* return 16 bit representation of irq status, msB = bankb */
	return irq_status;
}

static irqreturn_t pi4ioxx_irq_handler(int irq, void *data)
{
	struct pi4ioxx_priv *pi4io = data;
	uint16_t pending;
	uint16_t level;

	/* Get the current state of the ISR of both banks, NOTE:
	   does not clear mask yet. */
	pending = pi4ioxx_irq_pending(pi4io);
	if (pending == 0) {
		/* Spurious interrupt ? */
		pr_warn("No pending nested irqs in irq handler\n");
		return IRQ_NONE;
	}

	/* Process each each irq sequentially */
	while (pending) {
		level = __ffs(pending);	// first pin
		handle_nested_irq(irq_find_mapping
				  (pi4io->gpio.irq.domain, level)
		    );
		/* clear pending bit */
		pending &= ~(1 << level);
	}
	return IRQ_HANDLED;
}

static int pi4ioxx_irq_setup(struct pi4ioxx_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;
	struct device *dev = &client->dev;
	struct gpio_desc *gpio;
	int ret;
	int irq;
	int irq_base = 0;

	gpio = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	if (!gpio) {
		dev_warn(dev, "No irq pin for gpio chip");
		return 0;
	}

	mutex_init(&pi4io->irq_lock);
	irq = gpiod_to_irq(gpio);
	if (irq < 0) {
		dev_err(dev, "No irq for gpio=%d rc=%d",
			desc_to_gpio(gpio), irq);
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq,
					NULL, pi4ioxx_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING |
					IRQF_SHARED, dev_name(dev), pi4io);
	if (ret < 0) {
		dev_err(dev,
			"Failed to request irq for gpio=%d, irq=%d, rc=%d\n",
			desc_to_gpio(gpio), irq, ret);
		return ret;
	}

	/* Setup per-instance of device irq_chip */
	memset(&pi4io->irq_chip, 0, sizeof(struct irq_chip));
	pi4io->irq_chip.name = "pi4ioxx";
	pi4io->irq_chip.irq_mask = pi4ioxx_irq_mask;
	pi4io->irq_chip.irq_unmask = pi4ioxx_irq_unmask;
	pi4io->irq_chip.irq_bus_lock = pi4ioxx_irq_bus_lock;
	pi4io->irq_chip.irq_bus_sync_unlock = pi4ioxx_irq_bus_sync_unlock;
	pi4io->irq_chip.irq_set_type = pi4ioxx_irq_set_type;

	ret = gpiochip_irqchip_add_nested(&pi4io->gpio,
					  &pi4io->irq_chip,
					  irq_base,
					  handle_simple_irq, IRQ_TYPE_NONE);
	if (ret) {
		dev_err(dev, "could not connect irqchip to gpiochip\n");
		return ret;
	}

	gpiochip_set_nested_irqchip(&pi4io->gpio, &pi4io->irq_chip, irq);
	dev_info(dev, "Successfully setup nested irq_handlers\n");
	return 0;
}

#else
static int pi4ioxx_irq_setup(struct pi4ioxx_priv *pi4io)
{
	struct i2c_client *client = pi4io->i2c;

	dev_warn(&client->dev, "IRQ not supported with current config");
	return 0;
}
#endif

static int pi4ioxx_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret;
	struct device *dev = &client->dev;
	struct pi4ioxx_priv *pi4io;
	int pi4io_dev_id = (int)id->driver_data;
	if(pi4io_dev_id)
		dev_info(dev, "pi4io08 probe()\n");
	else
		dev_info(dev, "pi4io16 probe()\n");

	pi4io = devm_kzalloc(dev, sizeof(struct pi4ioxx_priv), GFP_KERNEL);
	if (!pi4io) {
		return -ENOMEM;
	}

	i2c_set_clientdata(client, pi4io);
	pi4io->i2c = client;

	pi4io->regmap = pi4ioxx_setup_regmap(client, pi4io_dev_id);
	if (IS_ERR(pi4io->regmap)) {
		ret = PTR_ERR(pi4io->regmap);
		dev_err(&client->dev, "Failed to init register map: %d\n",
			ret);
		return ret;
	}

	ret = pi4ioxx_reset_setup(pi4io);
	if (ret < 0) {
		dev_err(dev, "failed to configure reset-gpio: %d", ret);
		return ret;
	}

	/* Reset chip to nice state. */
	pi4ioxx_reset(pi4io);

	ret = pi4ioxx_gpio_setup(pi4io, pi4io_dev_id);
	if (ret < 0) {
		dev_err(dev, "Failed to setup GPIOs: %d", ret);
		return ret;
	}
	
//	pi4io->pinctrl_desc.name = "pi4ioe5v64xx-pinctrl";
//	pi4io->pinctrl_desc.pctlops = &pi4io_pinctrl_ops;
//	pi4io->pinctrl_desc.confops = &pi4io_pinconf_ops;
//	pi4io->pinctrl_desc.npins = pi4io->gpio.ngpio;
//	if (pi4io->pinctrl_desc.npins == 8)
//		pi4io->pinctrl_desc.pins = pi4iox08_pins;
//	else if (pi4io->pinctrl_desc.npins == 16)
//		pi4io->pinctrl_desc.pins = pi4iox16_pins;
//	pi4io->pinctrl_desc.owner = THIS_MODULE;
//
//	pi4io->pctldev = devm_pinctrl_register(dev, &pi4io->pinctrl_desc, pi4io);
//	if (IS_ERR(pi4io->pctldev)) {
//		ret = PTR_ERR(pi4io->pctldev);
//		return ret;
//	}
	
	ret = pi4ioxx_irq_setup(pi4io);
	if (ret < 0) {
		dev_err(dev, "Failed to setup IRQ: %d", ret);
		return ret;
	}
	dev_dbg(dev, "probe finished");

	return 0;
}

static int pi4ioxx_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id pi4ioxx_id_table[] = {
	{"pi4ioe5v6416", 0},
	{"pi4ioe5v6408", 1},
	{}
};

MODULE_DEVICE_TABLE(i2c, pi4ioxx_id_table);

#ifdef CONFIG_OF
static const struct of_device_id pi4ioxx_of_match[] = {
	{.compatible = "pericom,pi4ioe5v6416", .data = 0},
	{.compatible = "pericom,pi4ioe5v6408", .data = 1},
	{},
};

MODULE_DEVICE_TABLE(of, pi4ioxx_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id pi4io16_acpi_match_table[] = {
	{"PI4IO16", 0},
	{"PI4IO08", 1},
	{},
};
#endif

static struct i2c_driver pi4ioxx_driver = {
	.driver = {
		   .name = "pi4ioxx-gpio",
		   .of_match_table = of_match_ptr(pi4ioxx_of_match),
#ifdef CONFIG_ACPI
		   .acpi_match_table = ACPI_PTR(pi4io16_acpi_match_table),
#endif
	},
	.probe = pi4ioxx_probe,
	.remove = pi4ioxx_remove,
	.id_table = pi4ioxx_id_table,
};

static int __init pi4ioxx_init(void)
{
	return i2c_add_driver(&pi4ioxx_driver);
}

/* NOTE: not using module_i2c_driver macro in order to enumerate
 *	 gpio expander earlier.
 */
subsys_initcall(pi4ioxx_init);

static void __exit pi4ioxx_exit(void)
{
	i2c_del_driver(&pi4ioxx_driver);
}

module_exit(pi4ioxx_exit);

MODULE_AUTHOR("Tesla OpenSource <opensource@tesla.com>, dianjixz <dianjixz@m5stack.com>");
MODULE_DESCRIPTION("PI4IOE5V64XX 8/16-bit I2C GPIO expander driver");
MODULE_LICENSE("GPL v2");