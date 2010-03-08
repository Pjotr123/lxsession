/**
 * Copyright (c) 2010 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>
#include <locale.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <signal.h>

#include "dbus-interface.h"

/* Command parameters. */
static char * prompt = NULL;
static char * banner_side = NULL;
static char * banner_path = NULL;

static GOptionEntry opt_entries[] =
{
    { "prompt", 'p', 0, G_OPTION_ARG_STRING, &prompt, N_("Custom message to show on the dialog"), N_("message") },
    { "banner", 'b', 0, G_OPTION_ARG_STRING, &banner_path, N_("Banner to show on the dialog"), N_("image file") },
    { "side", 's', 0, G_OPTION_ARG_STRING, &banner_side, N_("Position of the banner"), "top|left|right|bottom" },
    { NULL }
};

typedef struct {
    GPid lxsession_pid;			/* Process ID of lxsession */

    int shutdown_available : 1;		/* Shutdown is available */
    int reboot_available : 1;		/* Reboot is available */
    int suspend_available : 1;		/* Suspend is available */
    int hibernate_available : 1;	/* Hibernate is available */
    int switch_user_available : 1;	/* Switch User is available */

    int shutdown_ConsoleKit : 1;	/* Shutdown is available via ConsoleKit */
    int reboot_ConsoleKit : 1;		/* Reboot is available via ConsoleKit */
    int suspend_DeviceKit : 1;		/* Suspend is available via DeviceKit */
    int hibernate_DeviceKit : 1;	/* Hibernate is available via DeviceKit */
    int shutdown_HAL : 1;		/* Shutdown is available via HAL */
    int reboot_HAL : 1;			/* Reboot is available via HAL */
    int suspend_HAL : 1;		/* Suspend is available via HAL */
    int hibernate_HAL : 1;		/* Hibernate is available via HAL */
    int switch_user_KDE : 1;		/* Switch User is available via KDE */

} HandlerContext;

static void logout_clicked(GtkButton * button, HandlerContext * handler_context);
static void shutdown_clicked(GtkButton * button, HandlerContext * handler_context);
static void reboot_clicked(GtkButton * button, HandlerContext * handler_context);
static void suspend_clicked(GtkButton * button, HandlerContext * handler_context);
static void hibernate_clicked(GtkButton * button, HandlerContext * handler_context);
static void switch_user_clicked(GtkButton * button, HandlerContext * handler_context);
static void cancel_clicked(GtkButton * button, gpointer user_data);
static GtkPositionType get_banner_position(void);
static GdkPixbuf * get_background_pixbuf(void);

/* Handler for "clicked" signal on Logout button. */
static void logout_clicked(GtkButton * button, HandlerContext * handler_context)
{
    kill(handler_context->lxsession_pid, SIGTERM);
    gtk_main_quit();
}

/* Handler for "clicked" signal on Shutdown button. */
static void shutdown_clicked(GtkButton * button, HandlerContext * handler_context)
{
    if (handler_context->shutdown_ConsoleKit)
        dbus_ConsoleKit_Stop();
    else if (handler_context->shutdown_HAL)
        dbus_HAL_Shutdown();
    gtk_main_quit();
}

/* Handler for "clicked" signal on Reboot button. */
static void reboot_clicked(GtkButton * button, HandlerContext * handler_context)
{
    if (handler_context->reboot_ConsoleKit)
        dbus_ConsoleKit_Restart();
    else if (handler_context->reboot_HAL)
        dbus_HAL_Reboot();
    gtk_main_quit();
}

/* Handler for "clicked" signal on Suspend button. */
static void suspend_clicked(GtkButton * button, HandlerContext * handler_context)
{
    if (handler_context->suspend_DeviceKit)
        dbus_DeviceKit_Suspend();
    else if (handler_context->suspend_HAL)
        dbus_HAL_Suspend();
    gtk_main_quit();
}

/* Handler for "clicked" signal on Hibernate button. */
static void hibernate_clicked(GtkButton * button, HandlerContext * handler_context)
{
    if (handler_context->hibernate_DeviceKit)
        dbus_DeviceKit_Hibernate();
    else if (handler_context->hibernate_HAL)
        dbus_HAL_Hibernate();
    gtk_main_quit();
}

/* Handler for "clicked" signal on Switch User button. */
static void switch_user_clicked(GtkButton * button, HandlerContext * handler_context)
{
    if (handler_context->switch_user_KDE)
        g_spawn_command_line_sync("kdmctl reserve", NULL, NULL, NULL, NULL);
    gtk_main_quit();
}

/* Handler for "clicked" signal on Cancel button. */
static void cancel_clicked(GtkButton * button, gpointer user_data)
{
    gtk_main_quit();
}

