/*
 * dbus-server.c
 * Copyright 2013 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/equalizer.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>

#include "aud-dbus.h"
#include "main.h"

typedef ObjAudacious Obj;
typedef GDBusMethodInvocation Invoc;

#define FINISH(name) \
 obj_audacious_complete_##name (obj, invoc)

#define FINISH2(name, ...) \
 obj_audacious_complete_##name (obj, invoc, __VA_ARGS__)

static int current_playlist ()
{
    int list = aud_playlist_get_playing ();
    return (list >= 0) ? list : aud_playlist_get_active ();
}

#define CURRENT current_playlist ()

static Index<PlaylistAddItem> strv_to_index (const char * const * strv)
{
    Index<PlaylistAddItem> index;
    while (* strv)
        index.append (String (* strv ++));

    return index;
}

static gboolean do_add (Obj * obj, Invoc * invoc, const char * file)
{
    aud_playlist_entry_insert (CURRENT, -1, file, Tuple (), false);
    FINISH (add);
    return true;
}

static gboolean do_add_list (Obj * obj, Invoc * invoc, const char * const * filenames)
{
    aud_playlist_entry_insert_batch (CURRENT, -1, strv_to_index (filenames), false);
    FINISH (add_list);
    return true;
}

static gboolean do_add_url (Obj * obj, Invoc * invoc, const char * url)
{
    aud_playlist_entry_insert (CURRENT, -1, url, Tuple (), false);
    FINISH (add_url);
    return true;
}

static gboolean do_advance (Obj * obj, Invoc * invoc)
{
    aud_drct_pl_next ();
    FINISH (advance);
    return true;
}

static gboolean do_auto_advance (Obj * obj, Invoc * invoc)
{
    FINISH2 (auto_advance, ! aud_get_bool (nullptr, "no_playlist_advance"));
    return true;
}

static gboolean do_balance (Obj * obj, Invoc * invoc)
{
    FINISH2 (balance, aud_drct_get_volume_balance ());
    return true;
}

static gboolean do_clear (Obj * obj, Invoc * invoc)
{
    int playlist = CURRENT;
    aud_playlist_entry_delete (playlist, 0, aud_playlist_entry_count (playlist));
    FINISH (clear);
    return true;
}

static gboolean do_delete (Obj * obj, Invoc * invoc, unsigned pos)
{
    aud_playlist_entry_delete (CURRENT, pos, 1);
    FINISH (delete);
    return true;
}

static gboolean do_delete_active_playlist (Obj * obj, Invoc * invoc)
{
    aud_playlist_delete (CURRENT);
    FINISH (delete_active_playlist);
    return true;
}

static gboolean do_eject (Obj * obj, Invoc * invoc)
{
    if (! aud_get_headless_mode ())
        aud_ui_show_filebrowser (true);

    FINISH (eject);
    return true;
}

static gboolean do_equalizer_activate (Obj * obj, Invoc * invoc, gboolean active)
{
    aud_set_bool (nullptr, "equalizer_active", active);
    FINISH (equalizer_activate);
    return true;
}

static gboolean do_get_active_playlist (Obj * obj, Invoc * invoc)
{
    FINISH2 (get_active_playlist, CURRENT);
    return true;
}

static gboolean do_get_active_playlist_name (Obj * obj, Invoc * invoc)
{
    String title = aud_playlist_get_title (CURRENT);
    FINISH2 (get_active_playlist_name, title ? title : "");
    return true;
}

static gboolean do_get_eq (Obj * obj, Invoc * invoc)
{
    double preamp = aud_get_double (nullptr, "equalizer_preamp");
    double bands[AUD_EQ_NBANDS];
    aud_eq_get_bands (bands);

    GVariant * var = g_variant_new_fixed_array (G_VARIANT_TYPE_DOUBLE, bands,
     AUD_EQ_NBANDS, sizeof (double));
    FINISH2 (get_eq, preamp, var);
    return true;
}

static gboolean do_get_eq_band (Obj * obj, Invoc * invoc, int band)
{
    FINISH2 (get_eq_band, aud_eq_get_band (band));
    return true;
}

static gboolean do_get_eq_preamp (Obj * obj, Invoc * invoc)
{
    FINISH2 (get_eq_preamp, aud_get_double (nullptr, "equalizer_preamp"));
    return true;
}

static gboolean do_get_info (Obj * obj, Invoc * invoc)
{
    int bitrate, samplerate, channels;
    aud_drct_get_info (bitrate, samplerate, channels);
    FINISH2 (get_info, bitrate, samplerate, channels);
    return true;
}

static gboolean do_get_playqueue_length (Obj * obj, Invoc * invoc)
{
    FINISH2 (get_playqueue_length, aud_playlist_queue_count (CURRENT));
    return true;
}

static gboolean do_get_tuple_fields (Obj * obj, Invoc * invoc)
{
    const char * fields[Tuple::n_fields + 1];

    for (auto f : Tuple::all_fields ())
        fields[f] = Tuple::field_get_name (f);

    fields[Tuple::n_fields] = nullptr;

    FINISH2 (get_tuple_fields, fields);
    return true;
}

static gboolean do_info (Obj * obj, Invoc * invoc)
{
    int bitrate, samplerate, channels;
    aud_drct_get_info (bitrate, samplerate, channels);
    FINISH2 (info, bitrate, samplerate, channels);
    return true;
}

static gboolean do_jump (Obj * obj, Invoc * invoc, unsigned pos)
{
    aud_playlist_set_position (CURRENT, pos);
    FINISH (jump);
    return true;
}

static gboolean do_length (Obj * obj, Invoc * invoc)
{
    FINISH2 (length, aud_playlist_entry_count (CURRENT));
    return true;
}

static gboolean do_main_win_visible (Obj * obj, Invoc * invoc)
{
    FINISH2 (main_win_visible, ! aud_get_headless_mode () && aud_ui_is_shown ());
    return true;
}

static gboolean do_new_playlist (Obj * obj, Invoc * invoc)
{
    int playlist = CURRENT + 1;
    aud_playlist_insert (playlist);
    aud_playlist_set_active (playlist);
    aud_drct_stop ();
    FINISH (new_playlist);
    return true;
}

static gboolean do_number_of_playlists (Obj * obj, Invoc * invoc)
{
    FINISH2 (number_of_playlists, aud_playlist_count ());
    return true;
}

static gboolean do_open_list (Obj * obj, Invoc * invoc, const char * const * filenames)
{
    aud_drct_pl_open_list (strv_to_index (filenames));
    FINISH (open_list);
    return true;
}

static gboolean do_open_list_to_temp (Obj * obj, Invoc * invoc, const char * const * filenames)
{
    aud_drct_pl_open_temp_list (strv_to_index (filenames));
    FINISH (open_list_to_temp);
    return true;
}

static gboolean do_pause (Obj * obj, Invoc * invoc)
{
    aud_drct_pause ();
    FINISH (pause);
    return true;
}

static gboolean do_paused (Obj * obj, Invoc * invoc)
{
    FINISH2 (paused, aud_drct_get_paused ());
    return true;
}

static gboolean do_play (Obj * obj, Invoc * invoc)
{
    aud_drct_play ();
    FINISH (play);
    return true;
}

static gboolean do_play_active_playlist (Obj * obj, Invoc * invoc)
{
    aud_playlist_play (CURRENT);
    FINISH (play_active_playlist);
    return true;
}

static gboolean do_play_pause (Obj * obj, Invoc * invoc)
{
    aud_drct_play_pause ();
    FINISH (play_pause);
    return true;
}

static gboolean do_playing (Obj * obj, Invoc * invoc)
{
    FINISH2 (playing, aud_drct_get_playing ());
    return true;
}

static gboolean do_playlist_add (Obj * obj, Invoc * invoc, const char * list)
{
    aud_playlist_entry_insert (CURRENT, -1, list, Tuple (), false);
    FINISH (playlist_add);
    return true;
}

static gboolean do_playlist_enqueue_to_temp (Obj * obj, Invoc * invoc, const char * url)
{
    aud_drct_pl_open_temp (url);
    FINISH (playlist_enqueue_to_temp);
    return true;
}

static gboolean do_playlist_ins_url_string (Obj * obj, Invoc * invoc, const char * url, int pos)
{
    aud_playlist_entry_insert (CURRENT, pos, url, Tuple (), false);
    FINISH (playlist_ins_url_string);
    return true;
}

static gboolean do_playqueue_add (Obj * obj, Invoc * invoc, int pos)
{
    aud_playlist_queue_insert (CURRENT, -1, pos);
    FINISH (playqueue_add);
    return true;
}

static gboolean do_playqueue_clear (Obj * obj, Invoc * invoc)
{
    int playlist = CURRENT;
    aud_playlist_queue_delete (playlist, 0, aud_playlist_queue_count (playlist));
    FINISH (playqueue_clear);
    return true;
}

static gboolean do_playqueue_is_queued (Obj * obj, Invoc * invoc, int pos)
{
    bool queued = (aud_playlist_queue_find_entry (CURRENT, pos) >= 0);
    FINISH2 (playqueue_is_queued, queued);
    return true;
}

static gboolean do_playqueue_remove (Obj * obj, Invoc * invoc, int pos)
{
    int playlist = CURRENT;
    int qpos = aud_playlist_queue_find_entry (playlist, pos);

    if (qpos >= 0)
        aud_playlist_queue_delete (playlist, qpos, 1);

    FINISH (playqueue_remove);
    return true;
}

static gboolean do_plugin_enable (Obj * obj, Invoc * invoc, const char * name, gboolean enable)
{
    PluginHandle * plugin = aud_plugin_lookup_basename (name);
    if (! plugin)
    {
        AUDERR ("No such plugin: %s\n", name);
        return false;
    }

    aud_plugin_enable (plugin, enable);
    FINISH (plugin_enable);
    return true;
}

static gboolean do_plugin_is_enabled (Obj * obj, Invoc * invoc, const char * name)
{
    PluginHandle * plugin = aud_plugin_lookup_basename (name);
    if (! plugin)
    {
        AUDERR ("No such plugin: %s\n", name);
        return false;
    }

    FINISH2 (plugin_is_enabled, aud_plugin_get_enabled (plugin));
    return true;
}

static gboolean do_position (Obj * obj, Invoc * invoc)
{
    FINISH2 (position, aud_playlist_get_position (CURRENT));
    return true;
}

static gboolean do_queue_get_list_pos (Obj * obj, Invoc * invoc, unsigned qpos)
{
    FINISH2 (queue_get_list_pos, aud_playlist_queue_get_entry (CURRENT, qpos));
    return true;
}

static gboolean do_queue_get_queue_pos (Obj * obj, Invoc * invoc, unsigned pos)
{
    FINISH2 (queue_get_queue_pos, aud_playlist_queue_find_entry (CURRENT, pos));
    return true;
}

static gboolean do_quit (Obj * obj, Invoc * invoc)
{
    aud_quit ();
    FINISH (quit);
    return true;
}

static gboolean do_record (Obj * obj, Invoc * invoc)
{
    aud_drct_enable_record (! aud_drct_get_record_enabled ());
    FINISH (record);
    return true;
}

static gboolean do_recording (Obj * obj, Invoc * invoc)
{
    FINISH2 (recording, aud_drct_get_record_enabled ());
    return true;
}

static gboolean do_repeat (Obj * obj, Invoc * invoc)
{
    FINISH2 (repeat, aud_get_bool (nullptr, "repeat"));
    return true;
}

static gboolean do_reverse (Obj * obj, Invoc * invoc)
{
    aud_drct_pl_prev ();
    FINISH (reverse);
    return true;
}

static gboolean do_seek (Obj * obj, Invoc * invoc, unsigned pos)
{
    aud_drct_seek (pos);
    FINISH (seek);
    return true;
}

static gboolean do_set_active_playlist (Obj * obj, Invoc * invoc, int playlist)
{
    aud_playlist_set_active (playlist);

    // check that the requested playlist exists before switching playback
    if (aud_playlist_get_active () == playlist && aud_drct_get_playing ())
        aud_playlist_play (playlist, aud_drct_get_paused ());

    FINISH (set_active_playlist);
    return true;
}

static gboolean do_set_active_playlist_name (Obj * obj, Invoc * invoc, const char * title)
{
    aud_playlist_set_title (CURRENT, title);
    FINISH (set_active_playlist_name);
    return true;
}

static gboolean do_set_eq (Obj * obj, Invoc * invoc, double preamp, GVariant * var)
{
    if (! g_variant_is_of_type (var, G_VARIANT_TYPE ("ad")))
        return false;

    size_t nbands = 0;
    const double * bands = (double *) g_variant_get_fixed_array (var, & nbands, sizeof (double));

    if (nbands != AUD_EQ_NBANDS)
        return false;

    aud_set_double (nullptr, "equalizer_preamp", preamp);
    aud_eq_set_bands (bands);
    FINISH (set_eq);
    return true;
}

static gboolean do_set_eq_band (Obj * obj, Invoc * invoc, int band, double value)
{
    aud_eq_set_band (band, value);
    FINISH (set_eq_band);
    return true;
}

static gboolean do_set_eq_preamp (Obj * obj, Invoc * invoc, double preamp)
{
    aud_set_double (nullptr, "equalizer_preamp", preamp);
    FINISH (set_eq_preamp);
    return true;
}

static gboolean do_set_volume (Obj * obj, Invoc * invoc, int vl, int vr)
{
    aud_drct_set_volume ({vl, vr});
    FINISH (set_volume);
    return true;
}

static gboolean do_show_about_box (Obj * obj, Invoc * invoc, gboolean show)
{
    if (! aud_get_headless_mode ())
    {
        if (show)
            aud_ui_show_about_window ();
        else
            aud_ui_hide_about_window ();
    }

    FINISH (show_about_box);
    return true;
}

static gboolean do_show_filebrowser (Obj * obj, Invoc * invoc, gboolean show)
{
    if (! aud_get_headless_mode ())
    {
        if (show)
            aud_ui_show_filebrowser (false);
        else
            aud_ui_hide_filebrowser ();
    }

    FINISH (show_filebrowser);
    return true;
}

static gboolean do_show_jtf_box (Obj * obj, Invoc * invoc, gboolean show)
{
    if (! aud_get_headless_mode ())
    {
        if (show)
            aud_ui_show_jump_to_song ();
        else
            aud_ui_hide_jump_to_song ();
    }

    FINISH (show_jtf_box);
    return true;
}

static gboolean do_show_main_win (Obj * obj, Invoc * invoc, gboolean show)
{
    if (! aud_get_headless_mode ())
        aud_ui_show (show);

    FINISH (show_main_win);
    return true;
}

static gboolean do_show_prefs_box (Obj * obj, Invoc * invoc, gboolean show)
{
    if (! aud_get_headless_mode ())
    {
        if (show)
            aud_ui_show_prefs_window ();
        else
            aud_ui_hide_prefs_window ();
    }

    FINISH (show_prefs_box);
    return true;
}

static gboolean do_shuffle (Obj * obj, Invoc * invoc)
{
    FINISH2 (shuffle, aud_get_bool (nullptr, "shuffle"));
    return true;
}

static gboolean do_song_filename (Obj * obj, Invoc * invoc, unsigned pos)
{
    String filename = aud_playlist_entry_get_filename (CURRENT, pos);
    FINISH2 (song_filename, filename ? filename : "");
    return true;
}

static gboolean do_song_frames (Obj * obj, Invoc * invoc, unsigned pos)
{
    Tuple tuple = aud_playlist_entry_get_tuple (CURRENT, pos);
    FINISH2 (song_frames, aud::max (0, tuple.get_int (Tuple::Length)));
    return true;
}

static gboolean do_song_length (Obj * obj, Invoc * invoc, unsigned pos)
{
    Tuple tuple = aud_playlist_entry_get_tuple (CURRENT, pos);
    int length = aud::max (0, tuple.get_int (Tuple::Length));
    FINISH2 (song_length, length / 1000);
    return true;
}

static gboolean do_song_title (Obj * obj, Invoc * invoc, unsigned pos)
{
    Tuple tuple = aud_playlist_entry_get_tuple (CURRENT, pos);
    String title = tuple.get_str (Tuple::FormattedTitle);
    FINISH2 (song_title, title ? title : "");
    return true;
}

static gboolean do_song_tuple (Obj * obj, Invoc * invoc, unsigned pos, const char * key)
{
    Tuple::Field field = Tuple::field_by_name (key);
    Tuple tuple;
    GVariant * var;

    if (field >= 0)
        tuple = aud_playlist_entry_get_tuple (CURRENT, pos);

    switch (tuple.get_value_type (field))
    {
    case Tuple::String:
        var = g_variant_new_string (tuple.get_str (field));
        break;

    case Tuple::Int:
        var = g_variant_new_int32 (tuple.get_int (field));
        break;

    default:
        var = g_variant_new_string ("");
        break;
    }

    FINISH2 (song_tuple, g_variant_new_variant (var));
    return true;
}

static gboolean do_startup_notify (Obj * obj, Invoc * invoc, const char * id)
{
    aud_ui_startup_notify (id);
    FINISH (startup_notify);
    return true;
}

static gboolean do_status (Obj * obj, Invoc * invoc)
{
    const char * status = "stopped";
    if (aud_drct_get_playing ())
        status = aud_drct_get_paused () ? "paused" : "playing";

    FINISH2 (status, status);
    return true;
}

static gboolean do_stop (Obj * obj, Invoc * invoc)
{
    aud_drct_stop ();
    FINISH (stop);
    return true;
}

static gboolean do_stop_after (Obj * obj, Invoc * invoc)
{
    FINISH2 (stop_after, aud_get_bool (nullptr, "stop_after_current_song"));
    return true;
}

static gboolean do_stopped (Obj * obj, Invoc * invoc)
{
    FINISH2 (stopped, ! aud_drct_get_playing ());
    return true;
}

static gboolean do_time (Obj * obj, Invoc * invoc)
{
    FINISH2 (time, aud_drct_get_time ());
    return true;
}

static gboolean do_toggle_auto_advance (Obj * obj, Invoc * invoc)
{
    aud_set_bool (nullptr, "no_playlist_advance", ! aud_get_bool (nullptr, "no_playlist_advance"));
    FINISH (toggle_auto_advance);
    return true;
}

static gboolean do_toggle_repeat (Obj * obj, Invoc * invoc)
{
    aud_set_bool (nullptr, "repeat", ! aud_get_bool (nullptr, "repeat"));
    FINISH (toggle_repeat);
    return true;
}

static gboolean do_toggle_shuffle (Obj * obj, Invoc * invoc)
{
    aud_set_bool (nullptr, "shuffle", ! aud_get_bool (nullptr, "shuffle"));
    FINISH (toggle_shuffle);
    return true;
}

static gboolean do_toggle_stop_after (Obj * obj, Invoc * invoc)
{
    aud_set_bool (nullptr, "stop_after_current_song", ! aud_get_bool (nullptr, "stop_after_current_song"));
    FINISH (toggle_stop_after);
    return true;
}

static gboolean do_version (Obj * obj, Invoc * invoc)
{
    FINISH2 (version, VERSION);
    return true;
}

static gboolean do_volume (Obj * obj, Invoc * invoc)
{
    StereoVolume volume = aud_drct_get_volume ();
    FINISH2 (volume, volume.left, volume.right);
    return true;
}

static const struct
{
    const char * signal;
    GCallback callback;
}
handlers[] =
{
    {"handle-add", (GCallback) do_add},
    {"handle-add-list", (GCallback) do_add_list},
    {"handle-add-url", (GCallback) do_add_url},
    {"handle-advance", (GCallback) do_advance},
    {"handle-auto-advance", (GCallback) do_auto_advance},
    {"handle-balance", (GCallback) do_balance},
    {"handle-clear", (GCallback) do_clear},
    {"handle-delete", (GCallback) do_delete},
    {"handle-delete-active-playlist", (GCallback) do_delete_active_playlist},
    {"handle-eject", (GCallback) do_eject},
    {"handle-equalizer-activate", (GCallback) do_equalizer_activate},
    {"handle-get-active-playlist", (GCallback) do_get_active_playlist},
    {"handle-get-active-playlist-name", (GCallback) do_get_active_playlist_name},
    {"handle-get-eq", (GCallback) do_get_eq},
    {"handle-get-eq-band", (GCallback) do_get_eq_band},
    {"handle-get-eq-preamp", (GCallback) do_get_eq_preamp},
    {"handle-get-info", (GCallback) do_get_info},
    {"handle-get-playqueue-length", (GCallback) do_get_playqueue_length},
    {"handle-get-tuple-fields", (GCallback) do_get_tuple_fields},
    {"handle-info", (GCallback) do_info},
    {"handle-jump", (GCallback) do_jump},
    {"handle-length", (GCallback) do_length},
    {"handle-main-win-visible", (GCallback) do_main_win_visible},
    {"handle-new-playlist", (GCallback) do_new_playlist},
    {"handle-number-of-playlists", (GCallback) do_number_of_playlists},
    {"handle-open-list", (GCallback) do_open_list},
    {"handle-open-list-to-temp", (GCallback) do_open_list_to_temp},
    {"handle-pause", (GCallback) do_pause},
    {"handle-paused", (GCallback) do_paused},
    {"handle-play", (GCallback) do_play},
    {"handle-play-active-playlist", (GCallback) do_play_active_playlist},
    {"handle-play-pause", (GCallback) do_play_pause},
    {"handle-playing", (GCallback) do_playing},
    {"handle-playlist-add", (GCallback) do_playlist_add},
    {"handle-playlist-enqueue-to-temp", (GCallback) do_playlist_enqueue_to_temp},
    {"handle-playlist-ins-url-string", (GCallback) do_playlist_ins_url_string},
    {"handle-playqueue-add", (GCallback) do_playqueue_add},
    {"handle-playqueue-clear", (GCallback) do_playqueue_clear},
    {"handle-playqueue-is-queued", (GCallback) do_playqueue_is_queued},
    {"handle-playqueue-remove", (GCallback) do_playqueue_remove},
    {"handle-plugin-enable", (GCallback) do_plugin_enable},
    {"handle-plugin-is-enabled", (GCallback) do_plugin_is_enabled},
    {"handle-position", (GCallback) do_position},
    {"handle-queue-get-list-pos", (GCallback) do_queue_get_list_pos},
    {"handle-queue-get-queue-pos", (GCallback) do_queue_get_queue_pos},
    {"handle-quit", (GCallback) do_quit},
    {"handle-recording", (GCallback) do_recording},
    {"handle-record", (GCallback) do_record},
    {"handle-repeat", (GCallback) do_repeat},
    {"handle-reverse", (GCallback) do_reverse},
    {"handle-seek", (GCallback) do_seek},
    {"handle-set-active-playlist", (GCallback) do_set_active_playlist},
    {"handle-set-active-playlist-name", (GCallback) do_set_active_playlist_name},
    {"handle-set-eq", (GCallback) do_set_eq},
    {"handle-set-eq-band", (GCallback) do_set_eq_band},
    {"handle-set-eq-preamp", (GCallback) do_set_eq_preamp},
    {"handle-set-volume", (GCallback) do_set_volume},
    {"handle-show-about-box", (GCallback) do_show_about_box},
    {"handle-show-filebrowser", (GCallback) do_show_filebrowser},
    {"handle-show-jtf-box", (GCallback) do_show_jtf_box},
    {"handle-show-main-win", (GCallback) do_show_main_win},
    {"handle-show-prefs-box", (GCallback) do_show_prefs_box},
    {"handle-shuffle", (GCallback) do_shuffle},
    {"handle-song-filename", (GCallback) do_song_filename},
    {"handle-song-frames", (GCallback) do_song_frames},
    {"handle-song-length", (GCallback) do_song_length},
    {"handle-song-title", (GCallback) do_song_title},
    {"handle-song-tuple", (GCallback) do_song_tuple},
    {"handle-startup-notify", (GCallback) do_startup_notify},
    {"handle-status", (GCallback) do_status},
    {"handle-stop", (GCallback) do_stop},
    {"handle-stop-after", (GCallback) do_stop_after},
    {"handle-stopped", (GCallback) do_stopped},
    {"handle-time", (GCallback) do_time},
    {"handle-toggle-auto-advance", (GCallback) do_toggle_auto_advance},
    {"handle-toggle-repeat", (GCallback) do_toggle_repeat},
    {"handle-toggle-shuffle", (GCallback) do_toggle_shuffle},
    {"handle-toggle-stop-after", (GCallback) do_toggle_stop_after},
    {"handle-version", (GCallback) do_version},
    {"handle-volume", (GCallback) do_volume}
};

static GMainLoop * mainloop = nullptr;
static unsigned owner_id = 0;

static GDBusInterfaceSkeleton * skeleton = nullptr;

static void name_acquired (GDBusConnection *, const char * name, void *)
{
    AUDINFO ("Owned D-Bus name (%s) on session bus.\n", name);

    g_main_loop_quit (mainloop);
}

static void name_lost (GDBusConnection *, const char * name, void *)
{
    AUDINFO ("Owning D-Bus name (%s) failed, already taken?\n", name);

    g_bus_unown_name (owner_id);
    owner_id = 0;

    g_main_loop_quit (mainloop);
}

StartupType dbus_server_init (bool new_instance)
{
    auto startup = StartupType::Unknown;
    int instance = 1;

    GError * error = nullptr;
    GDBusConnection * bus = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, & error);
    GMainContext * context;

    if (! bus)
        goto ERROR;

    skeleton = (GDBusInterfaceSkeleton *) obj_audacious_skeleton_new ();

    for (auto & handler : handlers)
        g_signal_connect (skeleton, handler.signal, handler.callback, nullptr);

    if (! g_dbus_interface_skeleton_export (skeleton, bus, "/org/atheme/audacious", & error))
        goto ERROR;

    context = g_main_context_new ();
    g_main_context_push_thread_default (context);

    while (1)
    {
        StringBuf name = (instance == 1) ? str_copy ("org.atheme.audacious") :
                         str_printf ("org.atheme.audacious-%d", instance);

        owner_id = g_bus_own_name (G_BUS_TYPE_SESSION, name,
         (GBusNameOwnerFlags) 0, nullptr, name_acquired, name_lost, nullptr, nullptr);

        mainloop = g_main_loop_new (context, true);
        g_main_loop_run (mainloop);
        g_main_loop_unref (mainloop);
        mainloop = nullptr;

        // primary instance is not running, or we are starting a new instance
        // and have found an available bus name
        if (owner_id)
        {
            startup = StartupType::Server;
            aud_set_instance (instance);
            break;
        }

        // primary instance is already running, and we are not starting a new instance
        if (! new_instance)
        {
            startup = StartupType::Client;
            break;
        }

        // we are starting a new instance and have not found an available bus
        // name (we will start up anyway, but without D-Bus support)
        if (++ instance > 9)
        {
            AUDERR ("D-Bus error: No available bus names\n");
            break;
        }
    }

    g_main_context_pop_thread_default (context);
    g_main_context_unref (context);

ERROR:
    if (error)
    {
        AUDERR ("D-Bus error: %s\n", error->message);
        g_error_free (error);
    }

    if (startup != StartupType::Server)
        dbus_server_cleanup ();

    return startup;
}

void dbus_server_cleanup ()
{
    if (owner_id)
    {
        g_bus_unown_name (owner_id);
        owner_id = 0;
    }

    if (skeleton)
    {
        g_object_unref (skeleton);
        skeleton = nullptr;
    }
}
