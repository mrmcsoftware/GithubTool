/*********************************************************
   Sample program to access Github.com's API
   Copyright © 2023 Mark Craig

   https://www.youtube.com/MrMcSoftware
**********************************************************

To compile, you'll need both OpenSSL and the Jansson JSON
API library (https://github.com/akheron/jansson) installed
(or use yyjson (https://github.com/ibireme/yyjson))

Linux, compile with:

    cc -o github github.c -lssl -ljansson
or, to use yyjson instead of jansson:
    cc -DYYJSON -I. -o github github.c -lssl

Windows, if using Microsoft Visual C compiler, compile with:

    cl github.c /MD libcrypto.lib libssl.lib ws2_32.lib
or, to use yyjson instead of jansson:
    cl github.c /D YYJSON /MD -I. libcrypto.lib libssl.lib ws2_32.lib
or, to compile with Visual Studio 6 (or is it old Microsoft SDK):
    cl github.c /D NO_GETADDRINFO /D YYJSON /MD -I. libcrypto.lib libssl.lib ws2_32.lib

**********************************************************
NOTE: The network code is based on code from the internet.
Unfortunately, I can't remember where I saw it, so can't
give attribution.
**********************************************************/

#define _USING_V110_SDK71_ // To get rid of sal_supp.h __useHeader warnings, etc

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/ssl.h>
#ifdef YYJSON
#include <janssonlike.h>
#else
#include <jansson.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

//#define NO_GETADDRINFO

#ifdef NO_GETADDRINFO

typedef struct addrinfo
	{
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	size_t ai_addrlen;
	char *ai_canonname;
	struct sockaddr *ai_addr;
	struct addrinfo *ai_next;
	} ADDRINFO;

struct addrinfo _addrInfo;
struct sockaddr_in _sockAddr;

// NOTE: you can use gai_strerror from Microsoft SDK v7.1 and beyond.
//       I didn't include it here because it might be copyrighted

#endif

#ifdef YYJSON

yyjson_doc *_jsonDoc = NULL;

#endif

SSL_CTX *ssl_ctx;
SSL *conn;
int sockfd = 0;
struct addrinfo *res = NULL;
char *port = "https", name[256];
char *ansicols[2][11] =				// ANSI color escape codes
	{{ // light mode colors
		// probably for light mode, should either disable color (-C) or choose
		// more appropriate colors
	"\033[0;30;7m", /* black reverse */
	"\033[0;33;3m", /* dark yellow */
	"\033[0;36;3m", /* dark cyan */
	"\033[0;32;3m", /* dark green */
	"\033[97;41m", /* red reverse white letters */
	//"\033[0;31;7m", /* red reverse black letters */
	//"\033[0;91;7m", /* bright red reverse black letters */
	"\033[0;91;1m", /* bold red */
	"\033[0;95;1m", /* bold bright magenta */
	"\033[0;34;1m", /* bold blue */
	"\033[0;93;7m", /* bright yellow reverse */
	"\033[0;30;3m", /* dark gray */
	"\033[0;30;1m" /*  bold black */
	},
	{ // dark mode colors
	"\033[0;37;7m", /* white reverse */
	"\033[0;33;1m", /* bold yellow */
	"\033[0;36;1m", /* bold cyan */
	"\033[0;32;1m", /* bold green */
	"\033[97;41m", /* red reverse white letters */
	//"\033[0;31;7m", /* red reverse black letters */
	//"\033[0;91;7m", /* bright red reverse black letters */
	"\033[0;91;1m", /* bold red */
	"\033[0;95;1m", /* bold bright magenta */
	"\033[0;34;1m", /* bold blue */
	"\033[0;93;7m", /* bright yellow reverse */
	"\033[0;37;0m", /* white */
	"\033[0;37;1m" /* bold white */
	}};
char **cols = ansicols[1];
int col = 1, b = 10, bw = 1;		// color output variables/flags
int per_page = 100, page = 0;		// result pagination variables
// These buffers should be more than enough (I know, should dyn. allocate)
char request[4096];
char response[655360];

char *getUserName(char *);

