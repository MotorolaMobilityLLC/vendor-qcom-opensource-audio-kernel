/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "../../../drivers/clk/qcom/common.h"
#include <linux/platform_device.h>
#include <dt-bindings/clock/qcom,audio-ext-clk.h>
#include <dsp/q6afe-v2.h>
#include "audio-ext-clk-up.h"

enum {
	AUDIO_EXT_CLK_PMI,
	AUDIO_EXT_CLK_LNBB2,
	AUDIO_EXT_CLK_LPASS,
	AUDIO_EXT_CLK_MAX,
};

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
	char __iomem *base;
};

struct audio_ext_clk {
	struct pinctrl_info pnctrl_info;
	struct clk_fixed_factor fact;
};

struct audio_ext_clk_priv {
	struct device *dev;
	int clk_src;
	struct afe_clk_set clk_cfg;
	struct audio_ext_clk audio_clk;
};

static inline struct audio_ext_clk_priv *to_audio_clk(struct clk_hw *hw)
{
	return container_of(hw, struct audio_ext_clk_priv, audio_clk.fact.hw);
}

static int audio_ext_clk_prepare(struct clk_hw *hw)
{
	struct audio_ext_clk_priv *clk_priv = to_audio_clk(hw);
	struct pinctrl_info *pnctrl_info = &clk_priv->audio_clk.pnctrl_info;
	int ret;

	if (clk_priv->clk_src == AUDIO_EXT_CLK_LPASS) {
		clk_priv->clk_cfg.enable = 1;
		ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &clk_priv->clk_cfg);
		if (ret < 0) {
			pr_err("%s afe_set_digital_codec_core_clock failed\n",
				__func__);
			return ret;
		}
	}

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
				pnctrl_info->active);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
				__func__, ret);
			return -EIO;
		}
	}

	if (pnctrl_info->base)
		iowrite32(1, pnctrl_info->base);
	return 0;
}

static void audio_ext_clk_unprepare(struct clk_hw *hw)
{
	struct audio_ext_clk_priv *clk_priv = to_audio_clk(hw);
	struct pinctrl_info *pnctrl_info = &clk_priv->audio_clk.pnctrl_info;
	int ret;

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->sleep);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
				__func__, ret);
			return;
		}
	}

	if (clk_priv->clk_src == AUDIO_EXT_CLK_LPASS) {
		clk_priv->clk_cfg.enable = 0;
		ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &clk_priv->clk_cfg);
		if (ret < 0)
			pr_err("%s: afe_set_lpass_clk_cfg failed, ret = %d\n",
				__func__, ret);
	}

	if (pnctrl_info->base)
		iowrite32(0, pnctrl_info->base);
}

static const struct clk_ops audio_ext_clk_ops = {
	.prepare = audio_ext_clk_prepare,
	.unprepare = audio_ext_clk_unprepare,
};

static struct audio_ext_clk audio_clk_array[] = {
	{
		.pnctrl_info = {NULL},
		.fact = {
			.mult = 1,
			.div = 1,
			.hw.init = &(struct clk_init_data){
				.name = "audio_ext_pmi_clk",
				.parent_names = (const char *[])
							{ "qpnp_clkdiv_1" },
				.num_parents = 1,
				.ops = &audio_ext_clk_ops,
			},
		},
	},
	{
		.pnctrl_info = {NULL},
		.fact = {
			.mult = 1,
			.div = 1,
			.hw.init = &(struct clk_init_data){
				.name = "audio_ext_pmi_lnbb_clk",
				.parent_names = (const char *[])
							{ "ln_bb_clk2" },
				.num_parents = 1,
				.ops = &clk_dummy_ops,
			},
		},
	},
	{
		.pnctrl_info = {NULL},
		.fact = {
			.mult = 1,
			.div = 1,
			.hw.init = &(struct clk_init_data){
				.name = "audio_lpass_mclk",
				.ops = &audio_ext_clk_ops,
			},
		},
	},
};

static int audio_get_pinctrl(struct platform_device *pdev)
{
	struct device *dev =  &pdev->dev;
	struct audio_ext_clk_priv *clk_priv = platform_get_drvdata(pdev);
	struct pinctrl_info *pnctrl_info;
	struct pinctrl *pinctrl;
	int ret;
	u32 reg;

	pnctrl_info = &clk_priv->audio_clk.pnctrl_info;
	if (pnctrl_info->pinctrl) {
		dev_err(dev, "%s: already requested before\n",
			__func__);
		return -EINVAL;
	}

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(dev, "%s: Unable to get pinctrl handle\n",
			__func__);
		return -EINVAL;
	}
	pnctrl_info->pinctrl = pinctrl;
	/* get all state handles from Device Tree */
	pnctrl_info->sleep = pinctrl_lookup_state(pinctrl, "sleep");
	if (IS_ERR(pnctrl_info->sleep)) {
		dev_err(dev, "%s: could not get sleep pinstate\n",
			__func__);
		goto err;
	}
	pnctrl_info->active = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(pnctrl_info->active)) {
		dev_err(dev, "%s: could not get active pinstate\n",
			__func__);
		goto err;
	}
	/* Reset the TLMM pins to a default state */
	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->sleep);
	if (ret) {
		dev_err(dev, "%s: Disable TLMM pins failed with %d\n",
			__func__, ret);
		goto err;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,mclk-clk-reg", &reg);
	if (ret < 0) {
		dev_dbg(dev, "%s: miss mclk reg\n", __func__);
	} else {
		pnctrl_info->base = ioremap(reg, sizeof(u32));
		if (pnctrl_info->base ==  NULL) {
			dev_err(dev, "%s ioremap failed\n", __func__);
			goto err;
		}
	}

	return 0;

err:
	devm_pinctrl_put(pnctrl_info->pinctrl);
	return -EINVAL;
}