/* Convert the --side parameter to a GtkPositionType. */
static GtkPositionType get_banner_position(void)
{
    if (banner_side != NULL)
    {
        if (strcmp(banner_side, "right") == 0)
            return GTK_POS_RIGHT;
        if (strcmp(banner_side, "top") == 0)
            return GTK_POS_TOP;
        if (strcmp(banner_side, "bottom") == 0)
            return GTK_POS_BOTTOM;
    }
    return GTK_POS_LEFT;
}

/* Get the background pixbuf. */
static GdkPixbuf * get_background_pixbuf(void)
{
    /* Get the root window pixmap. */
    GdkScreen * screen = gdk_screen_get_default();
    GdkPixbuf * pixbuf = gdk_pixbuf_get_from_drawable(
        NULL,					/* Allocate a new pixbuf */
        gdk_get_default_root_window(),		/* The drawable */
        NULL,					/* Its colormap */
        0, 0, 0, 0,				/* Coordinates */
        gdk_screen_get_width(screen),		/* Width */
        gdk_screen_get_height(screen));		/* Height */

    /* Make the background darker. */
    if (pixbuf != NULL)
    {
        unsigned char * pixels = gdk_pixbuf_get_pixels(pixbuf);
        int width = gdk_pixbuf_get_width(pixbuf);
        int height = gdk_pixbuf_get_height(pixbuf);
        int pixel_stride = ((gdk_pixbuf_get_has_alpha(pixbuf)) ? 4 : 3);
        int row_stride = gdk_pixbuf_get_rowstride(pixbuf);
        int y;
        for (y = 0; y < height; y += 1)
        {
            unsigned char * p = pixels;
            int x;
            for (x = 0; x < width; x += 1)
            {
                p[0] = p[0] / 2;
                p[1] = p[1] / 2;
                p[2] = p[2] / 2;
                p += pixel_stride;
            }
            pixels += row_stride;
        }
    }
    return pixbuf;
}

/* Handler for "expose_event" on drawing areas. */
gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, GdkPixbuf * pixbuf)
{
    if (pixbuf != NULL)
    {
        /* Copy the appropriate rectangle of the root window pixmap to the drawing area.
         * All drawing areas are immediate children of the toplevel window, so the allocation yields the source coordinates directly. */
        gdk_draw_pixbuf(
            widget->window,					/* Drawable to render to */
            NULL,						/* GC for clipping */
            pixbuf,						/* Source pixbuf */
            widget->allocation.x, widget->allocation.y,		/* Source coordinates */
            0, 0,						/* Destination coordinates */
            widget->allocation.width, widget->allocation.height,
            GDK_RGB_DITHER_NORMAL,				/* Dither type */
            0, 0);						/* Dither offsets */
    }
    return TRUE;
}

