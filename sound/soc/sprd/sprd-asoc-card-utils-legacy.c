/*
 * sound/soc/sprd/sprd-asoc-card-utils-legacy.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("BOARD")""fmt

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-card-utils-legacy.h"
#include "sprd-asoc-common.h"

#define FUN_REG(f) ((unsigned short)(-(f+1)))

#define SWITCH_FUN_ON	1
#define SWITCH_FUN_OFF	0

struct board_mute {
	int need_mute;
	int is_on;
};

static struct board_priv {
	int func_switch[BOARD_FUNC_MAX];
	struct board_mute m[BOARD_FUNC_MUTE_MAX];
	int pa_type[BOARD_FUNC_MUTE_MAX];
} board;

#define BOARD_EXT_SPK "Ext Spk"
#define BOARD_EXT_SPK1 "Ext Spk1"
#define BOARD_EXT_EAR "Ext Ear"
#define BOARD_HEADPHONE_JACK "HeadPhone Jack"
#define BOARD_LINE_JACK "Line Jack"
#define BOARD_MIC_JACK "Mic Jack"
#define BOARD_AUX_MIC_JACK "Aux Mic Jack"
#define BOARD_HP_MIC_JACK "HP Mic Jack"
#define BOARD_DMIC_JACK "DMic Jack"
#define BOARD_DMIC1_JACK "DMic1 Jack"
#define BOARD_DIG_FM_JACK "Dig FM Jack"
#define BOARD_INTER_HP_PA "inter HP PA"
#define BOARD_INTER_SPK_PA "inter Spk PA"
#define BOARD_INTER_SPK1_PA "inter Spk1 PA"
#define BOARD_INTER_EAR_PA "inter Ear PA"

static const char *func_name[BOARD_FUNC_MAX] = {
	BOARD_EXT_SPK,
	BOARD_EXT_SPK1,
	BOARD_EXT_EAR,
	BOARD_HEADPHONE_JACK,
	BOARD_LINE_JACK,
	BOARD_MIC_JACK,
	BOARD_AUX_MIC_JACK,
	BOARD_HP_MIC_JACK,
	BOARD_DMIC_JACK,
	BOARD_DMIC1_JACK,
	BOARD_DIG_FM_JACK,
};

/* Hook utils */
static struct sprd_asoc_ext_hook *ext_hook;

int sprd_asoc_ext_hook_register(struct sprd_asoc_ext_hook *hook)
{
	ext_hook = hook;

	return 0;
}

int sprd_asoc_ext_hook_unregister(struct sprd_asoc_ext_hook *hook)
{
	if (ext_hook != hook) {
		pr_err("ERR:Maybe Unregister other's Hook?\n");
		return -EBUSY;
	}
	ext_hook = 0;
	return 0;
}

#define SAFE_CALL(func, id, on) (func ? func(id, on) : HOOK_NONE)

static int board_ext_hook(int ext_ctrl_id, int func_id, int on)
{
	int ret = HOOK_NONE;

	if (func_id < 0 || func_id >= BOARD_FUNC_MAX) {
		pr_err("%s func id(%d) is invalid.\n", __func__, func_id);
		return -EINVAL;
	}
	if (ext_ctrl_id < 0 || ext_ctrl_id >= EXT_CTRL_MAX) {
		pr_err("%s func id(%d) is invalid.\n", __func__, func_id);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("external ctrl hook(%d) with function id(%d), on=%d\n",
		       ext_ctrl_id, func_id, on);
	if (ext_hook)
		ret = SAFE_CALL(ext_hook->ext_ctrl[ext_ctrl_id], func_id, on);

	return ret;
}

static void board_ext_pin_control(struct snd_soc_dapm_context *dapm,
				  int s, int e)
{
	int i;

	if (s < 0 || s >= BOARD_FUNC_MAX) {
		pr_err("%s start func id(%d) is invalid.\n", __func__, s);
		return;
	}
	if (e < 0 || e > BOARD_FUNC_MAX) {
		pr_err("%s end func id(%d) is invalid.\n", __func__, e);
		return;
	}

	for (i = s; i < e; i++) {
		if (board.func_switch[i] == SWITCH_FUN_ON &&
			(i >= BOARD_FUNC_MUTE_MAX ||
			(i < BOARD_FUNC_MUTE_MAX && !board.m[i].need_mute)))
			snd_soc_dapm_enable_pin(dapm, func_name[i]);
		else
			snd_soc_dapm_disable_pin(dapm, func_name[i]);
	}

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);
}