int init_connection(char *hostname, char *port, struct addrinfo **res)
{
struct addrinfo hints;

#ifdef NO_GETADDRINFO

struct hostent *host;
char *IP;

host = gethostbyname(hostname);
if (host == NULL) { return(WSAGetLastError()); }
IP = inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
_sockAddr.sin_family = AF_INET;
_sockAddr.sin_port = htons(443); // https
_sockAddr.sin_addr.s_addr = inet_addr(IP);
_addrInfo.ai_addr = (struct sockaddr *)&_sockAddr;
_addrInfo.ai_family = AF_INET;
_addrInfo.ai_socktype = SOCK_STREAM;
_addrInfo.ai_protocol = IPPROTO_TCP;
_addrInfo.ai_addrlen = sizeof(struct sockaddr);
*res=&_addrInfo;
return(0);

#else

memset(&hints, 0, sizeof(hints));
hints.ai_family = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;
return(getaddrinfo(hostname, port, &hints, res));

#endif
}

int make_connection(struct addrinfo *res)
{
int sockfd, status;

sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
if (sockfd <= 0) { return(-1); }
status = connect(sockfd, res->ai_addr, res->ai_addrlen);
if (status != 0) { return(-1); }
return(sockfd);
}

int make_request(int sockfd, char *hostname, char *request_path)
{
int bytes_sent;

sprintf(request, "GET %s HTTP/1.0\r\nUser-Agent: Mozilla/5.0\r\nHost: %s\r\nConnection: close\r\n\r\n", request_path, hostname);
//printf("request='%s'\n", request);
bytes_sent = SSL_write(conn, request, strlen(request));
return(bytes_sent);
}

int fetch_response(int sockfd)
{
size_t bytes_received;
int status = 0, total = 0;
char *p = response;

while (1)
	{
	bytes_received = SSL_read(conn, p, 1024);
	p += bytes_received;
	total += bytes_received;
	if (bytes_received == -1) { return(-1); }
	if (bytes_received == 0) { break; }
	}
response[total] = '\0';
return(status);
}

error()
{
if (sockfd > 0)
	{
#ifdef _WIN32
	closesocket(sockfd);
#else
	close(sockfd);
#endif
	}
#ifndef NO_GETADDRINFO
if (res != NULL) { freeaddrinfo(res); }
#endif
// probably should free/close SSL stuff
exit(1);
}

getJSON(char *hostname, char *path)
{
int status = 0;
int err;

sockfd = make_connection(res);
if (!(sockfd > 0)) { fprintf(stderr, "Could not make connection to '%s' on port '%s'\n", hostname, port); error(); }
conn = SSL_new(ssl_ctx);
SSL_set_fd(conn, sockfd);
err = SSL_connect(conn);
if (err != 1) { printf("SSL error\n"); }
status = make_request(sockfd, hostname, path);
if (!(status > 0)) { fprintf(stderr, "Sending request failed\n"); error(); }
status = fetch_response(sockfd);
if (!(status >= 0)) { fprintf(stderr, "Fetching response failed\n"); error(); }
if ((SSL_shutdown(conn)) == 0) { SSL_shutdown(conn); }
SSL_free(conn);
#ifdef _WIN32
closesocket(sockfd);
#else
close(sockfd);
#endif
}

