/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "glade.h"

#include "plugin.h"
#include "pluginenum.h"
#include "input.h"
#include "effect.h"
#include "general.h"
#include "output.h"
#include "visualization.h"

#include "main.h"
#include "skin.h"
#include "urldecode.h"
#include "util.h"
#include "dnd.h"
#include "libaudacious/configdb.h"
#include "libaudacious/titlestring.h"

#include "playlist.h"

#include "mainwin.h"
#include "ui_playlist.h"
#include "skinwin.h"
#include "playlist_list.h"
#include "build_stamp.h"
#include "ui_fileinfo.h"
#include "ui_playlist.h"

GtkWidget *fileinfo_win;
GtkWidget *filepopup_win;
GdkPixbuf *filepopup_pixbuf;

static void
fileinfo_entry_set_text(const char *entry, const char *text)
{
	GladeXML *xml = g_object_get_data(G_OBJECT(fileinfo_win), "glade-xml");
	GtkWidget *widget = glade_xml_get_widget(xml, entry);

	if (xml == NULL || widget == NULL)
		return;

	gtk_entry_set_text(GTK_ENTRY(widget), text);
}

static void
fileinfo_entry_set_text_free(const char *entry, char *text)
{
	GladeXML *xml = g_object_get_data(G_OBJECT(fileinfo_win), "glade-xml");
	GtkWidget *widget = glade_xml_get_widget(xml, entry);

	if (xml == NULL || widget == NULL)
		return;

	gtk_entry_set_text(GTK_ENTRY(widget), text);

	g_free(text);
}

static void
fileinfo_entry_set_image(const char *entry, const char *text)
{
	GladeXML *xml = g_object_get_data(G_OBJECT(fileinfo_win), "glade-xml");
	GtkWidget *widget = glade_xml_get_widget(xml, entry);
	GdkPixbuf *pixbuf;

	if (xml == NULL || widget == NULL)
		return;

	pixbuf = gdk_pixbuf_new_from_file(text, NULL);

	if (pixbuf == NULL)
		return;

	if (gdk_pixbuf_get_height(GDK_PIXBUF(pixbuf)) > 150)
	{
		GdkPixbuf *pixbuf2 = gdk_pixbuf_scale_simple(GDK_PIXBUF(pixbuf), 150, 150, GDK_INTERP_BILINEAR);
		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = pixbuf2;
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(widget), GDK_PIXBUF(pixbuf));
	g_object_unref(G_OBJECT(pixbuf));
}

static void
filepopup_entry_set_text(const char *entry, const char *text)
{
	GladeXML *xml = g_object_get_data(G_OBJECT(filepopup_win), "glade-xml");
	GtkWidget *widget = glade_xml_get_widget(xml, entry);

	if (xml == NULL || widget == NULL)
		return;

	gtk_label_set_text(GTK_LABEL(widget), text);
}

static void
filepopup_entry_set_image(const char *entry, const char *text)
{
	GladeXML *xml = g_object_get_data(G_OBJECT(filepopup_win), "glade-xml");
	GtkWidget *widget = glade_xml_get_widget(xml, entry);
	GdkPixbuf *pixbuf;

	if (xml == NULL || widget == NULL)
		return;

	pixbuf = gdk_pixbuf_new_from_file(text, NULL);

	if (pixbuf == NULL)
		return;

	if (gdk_pixbuf_get_height(GDK_PIXBUF(pixbuf)) > 150)
	{
		GdkPixbuf *pixbuf2 = gdk_pixbuf_scale_simple(GDK_PIXBUF(pixbuf), 150, 150, GDK_INTERP_BILINEAR);
		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = pixbuf2;
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(widget), GDK_PIXBUF(pixbuf));
	g_object_unref(G_OBJECT(pixbuf));
}

static void
filepopup_entry_set_text_free(const char *entry, char *text)
{
	GladeXML *xml = g_object_get_data(G_OBJECT(filepopup_win), "glade-xml");
	GtkWidget *widget = glade_xml_get_widget(xml, entry);

	if (xml == NULL || widget == NULL)
		return;

	gtk_label_set_text(GTK_LABEL(widget), text);

	g_free(text);
}

