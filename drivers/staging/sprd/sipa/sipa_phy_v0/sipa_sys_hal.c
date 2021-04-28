#include "../sipa_hal_priv.h"

static u32 sipa_sys_hal_module_soft_rst(void __iomem *reg_base,	u32 sys)
{
	u32 flag = 0;

	flag = sipa_sys_phy_module_soft_rst(reg_base, sys);

	return flag;
}

static u32 sipa_sys_hal_module_enable(void __iomem *reg_base,
				      u32 enable, u32 sys)
{
	u32 flag = 0;

	if (enable)
		flag = sipa_sys_phy_module_enable(reg_base, sys);
	else
		flag = sipa_sys_phy_module_disable(reg_base, sys);

	return flag;
}

u32 sipa_sys_proc_init(struct sipa_sys_ops *ops)
{
	IPA_LOG("%s\n", __func__);

	ops->module_enable = sipa_sys_hal_module_enable;
	ops->module_soft_rst = sipa_sys_hal_module_soft_rst;

	return TRUE;
}
EXPORT_SYMBOL(sipa_sys_proc_init);


