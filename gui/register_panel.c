/*  file maybe no need no compile */
#include <gtk/gtk.h>

#include "gui_client.h"

void ball_register_panel_init(BallRegisterPanel * register_panel, BallRegisterPanelClass * register_panel_class)
{
	GtkWidget * grid;
	GtkWidget * button_box;

	GtkWidget * window = GTK_WIDGET(register_panel);

	gtk_widget_set_size_request(window, 400, 300);
	gtk_window_set_title(GTK_WINDOW(window), "Register");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

	login_panel->label_name = gtk_label_new("Name");
	gtk_widget_set_halign(login_panel->label_name, GTK_ALIGN_START);
	login_panel->label_passwd = gtk_label_new("Passwd");
	gtk_widget_set_halign(login_panel->label_passwd, GTK_ALIGN_START);
	login_panel->label_passwd_again = gtk_label_new("Passwd Again");
	gtk_widget_set_halign(login_panel->label_passwd_again, GTK_ALIGN_START);

	login_panel->entry_name = gtk_entry_new();
	gtk_widget_set_hexpand(login_panel->entry_name, TRUE);

	login_panel->entry_passwd = gtk_entry_new();
	gtk_widget_set_hexpand(login_panel->entry_passwd, TRUE);

	login_panel->entry_passwd_again = gtk_entry_new();
	gtk_widget_set_hexpand(login_panel->entry_passwd_again, TRUE);

	button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing(GTK_BOX(button_box), 5);
	gtk_widget_set_margin_top(button_box, 30);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_CENTER);
	gtk_grid_attach(GTK_GRID(grid), button_box, 0, 2, 2, 1);

	login_panel->button_login = gtk_button_new_with_label("Login");
	login_panel->button_cancel = gtk_button_new_with_label("Cancel");

	gtk_box_pack_start(GTK_BOX(button_box), login_panel->button_login, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(button_box), login_panel->button_cancel, FALSE, FALSE, 5);

	g_signal_connect(login_panel->button_login, "clicked", G_CALLBACK(ball_process_login), login_panel);

}

GType ball_register_panel_get_type()
{
	static GType type;

	if (!type)
	{
		GTypeInfo typeInfo = {
			sizeof(BallRegisterPanelClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(BallRegisterPanel),
			0,
			(GInstanceInitFunc)ball_register_panel_init,
			NULL,
		};

		type = g_type_register_static(GTK_TYPE_WINDOW, "register_panel", &typeInfo, 0);
	}

	return type;
}

GtkWidget * ball_register_panel_new()
{
	return GTK_WIDGET(g_object_new(BALL_TYPE_REGISTER_PANEL, "type", GTK_WINDOW_TOPLEVEL, NULL));
}