main(int argc, char **argv)
{
int status = 0, i, repos = 1, user = 1, gists = 1, override = 0;
char *hostname = "api.github.com";
#ifdef _WIN32
WSADATA wd = {0};
WSAStartup(WINSOCK_VERSION, &wd);
#endif

#ifdef _WIN32
tzset();
#endif
for (i = 1; i < argc; i++)
	{
	if (argv[i][0] == '-')
		{
		switch (argv[i][1])
			{
			case 'C': col = 0; break;
			case 'c': override = 1; break;
			case 'b': bw = 1-bw; b = 9+bw; break;
			case 'P': sscanf(argv[i]+2, "%d", &per_page); break;
			case 'p': sscanf(argv[i]+2, "%d", &page); break;
			case 'r': repos = 0; break;
			case 'u': user = 0; break;
			case 'g': gists = 0; break;
			case 'l': cols = ansicols[0]; break;
			case 'h':
			default:
				printf("Usage: github {name} [-C] [-c] [-b] [-P{#}] [-p{#}] [-r] [-u] [-g] [-l] [-h]\n");
				printf("              -C - Don't use color output\n");
				printf("              -c - Output ANSI colors even if output redirected\n");
				printf("              -b - Toggle use of bold white for data\n");
				printf("              -P - Specify results per page (max 100)\n");
				printf("              -p - Specify page number of results\n");
				printf("              -r - Don't list user's repositories\n");
				printf("              -u - Don't list user's information\n");
				printf("              -g - Don't list user's gists\n");
				printf("              -l - Use light mode colors\n");
				printf("              -h - Help\n");
				exit(1);
			}
		}
	else { strcpy(name, argv[i]); }
	}
status = init_connection(hostname, port, &res);
// Change if you have gai_strerror:
#if WIN32 && _MSC_VER == 1200
if (status != 0) { fprintf(stderr, "Could not resolve host: %d\n", status); error(); }
#else
if (status != 0) { fprintf(stderr, "Could not resolve host: %s\n", gai_strerror(status)); error(); }
#endif
SSL_load_error_strings();
SSL_library_init();
ssl_ctx = SSL_CTX_new(SSLv23_client_method());
// if redirecting output, don't use ANSI colors (unless override)
if (!(isatty(1) || override)) { col = 0; }
if (!col) { cols[0]=cols[1]=cols[2]=cols[3]=cols[4]=cols[5]=cols[6]=cols[7]=cols[8]=cols[b]=cols[10] = ""; }
if (name[0] == '@')
	{
	// call by user id suggested by Coffee-Puff on reddit using
	// method pointed out by statusfailed on stackoverflow (referring
	// to docs.github.com)
	unsigned long id;
	char *sptr;
	sscanf(name+1, "%ld", &id);
	sprintf(response, "/users?since=%ld&per_page=1", id-1);
	getJSON(hostname, response);
	if (!(sptr = getUserName(strstr(response, "\r\n\r\n")))) { printf("%sError: User Id '%ld' not found%s\n", cols[4], id, cols[b]); user = repos = gists = 0; }
	else { strcpy(name, sptr); }
#ifdef YYJSON
	yyjson_doc_free(_jsonDoc);
#endif
	}
if (user)
	{
	strcpy(response, "/users/");
	strcat(response, name);
	getJSON(hostname, response);
	if (!getUserInfo(strstr(response, "\r\n\r\n"))) { repos = gists = 0; }
#ifdef YYJSON
	yyjson_doc_free(_jsonDoc);
#endif
	}
if (repos)
	{
	sprintf(response, "/users/%s/repos?per_page=%d", name, per_page);
	if (page) { sprintf(request, "&page=%d", page); strcat(response, request); }
	getJSON(hostname, response);
	getUserRepos(strstr(response, "\r\n\r\n"));
#ifdef YYJSON
	yyjson_doc_free(_jsonDoc);
#endif
	}
if (gists)
	{
	sprintf(response, "/users/%s/gists?per_page=%d", name, per_page);
	if (page) { sprintf(request, "&page=%d", page); strcat(response, request); }
	getJSON(hostname, response);
	getUserGists(strstr(response, "\r\n\r\n"));
#ifdef YYJSON
	yyjson_doc_free(_jsonDoc);
#endif
	}
if (b == 10) { printf("%s", cols[9]); }
#ifndef NO_GETADDRINFO
freeaddrinfo(res);
#endif
}

json_t *load_json(const char *text)
{
#ifdef YYJSON
yyjson_doc *root;

root=yyjson_read(text, strlen(text), 0);
_jsonDoc = root;
if (root) { return(yyjson_doc_get_root(root)); }
else
	{
	fprintf(stderr, "json error\n");
	return((json_t *)0);
	}
#else
json_t *root;
json_error_t error;

root = json_loads(text, 0, &error);
if (root) { return(root); }
else
	{
	fprintf(stderr, "json error on line %d: %s\n", error.line, error.text);
	return((json_t *)0);
	}
#endif
}

