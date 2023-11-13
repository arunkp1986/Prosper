// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>

#include "komeda_dev.h"
#include "komeda_kms.h"

/**
 * komeda_crtc_atomic_check - build display output data flow
 * @crtc: DRM crtc
 * @state: the crtc state object
 *
 * crtc_atomic_check is the final check stage, so beside build a display data
 * pipeline according to the crtc_state, but still needs to release or disable
 * the unclaimed pipeline resources.
 *
 * RETURNS:
 * Zero for success or -errno
 */
static int
komeda_crtc_atomic_check(struct drm_crtc *crtc,
			 struct drm_crtc_state *state)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(state);
	int err;

	if (state->active) {
		err = komeda_build_display_data_flow(kcrtc, kcrtc_st);
		if (err)
			return err;
	}

	/* release unclaimed pipeline resources */
	err = komeda_release_unclaimed_resources(kcrtc->master, kcrtc_st);
	if (err)
		return err;

	return 0;
}

static u32 komeda_calc_mclk(struct komeda_crtc_state *kcrtc_st)
{
	unsigned long mclk = kcrtc_st->base.adjusted_mode.clock * 1000;

	return mclk;
}

/* For active a crtc, mainly need two parts of preparation
 * 1. adjust display operation mode.
 * 2. enable needed clk
 */
static int
komeda_crtc_prepare(struct komeda_crtc *kcrtc)
{
	struct komeda_dev *mdev = kcrtc->base.dev->dev_private;
	struct komeda_pipeline *master = kcrtc->master;
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(kcrtc->base.state);
	unsigned long pxlclk_rate = kcrtc_st->base.adjusted_mode.clock * 1000;
	u32 new_mode;
	int err;

	mutex_lock(&mdev->lock);

	new_mode = mdev->dpmode | BIT(master->id);
	if (WARN_ON(new_mode == mdev->dpmode)) {
		err = 0;
		goto unlock;
	}

	err = mdev->funcs->change_opmode(mdev, new_mode);
	if (err) {
		DRM_ERROR("failed to change opmode: 0x%x -> 0x%x.\n,",
			  mdev->dpmode, new_mode);
		goto unlock;
	}

	mdev->dpmode = new_mode;
	/* Only need to enable mclk on single display mode, but no need to
	 * enable mclk it on dual display mode, since the dual mode always
	 * switch from single display mode, the mclk already enabled, no need
	 * to enable it again.
	 */
	if (new_mode != KOMEDA_MODE_DUAL_DISP) {
		err = clk_set_rate(mdev->mclk, komeda_calc_mclk(kcrtc_st));
		if (err)
			DRM_ERROR("failed to set mclk.\n");
		err = clk_prepare_enable(mdev->mclk);
		if (err)
			DRM_ERROR("failed to enable mclk.\n");
	}

	err = clk_prepare_enable(master->aclk);
	if (err)
		DRM_ERROR("failed to enable axi clk for pipe%d.\n", master->id);
	err = clk_set_rate(master->pxlclk, pxlclk_rate);
	if (err)
		DRM_ERROR("failed to set pxlclk for pipe%d\n", master->id);
	err = clk_prepare_enable(master->pxlclk);
	if (err)
		DRM_ERROR("failed to enable pxl clk for pipe%d.\n", master->id);

unlock:
	mutex_unlock(&mdev->lock);

	return err;
}

static int
komeda_crtc_unprepare(struct komeda_crtc *kcrtc)
{
	struct komeda_dev *mdev = kcrtc->base.dev->dev_private;
	struct komeda_pipeline *master = kcrtc->master;
	u32 new_mode;
	int err;

	mutex_lock(&mdev->lock);

	new_mode = mdev->dpmode & (~BIT(master->id));

	if (WARN_ON(new_mode == mdev->dpmode)) {
		err = 0;
		goto unlock;
	}

	err = mdev->funcs->change_opmode(mdev, new_mode);
	if (err) {
		DRM_ERROR("failed to change opmode: 0x%x -> 0x%x.\n,",
			  mdev->dpmode, new_mode);
		goto unlock;
	}

	mdev->dpmode = new_mode;

	clk_disable_unprepare(master->pxlclk);
	clk_disable_unprepare(master->aclk);
	if (new_mode == KOMEDA_MODE_INACTIVE)
		clk_disable_unprepare(mdev->mclk);

unlock:
	mutex_unlock(&mdev->lock);

	return err;
}

void komeda_crtc_handle_event(struct komeda_crtc   *kcrtc,
			      struct komeda_events *evts)
{
	struct drm_crtc *crtc = &kcrtc->base;
	u32 events = evts->pipes[kcrtc->master->id];

	if (events & KOMEDA_EVENT_VSYNC)
		drm_crtc_handle_vblank(crtc);

	/* will handle it together with the write back support */
	if (events & KOMEDA_EVENT_EOW)
		DRM_DEBUG("EOW.\n");

