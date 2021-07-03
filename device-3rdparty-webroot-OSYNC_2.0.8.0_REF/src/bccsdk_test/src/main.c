#include "bccsdk.h"
#include "bc_url.h"
#include "bc_lcp.h"
#include "bc_string.h"
#include "bc_net.h"
#include "bc_proto.h"
#include "bc_queue.h"
#include "bc_db.h"
#include "bc_rtu.h"
#include "bc_alloc.h"
#include "bc_stats.h"
#include "bc_cache.h"
#include "icache.h"
#include "bc_poll.h"
#include "md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <syslog.h>
#include <sys/time.h>
#ifdef HAS_POLLER
#include "poller.h"
#endif

static const char* urls[] = {
    "www.yahoo.com:80/test/foo////////////",
    "www.yahoo.com:80/test/path",
    "http://www.google.com/blarg/flarn?whazzit=x",
    "http://www.google.com/honk/honk/",
    "https://www.sex.com/show/me/your/naughty/bits/xxx.jpg",
    "http://www.guardian.co.uk/",
    "http://www.huffingtonpost.com/26/nsa-porn-muslims_n_4346128.html?utm_hp_ref=politics",
    "http://www.dot.ca.gov/dist11/d11tmc/sdmap/showmap.php?route=sb805",
    "foo://user:password@www.hostname.com:420/path/path?query=blah",
    "192.188.21.7:80/path/path/path/path?query=blah",
    "http://192.188.21.7:80/path/path?query=blah",
    "http://gmail.google.com/",
    "http://finance.yahoo.com/",
    "http://www.google.com/",
    "http://slashdot.org/",
    "www.amazon.com",
    "aws.amazon.com",
    "http://new.madison.com/clients/1TodaysDeals/Clark's%20Hooves%20&%20Feathers%20Small%20Logo.png",
    "http://ad.doubleclick.net/adj/N339.135230.MICROSOFTONLINEINC.M/B7298745.32;sz=300x250;;dcopt=rcl;click0=http://wrapper.g.msn.com/GRedirect.aspx?g.msn.com/2AD000BI/108000000000152520.1?!&&PID=11387088&UIT=A&TargetID=101151353&N=1361266716&PG=NBCSMX&ASID=493"

};

static const int url_count = sizeof(urls) / sizeof(const char*);

void print_req(struct bc_url_request* req) {
    int i;
    char cc[16];
    char buf[128] = {0};
    for (i = 0; i < 5; ++i) {
	if (req->data.cc[i].category == 0)
	    break;
	sprintf(cc, "%d,%d", req->data.cc[i].category, req->data.cc[i].confidence);
	if (i > 0)
	    strcat(buf, " ");
	strcat(buf, cc);
    }
    if (strlen(buf) == 0)
	strcpy(buf, "Uncategorized");
    printf("%s - ",
	   (req->data.flags & BCA_FLAG_CACHED) != 0 ? "CACHED" : "CLOUD");
    printf("URL: %s LCP: %s CC: %s REP: %d A1: %d\n", req->url, req->lcp, buf,
	   req->data.reputation, (req->data.flags & BCA_FLAG_ONE_CAT) ? 1 : 0);
}

static void print_stats(void) {
    struct bc_stats s;
    bc_stats_get(&s);
    printf("--- RUN STATS ---\n");
    printf("cache: %u\ncloud: %u\nlookups: %u\ndb: %u\nuncat: %u\ncache: %u\n",
	   s.cache_hits, s.cloud_hits, s.cloud_lookups, s.db_hits,
	   s.uncat_responses, s.cache_size);
    printf("queue: %u\ntrust: %u\nguest: %u\n",
	   s.queue_depth, s.trust_score, s.guest_score);
    printf("Errors - N: %u H: %u B: %u\n",
	   s.net_errors, s.http_errors, s.bcap_errors);
}

/* Subtract the `struct timeval' values X and Y,
 * storing the result in RESULT.
 * Return 1 if the difference is negative, otherwise 0.
 */