static int audio_put_pinctrl(struct platform_device *pdev)
{
	struct audio_ext_clk_priv *clk_priv = platform_get_drvdata(pdev);
	struct pinctrl_info *pnctrl_info = NULL;

	pnctrl_info = &clk_priv->audio_clk.pnctrl_info;
	if (pnctrl_info && pnctrl_info->pinctrl) {
		devm_pinctrl_put(pnctrl_info->pinctrl);
		pnctrl_info->pinctrl = NULL;
	}

	return 0;
}

static int audio_get_clk_data(struct platform_device *pdev)
{
	int ret;
	struct clk *audio_clk;
	struct clk_hw *clkhw;
	struct clk_onecell_data *clk_data;
	struct audio_ext_clk_priv *clk_priv = platform_get_drvdata(pdev);

	clk_data = devm_kzalloc(&pdev->dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clk_num = 1;
	clk_data->clks = devm_kzalloc(&pdev->dev,
				sizeof(struct clk *),
				GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clkhw = &clk_priv->audio_clk.fact.hw;
	audio_clk = devm_clk_register(&pdev->dev, clkhw);
	if (IS_ERR(audio_clk)) {
		dev_err(&pdev->dev,
			"%s: clock register failed for clk_src = %d\\n",
			__func__, clk_priv->clk_src);
		ret = PTR_ERR(audio_clk);
		return ret;
	}
	clk_data->clks[0] = audio_clk;

	ret = of_clk_add_provider(pdev->dev.of_node,
			 of_clk_src_onecell_get, clk_data);
	if (ret)
		dev_err(&pdev->dev, "%s: clock add failed for clk_src = %d\n",
			__func__, clk_priv->clk_src);

	return ret;
}

static int audio_ref_clk_probe(struct platform_device *pdev)
{
	int ret;
	struct audio_ext_clk_priv *clk_priv;
	u32 clk_freq = 0, clk_id = 0, clk_src = 0, use_pinctrl = 0;

	clk_priv = devm_kzalloc(&pdev->dev, sizeof(*clk_priv), GFP_KERNEL);
	if (!clk_priv)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,codec-ext-clk-src",
			&clk_src);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not get clk source, ret = %d\n",
				__func__, ret);
		return ret;
	}

	if (clk_src >= AUDIO_EXT_CLK_MAX) {
		dev_err(&pdev->dev, "%s: Invalid clk source = %d\n",
				__func__, clk_src);
		return -EINVAL;
	}

	clk_priv->clk_src = clk_src;
	memcpy(&clk_priv->audio_clk, &audio_clk_array[clk_src],
		   sizeof(struct audio_ext_clk));

	/* Init lpass clk default values */
	clk_priv->clk_cfg.clk_set_minor_version =
					Q6AFE_LPASS_CLK_CONFIG_API_VERSION;
	clk_priv->clk_cfg.clk_id = Q6AFE_LPASS_CLK_ID_SPEAKER_I2S_OSR;
	clk_priv->clk_cfg.clk_freq_in_hz = Q6AFE_LPASS_OSR_CLK_9_P600_MHZ;
	clk_priv->clk_cfg.clk_attri = Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO;

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,codec-lpass-ext-clk-freq",
			&clk_freq);
	if (!ret)
		clk_priv->clk_cfg.clk_freq_in_hz = clk_freq;

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,codec-lpass-clk-id",
			&clk_id);
	if (!ret)
		clk_priv->clk_cfg.clk_id = clk_id;

	dev_dbg(&pdev->dev, "%s: ext-clk freq: %d, lpass clk_id: %d, clk_src: %d\n",
			__func__, clk_priv->clk_cfg.clk_freq_in_hz,
			clk_priv->clk_cfg.clk_id, clk_priv->clk_src);
	platform_set_drvdata(pdev, clk_priv);

	/*
	 * property qcom,use-pinctrl to be defined in DTSI to val 1
	 * for clock nodes using pinctrl
	 */
	of_property_read_u32(pdev->dev.of_node, "qcom,use-pinctrl",
			     &use_pinctrl);
	dev_dbg(&pdev->dev, "%s: use-pinctrl : %d\n",
		__func__, use_pinctrl);

	if (use_pinctrl) {
		ret = audio_get_pinctrl(pdev);
		if (ret) {
			dev_err(&pdev->dev, "%s: Parsing PMI pinctrl failed\n",
				__func__);
			return ret;
		}
	}

	ret = audio_get_clk_data(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: clk_init is failed\n",
			__func__);
		audio_put_pinctrl(pdev);
		return ret;
	}

	return 0;
}

static int audio_ref_clk_remove(struct platform_device *pdev)
{
	audio_put_pinctrl(pdev);

	return 0;
}

static const struct of_device_id audio_ref_clk_match[] = {
	{.compatible = "qcom,audio-ref-clk"},
	{}
};
MODULE_DEVICE_TABLE(of, audio_ref_clk_match);

static struct platform_driver audio_ref_clk_driver = {
	.driver = {
		.name = "audio-ref-clk",
		.owner = THIS_MODULE,
		.of_match_table = audio_ref_clk_match,
	},
	.probe = audio_ref_clk_probe,
	.remove = audio_ref_clk_remove,
};

int audio_ref_clk_platform_init(void)
{
	return platform_driver_register(&audio_ref_clk_driver);
}

void audio_ref_clk_platform_exit(void)
{
	platform_driver_unregister(&audio_ref_clk_driver);
}

MODULE_DESCRIPTION("Audio Ref Up Clock module platform driver");
MODULE_LICENSE("GPL v2");
