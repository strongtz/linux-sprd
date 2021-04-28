#ifndef __SPRD_CPUFREQ_H__
#define __SPRD_CPUFREQ_H__

#if defined(CONFIG_ARM_SPRD_HW_CPUFREQ) || defined(CONFIG_ARM_SPRD_SW_CPUFREQ)
unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now);
#else
static unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now)
{
	return 0;
}
#endif
#endif