static int timeval_subtract(struct timeval* result,
			    struct timeval* x, struct timeval* y) {
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
	int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
	int nsec = (x->tv_usec - y->tv_usec) / 1000000;
	y->tv_usec += 1000000 * nsec;
	y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     * tv_usec is certainly positive.
     */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

static void print_elapsed_time(struct timeval* start, struct timeval* end) {
    struct timeval elapsed = { 0 };
    timeval_subtract(&elapsed, end, start);
    printf("*** time: %lu.%.06lu ***\n", elapsed.tv_sec, elapsed.tv_usec);
}


static void cloud_callback(struct bc_url_request* req, int rc) {
    if (rc == -1)
	printf("Cloud lookup for %s failed\n", req->url);
    else
	print_req(req);
    bc_free(req);
}

static FILE* next_file(DIR* dir, const char* prefix) {
    char path[2048];
    struct dirent* dent;
    if (!dir)
	return 0;
    while (1) {
	dent = readdir(dir);
	if (!dent)
	    {
		return 0;
	    }
	if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
	    continue;
	else if (dent->d_type == DT_REG)
	    break;
    }
    sprintf(path, "%s/%s", prefix, dent->d_name);
    return fopen(path, "r");
}

static int read_data_file(const char* directory, int reset) {
    static DIR* dir = 0;
    static FILE* file = 0;
    static int done = 0;
    static unsigned int serial = 0;
    const char* path = "data";
    char* url = 0;
    int res;
    char buf[2048];
    int len;
    if (dir)
	path = directory;

    if (reset) {
	if (dir)
	    closedir(dir);
	if (file)
	    fclose(file);
	dir = 0;
	file = 0;
	done = 0;
	return 0;
    }

    if (done)
	return 0;
    if (!dir) {
	dir = opendir(path);
	if (!dir) {
	    perror("opening directory");
	    done = 1;
	    return 0;
	}
    }
    if (!file) {
	file = next_file(dir, path);
	if (!file)
	    {
		closedir(dir);
		dir = 0;
		done = 1;
		return 0;
	    }
    }
    while (!url) {
	url = fgets(buf, 2047, file);
	buf[2047] = '\0';
	if (!url) {
	    fclose(file);
	    file = next_file(dir, path);
	    if (!file) {
		closedir(dir);
		dir = 0;
		done = 1;
		return 0;
	    }
	} else {
	    len = strlen(url);
	    if (url[len - 1] == '\n')
		url[len - 1] = '\0';
	}
	if (!*url)
	    url = 0;
    }
    struct bc_url_request* req =
	(struct bc_url_request*) bc_malloc(sizeof(struct bc_url_request));
    if (bc_init_request(req, url, 1, ++serial) == -1) {
	fprintf(stderr, "Error initializing request for %s\n", url);
	bc_free(req);
	return 0;
    }
    res = bc_cache_lookup(req);
    if (res > 0) {
	print_req(req);
	bc_free(req);
    }
    else if (res == 0) {
	if (bc_cloud_lookup(req, cloud_callback) == -1) {
	    printf("Cloud lookup for %s failed\n", req->url);
	    bc_free(req);
	}
    } else {
	bc_free(req);
    }
    return 1;
}

static void test_api(const char* host) {
    static unsigned int serial = 0;
    int res;
    struct bc_url_request* req =
	(struct bc_url_request*) bc_malloc(sizeof(struct bc_url_request));
    if (bc_init_request(req, host, 1, ++serial) == -1) {
	fprintf(stderr, "Error initializing request for %s\n", host);
	bc_free(req);
	return;
    }
    res = bc_cache_lookup(req);
    if (res > 0) {
	print_req(req);
	bc_free(req);
    } else if (res == 0) {
	if (bc_cloud_lookup(req, cloud_callback) == -1) {
	    printf("Cloud lookup for %s failed\n", req->url);
	    bc_free(req);
	}
    } else {
	bc_free(req);
    }
}

static void test_workflow(void) {
    printf("*** TEST WORKFLOW ***\n");
    test_api("www.google.com");
    test_api("www.amazonlove.org");
    test_api("www.thegolfchannel.com");
    test_api("www.golfchannel.com");
    test_api("www.brightcloud.com");
    test_api("www.proxy.org");
    test_api("netklass.com");
    //test_api("www.yahoo.com");
    //test_api("www.google.com");
    //test_api("www.netflix.com");
    //test_api("cdn1.netflix.com");
    //test_api("www1.netflix.com");
    //test_api("54.243.95.128");
    //test_api("108.175.42.210");
}

static void test_workflow2(void) {
    int i;
    printf("*** TEST WORKFLOW 2 ***\n");
    for (i = 0; i < url_count; ++i) {
	test_api(urls[i]);
    }
}

void timer_handler(void* data, int timer) {
    static time_t timeout = 0;
    time_t now;
    if (!read_data_file(0, 0)) {
	if (!bc_net_active() && bc_queue_empty()) {
	    now = time(0);
	    if (timeout == 0)
		timeout = now;
	    if (timeout + 1 < now) {
		read_data_file(0, 1);
		print_stats();
		fflush(stdout);
		struct bc_poller* poller = (struct bc_poller*) data;
		bc_poll_remove_timer(poller, timer);
		bc_poll_terminate(poller, 0);
	    }
	}
    }
}

static void test_poller(struct bc_poller* poller) {
    bc_net_set_poller(poller);
    bc_poll_add_timer(poller, 100, timer_handler, poller, 1);
    bc_poll_event_loop(poller);
}

static void test_url(const char* url) {
    test_api(url);
}

static void test_data(const char* directory)
{
    while (read_data_file(directory, 0)) {
    }
}

void test_db_read(const char* path) {
    struct bc_db_context ctx;
    struct bc_db_record rec;
    //const char* path = "/tmp/full_bcdb_rep_1m_4.270.bin";
    printf("*** TEST DB READ ***\n");
    if (bc_db_open(path, &ctx) == 0) {
	while (bc_db_next(&ctx, &rec)) {
	    printf("%llx%llx CC: %d,%d R: %d\n", (unsigned long long) rec.md5[0],
		   (unsigned long long) rec.md5[1], rec.data.cc[0].category,
		   rec.data.cc[0].confidence, rec.data.reputation);
	}
	bc_db_close(&ctx);
    }
}

void test_db(void) {
    struct bc_db_request req;
    struct bc_db_context ctx;
    struct bc_db_record rec;
    char path[PATH_MAX + 1];
    printf("*** TEST DB DOWNLOAD ***\n");
    memset(&req, 0, sizeof(struct bc_db_request));
    req.current_major = 0;
    req.current_minor = 0;
    if (bc_db_update(&req) != -1) {
	printf("Update available: %d\n", req.update_available);
	printf("f: %s s: %s M: %d m: %d\n", req.filename, req.checksum,
	       req.update_major, req.update_minor);
	sprintf(path, "/tmp/%s", req.filename);
	if (bc_db_open(path, &ctx) == 0) {
	    while (bc_db_next(&ctx, &rec)) {
		bc_cache_add_db(rec.md5, &rec.data);
		//printf("%llx%llx CC: %d,%d R: %d\n", rec.md5[0], rec.md5[1],
		//	rec.data.cc[0].category, rec.data.cc[0].confidence, rec.data.reputation);
	    }
	    bc_db_close(&ctx);
	}
    } else
	printf("bc_db_update() failed\n");
}

struct cmd_line {
    int sleep;
    int time;
    int count;
    int mb;
    int report;
    int poll;
    int rc;
    int load_db;
    int download_db;
    const char* dir;
    const char* db_file;
    const char* file;
    const char* url;
};

static int parse_cmd_line(int argc, char** argv, struct cmd_line* cl) {
    int opt;
    cl->count = 1;
    cl->mb = 4;
    while ((opt = getopt(argc, argv, "s:c:trpm:n:q:d:f:u:z")) != -1) {
	switch (opt) {
	case 's':
	    cl->sleep = atoi(optarg);
	    break;
	case 'c':
	    cl->rc = atoi(optarg);
	    break;
	case 'd':
	    /* printf("optarg: %s\n", optarg); */
	    cl->dir = optarg;
	    break;
	case 'f':
	    cl->file = optarg;
	    break;
	case 't':
	    cl->time = 1;
	    break;
	case 'n':
	    cl->count = atoi(optarg);
	    break;
	case 'm':
	    cl->mb = atoi(optarg);
	    break;
	case 'p':
	    cl->poll = 1;
	    break;
	case 'q':
	    cl->load_db = 1;
	    cl->db_file = optarg;
	    break;
	case 'r':
	    cl->report = 1;
	    break;
	case 'u':
	    cl->url = optarg;
	    break;
	case 'z':
	    cl->download_db = 1;
	    break;
	default:
	    fprintf(stderr,
		    "Usage %s [-m mb] [-s nsecs] [-t] [-n count] "
		    "[-d directory] [-f filename]\n",
		    argv[0]);
	    return -1;
	}
    }
    return 0;
}

int main(int argc, char** argv)
{
    int i;
    struct timeval start;
    struct timeval end;
    struct bc_init_params params = { 0 };
    struct cmd_line cl = { 0 };
    if (parse_cmd_line(argc, argv, &cl) != 0)
	return 2;

    openlog("bcap", LOG_PERROR, LOG_USER);

    struct bc_poller* poller = bc_poll_create(20, 20);
    params.max_cache_mb = 40;
    params.lcp_loop_limit = 6;
    params.timeout = 60;
    params.keep_alive = 60;
    params.ssl = 0;
    params.log_stderr = 1;
    params.enable_stats = 1;
    params.enable_rtu = 1;
    params.rtu_poll = 300;
    params.pool_size = 2;
    params.queue_poll = 8;
    params.poller = 0;
    bc_cpystrn(params.device, "deviceid_plumewifi", sizeof(params.device));
    bc_cpystrn(params.oem, "brightcloudsdk", sizeof(params.oem));
    bc_cpystrn(params.uid, "plumewifi_empjl", sizeof(params.uid));
    //bc_cpystrn(params.server, "54.218.220.119", sizeof(params.server));
    //bc_cpystrn(params.server, "toolbar.brightcloud.com", sizeof(params.server));
    bc_cpystrn(params.server, "bcap15.brightcloud.com", sizeof(params.server));
    bc_cpystrn(params.dbpath, "/tmp", sizeof(params.dbpath));
    bc_cpystrn(params.dbserver, "database.brightcloud.com", sizeof(params.dbserver));
    bc_cpystrn(params.proxy_server, "localhost", sizeof(params.proxy_server));
    bc_cpystrn(params.proxy_user, "username", sizeof(params.proxy_user));
    bc_cpystrn(params.proxy_pass, "password", sizeof(params.proxy_pass));
    params.proxy_port = 3128;
    //params.proxy = 1;
    if (cl.mb > 0)
	params.max_cache_mb = cl.mb;

    if (bc_initialize(&params) == -1)
	return 1;

    if (cl.load_db) {
	test_db_read(cl.db_file);
	return 0;
    }
    if (cl.sleep > 0) {
	int ret = daemon(1, 1);
	(void)ret; // XXX warning: variable 'ret' set but not used [-Wunused-but-set-variable]
	sleep(cl.sleep);
	bc_shutdown();
	return 0;
    }
    if (cl.rc > 0) {
	unsigned int rc = icache_calc_record_count(cl.rc);
	printf("Max records: %u\n", rc);
	bc_shutdown();
	return 0;
    }
    if (cl.dir) {
	for (i = 0; i < cl.count; ++i) {
	    if (cl.time)
		gettimeofday(&start, 0);
	    test_data(cl.dir);
	    if (cl.time)
		{
		    gettimeofday(&end, 0);
		    print_elapsed_time(&start, &end);
		}
	    read_data_file(0, 1);
	}
    }
    if (cl.download_db) {
	test_db();
    }
    if (cl.url) {
	test_url(cl.url);
    }
    if (cl.poll) {
	test_poller(poller);
    }
    if (cl.report)
	print_stats();

    test_workflow();
    test_workflow2();
    bc_shutdown();
    bc_poll_destroy(poller);
    return 0;
}
