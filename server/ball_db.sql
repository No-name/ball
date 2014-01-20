--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

--
-- Name: plpgsql; Type: EXTENSION; Schema: -; Owner: 
--

CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner: 
--

COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


SET search_path = public, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: memberinfo; Type: TABLE; Schema: public; Owner: ball; Tablespace: 
--

CREATE TABLE memberinfo (
    id integer NOT NULL,
    name character varying(80) NOT NULL,
    relationship character varying(80)[],
    passwd character varying(50) NOT NULL
);


ALTER TABLE public.memberinfo OWNER TO ball;

--
-- Name: memberinfo_id_seq; Type: SEQUENCE; Schema: public; Owner: ball
--

CREATE SEQUENCE memberinfo_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.memberinfo_id_seq OWNER TO ball;

--
-- Name: memberinfo_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: ball
--

ALTER SEQUENCE memberinfo_id_seq OWNED BY memberinfo.id;


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: ball
--

ALTER TABLE ONLY memberinfo ALTER COLUMN id SET DEFAULT nextval('memberinfo_id_seq'::regclass);


--
-- Data for Name: memberinfo; Type: TABLE DATA; Schema: public; Owner: ball
--

COPY memberinfo (id, name, relationship, passwd) FROM stdin;
\.


--
-- Name: memberinfo_id_seq; Type: SEQUENCE SET; Schema: public; Owner: ball
--

SELECT pg_catalog.setval('memberinfo_id_seq', 1, false);


--
-- Name: memberinfo_pkey; Type: CONSTRAINT; Schema: public; Owner: ball; Tablespace: 
--

ALTER TABLE ONLY memberinfo
    ADD CONSTRAINT memberinfo_pkey PRIMARY KEY (name);


--
-- Name: public; Type: ACL; Schema: -; Owner: postgres
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM postgres;
GRANT ALL ON SCHEMA public TO postgres;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- PostgreSQL database dump complete
--

