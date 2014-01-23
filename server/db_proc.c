#include <stdio.h>

#include <libpq-fe.h>

#include "ball_def.h"

#define BALL_MAX_SQL_BUF_LEN 1024
#define BALL_DB_CONN_INFO "dbname=ball user=ball password=123456"

static char * g_table_name = "memberinfo";
static char * g_name_col = "name";
static char * g_passwd_col = "passwd";

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

	/* we only select one field */
	if (nfields != 1)
	{
		fprintf(stderr, "field select failed: %d field\n", ntuples, nfields);
		return FALSE;
	}

	for (i = 0; i < ntuples; ++i)
	{
		member_name = PQgetvalue(result, i, 0);

		ball_pack_relationship_packet(name, member_name);
	}

	PQclear(result);
	PQfinish(conn);

	return TRUE;
}