static gboolean
filepopup_pointer_check_iter(gpointer unused)
{
	gint x, y, pos;
	TitleInput *tuple;
	static gint prev_x = 0, prev_y = 0, ctr = 0;
	gboolean skip = FALSE;

	if (!cfg.show_filepopup_for_tuple || playlistwin_is_shaded()
		|| playlistwin_list->pl_tooltips == FALSE
		|| gdk_window_at_pointer(NULL, NULL) != GDK_WINDOW(playlistwin->window)
		|| gdk_window_at_pointer(NULL, NULL) == NULL)
	{
		ctr = 0;
                if ( filepopup_win->window != NULL &&
                     gdk_window_is_viewable(GDK_WINDOW(filepopup_win->window)) )
                  filepopup_hide(NULL);
		return TRUE;
	}

	gdk_window_get_pointer(playlistwin->window, &x, &y, NULL);

	if (prev_x == x && prev_y == y)
		ctr++;
	else
	{
		ctr = 0;
		prev_x = x;
		prev_y = y;
		filepopup_hide(NULL);
		return TRUE;
	}

	if (filepopup_win->window == NULL)
		skip = TRUE;

        if (ctr >= 20 && (skip == TRUE || gdk_window_is_viewable(GDK_WINDOW(filepopup_win->window)) != TRUE))
        {
		pos = playlist_list_get_playlist_position(playlistwin_list, x, y);

		if (pos == -1)
  		{
			filepopup_hide(NULL);
			return TRUE;
	    	}

		tuple = playlist_get_tuple(pos);
		filepopup_show_for_tuple(tuple);
	}

	return TRUE;
}

void fileinfo_hide(gpointer unused)
{
	gtk_widget_hide(fileinfo_win);

	/* Clear it out. */
	fileinfo_entry_set_text("entry_title", "");
	fileinfo_entry_set_text("entry_artist", "");
	fileinfo_entry_set_text("entry_album", "");
	fileinfo_entry_set_text("entry_comment", "");
	fileinfo_entry_set_text("entry_genre", "");
	fileinfo_entry_set_text("entry_year", "");
	fileinfo_entry_set_text("entry_track", "");
	fileinfo_entry_set_text("entry_location", "");

	fileinfo_entry_set_image("image_artwork", DATA_DIR "/images/audio.png");
}

void filepopup_hide(gpointer unused)
{
	gtk_widget_hide(filepopup_win);

	filepopup_entry_set_text("label_title", "");
	filepopup_entry_set_text("label_artist", "");
	filepopup_entry_set_text("label_album", "");
	filepopup_entry_set_text("label_genre", "");
	filepopup_entry_set_text("label_track", "");
	filepopup_entry_set_text("label_year", "");
	filepopup_entry_set_text("label_length", "");

	filepopup_entry_set_image("image_artwork", DATA_DIR "/images/audio.png");

	gtk_window_resize(GTK_WINDOW(filepopup_win), 1, 1);
}

void
create_fileinfo_window(void)
{
	const gchar *glade_file = DATA_DIR "/glade/fileinfo.glade";
	GladeXML *xml;
	GtkWidget *widget;

	xml = glade_xml_new_or_die(_("Track Information Window"), glade_file, NULL, NULL);

	glade_xml_signal_autoconnect(xml);

	fileinfo_win = glade_xml_get_widget(xml, "fileinfo_win");
	g_object_set_data(G_OBJECT(fileinfo_win), "glade-xml", xml);
	gtk_window_set_transient_for(GTK_WINDOW(fileinfo_win), GTK_WINDOW(mainwin));

	widget = glade_xml_get_widget(xml, "image_artwork");
	gtk_image_set_from_file(GTK_IMAGE(widget), DATA_DIR "/images/audio.png");

	widget = glade_xml_get_widget(xml, "btn_close");
	g_signal_connect(G_OBJECT(widget), "clicked", (GCallback) fileinfo_hide, NULL);
}

void
create_filepopup_window(void)
{
	const gchar *glade_file = DATA_DIR "/glade/fileinfo_popup.glade";
	GladeXML *xml;
	GtkWidget *widget;

	xml = glade_xml_new_or_die(_("Track Information Popup"), glade_file, NULL, NULL);

	glade_xml_signal_autoconnect(xml);

	filepopup_win = glade_xml_get_widget(xml, "win_pl_popup");
	g_object_set_data(G_OBJECT(filepopup_win), "glade-xml", xml);
	gtk_window_set_transient_for(GTK_WINDOW(filepopup_win), GTK_WINDOW(mainwin));

	widget = glade_xml_get_widget(xml, "image_artwork");
	gtk_image_set_from_file(GTK_IMAGE(widget), DATA_DIR "/images/audio.png");

	g_timeout_add(50, filepopup_pointer_check_iter, NULL);
}

void
fileinfo_show_for_tuple(TitleInput *tuple)
{
	gchar *tmp = NULL;

	if (tuple == NULL)
		return;

	gtk_widget_realize(fileinfo_win);

	fileinfo_entry_set_text("entry_title", tuple->track_name);
	fileinfo_entry_set_text("entry_artist", tuple->performer);
	fileinfo_entry_set_text("entry_album", tuple->album_name);
	fileinfo_entry_set_text("entry_comment", tuple->comment);
	fileinfo_entry_set_text("entry_genre", tuple->genre);

	tmp = g_strdup_printf("%s/%s", tuple->file_path, tuple->file_name);
	if(tmp){
		fileinfo_entry_set_text_free("entry_location", str_to_utf8(tmp));
		g_free(tmp);
		tmp = NULL;
	}

	if (tuple->year != 0)
		fileinfo_entry_set_text_free("entry_year", g_strdup_printf("%d", tuple->year));

	if (tuple->track_number != 0)
		fileinfo_entry_set_text_free("entry_track", g_strdup_printf("%d", tuple->track_number));

	tmp = fileinfo_recursive_get_image(tuple->file_path, 0);
	
	if(tmp)
	{
		fileinfo_entry_set_image("image_artwork", tmp);
		g_free(tmp);
	}
	
	gtk_widget_show(fileinfo_win);
}

