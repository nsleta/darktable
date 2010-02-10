#include "common/darktable.h"
#include "control/control.h"
#include "control/jobs.h"
#include "control/conf.h"
#include "gui/gtk.h"
#include "libs/lib.h"
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <lcms.h>

DT_MODULE

typedef struct dt_lib_export_t
{
  GtkSpinButton *width, *height;
  GtkComboBox *format, *profile, *intent;
  GtkScale *quality;
  GList *profiles;
}
dt_lib_export_t;

typedef struct dt_lib_export_profile_t
{
  char filename[512]; // icc file name
  char name[512];     // product name
  int  pos;           // position in combo box    
}
dt_lib_export_profile_t;

const char*
name ()
{
  return _("export selected");
}

static void
export_button_clicked (GtkWidget *widget, gpointer user_data)
{
  dt_lib_module_t *self = (dt_lib_module_t *)user_data;
  dt_lib_export_t *d = (dt_lib_export_t *)self->data;
  // read "format" to global settings
  int i = gtk_combo_box_get_active(d->format);
  if     (i == 0)  dt_conf_set_int ("plugins/lighttable/export/format", DT_DEV_EXPORT_JPG);
  else if(i == 1)  dt_conf_set_int ("plugins/lighttable/export/format", DT_DEV_EXPORT_PNG);
  else if(i == 2)  dt_conf_set_int ("plugins/lighttable/export/format", DT_DEV_EXPORT_PPM16);
  else if(i == 3)  dt_conf_set_int ("plugins/lighttable/export/format", DT_DEV_EXPORT_PFM);
  dt_control_export();
}

static void
export_quality_changed (GtkRange *range, gpointer user_data)
{
  GtkWidget *widget;
  int quality = (int)gtk_range_get_value(range);
  dt_conf_set_int ("plugins/lighttable/export/quality", quality);
  widget = glade_xml_get_widget (darktable.gui->main_window, "center");
  gtk_widget_queue_draw(widget);
}

static void
width_changed (GtkSpinButton *spin, gpointer user_data)
{
  int value = gtk_spin_button_get_value(spin);
  dt_conf_set_int ("plugins/lighttable/export/width", value);
}

static void
height_changed (GtkSpinButton *spin, gpointer user_data)
{
  int value = gtk_spin_button_get_value(spin);
  dt_conf_set_int ("plugins/lighttable/export/height", value);
}

void
gui_reset (dt_lib_module_t *self)
{
  dt_lib_export_t *d = (dt_lib_export_t *)self->data;
  int quality = MIN(100, MAX(1, dt_conf_get_int ("plugins/lighttable/export/quality")));
  gtk_range_set_value(GTK_RANGE(d->quality), quality);
  int k = dt_conf_get_int ("plugins/lighttable/export/format");
  int i = 0;
  if     (k == DT_DEV_EXPORT_JPG)   i = 0;
  else if(k == DT_DEV_EXPORT_PNG)   i = 1;
  else if(k == DT_DEV_EXPORT_PPM16) i = 2;
  else if(k == DT_DEV_EXPORT_PFM)   i = 3;
  gtk_combo_box_set_active(d->format, i);
  gtk_combo_box_set_active(d->intent, (int)dt_conf_get_int("plugins/lighttable/export/iccintent") + 1);
  int iccfound = 0;
  gchar *iccprofile = dt_conf_get_string("plugins/lighttable/export/iccprofile");
  GList *prof = d->profiles;
  while(prof)
  {
    dt_lib_export_profile_t *pp = (dt_lib_export_profile_t *)prof->data;
    if(!strcmp(pp->filename, iccprofile))
    {
      gtk_combo_box_set_active(d->profile, pp->pos);
      iccfound = 1;
    }
    if(iccfound) break;
    prof = g_list_next(prof);
  }
  g_free(iccprofile);
  gtk_combo_box_set_active(d->profile, 0);
}