/* Main program. */
int main(int argc, char * argv[])
{
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    /* Initialize GTK (via g_option_context_parse) and parse command line arguments. */
    GOptionContext * context = g_option_context_new("");
    g_option_context_add_main_entries(context, opt_entries, GETTEXT_PACKAGE);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    GError * err = NULL;
    if ( ! g_option_context_parse(context, &argc, &argv, &err))
    {
        g_print(_("Error: %s\n"), err->message);
        g_error_free(err);
        return 1;
    }
    g_option_context_free(context);

    HandlerContext handler_context;
    memset(&handler_context, 0, sizeof(handler_context));

    /* Ensure that we are running under lxsession. */
    const char * p = g_getenv("_LXSESSION_PID");
    if (p != NULL) handler_context.lxsession_pid = atoi(p);
    if (handler_context.lxsession_pid == 0)
    {
        g_print( _("Error: %s\n"), _("LXSession is not running."));
        return 1;
    }   

    /* Initialize capabilities of the ConsoleKit mechanism. */
    if (dbus_ConsoleKit_CanStop())
    {
        handler_context.shutdown_available = TRUE;
        handler_context.shutdown_ConsoleKit = TRUE;
    }
    if (dbus_ConsoleKit_CanRestart())
    {
        handler_context.reboot_available = TRUE;
        handler_context.reboot_ConsoleKit = TRUE;
    }

#ifdef LATENT_DEVICEKIT_SUPPORT
    /* Initialize capabilities of the DeviceKit mechanism. */
    if (dbus_DeviceKit_CanSuspend())
    {
        handler_context.suspend_available = TRUE;
        handler_context.suspend_DeviceKit = TRUE;
    }
    if (dbus_DeviceKit_CanHibernate())
    {
        handler_context.hibernate_available = TRUE;
        handler_context.hibernate_DeviceKit = TRUE;
    }
#endif

    /* Initialize capabilities of the HAL mechanism. */
    if (dbus_HAL_CanShutdown())
    {
        handler_context.shutdown_available = TRUE;
        handler_context.shutdown_HAL = TRUE;
    }
    if (dbus_HAL_CanReboot())
    {
        handler_context.reboot_available = TRUE;
        handler_context.reboot_HAL = TRUE;
    }
    if (dbus_HAL_CanSuspend())
    {
        handler_context.suspend_available = TRUE;
        handler_context.suspend_HAL = TRUE;
    }
    if (dbus_HAL_CanHibernate())
    {
        handler_context.hibernate_available = TRUE;
        handler_context.hibernate_HAL = TRUE;
    }

    /* If we are under KDM, its "Switch User" is available. */
    if (g_file_test("/var/run/kdm.pid", G_FILE_TEST_EXISTS))
    {
        gchar * test = g_find_program_in_path("kdmctl");
        if (test != NULL)
        {
            g_free(test);
            handler_context.switch_user_available = TRUE;
            handler_context.switch_user_KDE = TRUE;
        }
    }

    /* Make the button images accessible. */
    gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(), PACKAGE_DATA_DIR "/lxsession/images");

    /* Get the background pixbuf. */
    GdkPixbuf * pixbuf = get_background_pixbuf();

    /* Create the toplevel window. */
    GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(window));

    /* Create a vertical box as the child of the toplevel window. */
    GtkWidget * outermost = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), outermost);

    /* Create a drawing area as the child of the toplevel window.
     * This drawing area is as wide as the screen and as tall as the area above the user controls. */
    GtkWidget * top_drawing_area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(outermost), top_drawing_area, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(top_drawing_area), "expose_event", G_CALLBACK(expose_event), pixbuf);

    /* Create a horizontal box as the child of the outermost box. */
    GtkWidget * horizontal = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outermost), horizontal, FALSE, FALSE, 0);

    /* Create a drawing area as the child of the toplevel window.
     * This drawing area is as wide as the screen and as tall as the area below the user controls. */
    GtkWidget * bottom_drawing_area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(outermost), bottom_drawing_area, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(bottom_drawing_area), "expose_event", G_CALLBACK(expose_event), pixbuf);

    /* Create a drawing area as the child of the horizontal box.
     * This drawing area is as wide as the area left of the user controls and as tall as the user controls. */
    GtkWidget * left_drawing_area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(horizontal), left_drawing_area, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(left_drawing_area), "expose_event", G_CALLBACK(expose_event), pixbuf);

    /* Create a vertical box as the child of the horizontal box.  This will contain the user controls. */
    GtkWidget * controls = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(controls), 6);

    /* If specified, apply a user-specified banner image. */
    if (banner_path != NULL)
    {
        GtkWidget * banner_image = gtk_image_new_from_file(banner_path);
        GtkPositionType banner_position = get_banner_position();

        switch (banner_position)
        {
            case GTK_POS_LEFT:
            case GTK_POS_RIGHT:
                {
                /* Create a horizontal box to contain the image and the controls. */
                GtkWidget * box = gtk_hbox_new(FALSE, 2);
                gtk_box_pack_start(GTK_BOX(horizontal), box, FALSE, FALSE, 0);

                /* Pack the image and a separator. */
                gtk_misc_set_alignment(GTK_MISC(banner_image), 0.5, 0.0);
                if (banner_position == GTK_POS_LEFT)
                {
                    gtk_box_pack_start(GTK_BOX(box), banner_image, FALSE, FALSE, 2);
                    gtk_box_pack_start(GTK_BOX(box), gtk_vseparator_new(), FALSE, FALSE, 2);
                    gtk_box_pack_start(GTK_BOX(box), controls, FALSE, FALSE, 2);
                }
                else
                {
                    gtk_box_pack_start(GTK_BOX(box), controls, FALSE, FALSE, 2);
                    gtk_box_pack_end(GTK_BOX(box), gtk_vseparator_new(), FALSE, FALSE, 2);
                    gtk_box_pack_end(GTK_BOX(box), banner_image, FALSE, FALSE, 2);
                }
                }
                break;

            case GTK_POS_TOP:
                gtk_box_pack_start(GTK_BOX(controls), banner_image, FALSE, FALSE, 2);
                gtk_box_pack_start(GTK_BOX(controls), gtk_hseparator_new(), FALSE, FALSE, 2);
                gtk_box_pack_start(GTK_BOX(horizontal), controls, FALSE, FALSE, 0);
                break;

            case GTK_POS_BOTTOM:
                gtk_box_pack_end(GTK_BOX(controls), banner_image, FALSE, FALSE, 2);
                gtk_box_pack_end(GTK_BOX(controls), gtk_hseparator_new(), FALSE, FALSE, 2);
                gtk_box_pack_start(GTK_BOX(horizontal), controls, FALSE, FALSE, 0);
                break;
        }
    }
    else
        gtk_box_pack_start(GTK_BOX(horizontal), controls, FALSE, FALSE, 0);

    /* Create the label. */
    GtkWidget * label = gtk_label_new("");
    if (prompt == NULL)
    {
        const char * session_name = g_getenv("DESKTOP_SESSION");
        if (session_name == NULL)
            session_name = "LXDE";
        prompt = g_strdup_printf(_("<b><big>Logout %s session?</big></b>"), session_name);
    }
    gtk_label_set_markup(GTK_LABEL(label), prompt);
    gtk_box_pack_start(GTK_BOX(controls), label, FALSE, FALSE, 4);

    /* Create the Logout button. */
    GtkWidget * logout_button = gtk_button_new_with_mnemonic(_("_Logout"));
    GtkWidget * image = gtk_image_new_from_icon_name("system-log-out", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(logout_button), image);
    g_signal_connect(G_OBJECT(logout_button), "clicked", G_CALLBACK(logout_clicked), &handler_context);
    gtk_box_pack_start(GTK_BOX(controls), logout_button, FALSE, FALSE, 4);

    /* Create the Shutdown button. */
    if (handler_context.shutdown_available)
    {
        GtkWidget * shutdown_button = gtk_button_new_with_mnemonic(_("Sh_utdown"));
        GtkWidget * image = gtk_image_new_from_icon_name("system-shutdown", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(shutdown_button), image);
        g_signal_connect(G_OBJECT(shutdown_button), "clicked", G_CALLBACK(shutdown_clicked), &handler_context);
        gtk_box_pack_start(GTK_BOX(controls), shutdown_button, FALSE, FALSE, 4);
    }

    /* Create the Reboot button. */
    if (handler_context.reboot_available)
    {
        GtkWidget * reboot_button = gtk_button_new_with_mnemonic(_("_Reboot"));
        GtkWidget * image = gtk_image_new_from_icon_name("gnome-session-reboot", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(reboot_button), image);
        g_signal_connect(G_OBJECT(reboot_button), "clicked", G_CALLBACK(reboot_clicked), &handler_context);
        gtk_box_pack_start(GTK_BOX(controls), reboot_button, FALSE, FALSE, 4);
    }

    /* Create the Suspend button. */
    if (handler_context.suspend_available)
    {
        GtkWidget * suspend_button = gtk_button_new_with_mnemonic(_("_Suspend"));
        GtkWidget * image = gtk_image_new_from_icon_name("gnome-session-suspend", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(suspend_button), image);
        g_signal_connect(G_OBJECT(suspend_button), "clicked", G_CALLBACK(suspend_clicked), &handler_context);
        gtk_box_pack_start(GTK_BOX(controls), suspend_button, FALSE, FALSE, 4);
    }

    /* Create the Hibernate button. */
    if (handler_context.hibernate_available)
    {
        GtkWidget * hibernate_button = gtk_button_new_with_mnemonic(_("_Hibernate"));
        GtkWidget * image = gtk_image_new_from_icon_name("gnome-session-hibernate", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(hibernate_button), image);
        g_signal_connect(G_OBJECT(hibernate_button), "clicked", G_CALLBACK(hibernate_clicked), &handler_context);
        gtk_box_pack_start(GTK_BOX(controls), hibernate_button, FALSE, FALSE, 4);
    }

    /* Create the Switch User button. */
    if (handler_context.switch_user_available)
    {
        GtkWidget * switch_user_button = gtk_button_new_with_mnemonic(_("S_witch User"));
        GtkWidget * image = gtk_image_new_from_icon_name("gnome-session-switch", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(switch_user_button), image);
        g_signal_connect(G_OBJECT(switch_user_button), "clicked", G_CALLBACK(switch_user_clicked), &handler_context);
        gtk_box_pack_start(GTK_BOX(controls), switch_user_button, FALSE, FALSE, 4);
    }

    /* Create the Cancel button. */
    GtkWidget * cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(G_OBJECT(cancel_button), "clicked", G_CALLBACK(cancel_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(controls), cancel_button, FALSE, FALSE, 4);

    /* Create a drawing area as the child of the horizontal box.
     * This drawing area is as wide as the area right of the user controls and as tall as the user controls. */
    GtkWidget * right_drawing_area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(horizontal), right_drawing_area, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(right_drawing_area), "expose_event", G_CALLBACK(expose_event), pixbuf);

    /* Show everything. */
    gtk_widget_show_all(window);

    /* Run the main event loop. */
    gtk_main();

    /* Return. */
    return 0;
}