static gboolean
cover_name_filter(const gchar *name, const gchar *filter, const gboolean ret_on_empty)
{
	gboolean result = FALSE;
	gchar **splitted;
	gchar *current;
	gchar *lname;
	gint i;

	if (!filter || strlen(filter) == 0) {
		return ret_on_empty;
	}

	splitted = g_strsplit(filter, ",", 0);

	lname = g_strdup(name);
	g_strdown(lname);

	for (i = 0; !result && (current = splitted[i]); i++) {
		gchar *stripped = g_strstrip(g_strdup(current));
		g_strdown(stripped);

		result = result || strstr(lname, stripped);

		g_free(stripped);
	}

	g_free(lname);
	g_strfreev(splitted);

	return result;
}

/* Check wether it's an image we want */
static gboolean
is_front_cover_image(const gchar *name)
{
	char *ext;

	ext = strrchr(name, '.');
	if (!ext) {
		/* No file extension */
		return FALSE;
	}

	if (g_strcasecmp(ext, ".jpg") != 0 &&
	    g_strcasecmp(ext, ".jpeg") != 0 &&
	    g_strcasecmp(ext, ".png") != 0) {
		/* No recognized file extension */
		return FALSE;
	}

	return cover_name_filter(name, cfg.cover_name_include, TRUE) &&
	       !cover_name_filter(name, cfg.cover_name_exclude, FALSE);
}

gchar*
fileinfo_recursive_get_image(const gchar* path, gint depth)
{
	GDir *d;

	if(depth > 3)
		return NULL;
	
	d = g_dir_open(path, 0, NULL);

	if (d)
	{	
		const gchar *f = g_dir_read_name(d);
		
		while (f)
		{
			gchar *newpath = g_strdup_printf("%s/%s", path, f);

			if (is_front_cover_image(f))
			{
				/* We found a suitable file in the current
				 * directory, use that. The string will be
				 * freed by the caller */
				return newpath;
			}
			else
			{
				/* File/directory wasn't suitable, try and recurse into it.
				 * This should either return a filename for a image file, 
				 * or NULL if there was no suitable file, or 'f' wasn't a dir.
				 */
				gchar *tmp = fileinfo_recursive_get_image(newpath, depth+1);
				
				if(tmp)
				{
					g_free(newpath);
					return tmp;
				}
				else
				{
					/* Not got anything, move onto the next item in the directory */
					f = g_dir_read_name(d);
				}
			}
		}
		
		g_dir_close(d);
	}
	
	return NULL;
}

void
filepopup_show_for_tuple(TitleInput *tuple)
{
	gchar *tmp;
	gint x, y, x_off = 3, y_off = 3, h, w;

	if (tuple == NULL)
		return;

	gtk_widget_realize(filepopup_win);

	filepopup_entry_set_text("label_title", tuple->track_name);
	filepopup_entry_set_text("label_artist", tuple->performer);
	filepopup_entry_set_text("label_album", tuple->album_name);
	filepopup_entry_set_text("label_genre", tuple->genre);

	if (tuple->length != -1)
		filepopup_entry_set_text_free("label_length", g_strdup_printf("%d:%02d", tuple->length / 60000, (tuple->length / 1000) % 60));

	if (tuple->year != 0)
		filepopup_entry_set_text_free("label_year", g_strdup_printf("%d", tuple->year));

	if (tuple->track_number != 0)
		filepopup_entry_set_text_free("label_track", g_strdup_printf("%d", tuple->track_number));

	tmp = fileinfo_recursive_get_image(tuple->file_path, 0);
	
	if(tmp)
	{
		filepopup_entry_set_image("image_artwork", tmp);
		g_free(tmp);
	}
	
	gdk_window_get_pointer(NULL, &x, &y, NULL);
	gtk_window_get_size(GTK_WINDOW(filepopup_win), &w, &h);
	if (gdk_screen_width()-(w+3) < x) x_off = (w*-1)-3;
	if (gdk_screen_height()-(h+3) < y) y_off = (h*-1)-3;
	gtk_window_move(GTK_WINDOW(filepopup_win), x + x_off, y + y_off);

	gtk_widget_show(filepopup_win);
}

void
fileinfo_show_for_path(gchar *path)
{
	TitleInput *tuple = input_get_song_tuple(path);

	if (tuple == NULL)
		return input_file_info_box(path);

	fileinfo_show_for_tuple(tuple);

	bmp_title_input_free(tuple);
}