static inline void inter_pa_pin_control(struct snd_soc_card *card,
					const char *id, bool enable, bool sync)
{
	if (enable)
		snd_soc_dapm_enable_pin(&card->dapm, id);
	else
		snd_soc_dapm_disable_pin(&card->dapm, id);

	if (sync) {
		/* signal a DAPM event */
		snd_soc_dapm_sync(&card->dapm);
	}
}

static void board_inter_pa_pin_control(struct snd_soc_card *card, int id,
				       bool sync)
{
	char *pa_name;

	if (id < 0 || id >= BOARD_FUNC_MUTE_MAX) {
		pr_err("%s func id(%d) is invalid!\n", __func__, id);
		return;
	}

	switch (id) {
	case BOARD_FUNC_SPK:
		pa_name = BOARD_INTER_SPK_PA;
		break;
	case BOARD_FUNC_SPK1:
		pa_name = BOARD_INTER_SPK1_PA;
		break;
	case BOARD_FUNC_EAR:
		pa_name = BOARD_INTER_EAR_PA;
		break;
	case BOARD_FUNC_HP:
		pa_name = BOARD_INTER_HP_PA;
		break;
	default:
		return;
	}

	if (board.pa_type[id] & HOOK_BPY) {
		int enable = 1;

		if (board.func_switch[id] != SWITCH_FUN_ON ||
		    board.m[id].need_mute)
			enable = 0;
		inter_pa_pin_control(card, pa_name, enable, sync);
	}
}

/* For devices with mute function. */
static void board_ext_enable(struct snd_soc_card *card, int enable, int func_id)
{
	int ret;
	int ext_ctrl_id;

	if (func_id >= BOARD_FUNC_MUTE_MAX || func_id < 0) {
		pr_err("%s func id(%d) is invalid!\n", __func__, func_id);
		return;
	}

	switch (func_id) {
	case BOARD_FUNC_SPK:
	case BOARD_FUNC_SPK1:
		ext_ctrl_id = EXT_CTRL_SPK;
		break;
	case BOARD_FUNC_HP:
		ext_ctrl_id = EXT_CTRL_HP;
		break;
	case BOARD_FUNC_EAR:
		ext_ctrl_id = EXT_CTRL_EAR;
		break;
	default:
		pr_warn("%s no hook for func type %d\n", __func__, func_id);
		return;
	}

	if (enable && board.m[func_id].need_mute)
		enable = 0;

	pr_debug("%s ext_ctrl_id %d func_id %d enable %d\n",
		__func__, ext_ctrl_id, func_id, enable);
	ret = board_ext_hook(ext_ctrl_id, func_id, enable);
	if (ret < 0) {
		pr_err("ERR:Call external earpiece control failed %d!\n", ret);
		return;
	}
}

static int board_headphone_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Headphone Switch %s\n", STR_ON_OFF(on));
	board_ext_enable(w->dapm->card, on, BOARD_FUNC_HP);

	return 0;
}

static int board_earpiece_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Earpiece Switch %s\n", STR_ON_OFF(on));
	board.m[BOARD_FUNC_EAR].is_on = on;
	board_ext_enable(w->dapm->card, on, BOARD_FUNC_EAR);

	return 0;
}

