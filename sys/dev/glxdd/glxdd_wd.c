#ifdef WATCHDOG
/* Watchdog interface */
static int cs5536_watchdog_enable(device_t);
static int cs5536_watchdog_disable(device_t);
static int cs5536_watchdog_configure(device_t, struct timespec *,
	    watchdog_action_t);
static int cs5536_watchdog_rearm(device_t);
#endif


#ifdef WATCHDOG
/* Watchdog interface */
static int
cs5536_watchdog_enable(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	uint16_t val;

	/* Bits [12:0] are write once; check if the counter has been setup. */
	val = inw(cs->cs_lbar_mfgpt + MFGPT0_SETUP);
	if ((val & MFGPT_SETUP) == 0)
		return EINVAL;

	outw(cs->cs_lbar_mfgpt + MFGPT0_SETUP, MFGPT_CNT_EN);

	return 0;
}

static int
cs5536_watchdog_disable(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	uint16_t val;

	/* Bits [12:0] are write once, check if the counter has been setup; */
	val = inw(cs->cs_lbar_mfgpt + MFGPT0_SETUP);
	if ((val & MFGPT_SETUP) && (val & MFGPT_CNT_EN) == 0)
		return EINVAL;

	/* Stop and reset counter */
	outw(cs->cs_lbar_mfgpt + MFGPT0_SETUP, 0);
	outw(cs->cs_lbar_mfgpt + MFGPT0_CNT, 0);

	return 0;
}

static int
cs5536_watchdog_configure(device_t dev, struct timespec *ts,
    watchdog_action_t action)
{
	struct cs5536_softc *cs = device_get_softc(dev);
	int count;
	uint32_t m;
	uint16_t val;

	count = ts->tv_sec * 2; /* XXX arg.... */

	val = inw(cs->cs_lbar_mfgpt + MFGPT0_SETUP);

	/* Bits [12:0] are write once, */
	if ((val & MFGPT_SETUP) == 0) {
		/* Set up MFGPT0, 32khz, prescaler 16k, C2 event */
		val = MFGPT_SCALE_16384;
		val |= MFGPT_CLKSEL_32K;
		val |= MFGPT_CMP2MODE(MFGPT_CMPMODE_EVENT);

		outw(cs->cs_lbar_mfgpt + MFGPT0_SETUP, val);
	}

	/* set comparator 2 */
	outw(cs->cs_lbar_mfgpt + MFGPT0_CMP2, count);

	/* reset counter */
	outw(cs->cs_lbar_mfgpt + MFGPT0_CNT, 0);

	/* Arm reset */
	m = rdmsr(MSR_MFTPT_NR);
	switch (action) {
	case WATCHDOG_ACTION_RESET:
		m &= ~(MSR_MFGPT0_C2_NMIM | NMI_LEG);
		m |= MSR_MFGPT0_C2_RSTEN;
		break;
	case WATCHDOG_ACTION_NMI:
		m &= ~MSR_MFGPT0_C2_RSTEN;
		m |= (MSR_MFGPT0_C2_NMIM | NMI_LEG);
		break;
	}
	wrmsr(MSR_MFTPT_NR, m);

	return 0;
}

static int
cs5536_watchdog_rearm(device_t dev)
{
	struct cs5536_softc *cs = device_get_softc(dev);

	outw(cs->cs_lbar_mfgpt + MFGPT0_CNT, 0);

	return 0;
}
#endif
