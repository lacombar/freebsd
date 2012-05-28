# $FreeBSD$

INTERFACE hwmon;

#
#
#
METHOD int get_name {
	device_t		dev;
	const char **		namep;
};

#
#
#
METHOD int get_update_interval {
	device_t		dev;
	int *			intervalp;
};

#
#
#
METHOD int set_update_interval {
	device_t		dev;
	int			interval;
};

#
# Voltages
#
METHOD int in_get_label {
	device_t		dev;
	int			channel;
	const char **		labelp;
};

#
#
#
METHOD int in_get_min {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_set_min {
	device_t		dev;
	int			channel;
	int			value;
};

#
#
#
METHOD int in_get_lcrit {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_set_lcrit {
	device_t		dev;
	int			channel;
	int			value;
};

#
#
#
METHOD int in_get_max {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_set_max {
	device_t		dev;
	int			channel;
	int			value;
};

#
#
#
METHOD int in_get_crit {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_set_crit {
	device_t		dev;
	int			channel;
	int			value;
};

#
#
#
METHOD int in_get_input {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_get_average {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_get_lowest {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_get_highest {
	device_t		dev;
	int			channel;
	int *			valuep;
};

#
#
#
METHOD int in_reset_history {
	device_t		dev;
	int			channel;
};

