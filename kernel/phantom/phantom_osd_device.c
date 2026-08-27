// SPDX-License-Identifier: GPL-2.0

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <uapi/linux/phantom_osd.h>

#include "osd.h"
#include "osd_manager.h"
#include "osd_render.h"
#include "osd_desktop.h"

#define PHANTOM_OSD_MAX_WINDOWS 32

static struct phantom_osd_window *
phantom_windows[PHANTOM_OSD_MAX_WINDOWS];

static long phantom_osd_ioctl(
	struct file *file,
	unsigned int command,
	unsigned long argument)
{
	switch (command) {

	case PHANTOM_OSD_IOC_CREATE_WINDOW: {
		struct phantom_osd_uapi_window request;
		struct phantom_osd_window *window;
		int i;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		request.title[sizeof(request.title) - 1] = '\0';

		window = phantom_osd_window_create(
			request.title,
			request.x,
			request.y,
			request.width,
			request.height,
			request.flags);

		if (!window)
			return -ENOMEM;

		for (i = 0; i < PHANTOM_OSD_MAX_WINDOWS; i++) {
			if (phantom_windows[i])
				continue;

			phantom_windows[i] = window;
			request.window_id = window->id;

			if (copy_to_user((void __user *)argument,
					 &request,
					 sizeof(request))) {
				phantom_osd_window_destroy(window);
				phantom_windows[i] = NULL;
				return -EFAULT;
			}

			return 0;
		}

		phantom_osd_window_destroy(window);
		return -ENOSPC;
	}

	case PHANTOM_OSD_IOC_LABEL: {
		struct phantom_osd_uapi_text request;
		struct phantom_osd_window *window;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		request.text[sizeof(request.text) - 1] = '\0';

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		if (!phantom_osd_label_create(window,
					      request.x,
					      request.y,
					      request.width,
					      request.text))
			return -ENOMEM;

		return 0;
	}

	case PHANTOM_OSD_IOC_BUTTON: {
		struct phantom_osd_uapi_button request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		request.text[sizeof(request.text) - 1] = '\0';

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		widget = phantom_osd_button_create(window,
						   request.x,
						   request.y,
						   request.width,
						   request.text);
		if (!widget)
			return -ENOMEM;

		request.widget_id = widget->id;

		if (copy_to_user((void __user *)argument,
				 &request,
				 sizeof(request)))
			return -EFAULT;

		return 0;
	}

	case PHANTOM_OSD_IOC_MENU: {
		struct phantom_osd_uapi_menu request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;
		const char *items[16];
		u32 i;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		if (!request.count || request.count > 16)
			return -EINVAL;

		if (request.height < request.count)
			return -EINVAL;

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		for (i = 0; i < request.count; i++) {
			request.items[i][sizeof(request.items[i]) - 1] = '\0';
			items[i] = request.items[i];
		}

		widget = phantom_osd_menu_create(window,
						 request.x,
						 request.y,
						 request.width,
						 request.height,
						 items,
						 request.count);
		if (!widget)
			return -ENOMEM;

		request.widget_id = widget->id;

		if (copy_to_user((void __user *)argument,
				 &request,
				 sizeof(request)))
			return -EFAULT;

		return 0;
	}

	case PHANTOM_OSD_IOC_PROGRESS: {
		struct phantom_osd_uapi_progress request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		if (request.percent > 100)
			return -EINVAL;

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		widget = phantom_osd_progress_create(window,
						     request.x,
						     request.y,
						     request.width);
		if (!widget)
			return -ENOMEM;

		request.widget_id = widget->id;

		if (copy_to_user((void __user *)argument,
				 &request,
				 sizeof(request)))
			return -EFAULT;

		return phantom_osd_progress_set(widget,
						request.percent,
						request.eta_seconds);
	}

	case PHANTOM_OSD_IOC_RENDER:
		return phantom_osd_render_frame();

	case PHANTOM_OSD_IOC_FOCUS: {
		u32 window_id;
		struct phantom_osd_window *window;

		if (copy_from_user(&window_id,
				   (void __user *)argument,
				   sizeof(window_id)))
			return -EFAULT;

		window = phantom_osd_manager_find_window(window_id);
		if (!window)
			return -ENOENT;

		return phantom_osd_window_focus(window);
	}

	case PHANTOM_OSD_IOC_STATUS: {
		struct phantom_osd_uapi_text request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		request.text[sizeof(request.text) - 1] = '\0';

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		/*
		 * Status: jeśli widget status już istnieje, zaktualizuj tekst.
		 * Na razie tworzymy nowy (prostsze). Później można cache'ować.
		 */
		widget = phantom_osd_status_create(window,
						   request.x,
						   request.y,
						   request.width,
						   request.text);
		if (!widget)
			return -ENOMEM;

		return 0;
	}

	case PHANTOM_OSD_IOC_MENU_SET_SELECTED: {
		struct phantom_osd_uapi_widget_selection request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		widget = phantom_osd_widget_find(window, request.widget_id);
		if (!widget)
			return -ENOENT;

		return phantom_osd_menu_set_selected(widget, request.selected);
	}

	case PHANTOM_OSD_IOC_BUTTON_SET_SELECTED: {
		struct phantom_osd_uapi_widget_selection request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;

		if (copy_from_user(&request,
				   (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;

		window = phantom_osd_manager_find_window(request.window_id);
		if (!window)
			return -ENOENT;

		widget = phantom_osd_widget_find(window, request.widget_id);
		if (!widget)
			return -ENOENT;

		return phantom_osd_button_set_selected(
			widget,
			request.selected != 0);
	}

	case PHANTOM_OSD_IOC_SET_DESKTOP: {
		struct phantom_osd_uapi_desktop request;

		if (copy_from_user(&request, (void __user *)argument,
				   sizeof(request)))
			return -EFAULT;
		return phantom_osd_desktop_resize(request.width,
						  request.height);
	}

	default:
		return -ENOTTY;
	}
}

static const struct file_operations phantom_osd_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= phantom_osd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
#endif
};

static struct miscdevice phantom_osd_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "phantom_osd",
	.fops	= &phantom_osd_fops,
	.mode	= 0600,
};

int phantom_osd_device_init(void)
{
	return misc_register(&phantom_osd_device);
}

void phantom_osd_device_shutdown(void)
{
	misc_deregister(&phantom_osd_device);
}
