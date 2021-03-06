/*
 * Copyright  2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"
#include "linux/wait.h"


/**
 * intel_enable_perfmon_interrupt - enable perfmon interrupt
 *
 */
static int intel_enable_perfmon_interrupt(struct drm_device *dev,
						int enable)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long irqflags;

	if (!(IS_GEN7(dev)))
		return -EINVAL;

	spin_lock_irqsave(&dev_priv->irq_lock, irqflags);

	if (enable)
		ilk_enable_gt_irq(dev_priv,
				  GT_RENDER_PERFMON_BUFFER_INTERRUPT);
	else
		ilk_disable_gt_irq(dev_priv,
				   GT_RENDER_PERFMON_BUFFER_INTERRUPT);

	spin_unlock_irqrestore(&dev_priv->irq_lock, irqflags);

	return 0;
}

/**
 * intel_wait_perfmon_interrupt - wait for perfmon buffer interrupt
 *
 * Blocks until perfmon buffer half full interrupt occurs or the wait
 * times out.
 */
static int intel_wait_perfmon_interrupt(struct drm_device *dev,
						int timeout_ms)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int counter = atomic_read(&dev_priv->perfmon_buffer_interrupts);
	int retcode = I915_PERFMON_IRQ_WAIT_OK;
	int time_left = 0;

	if (!(IS_GEN7(dev)))
		return -EINVAL;

	time_left = wait_event_interruptible_timeout(
		dev_priv->perfmon_buffer_queue,
		atomic_read(&dev_priv->perfmon_buffer_interrupts) != counter,
		timeout_ms * HZ / 1000);

	if (time_left == 0)
		retcode = I915_PERFMON_IRQ_WAIT_TIMEOUT;
	else if (time_left == -ERESTARTSYS)
		retcode = I915_PERFMON_IRQ_WAIT_INTERRUPTED;
	else if (time_left < 0)
		retcode = I915_PERFMON_IRQ_WAIT_FAILED;

	return retcode;
}

/**
 * i915_perfmon_ioctl - performance monitoring support
 *
 * Main entry point to performance monitoring support
 * IOCTLs.
 */
int i915_perfmon_ioctl(struct drm_device *dev, void *data,
	struct drm_file *file)
{
	struct drm_i915_perfmon *perfmon = data;
	int retcode = 0;

	switch (perfmon->op) {
	case I915_PERFMON_SET_BUFFER_IRQS:
		retcode = intel_enable_perfmon_interrupt(
				dev,
				perfmon->data.set_irqs.enable);
		break;
	case I915_PERFMON_WAIT_BUFFER_IRQS:
		if (perfmon->data.wait_irqs.timeout >
				I915_PERFMON_WAIT_IRQ_MAX_TIMEOUT_MS)
			retcode =  -EINVAL;
		else
			perfmon->data.wait_irqs.ret_code =
				intel_wait_perfmon_interrupt(
					dev,
					perfmon->data.wait_irqs.timeout);
		break;
	default:
		/* unknown operation */
		retcode = -EINVAL;
		break;
	}

	return retcode;
}
