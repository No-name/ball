#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "ball.h"

#include "gui_client.h"

extern struct list_head message_need_sended;
extern GMutex mutex_for_message_need_sended;

extern struct list_head message_comming;
extern GMutex mutex_for_message_comming;

void ball_process_login(GtkWidget * button, BallLoginPanel * login_panel)
{
	GtkWidget * window;

	struct message_packet * msg;

	int name_len, passwd_len;
	gchar * name, * passwd;

	name = gtk_entry_get_text(GTK_ENTRY(login_panel->entry_name));
	name_len = strlen(name);

	passwd = gtk_entry_get_text(GTK_ENTRY(login_panel->entry_passwd));
	passwd_len = strlen(passwd);

	if (!(name_len && passwd_len))
		return;

	msg = malloc(sizeof(struct message_packet));

	msg->type = MSG_TYPE_LOGIN;
	msg->version = 0x01;

	msg->login_info.name_len = name_len;
	msg->login_info.name = name;

	msg->login_info.passwd_len = passwd_len;
	msg->login_info.passwd = passwd;

	package_message(msg);

	g_mutex_lock(&mutex_for_message_need_sended);
	list_add_tail(&msg->next, &message_need_sended);
	g_mutex_unlock(&mutex_for_message_need_sended);

	ball_set_myself_name(name);

	window = ball_main_panel_new();
	gtk_widget_show_all(window);
	gtk_window_set_title(GTK_WINDOW(window), name);

	ball_set_main_panel(BALL_MAIN_PANEL(window));

	gtk_widget_destroy(GTK_WIDGET(login_panel));
}

void ball_login_panel_init(BallLoginPanel * login_panel, BallLoginPanelClass * login_panel_class)
{
	GtkWidget * grid;
	GtkWidget * button_box;

	GtkWidget * window = GTK_WIDGET(login_panel);

	gtk_widget_set_size_request(window, 400, 300);
	gtk_window_set_title(GTK_WINDOW(window), "Hi");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	//g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	grid = gtk_grid_new();
	gtk_widget_set_margin_top(grid, 40);
	gtk_widget_set_margin_left(grid, 50);
	gtk_widget_set_margin_right(grid, 50);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
	gtk_container_add(GTK_CONTAINER(window), grid);

	login_panel->label_name = gtk_label_new("Name");
	gtk_widget_set_halign(login_panel->label_name, GTK_ALIGN_START);
	login_panel->label_passwd = gtk_label_new("Passwd");
	gtk_widget_set_halign(login_panel->label_passwd, GTK_ALIGN_START);

	login_panel->entry_name = gtk_entry_new();
	gtk_widget_set_hexpand(login_panel->entry_name, TRUE);

	login_panel->entry_passwd = gtk_entry_new();
	gtk_widget_set_hexpand(login_panel->entry_passwd, TRUE);

	gtk_grid_attach(GTK_GRID(grid), login_panel->label_name, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), login_panel->label_passwd, 0, 1, 1, 1);

	gtk_grid_attach(GTK_GRID(grid), login_panel->entry_name, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), login_panel->entry_passwd, 1, 1, 1, 1);

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

GType ball_login_panel_get_type()
{
	static GType type;
	
	if (!type)
	{
		GTypeInfo typeInfo = {
			sizeof(BallLoginPanelClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(BallLoginPanel),
			0,
			(GInstanceInitFunc)ball_login_panel_init,
			NULL,
		};

		type = g_type_register_static(GTK_TYPE_WINDOW, "login_panel", &typeInfo, 0);
	}

	return type;
}

GtkWidget * ball_login_panel_new()
{
	return GTK_WIDGET(g_object_new(BALL_TYPE_LOGIN_PANEL, "type", GTK_WINDOW_TOPLEVEL, NULL));
}
