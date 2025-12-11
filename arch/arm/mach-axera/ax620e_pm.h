#ifndef __AX620E_PM_H__
#define __AX620E_PM_H__

#ifdef CONFIG_ARCH_AX620E
int ax620e_suspend_enter(suspend_state_t state);
int ax620e_suspend_prepare(void);
void ax620e_suspend_finish(void);
int ax620e_suspend_init(struct device_node *np);
#else
int ax620e_suspend_enter(suspend_state_t state)
{}
int ax620e_suspend_prepare(void)
{}
void ax620e_suspend_finish(void)
{}
int ax620e_suspend_init(struct device_node *np)
{}
#endif
#endif