static void
profile_changed (GtkComboBox *widget, dt_lib_export_t *d)
{
  int pos = gtk_combo_box_get_active(widget);
  GList *prof = d->profiles;
  while(prof)
  { // could use g_list_nth. this seems safer?
    dt_lib_export_profile_t *pp = (dt_lib_export_profile_t *)prof->data;
    if(pp->pos == pos)
    {
      dt_conf_set_string("plugins/lighttable/export/iccprofile", pp->filename);
      return;
    }
    prof = g_list_next(prof);
  }
  dt_conf_set_string("plugins/lighttable/export/iccprofile", "image");
}

static void
intent_changed (GtkComboBox *widget, dt_lib_export_t *d)
{
  int pos = gtk_combo_box_get_active(widget);
  dt_conf_set_int("plugins/lighttable/export/iccintent", pos-1);
}

void
gui_init (dt_lib_module_t *self)
{
  dt_lib_export_t *d = (dt_lib_export_t *)malloc(sizeof(dt_lib_export_t));
  self->data = (void *)d;
  self->widget = gtk_vbox_new(FALSE, 5);
  
  GtkBox *hbox;
  GtkWidget *label;
  hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
  d->width  = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 10000, 1));
  gtk_object_set(GTK_OBJECT(d->width), "tooltip-text", _("maximum output width\nset to 0 for no scaling"), NULL);
  d->height = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 10000, 1));
  gtk_object_set(GTK_OBJECT(d->height), "tooltip-text", _("maximum output height\nset to 0 for no scaling"), NULL);
  label = gtk_label_new(_("maximum size"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(hbox, label, TRUE, TRUE, 5);
  gtk_box_pack_start(hbox, GTK_WIDGET(d->width), FALSE, FALSE, 5);
  gtk_box_pack_start(hbox, gtk_label_new(_("x")), FALSE, FALSE, 5);
  gtk_box_pack_start(hbox, GTK_WIDGET(d->height), FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(hbox), TRUE, TRUE, 0);

  hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
  d->quality = GTK_SCALE(gtk_hscale_new_with_range(0, 100, 1));
  gtk_scale_set_value_pos(d->quality, GTK_POS_LEFT);
  gtk_range_set_value(GTK_RANGE(d->quality), 97);
  gtk_scale_set_digits(d->quality, 0);
  gtk_box_pack_start(hbox, gtk_label_new(_("quality")), FALSE, FALSE, 5);
  gtk_box_pack_start(hbox, GTK_WIDGET(d->quality), TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(hbox), TRUE, TRUE, 0);

  hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(hbox), TRUE, TRUE, 0);
  GtkBox *vbox1 = GTK_BOX(gtk_vbox_new(FALSE, 0));
  GtkBox *vbox2 = GTK_BOX(gtk_vbox_new(FALSE, 0));
  gtk_box_pack_start(hbox, GTK_WIDGET(vbox1), FALSE, FALSE, 5);
  gtk_box_pack_start(hbox, GTK_WIDGET(vbox2), TRUE, TRUE, 5);
  label = gtk_label_new(_("rendering intent"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(vbox1, label, TRUE, TRUE, 0);
  d->intent = GTK_COMBO_BOX(gtk_combo_box_new_text());
  gtk_combo_box_append_text(d->intent, _("image settings"));
  gtk_combo_box_append_text(d->intent, _("perceptual"));
  gtk_combo_box_append_text(d->intent, _("relative colorimetric"));
  gtk_combo_box_append_text(d->intent, _("saturation"));
  gtk_combo_box_append_text(d->intent, _("absolute colorimetric"));
  gtk_box_pack_start(vbox2, GTK_WIDGET(d->intent), TRUE, TRUE, 0);

  d->profiles = NULL;
  dt_lib_export_profile_t *prof = (dt_lib_export_profile_t *)malloc(sizeof(dt_lib_export_profile_t));
  strcpy(prof->filename, "sRGB");
  strcpy(prof->name, "sRGB");
  int pos;
  prof->pos = 1;
  d->profiles = g_list_append(d->profiles, prof);
  prof = (dt_lib_export_profile_t *)malloc(sizeof(dt_lib_export_profile_t));
  strcpy(prof->filename, "X profile");
  strcpy(prof->name, "X profile");
  pos = prof->pos = 2;
  d->profiles = g_list_append(d->profiles, prof);

  // read datadir/color/out/*.icc
  char datadir[1024], dirname[1024], filename[1024];
  dt_get_datadir(datadir, 1024);
  snprintf(dirname, 1024, "%s/color/out", datadir);
  cmsHPROFILE tmpprof;
  const gchar *d_name;
  GDir *dir = g_dir_open(dirname, 0, NULL);
  (void)cmsErrorAction(LCMS_ERROR_IGNORE);
  if(dir)
  {
    while((d_name = g_dir_read_name(dir)))
    {
      snprintf(filename, 1024, "%s/%s", dirname, d_name);
      tmpprof = cmsOpenProfileFromFile(filename, "r");
      if(tmpprof)
      {
        dt_lib_export_profile_t *prof = (dt_lib_export_profile_t *)malloc(sizeof(dt_lib_export_profile_t));
        strcpy(prof->name, cmsTakeProductDesc(tmpprof));
        strcpy(prof->filename, d_name);
        prof->pos = ++pos;
        cmsCloseProfile(tmpprof);
        d->profiles = g_list_append(d->profiles, prof);
      }
    }
    g_dir_close(dir);
  }
  GList *l = d->profiles;
  label = gtk_label_new(_("output profile"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start(vbox1, label, TRUE, TRUE, 0);
  d->profile = GTK_COMBO_BOX(gtk_combo_box_new_text());
  gtk_box_pack_start(vbox2, GTK_WIDGET(d->profile), TRUE, TRUE, 0);
  gtk_combo_box_append_text(d->profile, _("image settings"));
  while(l)
  {
    dt_lib_export_profile_t *prof = (dt_lib_export_profile_t *)l->data;
    if(!strcmp(prof->name, "X profile"))
      gtk_combo_box_append_text(d->profile, _("system display profile"));
    else
      gtk_combo_box_append_text(d->profile, prof->name);
    l = g_list_next(l);
  }
  gtk_combo_box_set_active(d->profile, 0);
  char tooltip[1024];
  snprintf(tooltip, 1024, _("icc profiles in %s/color/out"), datadir);
  gtk_object_set(GTK_OBJECT(d->profile), "tooltip-text", tooltip, NULL);
  g_signal_connect (G_OBJECT (d->intent), "changed",
                    G_CALLBACK (intent_changed),
                    (gpointer)d);
  g_signal_connect (G_OBJECT (d->profile), "changed",
                    G_CALLBACK (profile_changed),
                    (gpointer)d);

  hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
  d->format = GTK_COMBO_BOX(gtk_combo_box_new_text());
  gtk_combo_box_append_text(d->format, _("8-bit jpg"));
  gtk_combo_box_append_text(d->format, _("8-bit png"));
  gtk_combo_box_append_text(d->format, _("16-bit ppm"));
  gtk_combo_box_append_text(d->format, _("float pfm"));
  GtkButton *button = GTK_BUTTON(gtk_button_new_with_label(_("export")));
  gtk_box_pack_start(hbox, GTK_WIDGET(d->format), TRUE, TRUE, 0);
  gtk_box_pack_start(hbox, GTK_WIDGET(button), FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(hbox), TRUE, TRUE, 0);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (export_button_clicked),
                    (gpointer)self);
  g_signal_connect (G_OBJECT (d->quality), "value-changed",
                    G_CALLBACK (export_quality_changed),
                    (gpointer)0);
  g_signal_connect (G_OBJECT (d->width), "value-changed",
                    G_CALLBACK (width_changed),
                    (gpointer)0);
  g_signal_connect (G_OBJECT (d->height), "value-changed",
                    G_CALLBACK (height_changed),
                    (gpointer)0);
}

void
gui_cleanup (dt_lib_module_t *self)
{
  free(self->data);
  self->data = NULL;
}