static int board_speaker_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Speaker Switch %s\n", STR_ON_OFF(on));
	board.m[BOARD_FUNC_SPK].is_on = on;
	board_ext_enable(w->dapm->card, on, BOARD_FUNC_SPK);

	return 0;
}

static int board_speaker1_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Speaker2 Switch %s\n", STR_ON_OFF(on));
	board.m[BOARD_FUNC_SPK1].is_on = on;
	board_ext_enable(w->dapm->card, on, BOARD_FUNC_SPK1);

	return 0;
}

static int board_main_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Main MIC Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_MIC, BOARD_FUNC_MIC, on);

	return 0;
}

static int board_sub_mic_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Sub MIC Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_MIC, BOARD_FUNC_AUXMIC, on);

	return 0;
}

static int board_head_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Head MIC Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_MIC, BOARD_FUNC_HP_MIC, on);

	return 0;
}

static int board_dig0_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Digtial0 MIC Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_MIC, BOARD_FUNC_DMIC, on);

	return 0;
}

static int board_dig1_mic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Digtial1 MIC Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_MIC, BOARD_FUNC_DMIC1, on);

	return 0;
}

static int board_line_in_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("LINE IN Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_MIC, BOARD_FUNC_LINE, on);

	return 0;
}

static int board_dig_fm_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *k, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_dbg("Digtial FM Switch %s\n", STR_ON_OFF(on));
	board_ext_hook(EXT_CTRL_DFM, BOARD_FUNC_DFM, on);

	return 0;
}

static const struct snd_soc_dapm_widget _sprd_asoc_card_widgets[] = {
	SND_SOC_DAPM_MIC(BOARD_MIC_JACK, board_main_mic_event),
	SND_SOC_DAPM_MIC(BOARD_AUX_MIC_JACK, board_sub_mic_event),
	SND_SOC_DAPM_MIC(BOARD_HP_MIC_JACK, board_head_mic_event),
	SND_SOC_DAPM_MIC(BOARD_DMIC_JACK, board_dig0_mic_event),
	SND_SOC_DAPM_MIC(BOARD_DMIC1_JACK, board_dig1_mic_event),
	/*digital fm input */
	SND_SOC_DAPM_LINE(BOARD_DIG_FM_JACK, board_dig_fm_event),

	SND_SOC_DAPM_SPK(BOARD_EXT_SPK, board_speaker_event),
	SND_SOC_DAPM_SPK(BOARD_EXT_SPK1, board_speaker1_event),
	SND_SOC_DAPM_SPK(BOARD_EXT_EAR, board_earpiece_event),
	SND_SOC_DAPM_LINE(BOARD_LINE_JACK, board_line_in_event),
	SND_SOC_DAPM_HP(BOARD_HEADPHONE_JACK, board_headphone_event),
};

struct sprd_array_size sprd_asoc_card_widgets = {
	.ptr = _sprd_asoc_card_widgets,
	.size = ARRAY_SIZE(_sprd_asoc_card_widgets),
};
EXPORT_SYMBOL(sprd_asoc_card_widgets);

static int board_func_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = board.func_switch[id];

	return 0;
}

static int board_func_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	int id = FUN_REG(mc->reg);

	sp_asoc_pr_info("%s Switch %s\n", func_name[id],
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	if (board.func_switch[id] == ucontrol->value.integer.value[0])
		return 0;

	board.func_switch[id] = ucontrol->value.integer.value[0];
	board_inter_pa_pin_control(card, id, 1);
	board_ext_pin_control(&card->dapm, id, id + 1);

	return 1;
}

static int board_mute_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = board.m[id].need_mute;

	return 0;
}

static int board_mute_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	int id = FUN_REG(mc->reg);

	sp_asoc_pr_info("%s Switch %s\n", func_name[id],
			ucontrol->value.integer.value[0] ? "Mute" : "Unmute");

	if (board.m[id].need_mute == ucontrol->value.integer.value[0])
		return 0;

	board.m[id].need_mute = ucontrol->value.integer.value[0];
	board_ext_pin_control(&card->dapm, id, id + 1);
	board_inter_pa_pin_control(card, id, 1);

	return 1;
}

