#ifndef _GUI_CLIENT_H_
#define _GUI_CLIENT_H_

/********** FOLLOW DEFINE THE CHART PANEL ***************/

#define BALL_TYPE_CHART_PANEL (ball_chart_panel_get_type())
#define BALL_CHART_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BALL_TYPE_CHART_PANEL, BallChartPanel))
#define BALL_IS_CHART_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), BALL_TYPE_CHART_PANEL))
#define BALL_CHART_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), BALL_TYPE_CHART_PANEL, BallChartPanelClass))
#define BALL_IS_CHART_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BALL_TYPE_CHART_PANEL))
#define BALL_CHART_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), BALL_TYPE_CHART_PANEL, BallChartPanelClass))

typedef struct _BallChartPanel BallChartPanel;
typedef struct _BallChartPanelClass BallChartPanelClass;

struct _BallChartPanel
{
	GtkWindow parent;

	GtkWidget * msg_recv, * msg_send;
	GtkWidget * paned;
	GtkWidget * recv_scroll_window, * send_scroll_window;
	GtkTextBuffer * msg_recv_buffer, * msg_send_buffer;
	GtkTextTagTable * msg_tag_table;
	GtkWidget * msg_send_button_box, * send_button, * clear_button;
	GtkWidget * msg_send_view;
	GtkWidget * msg_reciver_view;
	GtkWidget * reciver_label, * recivers_combo_box;

	char * name;
	int name_len;
};

struct _BallChartPanelClass
{
	GtkWindowClass parent_class;
};

extern GtkWidget * ball_chart_panel_new();

/******************* FOLLOW DEFINE THE MAIN PANEL *******************/
#define BALL_TYPE_MAIN_PANEL (ball_main_panel_get_type())
#define BALL_MAIN_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BALL_TYPE_MAIN_PANEL, BallMainPanel))
#define BALL_IS_MAIN_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), BALL_TYPE_MAIN_PANEL))
#define BALL_MAIN_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), BALL_TYPE_MAIN_PANEL, BallMainPanelClass))
#define BALL_IS_MAIN_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BALL_TYPE_CHART_PANEL))
#define BALL_MAIN_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), BALL_TYPE_CHART_PANEL, BallMainPanelClass))

typedef struct _BallMainPanel BallMainPanel;
typedef struct _BallMainPanelClass BallMainPanelClass;

enum peer_info_em {
	PEER_INFO_NAME,
	PEER_INFO_MAX,
};

struct _BallMainPanel
{
	GtkWindow parent;

	GtkListStore * peer_list_store;
	GtkCellRenderer * name_renderer;
	GtkWidget * peer_list_view;
	GtkWidget * peer_scrolled_view;
};

struct _BallMainPanelClass
{
	GtkWindowClass parent_class;
};

extern GtkWidget * ball_main_panel_new();

/********************* FOLLOW DEFINE THE GENERAl STRUCT ******************/
struct peer_info {
	struct list_head next;

	char name[MAX_ACCOUNT_NAME_LEN];

	struct list_head msg_unshow;

	BallChartPanel * chart_panel;
};

extern char * ball_get_myself_name();

#endif
