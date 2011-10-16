#ifdef GPIO
/* GPIO interface */
static int cs5536_gpio_pin_max(device_t, int *);
static int cs5536_gpio_pin_set(device_t, uint32_t, uint32_t);
static int cs5536_gpio_pin_get(device_t, uint32_t, uint32_t *);
static int cs5536_gpio_pin_toggle(device_t, uint32_t);
static int cs5536_gpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int cs5536_gpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int cs5536_gpio_pin_getname(device_t, uint32_t, char *);
static int cs5536_gpio_pin_setflags(device_t, uint32_t, uint32_t);
#endif

#ifdef GPIO
/* GPIO interface */
static int
cs5536_gpio_pin_max(device_t dev, int *npins)
{
	*npins = 32;

	return 0;
}

static int
cs5536_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	return ENOTSUP;
}

static int
cs5536_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	return ENOTSUP;
}
#endif