static const char * const smartamp_boost_texts[] = {
	"Bypass", "5V"
};
static const struct soc_enum smartamp_boost_enum =
	SOC_ENUM_SINGLE_VIRT(ARRAY_SIZE(smartamp_boost_texts),
						smartamp_boost_texts);

static int smartamp_boost_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(card);
	struct smartamp_boost_data *spec_data = &mdata->boost_data;

	if (!spec_data) {
		pr_err("%s, Board special data is NULL!\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.enumerated.item[0] = spec_data->boost_mode;

	return 0;
}

static int smartamp_boost_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(card);
	struct smartamp_boost_data *spec_data = &mdata->boost_data;
	int item = ucontrol->value.enumerated.item[0];

	if (!spec_data) {
		pr_err("%s, Board special data is NULL!\n", __func__);
		return -EINVAL;
	}

	if (item > e->items) {
		pr_err("Invalid item(%d) for hp sel\n", item);
		return -EINVAL;
	}

	pr_debug("smartamp boost mode: '%s'\n", texts->texts[item]);
	spec_data->boost_mode = item;
	gpio_set_value(spec_data->gpios[SMARTAMP_BOOST_GPIO_MADE], !!item);

	return 0;
}

int sprd_asoc_card_parse_smartamp_boost(struct device *dev,
				  struct smartamp_boost_data *boost_data)
{
	int ret, i, index;
	unsigned long flags;
	const char *smartamp_boost_name = "sprd,smartamp-boost-names";
	const char *smartamp_boost_gpios = "sprd,smartamp-boost-gpios";
	struct device_node *np = dev->of_node;
	struct gpio_index_name {
		int index;
		const char *name;
	} gpio_maps[] = {
		{SMARTAMP_BOOST_GPIO_ENABLE, "boost_enable"},
		{SMARTAMP_BOOST_GPIO_MADE, "boost_mode"},
	};

	if (!boost_data) {
		pr_err("Board special data is NULL!\n");
		return -EINVAL;
	}
	if (of_property_read_bool(np, smartamp_boost_name)) {
		/* Parse gpios. */
		for (i = 0; i < ARRAY_SIZE(gpio_maps); i++) {
			const char *name = gpio_maps[i].name;

			index = of_property_match_string(np,
						smartamp_boost_name, name);
			if (index < 0) {
				pr_err("No match found for '%s' gpio!\n", name);
				return index;
			}
			ret = of_get_named_gpio_flags(np, smartamp_boost_gpios,
					index, NULL);
			if (ret < 0) {
				pr_err("Get gpio for '%s' failed(%d)!\n",
					name, ret);
				return ret;
			}
			boost_data->gpios[gpio_maps[i].index] = (u32)ret;
			if (gpio_maps[i].index == SMARTAMP_BOOST_GPIO_ENABLE)
				flags = GPIOF_OUT_INIT_HIGH;
			else
				flags = GPIOF_OUT_INIT_LOW;
			ret = devm_gpio_request_one(dev, ret, flags, name);
			if (ret < 0) {
				pr_err("Gpio '%s' request failed(%d)!\n",
					name, ret);
				return ret;
			}
		}
	}
	return 0;
}

#define BOARD_CODEC_FUNC(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), \
		0, 1, 0, board_func_get, board_func_set)

#define BOARD_CODEC_MUTE(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), \
		0, 1, 0, board_mute_get, board_mute_set)

