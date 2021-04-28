/*
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#define pr_fmt(fmt) "sipa_rm: %s " fmt, __func__

#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/sipa.h>
#include "sipa_rm.h"
#include "sipa_rm_res.h"
#include "sipa_rm_peers_list.h"
#include "sipa_rm_dep_graph.h"

struct sipa_rm_profile_vote_type {
	enum sipa_voltage_level volt[SIPA_RM_RES_MAX];
	enum sipa_voltage_level curr_volt;
	u32 bw_prods[SIPA_RM_RES_PROD_MAX];
	u32 bw_cons[SIPA_RM_RES_CONS_MAX];
	u32 curr_bw;
};

/**
 * struct sipa_rm_context_type - SIPA RM context
 *	data
 * @dep_graph - dependency graph
 * @sipa_rm_wq - work queue for SIPA RM
 * @sipa_rm_lock - spinlock for mutual exclusion
 *
 */
struct sipa_rm_context_type {
	struct sipa_rm_dep_graph *dep_graph;
	struct workqueue_struct *sipa_rm_wq;
	spinlock_t sipa_rm_lock;
};

/**
 * struct sipa_rm_it_private - SIPA RM Inactivity Timer private
 *	data
 * @initied: indicates if instance was initialized
 * @resource_name - resource name
 * @lock - spinlock for mutual exclusion
 * @work: delayed work object for running delayed releas
 *	function
 * @resource_requested: boolean flag indicates if resource was requested
 * @reschedule_work: boolean flag indicates to not release and to
 *	reschedule the release work.
 * @work_in_progress: boolean flag indicates is release work was scheduled.
 * @need_request: boolean flag indicates is need to call request function.
 * @jiffies: number of jiffies for timeout
 */
struct sipa_rm_it_private {
	bool initied;
	enum sipa_rm_res_id resource_name;
	spinlock_t lock;
	struct delayed_work work;
	bool resource_requested;
	bool reschedule_work;
	bool work_in_progress;
	bool need_request;
	unsigned long jiffies;
};

static struct sipa_rm_context_type *sipa_rm_ctx;

static const char *resource_name_to_str[SIPA_RM_RES_MAX] = {
	__stringify(SIPA_RM_RES_PROD_IPA),
	__stringify(SIPA_RM_RES_PROD_REMOTE_IPA),
	__stringify(SIPA_RM_RES_PROD_PAM_IPA),
	__stringify(SIPA_RM_RES_PROD_PAM_U3),
	__stringify(SIPA_RM_RES_PROD_PAM_SDIO),
	__stringify(SIPA_RM_RES_PROD_PAM_WIFI),
	__stringify(SIPA_RM_RES_PROD_PCIE3),
	__stringify(SIPA_RM_RES_PROD_USB3),
	__stringify(SIPA_RM_RES_PROD_SDSLAVE),
	__stringify(SIPA_RM_RES_PROD_PCIE2),
	__stringify(SIPA_RM_RES_PROD_PCIE_EP),
	__stringify(SIPA_RM_RES_PROD_MINI_AP),
	__stringify(SIPA_RM_RES_PROD_AP),
	__stringify(SIPA_RM_RES_PROD_CP),
	__stringify(SIPA_RM_RES_CONS_WWAN_UL),
	__stringify(SIPA_RM_RES_CONS_WWAN_DL),
	__stringify(SIPA_RM_RES_CONS_WLAN),
	__stringify(SIPA_RM_RES_CONS_USB),
};

static struct sipa_rm_it_private sipa_rm_it_handles[SIPA_RM_RES_MAX];

/**
 * sipa_rm_res_str() - returns string that represent the resource
 * @resource_name: [in] resource name
 */
const char *sipa_rm_res_str(enum sipa_rm_res_id resource_name)
{
	if (resource_name < 0 || resource_name >= SIPA_RM_RES_MAX)
		return "INVALID RESOURCE";

	return resource_name_to_str[resource_name];
};

