/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SPACEMIT_CPUFREQ_H__
#define __SPACEMIT_CPUFREQ_H__

#include <linux/types.h>

#ifdef CONFIG_SOC_SPACEMIT
struct private_data;

struct private_data *cpufreq_dt_find_data(int cpu);
void cpufreq_dt_add_data(struct private_data *priv);
#endif

#ifdef CONFIG_SOC_SPACEMIT_K1X
void remove_boost_sysfs_file(void);
void remove_policy_boost_sysfs_file(struct cpufreq_policy *policy);

void spacemit_cpufreq_ready(struct cpufreq_policy *policy);
int spacmeit_cpufreq_veritfy(struct cpufreq_policy_data *policy);
#endif

#endif /* __SPACEMIT_CPUFREQ_H__ */