static const struct snd_kcontrol_new _sprd_asoc_card_controls[] = {
	BOARD_CODEC_FUNC("Speaker Function", BOARD_FUNC_SPK),
	BOARD_CODEC_FUNC("Speaker1 Function", BOARD_FUNC_SPK1),
	BOARD_CODEC_FUNC("Earpiece Function", BOARD_FUNC_EAR),
	BOARD_CODEC_FUNC("HeadPhone Function", BOARD_FUNC_HP),
	BOARD_CODEC_FUNC("Line Function", BOARD_FUNC_LINE),
	BOARD_CODEC_FUNC("Mic Function", BOARD_FUNC_MIC),
	BOARD_CODEC_FUNC("Aux Mic Function", BOARD_FUNC_AUXMIC),
	BOARD_CODEC_FUNC("HP Mic Function", BOARD_FUNC_HP_MIC),
	BOARD_CODEC_FUNC("DMic Function", BOARD_FUNC_DMIC),
	BOARD_CODEC_FUNC("DMic1 Function", BOARD_FUNC_DMIC1),
	BOARD_CODEC_FUNC("Digital FM Function", BOARD_FUNC_DFM),

	BOARD_CODEC_MUTE("Speaker Mute", BOARD_FUNC_SPK),
	BOARD_CODEC_MUTE("Speaker2 Mute", BOARD_FUNC_SPK1),
	BOARD_CODEC_MUTE("HeadPhone Mute", BOARD_FUNC_HP),
	/* "Earpiece Mute" must be the last one. */
	BOARD_CODEC_MUTE("Earpiece Mute", BOARD_FUNC_EAR),
	/*smart amp boost function select*/
	SOC_ENUM_EXT("SmartAmp Boost", smartamp_boost_enum, smartamp_boost_get,
				smartamp_boost_set),
};

struct sprd_array_size sprd_asoc_card_controls = {
	.ptr = _sprd_asoc_card_controls,
	.size = ARRAY_SIZE(_sprd_asoc_card_controls),
};
EXPORT_SYMBOL(sprd_asoc_card_controls);

static void board_pa_type_check(int func_id)
{
	switch (func_id) {
	case BOARD_FUNC_SPK:
	case BOARD_FUNC_SPK1:
		board.pa_type[func_id] =
		    board_ext_hook(EXT_CTRL_SPK, func_id, 0);
		break;
	case BOARD_FUNC_HP:
		board.pa_type[func_id] =
		    board_ext_hook(EXT_CTRL_HP, func_id, 0);
		break;
	case BOARD_FUNC_EAR:
		board.pa_type[func_id] =
		    board_ext_hook(EXT_CTRL_EAR, func_id, 0);
		break;
	default:
		return;
	}
}

static void board_inter_pa_init(void)
{
	int id;

	for (id = BOARD_FUNC_SPK; id < BOARD_FUNC_MUTE_MAX; id++)
		board_pa_type_check(id);
}

static void board_inter_pa_pin_control_all(struct snd_soc_card *card)
{
	int id;

	for (id = BOARD_FUNC_SPK; id < BOARD_FUNC_MUTE_MAX; id++)
		board_inter_pa_pin_control(card, id, 0);
}

int sprd_asoc_board_comm_probe(void)
{
	board_inter_pa_init();

	return 0;
}
EXPORT_SYMBOL(sprd_asoc_board_comm_probe);

int sprd_asoc_board_comm_late_probe(struct snd_soc_card *card)
{
	int i;

	sprd_audio_debug_init(card->snd_card);

	board_inter_pa_pin_control_all(card);
	board_ext_pin_control(&card->dapm, 0, BOARD_FUNC_MAX);

	for (i = 0; i < BOARD_FUNC_MAX; i++)
		snd_soc_dapm_ignore_suspend(&card->dapm, func_name[i]);

	return 0;
}
EXPORT_SYMBOL(sprd_asoc_board_comm_late_probe);

void sprd_asoc_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	memset(&board.func_switch, 0, sizeof(board.func_switch));
	board_inter_pa_pin_control_all(card);
	board_ext_pin_control(&card->dapm, 0, BOARD_FUNC_MAX);
}
EXPORT_SYMBOL(sprd_asoc_shutdown);