static void sipa_rm_inactivity_timer_func(struct work_struct *work)
{
	struct sipa_rm_it_private *me = container_of(to_delayed_work(work),
					struct sipa_rm_it_private,
					work);
	unsigned long flags;

	pr_debug("timer expired for resource %d!\n",
		 me->resource_name);

	spin_lock_irqsave(
		&sipa_rm_it_handles[me->resource_name].lock, flags);
	if (sipa_rm_it_handles[me->resource_name].reschedule_work) {
		pr_debug("setting delayed work\n");
		sipa_rm_it_handles[me->resource_name].reschedule_work = false;
		queue_delayed_work(system_unbound_wq,
				   &sipa_rm_it_handles[me->resource_name].work,
				   sipa_rm_it_handles[me->resource_name].jiffies);
	} else if (sipa_rm_it_handles[me->resource_name].resource_requested) {
		pr_debug("not calling release\n");
		sipa_rm_it_handles[me->resource_name].work_in_progress = false;
	} else {
		pr_debug("calling release_resource on resource %d!\n",
			 me->resource_name);
		sipa_rm_release_resource(me->resource_name);
		sipa_rm_it_handles[me->resource_name].need_request = true;
		sipa_rm_it_handles[me->resource_name].work_in_progress = false;
	}
	spin_unlock_irqrestore(
		&sipa_rm_it_handles[me->resource_name].lock, flags);
}

/**
 * sipa_rm_is_initialized() - check sipa_rm is initialized
 *
 * Returns: true on initialized
 *
 * This function is called to check sipa_rm is initialized
 */
bool sipa_rm_is_initialized(void)
{
	return !!sipa_rm_ctx;
}

/**
 * sipa_rm_create_resource() - create resource
 * @create_params: [in] parameters needed
 *                  for resource initialization
 *
 * Returns: 0 on success, negative on failure
 *
 * This function is called by SIPA RM client to initialize client's resources.
 * This API should be called before any other SIPA RM API on a given resource
 * name.
 */
int sipa_rm_create_resource(struct sipa_rm_create_params *create_params)
{
	struct sipa_rm_resource *resource;
	unsigned long flags;
	int result;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	if (!create_params) {
		pr_err("invalid args\n");
		return -EINVAL;
	}
	pr_debug("%s\n", sipa_rm_res_str(create_params->name));

	if (create_params->floor_voltage < 0 ||
	    create_params->floor_voltage >= SIPA_VOLTAGE_MAX) {
		pr_err("invalid voltage %d\n",
		       create_params->floor_voltage);
		return -EINVAL;
	}

	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   create_params->name,
					   &resource) == 0) {
		pr_err("resource already exists\n");
		spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);
		return -EEXIST;
	}
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	result = sipa_rm_resource_create(create_params,
					 &resource);
	if (result) {
		pr_err("sipa_rm_resource_create() failed\n");
		return result;
	}

	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	result = sipa_rm_dep_graph_add(sipa_rm_ctx->dep_graph, resource);
	if (result) {
		pr_err("sipa_rm_dep_graph_add() failed\n");
		sipa_rm_resource_delete(resource);
	}
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_create_resource);

int sipa_rm_delete_resource(enum sipa_rm_res_id resource_name)
{
	struct sipa_rm_resource *resource;
	unsigned long flags;
	int result;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	pr_debug("%s\n", sipa_rm_res_str(resource_name));
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   resource_name,
					   &resource) != 0) {
		pr_err("resource does not exist\n");
		result = -EINVAL;
		goto bail;
	}
	result = sipa_rm_resource_delete(resource);
	if (result) {
		pr_err("sipa_rm_resource_delete() failed\n");
		goto bail;
	}
	result = sipa_rm_dep_graph_remove(sipa_rm_ctx->dep_graph,
					  resource_name);
	if (result) {
		pr_err("sipa_rm_dep_graph_remove() failed\n");
		goto bail;
	}
bail:
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_delete_resource);

int sipa_rm_register(enum sipa_rm_res_id res_id,
		     struct sipa_rm_register_params *reg_params)
{
	int result;
	unsigned long flags;
	struct sipa_rm_resource *resource;