	if (events & KOMEDA_EVENT_FLIP) {
		unsigned long flags;
		struct drm_pending_vblank_event *event;

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		if (kcrtc->disable_done) {
			complete_all(kcrtc->disable_done);
			kcrtc->disable_done = NULL;
		} else if (crtc->state->event) {
			event = crtc->state->event;
			/*
			 * Consume event before notifying drm core that flip
			 * happened.
			 */
			crtc->state->event = NULL;
			drm_crtc_send_vblank_event(crtc, event);
		} else {
			DRM_WARN("CRTC[%d]: FLIP happen but no pending commit.\n",
				 drm_crtc_index(&kcrtc->base));
		}
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void
komeda_crtc_do_flush(struct drm_crtc *crtc,
		     struct drm_crtc_state *old)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(crtc->state);
	struct komeda_dev *mdev = kcrtc->base.dev->dev_private;
	struct komeda_pipeline *master = kcrtc->master;

	DRM_DEBUG_ATOMIC("CRTC%d_FLUSH: active_pipes: 0x%x, affected: 0x%x.\n",
			 drm_crtc_index(crtc),
			 kcrtc_st->active_pipes, kcrtc_st->affected_pipes);

	/* step 1: update the pipeline/component state to HW */
	if (has_bit(master->id, kcrtc_st->affected_pipes))
		komeda_pipeline_update(master, old->state);

	/* step 2: notify the HW to kickoff the update */
	mdev->funcs->flush(mdev, master->id, kcrtc_st->active_pipes);
}

static void
komeda_crtc_atomic_enable(struct drm_crtc *crtc,
			  struct drm_crtc_state *old)
{
	komeda_crtc_prepare(to_kcrtc(crtc));
	drm_crtc_vblank_on(crtc);
	komeda_crtc_do_flush(crtc, old);
}

static void
komeda_crtc_atomic_disable(struct drm_crtc *crtc,
			   struct drm_crtc_state *old)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *old_st = to_kcrtc_st(old);
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_pipeline *master = kcrtc->master;
	struct completion *disable_done = &crtc->state->commit->flip_done;
	struct completion temp;
	int timeout;

	DRM_DEBUG_ATOMIC("CRTC%d_DISABLE: active_pipes: 0x%x, affected: 0x%x.\n",
			 drm_crtc_index(crtc),
			 old_st->active_pipes, old_st->affected_pipes);

	if (has_bit(master->id, old_st->active_pipes))
		komeda_pipeline_disable(master, old->state);

	/* crtc_disable has two scenarios according to the state->active switch.
	 * 1. active -> inactive
	 *    this commit is a disable commit. and the commit will be finished
	 *    or done after the disable operation. on this case we can directly
	 *    use the crtc->state->event to tracking the HW disable operation.
	 * 2. active -> active
	 *    the crtc->commit is not for disable, but a modeset operation when
	 *    crtc is active, such commit actually has been completed by 3
	 *    DRM operations:
	 *    crtc_disable, update_planes(crtc_flush), crtc_enable
	 *    so on this case the crtc->commit is for the whole process.
	 *    we can not use it for tracing the disable, we need a temporary
	 *    flip_done for tracing the disable. and crtc->state->event for
	 *    the crtc_enable operation.
	 *    That's also the reason why skip modeset commit in
	 *    komeda_crtc_atomic_flush()
	 */
	if (crtc->state->active) {
		struct komeda_pipeline_state *pipe_st;
		/* clear the old active_comps to zero */
		pipe_st = komeda_pipeline_get_old_state(master, old->state);
		pipe_st->active_comps = 0;

		init_completion(&temp);
		kcrtc->disable_done = &temp;
		disable_done = &temp;
	}

	mdev->funcs->flush(mdev, master->id, 0);

	/* wait the disable take affect.*/
	timeout = wait_for_completion_timeout(disable_done, HZ);
	if (timeout == 0) {
		DRM_ERROR("disable pipeline%d timeout.\n", kcrtc->master->id);
		if (crtc->state->active) {
			unsigned long flags;

			spin_lock_irqsave(&crtc->dev->event_lock, flags);
			kcrtc->disable_done = NULL;
			spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		}
	}

	drm_crtc_vblank_off(crtc);
	komeda_crtc_unprepare(kcrtc);
}

static void
komeda_crtc_atomic_flush(struct drm_crtc *crtc,
			 struct drm_crtc_state *old)
{
	/* commit with modeset will be handled in enable/disable */
	if (drm_atomic_crtc_needs_modeset(crtc->state))
		return;

	komeda_crtc_do_flush(crtc, old);
}

