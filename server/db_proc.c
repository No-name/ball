#include <stdio.h>

#include <libpq-fe.h>

#include "ball_def.h"

#include "list.h"
#include "ball.h"

#define BALL_MAX_SQL_BUF_LEN 1024
#define BALL_DB_CONN_INFO "dbname=ball user=ball password=123456"

static char * g_table_name = "memberinfo";
static char * g_name_col = "name";
static char * g_passwd_col = "passwd";

static PGconn * g_db_conn;

int ball_db_init_db_conn()
{
	g_db_conn = PQconnectdb(BALL_DB_CONN_INFO);
	if (CONNECTION_OK != PQstatus(g_db_conn))
	{
		fprintf(stderr, "%s\n", PQerrorMessage(g_db_conn));

		PQfinish(g_db_conn);

		g_db_conn = NULL;

		return FALSE;
	}

	return TRUE;
}

void ball_db_destroy_db_conn()
{
	PQfinish(g_db_conn);

	g_db_conn = NULL;
}

int ball_db_check_login(char * name, int name_len, char * passwd, int passwd_len)
{
	int len;
	char sql_buf[BALL_MAX_SQL_BUF_LEN];
	char name_null_term[MAX_ACCOUNT_NAME_LEN];
	int ntuples, nfields;
	PGconn * conn;
	PGresult * result;
	char * passwd_inner;

	/* as name not null terminate, we should construct */
	snprintf(name_null_term, name_len + 1, "%s", name);

	len = snprintf(sql_buf, BALL_MAX_SQL_BUF_LEN,
			"SELECT passwd from memberinfo WHERE name = '%s';", name_null_term);

	conn = PQconnectdb(BALL_DB_CONN_INFO);
	if (CONNECTION_OK != PQstatus(conn))
	{
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		return FALSE;
	}

	result = PQexec(conn, sql_buf);
	if (PGRES_TUPLES_OK != PQresultStatus(result))
	{
		fprintf(stderr, "%s\n", PQresultErrorMessage(result));
		return FALSE;
	}

	ntuples = PQntuples(result);
	nfields = PQnfields(result);

	if (ntuples == 0)
		return BALL_LOGIN_ACCOUNT_FAILED;

	passwd_inner = PQgetvalue(result, 0, 0);
	fprintf(stderr, "passwd in db: %s\n", passwd_inner);

	if (!strncmp(passwd_inner, passwd, passwd_len))
		return BALL_LOGIN_SUCCESS;

	return BALL_LOGIN_PASSWD_FAILED;
}

int ball_insert_member_info(char * name, char * passwd, char * sql_buf, int buf_len)
{
	int len;

	len = snprintf(sql_buf, buf_len, "INSERT INTO %s (%s, %s) VALUES ('%s', '%s');", g_table_name, g_name_col, g_passwd_col, name, passwd);

	if (len == buf_len)
		return 0;

	return 1;
}

int ball_add_member_relationship(char * name, char * relation, char * sql_buf, int buf_len)
{
	int len;
	len = snprintf(sql_buf, buf_len, "UPDATE %s SET relationship = ARRAY_APPEND(relationship, '%s') WHERE name = '%s';", g_table_name, relation, name);

	if (len == buf_len)
		return 0;

	return 1;
}

int ball_respond_relationship(char * name)
{
	int len;
	char sql_buf[1024];
	int ntuples, nfields;
	PGconn * conn;
	PGresult * result;
	char * member_name;
	int i;

	len = snprintf(sql_buf, BALL_MAX_SQL_BUF_LEN,
			"SELECT member from relationship WHERE master = '%s';", name);

	if (len == BALL_MAX_SQL_BUF_LEN)
		return FALSE;

	conn = PQconnectdb(BALL_DB_CONN_INFO);
	if (CONNECTION_OK != PQstatus(conn))
	{
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		return FALSE;
	}

	result = PQexec(conn, sql_buf);
	if (PGRES_TUPLES_OK != PQresultStatus(result))
	{
		fprintf(stderr, "%s\n", PQresultErrorMessage(result));
		return FALSE;
	}

	ntuples = PQntuples(result);
	nfields = PQnfields(result);

	if (ntuples == 0)
		return TRUE;

	for (i = 0; i < ntuples; ++i)
	{
		member_name = PQgetvalue(result, i, 0);

		ball_pack_relationship_packet(name, member_name);
	}

	PQclear(result);
	PQfinish(conn);

	return TRUE;
}