	if (!SIPA_RM_RESORCE_IS_CONS(res_id)) {
		pr_err("can be called on CONS only\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   res_id,
					   &resource) != 0) {
		pr_err("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = sipa_rm_resource_consumer_register(
			 (struct sipa_rm_res_cons *)resource,
			 reg_params,
			 true);
bail:
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_register);

int sipa_rm_deregister(enum sipa_rm_res_id res_id,
		       struct sipa_rm_register_params *reg_params)
{
	int result;
	unsigned long flags;
	struct sipa_rm_resource *resource;

	if (!SIPA_RM_RESORCE_IS_CONS(res_id)) {
		pr_err("can be called on CONS only\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   res_id,
					   &resource) != 0) {
		pr_err("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = sipa_rm_resource_consumer_deregister(
			 (struct sipa_rm_res_cons *)resource,
			 reg_params);
bail:
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_deregister);


int sipa_rm_add_dependency(enum sipa_rm_res_id cons,
			   enum sipa_rm_res_id prod)
{
	unsigned long flags;
	int result;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	pr_debug("%s -> %s\n", sipa_rm_res_str(cons),
		 sipa_rm_res_str(prod));
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	result = sipa_rm_dep_graph_add_dependency(
			 sipa_rm_ctx->dep_graph,
			 cons,
			 prod,
			 false);
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_add_dependency);

int sipa_rm_add_dependency_sync(enum sipa_rm_res_id cons,
				enum sipa_rm_res_id prod)
{
	int result;
	struct sipa_rm_resource *producer;
	unsigned long time;
	unsigned long flags;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	pr_debug("%s -> %s\n", sipa_rm_res_str(cons),
		 sipa_rm_res_str(prod));
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	result = sipa_rm_dep_graph_add_dependency(
			 sipa_rm_ctx->dep_graph,
			 cons,
			 prod,
			 false);
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (result == -EINPROGRESS) {
		sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					       prod,
					       &producer);
		pr_info("%s waits for GRANT of %s.\n",
			sipa_rm_res_str(cons),
			sipa_rm_res_str(prod));
		time = wait_for_completion_timeout(
			       &((struct sipa_rm_res_prod *)producer)->
			       request_prod_in_progress,
			       SIPA_RM_MAX_WAIT_TIME);
		result = 0;
		if (!time) {
			pr_err("TIMEOUT waiting for %s GRANT event.",
			       sipa_rm_res_str(prod));
			result = -ETIMEDOUT;
		} else {
			pr_info("%s waited for %s GRANT %lu time.\n",
				sipa_rm_res_str(cons),
				sipa_rm_res_str(prod),
				time);
		}
	}