static enum drm_mode_status
komeda_crtc_mode_valid(struct drm_crtc *crtc, const struct drm_display_mode *m)
{
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_pipeline *master = kcrtc->master;
	long mode_clk, pxlclk;

	if (m->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	/* main clock/AXI clk must be faster than pxlclk*/
	mode_clk = m->clock * 1000;
	pxlclk = clk_round_rate(master->pxlclk, mode_clk);
	if (pxlclk != mode_clk) {
		DRM_DEBUG_ATOMIC("pxlclk doesn't support %ld Hz\n", mode_clk);

		return MODE_NOCLOCK;
	}

	if (clk_round_rate(mdev->mclk, mode_clk) < pxlclk) {
		DRM_DEBUG_ATOMIC("mclk can't satisfy the requirement of %s-clk: %ld.\n",
				 m->name, pxlclk);

		return MODE_CLOCK_HIGH;
	}

	if (clk_round_rate(master->aclk, mode_clk) < pxlclk) {
		DRM_DEBUG_ATOMIC("aclk can't satisfy the requirement of %s-clk: %ld.\n",
				 m->name, pxlclk);

		return MODE_CLOCK_HIGH;
	}

	return MODE_OK;
}

static bool komeda_crtc_mode_fixup(struct drm_crtc *crtc,
				   const struct drm_display_mode *m,
				   struct drm_display_mode *adjusted_mode)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_pipeline *master = kcrtc->master;
	long mode_clk = m->clock * 1000;

	adjusted_mode->clock = clk_round_rate(master->pxlclk, mode_clk) / 1000;

	return true;
}

static const struct drm_crtc_helper_funcs komeda_crtc_helper_funcs = {
	.atomic_check	= komeda_crtc_atomic_check,
	.atomic_flush	= komeda_crtc_atomic_flush,
	.atomic_enable	= komeda_crtc_atomic_enable,
	.atomic_disable	= komeda_crtc_atomic_disable,
	.mode_valid	= komeda_crtc_mode_valid,
	.mode_fixup	= komeda_crtc_mode_fixup,
};

static void komeda_crtc_reset(struct drm_crtc *crtc)
{
	struct komeda_crtc_state *state;

	if (crtc->state)
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

	kfree(to_kcrtc_st(crtc->state));
	crtc->state = NULL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state) {
		crtc->state = &state->base;
		crtc->state->crtc = crtc;
	}
}

static struct drm_crtc_state *
komeda_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct komeda_crtc_state *old = to_kcrtc_st(crtc->state);
	struct komeda_crtc_state *new;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &new->base);

	new->affected_pipes = old->active_pipes;

	return &new->base;
}

static void komeda_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					     struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_kcrtc_st(state));
}

static int komeda_crtc_vblank_enable(struct drm_crtc *crtc)
{
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);

	mdev->funcs->on_off_vblank(mdev, kcrtc->master->id, true);
	return 0;
}

static void komeda_crtc_vblank_disable(struct drm_crtc *crtc)
{
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);

	mdev->funcs->on_off_vblank(mdev, kcrtc->master->id, false);
}

static const struct drm_crtc_funcs komeda_crtc_funcs = {
	.gamma_set		= drm_atomic_helper_legacy_gamma_set,
	.destroy		= drm_crtc_cleanup,
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= komeda_crtc_reset,
	.atomic_duplicate_state	= komeda_crtc_atomic_duplicate_state,
	.atomic_destroy_state	= komeda_crtc_atomic_destroy_state,
	.enable_vblank		= komeda_crtc_vblank_enable,
	.disable_vblank		= komeda_crtc_vblank_disable,
};

int komeda_kms_setup_crtcs(struct komeda_kms_dev *kms,
			   struct komeda_dev *mdev)
{
	struct komeda_crtc *crtc;
	struct komeda_pipeline *master;
	char str[16];
	int i;

	kms->n_crtcs = 0;

	for (i = 0; i < mdev->n_pipelines; i++) {
		crtc = &kms->crtcs[kms->n_crtcs];
		master = mdev->pipelines[i];

		crtc->master = master;
		crtc->slave  = NULL;

		if (crtc->slave)
			sprintf(str, "pipe-%d", crtc->slave->id);
		else
			sprintf(str, "None");

		DRM_INFO("crtc%d: master(pipe-%d) slave(%s) output: %s.\n",
			 kms->n_crtcs, master->id, str,
			 master->of_output_dev ?
			 master->of_output_dev->full_name : "None");

		kms->n_crtcs++;
	}

	return 0;
}

static struct drm_plane *
get_crtc_primary(struct komeda_kms_dev *kms, struct komeda_crtc *crtc)
{
	struct komeda_plane *kplane;
	struct drm_plane *plane;

	drm_for_each_plane(plane, &kms->base) {
		if (plane->type != DRM_PLANE_TYPE_PRIMARY)
			continue;

		kplane = to_kplane(plane);
		/* only master can be primary */
		if (kplane->layer->base.pipeline == crtc->master)
			return plane;
	}

	return NULL;
}

static int komeda_crtc_add(struct komeda_kms_dev *kms,
			   struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	int err;

	err = drm_crtc_init_with_planes(&kms->base, crtc,
					get_crtc_primary(kms, kcrtc), NULL,
					&komeda_crtc_funcs, NULL);
	if (err)
		return err;

	drm_crtc_helper_add(crtc, &komeda_crtc_helper_funcs);
	drm_crtc_vblank_reset(crtc);

	crtc->port = kcrtc->master->of_output_port;

	return 0;
}

int komeda_kms_add_crtcs(struct komeda_kms_dev *kms, struct komeda_dev *mdev)
{
	int i, err;

	for (i = 0; i < kms->n_crtcs; i++) {
		err = komeda_crtc_add(kms, &kms->crtcs[i]);
		if (err)
			return err;
	}

	return 0;
}