char *convertISO8601(const char *str)
{
struct tm tm;
time_t t;

memset(&tm, 0, sizeof(tm));
#ifdef _WIN32
// Not perfect, need to set tm_isdst
sscanf(str, "%4d-%2d-%2dT%2d:%2d:%2dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
tm.tm_year-=1900;
tm.tm_mon--;
tm.tm_isdst=-1;
t = mktime(&tm) + timezone; // Why + ?
t+=3600; // handle dst ?
#else
strptime(str, "%FT%T%z", &tm);
t = mktime(&tm) - timezone;
#endif
return(ctime(&t));
}

char *getUserName(char *string)
{
json_t *root,*result,*login;

root = load_json(string);
if (!root) { return(NULL); }
if ((json_is_array(root)) && (json_array_size(root)>0))
	{
	result = json_array_get(root, 0);
	login = json_object_get(result, "login");
	return((char *)json_string_value(login));
	}
else { return(NULL); }
}

int getUserInfo(char *string)
{
json_t *root,*message,*uname,*email,*company,*following,*bio,*locat,*twitter;
json_t *blog,*createdat,*updatedat,*public_repos,*public_gists,*followers,*id;
int i;

root = load_json(string);
if (!root) { return(0); }
message = json_object_get(root, "message");
if (message) { printf("%sError: %s%s\n", cols[4], json_string_value(message), cols[b]); return(0); }
uname = json_object_get(root, "name");
if (uname) { printf("%s%s%s\n", cols[1], json_string_value(uname), cols[b]); }
else { printf("%s(%s%s%s)\n", cols[b], cols[1], name, cols[b]); }
id = json_object_get(root, "id");
#ifdef YYJSON
if (json_is_integer(id)) { printf("%sUser ID: %lld%s\n", cols[5], yyjson_get_uint(id), cols[b]); }
#else
if (json_is_integer(id)) { printf("%sUser ID: %lld%s\n", cols[5], json_integer_value(id), cols[b]); }
#endif
email = json_object_get(root, "email");
if (json_is_string(email)) { printf("%s%s%s\n", cols[6], json_string_value(email), cols[b]); }
company = json_object_get(root, "company");
if (json_is_string(company)) { printf("%s\n", json_string_value(company)); }
twitter = json_object_get(root, "twitter_username");
if (json_is_string(twitter)) { printf("%s %s(Twitter)%s\n", json_string_value(twitter), cols[2], cols[b]); }
locat = json_object_get(root, "location");
if (json_is_string(locat)) { printf("%s\n", json_string_value(locat)); }
blog = json_object_get(root, "blog");
if (json_is_string(blog)) { printf("%s\n", json_string_value(blog)); }
bio = json_object_get(root, "bio");
if (json_is_string(bio)) { printf("----\n%s%s%s\n----\n", cols[3], json_string_value(bio), cols[b]); }
createdat = json_object_get(root, "created_at");
if (json_is_string(createdat)) { printf("%sJoined:%s %s", cols[2], cols[b], convertISO8601(json_string_value(createdat))); }
updatedat = json_object_get(root, "updated_at");
if (json_is_string(updatedat)) { printf("%sUpdated:%s %s", cols[2], cols[b], convertISO8601(json_string_value(updatedat))); }
public_repos = json_object_get(root, "public_repos");
if (public_repos) { printf("%d %spublic repos%s\n", (int)json_integer_value(public_repos), cols[2], cols[b]); }
public_gists = json_object_get(root, "public_gists");
if (public_gists) { printf("%d %spublic gists%s\n", (int)json_integer_value(public_gists), cols[2], cols[b]); }
followers = json_object_get(root, "followers");
if (followers) { printf("%d %sfollowers%s\n", (int)json_integer_value(followers), cols[2], cols[b]); }
following = json_object_get(root, "following");
if ((i = json_integer_value(following)) > 0) { printf("%d %sfollowings%s\n", i, cols[2], cols[b]); }
return(1);
}

getUserRepos(char *string)
{
json_t *root,*message,*repo,*rname,*fork,*description,*lang,*stargazers,*forks;
json_t *topics,*topic,*watchers,*homepage,*pages,*license,*licensename;
json_t *licensespdx,*oissues,*createdat,*pushedat;
char *s;
int i, j, n;

root = load_json(strstr(string, "\r\n\r\n"));
if (!root) { return; }
message = json_object_get(root, "message");
if (message) { printf("%sError: %s%s\n", cols[4], json_string_value(message), cols[b]); return; }
printf("\n----------%s Repos %s----------\n\n", cols[0], cols[b]);
if (json_is_array(root))
	{
	for (i = 0; i < json_array_size(root); i++)
		{
		repo = json_array_get(root, i);
		rname = json_object_get(repo, "name");
		fork = json_object_get(repo, "fork");
		if (json_is_true(fork)) { s = "  (Fork)"; } else { s = ""; }
		if (rname) { printf("%s%s%s%s\n", cols[1], json_string_value(rname), cols[b], s); }
		description = json_object_get(repo, "description");
		if (json_is_string(description)) { printf("----\n%s%s%s\n----\n", cols[3], json_string_value(description), cols[b]); }
		lang = json_object_get(repo, "language");
		j = 0;
		if (json_is_string(lang)) { printf("%sLanguage:%s %s    ", cols[2], cols[b], json_string_value(lang)); j = 1; }
		stargazers = json_object_get(repo, "stargazers_count");
		n = json_integer_value(stargazers);
		if (n > 0) { printf("%sStars:%s %d    ", cols[2], cols[b], n); j = 1; }
		forks = json_object_get(repo, "forks_count");
		n = json_integer_value(forks);
		if (n > 0) { printf("%sForks:%s %d", cols[2], cols[b], n); j = 1; }
		if (j) { printf("\n"); }
		topics = json_object_get(repo, "topics");
		if (json_is_array(topics) && (topic = json_array_get(topics,0)) != NULL)
			{
			printf("%sTopics:%s %s", cols[2], cols[b], json_string_value(topic));
			for (j = 1; j < json_array_size(topics); j++)
				{
				topic = json_array_get(topics, j);
				printf(", %s", json_string_value(topic));
				}
			printf("\n");
			}
		watchers = json_object_get(repo, "watchers");
		n = json_integer_value(watchers);
		printf("%sWatchers:%s %d\n", cols[2], cols[b], n);
		homepage = json_object_get(repo, "homepage");
		s = (char *)json_string_value(homepage);
		if ((s) && (s[0] != '\0')) { printf("%sHomepage:%s %s\n", cols[2], cols[1], s); }
		pages = json_object_get(repo, "has_pages");
		if (json_is_true(pages)) { printf("%sLive Demo (guess):%s https://%s.github.io/%s\n", cols[2], cols[1], name, json_string_value(rname)); }
		license = json_object_get(repo, "license");
		if (json_is_object(license))
			{
			licensename = json_object_get(license, "name");
			licensespdx = json_object_get(license, "spdx_id");
			printf("%sLicense:%s %s (%s)\n", cols[2], cols[b], json_string_value(licensename), json_string_value(licensespdx));
			}
		oissues = json_object_get(repo, "open_issues_count");
		n = json_integer_value(oissues);
		if (n > 0) { printf("%sOpen Issues:%s %d\n", cols[2], cols[b], n); }
		createdat = json_object_get(repo, "created_at");
		// Github.com uses push date to mean update
		pushedat = json_object_get(repo, "pushed_at");
		printf("%sCreated:%s %s", cols[2], cols[b], convertISO8601(json_string_value(createdat)));
		printf("%sUpdated:%s %s", cols[2], cols[b], convertISO8601(json_string_value(pushedat)));
		printf("\n");
		}
	}
}

/*
			var keys=Object.keys(data[i].files);
			for (j=0;j<keys.length;j++)
				{
				.files[keys[j]].raw_url .files[keys[j]].filename .files[keys[j]].size
*/

getUserGists(char *string)
{
json_t *root,*message,*gist,*id,*description,*createdat,*updatedat,*files;
json_t *value,*filename,*size;
int i, n;
#ifdef YYJSON
size_t _yyidx, _yymax;
yyjson_val *_yykey;
char *key;
#else
const char *key;
#endif

root = load_json(strstr(string, "\r\n\r\n"));
if (!root) { return; }
message = json_object_get(root, "message");
if (message) { printf("%sError: %s%s\n", cols[4], json_string_value(message), cols[b]); return; }
if (json_is_array(root))
	{
	n = json_array_size(root);
	if (n) { printf("\n----------%s Gists %s----------\n\n", cols[0], cols[b]); }
	for (i = 0; i < n; i++)
		{
		gist = json_array_get(root, i);
		id = json_object_get(gist, "id");
		if (id) { printf("%s%s%s\n", cols[1], json_string_value(id), cols[b]); }
		description = json_object_get(gist, "description");
		if (json_is_string(description)) { printf("----\n%s%s%s\n----\n", cols[3], json_string_value(description), cols[b]); }
		createdat = json_object_get(gist, "created_at");
		updatedat = json_object_get(gist, "updated_at");
		printf("%sCreated:%s %s", cols[2], cols[b], convertISO8601(json_string_value(createdat)));
		printf("%sUpdated:%s %s", cols[2], cols[b], convertISO8601(json_string_value(updatedat)));
		files = json_object_get(gist, "files");
		//printf("%d \n", json_object_size(files));
#ifdef YYJSON
		yyjson_obj_foreach(files, _yyidx, _yymax, _yykey, value)
#else
		json_object_foreach(files, key, value)
#endif
			{
			// In the case of github's gist response, my key variable should
			// be the filename (ie: same as value associated with filename key)
			filename = json_object_get(value, "filename");
			size = json_object_get(value, "size");
			printf("   %s%s%s    %d%s\n", cols[1], json_string_value(filename), cols[2], (int)json_integer_value(size), cols[b]);
			}
		}
	}
}
