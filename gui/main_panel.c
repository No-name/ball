#include <gtk/gtk.h>

#define BALL_TYPE_MAIN_PANEL (ball_main_panel_get_type())
#define BALL_MAIN_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BALL_TYPE_MAIN_PANEL, BallMainPanel))
#define BALL_IS_MAIN_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), BALL_TYPE_MAIN_PANEL))
#define BALL_MAIN_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), BALL_TYPE_MAIN_PANEL, BallMainPanelClass))
#define BALL_IS_MAIN_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BALL_TYPE_CHART_PANEL))
#define BALL_CHART_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), BALL_TYPE_CHART_PANEL, BallMainPanelClass))

typedef struct _BallMainPanel BallMainPanel;
typedef struct _BallMainPanelClass BallMainPanelClass;

enum peer_info_em {
	PEER_INFO_NAME,
	PEER_INFO_MAX,
};

GType peer_info_type[PEER_INFO_MAX] = 
{
	[PEER_INFO_NAME] = G_TYPE_STRING,
};

struct _BallMainPanel
{
	GtkWindow parent;

	GtkListStore * peer_list;
	GtkCellRenderer * name_renderer;
	GtkWidget * peer_list_view;
	GtkWidget * peer_scrolled_view;
};

struct _BallMainPanelClass
{
	GtkWindowClass parent_class;
};

void ball_main_panel_init(BallMainPanel * main_panel, BallMainPanelClass * main_panel_class)
{
	GtkTreeIter iter;
	GtkWidget * window = GTK_WIDGET(main_panel);

	gtk_window_set_title(GTK_WINDOW(window), "Nice Day");
	gtk_widget_set_size_request(window, 350, 800);
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	main_panel->peer_scrolled_view = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(window), main_panel->peer_scrolled_view);

	main_panel->peer_list = gtk_list_store_new(PEER_INFO_MAX, G_TYPE_STRING);

	main_panel->peer_list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(main_panel->peer_list));
	gtk_container_add(GTK_CONTAINER(main_panel->peer_scrolled_view), main_panel->peer_list_view);

	main_panel->name_renderer = gtk_cell_renderer_text_new();

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(main_panel->peer_list_view), -1, "Peer Name", main_panel->name_renderer, "text", PEER_INFO_NAME, NULL);

	/*
	 * for test insert some peer name to the store
	 */

	gtk_list_store_insert_with_values(main_panel->peer_list, &iter, -1, PEER_INFO_NAME, "Obama", -1);
	gtk_list_store_insert_with_values(main_panel->peer_list, &iter, -1, PEER_INFO_NAME, "Horbit", -1);
}

GType ball_main_panel_get_type()
{
	static GType type;

	if (!type)
	{
		GTypeInfo typeInfo = {
			sizeof(BallMainPanelClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(BallMainPanel),
			0,
			(GInstanceInitFunc)ball_main_panel_init,
			NULL,
		};

		type = g_type_register_static(GTK_TYPE_WINDOW, "main_panel", &typeInfo, 0);
	}

	return type;
}

GtkWidget * ball_main_panel_new()
{
	return GTK_WIDGET(g_object_new(BALL_TYPE_MAIN_PANEL, "type", GTK_WINDOW_TOPLEVEL, NULL));
}

int main(int ac, char ** av)
{
	GtkWidget * window;

	gtk_init(&ac, &av);

	window = ball_main_panel_new();

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