	return result;
}
EXPORT_SYMBOL(sipa_rm_add_dependency_sync);


int sipa_rm_delete_dependency(enum sipa_rm_res_id cons,
			      enum sipa_rm_res_id pord)
{
	unsigned long flags;
	int result;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	pr_debug("%s -> %s\n", sipa_rm_res_str(cons),
		 sipa_rm_res_str(pord));
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	result = sipa_rm_dep_graph_delete_dependency(
			 sipa_rm_ctx->dep_graph,
			 cons,
			 pord,
			 false);
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_delete_dependency);


int sipa_rm_request_resource(enum sipa_rm_res_id res_id)
{
	struct sipa_rm_resource *resource;
	unsigned long flags;
	int result;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	if (!SIPA_RM_RESORCE_IS_CONS(res_id)) {
		pr_err("can be called on CONS only\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   res_id,
					   &resource) != 0) {
		pr_err("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = sipa_rm_resource_consumer_request(
			 (struct sipa_rm_res_cons *)resource);

bail:
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_request_resource);

/**
 * sipa_rm_release_resource() - release resource
 * @resource_name: [in] name of the requested resource
 *
 * Returns: 0 on success, negative on failure
 *
 * All registered callbacks are called with IPA_RM_RESOURCE_RELEASED
 * on successful completion of this operation.
 */
int sipa_rm_release_resource(enum sipa_rm_res_id resource_name)
{
	unsigned long flags;
	struct sipa_rm_resource *resource;
	int result;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	if (!SIPA_RM_RESORCE_IS_CONS(resource_name)) {
		pr_err("can be called on CONS only\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   resource_name,
					   &resource) != 0) {
		pr_err("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = sipa_rm_resource_consumer_release(
			 (struct sipa_rm_res_cons *)resource);

bail:
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
EXPORT_SYMBOL(sipa_rm_release_resource);

/**
 * sipa_rm_notify_completion() -
 *	consumer driver notification for
 *	request_resource / release_resource operations
 *	completion
 * @event: notified event
 * @resource_name: resource name
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_notify_completion(enum sipa_rm_event event,
			      enum sipa_rm_res_id resource_name)
{
	unsigned long flags;

	if (unlikely(!sipa_rm_ctx)) {
		pr_err("SIPA RM was not initialized\n");
		return -EINVAL;
	}

	pr_debug("event %d on %s\n", event,
		 sipa_rm_res_str(resource_name));
	if (!SIPA_RM_RESORCE_IS_PROD(resource_name)) {
		pr_err("can be called on CONS only\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	sipa_rm_wq_send_cmd(SIPA_RM_WQ_RESOURCE_CB,
			    resource_name,
			    event);
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock,
			       flags);
	return 0;
}
EXPORT_SYMBOL(sipa_rm_notify_completion);

/**
 * sipa_rm_inactivity_timer_initsipa_rm_inactivity_timer_init() -
 *         Init function for SIPA RM
 * inactivity timer. This function shall be called prior calling
 * any other API of SIPA RM inactivity timer.
 *
 * @resource_name: Resource name. @see sipa_rm.h
 * @msecs: time in miliseccond, that SIPA RM inactivity timer
 * shall wait prior calling to sipa_rm_release_resource().
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int sipa_rm_inactivity_timer_init(enum sipa_rm_res_id resource_name,
				  unsigned long msecs)
{
	pr_debug("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= SIPA_RM_RES_MAX) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (sipa_rm_it_handles[resource_name].initied) {
		pr_err("resource %d already inited\n",
		       resource_name);
		return -EINVAL;
	}

	spin_lock_init(&sipa_rm_it_handles[resource_name].lock);
	sipa_rm_it_handles[resource_name].resource_name = resource_name;
	sipa_rm_it_handles[resource_name].jiffies = msecs_to_jiffies(msecs);
	sipa_rm_it_handles[resource_name].resource_requested = false;
	sipa_rm_it_handles[resource_name].reschedule_work = false;
	sipa_rm_it_handles[resource_name].work_in_progress = false;
	sipa_rm_it_handles[resource_name].need_request = true;

	INIT_DELAYED_WORK(&sipa_rm_it_handles[resource_name].work,
			  sipa_rm_inactivity_timer_func);
	sipa_rm_it_handles[resource_name].initied = 1;

	return 0;
}
EXPORT_SYMBOL(sipa_rm_inactivity_timer_init);

/**
 * sipa_rm_inactivity_timer_destroy() - De-Init function for SIPA
 * RM inactivity timer.
 *
 * @resource_name: Resource name. @see sipa_rm.h
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int sipa_rm_inactivity_timer_destroy(enum sipa_rm_res_id resource_name)
{
	pr_debug("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= SIPA_RM_RES_MAX) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (!sipa_rm_it_handles[resource_name].initied) {
		pr_err("resource %d already inited\n",
		       resource_name);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&sipa_rm_it_handles[resource_name].work);

	memset(&sipa_rm_it_handles[resource_name], 0,
	       sizeof(struct sipa_rm_it_private));

	return 0;
}
EXPORT_SYMBOL(sipa_rm_inactivity_timer_destroy);

/**
 * sipa_rm_inactivity_timer_request_resource() - Same as
 * sipa_rm_request_resource(), with a difference that calling to
 * this function will also cancel the inactivity timer, if
 * sipa_rm_inactivity_timer_release_resource() was called earlier.
 *
 * @resource_name: Resource name. @see sipa_rm.h
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int sipa_rm_inactivity_timer_request_resource(enum sipa_rm_res_id resource_name)
{
	int ret = 0;
	unsigned long flags;

	pr_debug(" resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= SIPA_RM_RES_MAX) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (!sipa_rm_it_handles[resource_name].initied) {
		pr_err("Not initialized\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&sipa_rm_it_handles[resource_name].lock, flags);
	sipa_rm_it_handles[resource_name].resource_requested = true;
	if (sipa_rm_it_handles[resource_name].need_request) {
		sipa_rm_it_handles[resource_name].need_request = false;
		ret = sipa_rm_request_resource(resource_name);
	}
	spin_unlock_irqrestore(&sipa_rm_it_handles[resource_name].lock, flags);

	return ret;
}
EXPORT_SYMBOL(sipa_rm_inactivity_timer_request_resource);

/**
 * sipa_rm_inactivity_timer_release_resource() - Sets the
 * inactivity timer to the timeout set by
 * sipa_rm_inactivity_timer_init(). When the timeout expires, SIPA
 * RM inactivity timer will call to sipa_rm_release_resource().
 * If a call to sipa_rm_inactivity_timer_request_resource() was
 * made BEFORE the timeout has expired, rge timer will be
 * cancelled.
 *
 * @resource_name: Resource name. @see sipa_rm.h
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int sipa_rm_inactivity_timer_release_resource(enum sipa_rm_res_id resource_name)
{
	unsigned long flags;

	pr_debug("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= SIPA_RM_RES_MAX) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (!sipa_rm_it_handles[resource_name].initied) {
		pr_err("Not initialized\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&sipa_rm_it_handles[resource_name].lock, flags);
	sipa_rm_it_handles[resource_name].resource_requested = false;
	if (sipa_rm_it_handles[resource_name].work_in_progress) {
		pr_debug("Timer already set, no sched again %d\n",
			 resource_name);
		sipa_rm_it_handles[resource_name].reschedule_work = true;
		spin_unlock_irqrestore(
			&sipa_rm_it_handles[resource_name].lock, flags);
		return 0;
	}
	sipa_rm_it_handles[resource_name].work_in_progress = true;
	sipa_rm_it_handles[resource_name].reschedule_work = false;
	pr_debug("setting delayed work\n");
	queue_delayed_work(system_unbound_wq,
			   &sipa_rm_it_handles[resource_name].work,
			   sipa_rm_it_handles[resource_name].jiffies);
	spin_unlock_irqrestore(&sipa_rm_it_handles[resource_name].lock, flags);

	return 0;
}
EXPORT_SYMBOL(sipa_rm_inactivity_timer_release_resource);

static void sipa_rm_wq_handler(struct work_struct *work)
{
	unsigned long flags;
	struct sipa_rm_resource *resource;
	struct sipa_rm_wq_work_type *sipa_rm_work =
		container_of(work,
			     struct sipa_rm_wq_work_type,
			     work);
	pr_debug("%s cmd=%d event=%d\n",
		 sipa_rm_res_str(sipa_rm_work->resource_name),
		 sipa_rm_work->wq_cmd,
		 sipa_rm_work->event);
	switch (sipa_rm_work->wq_cmd) {
	case SIPA_RM_WQ_NOTIFY_CONS:
		if (!SIPA_RM_RESORCE_IS_CONS(sipa_rm_work->resource_name)) {
			pr_err("resource is not PROD\n");
			return;
		}
		spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
		if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
						   sipa_rm_work->resource_name,
						   &resource) != 0) {
			pr_err("resource does not exists\n");
			spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock,
					       flags);
			return;
		}
		sipa_rm_resource_consumer_notify_clients(
			(struct sipa_rm_res_cons *)resource,
			sipa_rm_work->event);
		spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);
		break;
	case SIPA_RM_WQ_NOTIFY_PROD:
		break;
	case SIPA_RM_WQ_RESOURCE_CB:
		spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
		if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
						   sipa_rm_work->resource_name,
						   &resource) != 0) {
			pr_err("resource does not exists\n");
			spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock,
					       flags);
			return;
		}
		sipa_rm_resource_producer_handle_cb(
			(struct sipa_rm_res_prod *)resource,
			sipa_rm_work->event);
		spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);
		break;
	default:
		break;
	}
}

int sipa_rm_wq_send_cmd(enum sipa_rm_wq_cmd wq_cmd,
			enum sipa_rm_res_id resource_name,
			enum sipa_rm_event event)
{
	struct sipa_rm_resource *resource;
	struct sipa_rm_wq_work_type *work;

	if (sipa_rm_dep_graph_get_resource(sipa_rm_ctx->dep_graph,
					   resource_name,
					   &resource) != 0) {
		pr_err("resource does not exists\n");
		return -EINVAL;
	}

	work = &resource->work;
	INIT_WORK((struct work_struct *)work, sipa_rm_wq_handler);
	work->wq_cmd = wq_cmd;
	work->resource_name = resource_name;
	work->event = event;

	return queue_work(sipa_rm_ctx->sipa_rm_wq,
			    (struct work_struct *)work);
}

/**
 * sipa_rm_init() - initialize SIPA RM component
 *
 * Returns: 0 on success, negative otherwise
 */
int sipa_rm_init(void)
{
	int result;

	sipa_rm_ctx = kzalloc(sizeof(*sipa_rm_ctx), GFP_KERNEL);
	if (!sipa_rm_ctx)
		return -ENOMEM;

	sipa_rm_ctx->sipa_rm_wq = create_singlethread_workqueue("sipa_rm_wq");
	if (!sipa_rm_ctx->sipa_rm_wq) {
		pr_err("create workqueue failed\n");
		result = -ENOMEM;
		goto create_wq_fail;
	}
	result = sipa_rm_dep_graph_create(&(sipa_rm_ctx->dep_graph));
	if (result) {
		pr_err("create dependency graph failed\n");
		goto graph_alloc_fail;
	}
	spin_lock_init(&sipa_rm_ctx->sipa_rm_lock);

	return 0;
graph_alloc_fail:
	destroy_workqueue(sipa_rm_ctx->sipa_rm_wq);
create_wq_fail:
	kfree(sipa_rm_ctx);

	return result;
}

/**
 * sipa_rm_exit() - free all SIPA RM resources
 */
void sipa_rm_exit(void)
{
	sipa_rm_dep_graph_delete(sipa_rm_ctx->dep_graph);
	destroy_workqueue(sipa_rm_ctx->sipa_rm_wq);
	kfree(sipa_rm_ctx);
	sipa_rm_ctx = NULL;
}

/**
 * sipa_rm_stat() - print RM stat
 * @buf: [in] The user buff used to print
 * @size: [in] The size of buf
 * Returns: number of bytes used on success, negative on failure
 *
 * This function is called by sipa_debugfs in order to receive
 * a full picture of the current state of the RM
 */
int sipa_rm_stat(char *buf, int size)
{
	unsigned long flags;
	int i, cnt = 0, result = EINVAL;
	struct sipa_rm_resource *resource;

	if (!buf || size < 0)
		return result;

	spin_lock_irqsave(&sipa_rm_ctx->sipa_rm_lock, flags);
	for (i = 0; i < SIPA_RM_RES_CONS_MAX; ++i) {
		result = sipa_rm_dep_graph_get_resource(
				 sipa_rm_ctx->dep_graph,
				 i,
				 &resource);
		if (!result) {
			result = sipa_rm_resource_consumer_print_stat(
					 resource, buf + cnt,
					 size-cnt);
			if (result < 0)
				goto bail;
			cnt += result;
		}
	}

	result = cnt;
bail:
	spin_unlock_irqrestore(&sipa_rm_ctx->sipa_rm_lock, flags);

	return result;
}
