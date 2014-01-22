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
	char * relations;

	len = snprintf(sql_buf, BALL_MAX_SQL_BUF_LEN,
			"SELECT relationship from %s WHERE name = '%s';",
			g_table_name,
			name);

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

	if (ntuples != 1 || nfields != 1)
	{
		fprintf(stderr, "tuple: %d and field: %d\n", ntuples, nfields);
		return FALSE;
	}

	relations = PQgetvalue(result, 0, 0);

	fprintf(stderr, "relations: %s\n", relations);
	ball_pack_relationship_packet(name, relations);

	PQclear(result);
	PQfinish(conn);

	return TRUE;
}
