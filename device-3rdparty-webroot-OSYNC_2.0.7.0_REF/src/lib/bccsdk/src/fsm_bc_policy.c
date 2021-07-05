#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <regex.h>

#include "bccsdk.h"
#include "bc_string.h"
#include "bc_stats.h"
#include "fsm.h"
#include "fsm_policy.h"
#include "fsm_bc_policy.h"
#include "fsm_bc_service.h"
#include "log.h"
#include "os_regex.h"
#include "ovsdb_utils.h"

#define NUM_CATS 100
static char *cat_strings[NUM_CATS] =
{
    "Uncategorized",                        // 0
    "Real Estate",                          // 1
    "Computer and Internet Security",       // 2
    "Financial Services",                   // 3
    "Business and Economy",                 // 4
    "Computer and Internet Info",           // 5
    "Auctions",                             // 6
    "Shopping",                             // 7
    "Cult and Occult",                      // 8
    "Travel",                               // 9
    "Abused Drugs",                         // 10
    "Adult and Pornography",                // 11
    "Home",                                 // 12
    "Military",                             // 13
    "Social Network",                       // 14
    "Dead Sites",                           // 15
    "Individual Stock Advice and Tools",    // 16
    "Training and Tools",                   // 17
    "Dating",                               // 18
    "Sex Education",                        // 19
    "Religion",                             // 20
    "Entertainment and Arts",               // 21
    "Personal Sites and Blogs",             // 22
    "Legal",                                // 23
    "Local Information",                    // 24
    "Streaming Media",                      // 25
    "Job Search",                           // 26
    "Gambling",                             // 27
    "Translation",                          // 28
    "Reference and Research",               // 29
    "Shareware and Freeware",               // 30
    "P2P",                                  // 31
    "Marijuana",                            // 32
    "Hacking",                              // 33
    "Games",                                // 34
    "Philosophy and Political Advocacy",    // 35
    "Weapons",                              // 36
    "Pay to Surf",                          // 37
    "Hunting and Fishing",                  // 38
    "Society",                              // 39
    "Educational Institutions",             // 40
    "Online Greeting cards",                // 41
    "Sports",                               // 42
    "Swimsuits & Intimate Apparel",         // 43
    "Questionable",                         // 44
    "Kids",                                 // 45
    "Hate and Racism",                      // 46
    "Online Personal Storage",              // 47
    "Violence",                             // 48
    "Keyloggers and Monitoring",            // 49
    "Search Engines",                       // 50
    "Internet Portals",                     // 51
    "Web Advertisements",                   // 52
    "Cheating",                             // 53
    "Gross",                                // 54
    "Web based Email",                      // 55
    "Malware Sites",                        // 56
    "Phishing and Other Frauds",            // 57
    "Proxy Avoid and Anonymizers",          // 58
    "Spyware and Adware",                   // 59
    "Music",                                // 60
    "Government",                           // 61
    "Nudity",                               // 62
    "News and Media",                       // 63
    "Illegal",                              // 64
    "CDNs",                                 // 65
    "Internet Communications",              // 66
    "Bot Nets",                             // 67
    "Abortion",                             // 68
    "Health & Medicine",                    // 69
    "Confirmed SPAM Sources",               // 70
    "SPAM URLs",                            // 71
    "Unconfirmed SPAM Sources",             // 72
    "Open HTTP Proxies",                    // 73
    "Dynamic Content",                      // 74
    "Parked Sites",                         // 75
    "Alcohol and Tobacco",                  // 76
    "Private IP Addresses",                 // 77
    "Image and Video Search",               // 78
    "Fashion and Beauty",                   // 79
    "Recreation and Hobbies",               // 80
    "Motor Vehicles",                       // 81
    "Web Hosting",                          // 82
    "Provisioned (83)",
    "Provisioned (84)",
    "Provisioned (85)",
    "Provisioned (86)",
    "Provisioned (87)",
    "Provisioned (88)",
    "Provisioned (89)",
    "Provisioned (90)",
    "Provisioned (91)",
    "Provisioned (92)",
    "Provisioned (93)",
    "Provisioned (94)",
    "Provisioned (95)",
    "Provisioned (96)",
    "Provisioned (97)",
    "Provisioned (98)",
    "Provisioned (99)",
};

static int bc_serial = 0;

/**
 * @brief returns a string matching the category
 *
 * @param category the category
 */
char *
fsm_bc_report_cat(struct fsm_session *session, int category)
{
    if ((category >= NUM_CATS) || (category) < 0) return NULL;

    return cat_strings[category];
}


void
fsm_bc_set_param(ds_tree_t *tree, char *param_name,
                 char *param, size_t param_len)
{
    struct str_pair *pair;
    size_t len;
    char *val;

    pair = ds_tree_find(tree, param_name);
    if (pair == NULL) return;

    val = pair->value;
    len = strlen(val) + 1;

    if (len > param_len) {
        LOGE("%s: %s too long (max %zu)",
             __func__, val, sizeof(param_len));
        return;
    }
    memset(param, 0, param_len);
    bc_cpystrn(param, val, param_len);
}


static void
fsm_bc_set_cache_size(ds_tree_t *tree, struct bc_init_params *params)
{
    char *key = "bc_cache_size";
    struct str_pair *pair;
    char *val;
    long size;

    pair = ds_tree_find(tree, key);
    if (pair == NULL) return;

    val = pair->value;
    errno = 0;
    size = strtol(val, NULL, 10);
    if (errno != 0)
    {
        LOGE("%s: %s setting from %s failed: %s",
             __func__, key, val, strerror(errno));
        return;
    }
    params->max_cache_mb = (int)size;
    LOGI("%s: bc connection cache size set to %d", __func__,
         params->max_cache_mb);
}


static void
fsm_bc_set_keepalive(ds_tree_t *tree, struct bc_init_params *params)
{
    char *key = "bc_keepalive";
    struct str_pair *pair;
    long keepalive;
    char *val;

    pair = ds_tree_find(tree, key);
    if (pair == NULL) return;

    val = pair->value;

    errno = 0;
    keepalive = strtol(val, NULL, 10);
    if (errno != 0)
    {
        LOGE("%s: %s from %s failed: %s",
             __func__, key, val, strerror(errno));
        return;
    }
    params->keep_alive = (int)keepalive;
    LOGI("%s: %s set to %d", __func__, key,
         params->keep_alive);
}


static
void fsm_bc_set_params(struct fsm_session *session,
                       struct bc_init_params *params)
{
    struct fsm_session_conf *conf;
    ds_tree_t *tree;

    if (session == NULL) return;

    conf = session->conf;
    if (conf == NULL) return;

    tree = conf->other_config;
    if (tree == NULL) return;

    fsm_bc_set_param(tree, "bc_device", params->device, sizeof(params->device));
    fsm_bc_set_param(tree, "bc_oem", params->oem, sizeof(params->oem));
    fsm_bc_set_param(tree, "bc_uid", params->uid, sizeof(params->uid));
    fsm_bc_set_param(tree, "bc_server", params->server, sizeof(params->server));
    fsm_bc_set_param(tree, "bc_dbserver", params->dbserver, sizeof(params->dbserver));
    fsm_bc_set_cache_size(tree, params);
    fsm_bc_set_keepalive(tree, params);
}


static void
fsm_bc_set_default_cache_size(struct bc_init_params *params)
{
    struct sysinfo info;
    int cache_size = 10; // in MB
    int rc;

    memset(&info, 0, sizeof(info));
    rc = sysinfo(&info);
    if (rc != 0)
    {
        rc = errno;
        LOGE("%s: sysinfo failed: %s", __func__, strerror(rc));
    }

    /* Allocate 1% of total memory for bc_cache */
    cache_size = (info.totalram * 1) /100;
    cache_size /= (1024 * 1024);

    LOGD("%s: total ram %lu MB, cache size computed to %d MB",
         __func__, info.totalram / (1024 * 1024), cache_size);
    params->max_cache_mb = cache_size;
}


static void
fsm_bc_default_params(struct bc_init_params *params)
{
    bc_cpystrn(params->device, "deviceid_plumewifi", sizeof(params->device));
    bc_cpystrn(params->oem, "brightcloudsdk", sizeof(params->oem));
    bc_cpystrn(params->uid, "plumewifi_empjl", sizeof(params->uid));
    bc_cpystrn(params->server, "bcap15.brightcloud.com",
               sizeof(params->server));
    bc_cpystrn(params->dbserver, "database.brightcloud.com",
               sizeof(params->dbserver));
    fsm_bc_set_default_cache_size(params);
}


/**
 * @brief initializes the BC service
 *
 * Initializes the BC service once, with the parameters given within the session
 * @param session the fsm session containing the BC service config
 */
bool
fsm_bc_init(struct fsm_session *session)
{
    struct bc_init_params params;
    struct fsm_bc_mgr *mgr;

    mgr = fsm_bc_get_mgr();
    if (mgr->initialized) return true;

    LOGI("Initializing BC session");

    memset(&params, 0, sizeof(params));
    params.lcp_loop_limit = 6;
    params.timeout = 2;
    params.keep_alive = INT_MAX;
    params.ssl = 1;
    params.log_stderr = 1;
    params.enable_stats = 1;
    params.enable_rtu = 1;
    params.rtu_poll = 300;
    params.pool_size = 2;
    params.queue_poll = 8;
    params.poller = 0;
    bc_cpystrn(params.dbpath, "/tmp", sizeof(params.dbpath));
    // Set default credentials for backwards compatibility
    fsm_bc_default_params(&params);
    // Set cloud provided credentials, overwriting defaults
    fsm_bc_set_params(session, &params); // cloud provided credentials,

    if (bc_initialize(&params) == -1)
    {
        LOGE("bc initialization failed");
        return false;
    }

    mgr->initialized = true;
    return true;
}


void
fsm_bc_update_params(struct fsm_session *session)
{
    struct fsm_session_conf *conf;
    struct bc_init_params params;
    ds_tree_t *tree;

    if (session == NULL) return;

    conf = session->conf;
    if (conf == NULL) return;

    tree = conf->other_config;
    if (tree == NULL) return;

    memset(&params, 0, sizeof(params));

    fsm_bc_set_keepalive(tree, &params);
    bc_params_update(&params);
}


static
void cloud_callback(struct bc_url_request* req, int rc)
{
    struct fqdn_pending_req *dpr;

    dpr = (struct fqdn_pending_req *)req->context;
    dpr->categorized = FSM_FQDN_CAT_SUCCESS;
    if (rc == -1)
    {
        LOGE("Cloud lookup for %s failed%s\n", req->url,
             req->connect_error ? ": connection failed" : "");
        dpr->categorized = FSM_FQDN_CAT_FAILED;
    }
}


/**
 * @brief compute the latency indicators for a given request
 *
 * @param bc_session the session holding the latency indicators
 * @param start the clock_t value before the categorization API call
 * @param end the clock_t value before the categorization API call
 */
static void
fsm_bc_update_latencies(struct fsm_bc_session *bc_session,
                        struct timespec *start, struct timespec *end)
{
    long latency;
    long seconds;
    long nanoseconds;

    seconds = end->tv_sec - start->tv_sec;
    nanoseconds = end->tv_nsec - start->tv_nsec;

    /* Compute latency in nanoseconds */
    latency = (seconds * 1000000000L) + nanoseconds;

    /* Translate in milliseconds */
    latency /= 1000000L;

    bc_session->avg_latency += latency;
    bc_session->lookup_cnt++;

    if (latency < bc_session->min_latency) bc_session->min_latency = latency;
    if (latency > bc_session->max_latency) bc_session->max_latency = latency;
}


/**
 * @brief retrieves the categories of a fqdn
 *
 * @param session the fsm session triggering the request
 * @param req the request to categorize
 * @param policy the policy being checked against
 * @return true it categorization succeeded, false otherwise.
 */
bool
fsm_bc_cat_check(struct fsm_session *session,
                 struct fsm_policy_req *req,
                 struct fsm_policy *policy)
{
    struct fsm_bc_session *bc_session;
    struct fqdn_pending_req *fqdn_req;
    struct fsm_url_request *req_info;
    struct fsm_policy_rules *rules;
    struct fsm_url_reply *reply;
    bool valid_fqdn = false;
    struct timespec start;
    struct timespec end;
    int res, i;
    bool rc;

    rules = &policy->rules;

    fqdn_req = req->fqdn_req;
    req_info = fqdn_req->req_info;
    bc_session = fsm_bc_lookup_session(session->service);

    for (i = 0; i < fqdn_req->numq; i++)
    {
        struct bc_url_request bc_req;
        int j;
        bool loop;

        memset(&bc_req, 0, sizeof(bc_req));

        /*
         * 'reply' should not be allocated at this point. If allocated,
         *  a fqdn request if already pending for the url. So return
         *  to avoid a previos allocation, and memory leak due to missing
         *  the free.
         */
        if (req_info->reply)
        {
            LOGW("%s: reply pending for %s", __func__, req_info->url);
            return false;
        }

        reply = calloc(1, sizeof(*reply));
        if (reply == NULL) return false;
        req_info->reply = reply;
        reply->service_id = URL_BC_SVC;

        strncpy(bc_req.url, req_info->url, BCA_URL_LENGTH);
        bc_req.req_id = fqdn_req->req_id;
        bc_req.context = fqdn_req;

        memset(&start, 0, sizeof(start));
        memset(&end, 0, sizeof(end));
        /* Check BrightCloud output */
        res = bc_cache_lookup(&bc_req);

        if (res < 0)
        {
            LOGE("%s::%s::%d: ERROR: bc cache lookup for %s failed\n",
                   __FILE__, __func__, __LINE__,
                   req_info->url);
            fqdn_req->categorized = FSM_FQDN_CAT_FAILED;
            reply->lookup_status = -1; /* indicate cache error */
            reply->error = res;
            return false;
        }
        else if (res > 0)
        {
            fqdn_req->categorized = FSM_FQDN_CAT_SUCCESS;
        }
        else
        {
            bc_req.serial = ++bc_serial;

            res = strncmp(req_info->url, "http", strlen("http"));
            if (res)
            {
                valid_fqdn = fqdn_pre_validation(req_info->url);
                if (!valid_fqdn) return false;
            }

            memset(&start, 0, sizeof(start));
            memset(&end, 0, sizeof(end));
            clock_gettime(CLOCK_REALTIME, &start);
            res = bc_cloud_lookup(&bc_req, cloud_callback);
            clock_gettime(CLOCK_REALTIME, &end);
            fsm_bc_update_latencies(bc_session, &start, &end);
            if (fqdn_req->categorized == FSM_FQDN_CAT_FAILED)
            {
                LOGE("Cloud lookup for %s failed\n", req_info->url);
                reply->lookup_status = bc_req.http_status;
                reply->error = bc_req.error;
                reply->connection_error = bc_req.connect_error;

                return false;
            }
        }

        /* Compute the number of reported categories */
        j = 0;
        do {
            reply->categories[j] = bc_req.data.cc[j].category;
            reply->bc.confidence_levels[j] = bc_req.data.cc[j].confidence;
            loop = (reply->categories[j] != 0);
            loop &= (j < URL_REPORT_MAX_ELEMS);
            if (loop) j++;
        } while (loop);

        if (j == URL_REPORT_MAX_ELEMS) j--;
        reply->nelems = j;
        reply->bc.reputation = bc_req.data.reputation;

        req_info++;
    }

    rc = fsm_fqdncats_in_set(req, policy);
    /*
     * If category in set and policy applies to categories out of the set,
     * no match
     */
    if (rc && (rules->cat_op == CAT_OP_OUT)) return false;

    /*
     * If category not in set and policy applies to categories in the set,
     * no match
     */
    if (!rc && (rules->cat_op == CAT_OP_IN)) return false;

    return true;
}


void fsm_bc_get_stats(struct fsm_session *session,
                      struct fsm_url_stats *stats)
{
    struct fsm_bc_session *bc_session;
    struct bc_stats bc_stats;

    bc_session = fsm_bc_lookup_session(session);

    memset(&bc_stats, 0, sizeof(bc_stats));
    bc_stats_get(&bc_stats);

    stats->cloud_lookups = bc_stats.cloud_lookups;
    stats->cloud_hits = bc_stats.cloud_hits;
    stats->cache_hits = bc_stats.cache_hits;
    stats->categorization_failures = bc_stats.bcap_errors;
    stats->cloud_lookup_failures = bc_stats.net_errors;
    stats->uncategorized = bc_stats.uncat_responses;
    stats->cache_entries = bc_stats.cache_size;
    stats->cache_size = bc_stats.cache_max_entries;
    stats->min_lookup_latency = (uint32_t)(bc_session->min_latency);
    if (bc_session->min_latency == LONG_MAX) stats->min_lookup_latency = 0;
    stats->max_lookup_latency = (uint32_t)(bc_session->max_latency);
    if (bc_session->lookup_cnt != 0)
    {
        double avg;

        avg = bc_session->avg_latency;
        avg /= bc_session->lookup_cnt;
        stats->avg_lookup_latency = (uint32_t)avg;
    }
    bc_session->min_latency = LONG_MAX;
    bc_session->max_latency = 0;
    bc_session->avg_latency = 0;
    bc_session->lookup_cnt = 0;

    return;
}


/*
 * FQDN Pre validation before calling bright cloud lookup
 * Filters the basic format of (domain.tld)/(hostname.domain.tld)
 * fqdn string is taken from the below struct path
 * struct fqdn_pending_req->bc_url_request->url
 * Returns false upon any invalid fqdn, true if valid
 */
bool fqdn_pre_validation(char *fqdn_string)
{
    const char pattern_fqdn[] =
        "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.){1,}"
        "([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$";

    regex_t re;
    int rc;

     // returns false upon emptry string
    if (fqdn_string == NULL) return false;

     // check fqdn < 255, to abide by RFC 1032 defined size
    if (strlen(fqdn_string) > 255)
    {
        LOGE("Invalid FQDN length: %zu, string exceeds 255 characters\n",
             strlen(fqdn_string));
        return false;
    }

     // first compile the regex expression
    rc = regcomp(&re, pattern_fqdn, REG_EXTENDED);
    if (rc != 0)
    {
        LOGE("Fqdn validation regcomp() failed, reason(%d)\n", rc);
        goto err;
    }

     // generate regex match
    rc = regexec(&re, fqdn_string, (size_t) 0, NULL, 0);
    if (rc != 0)
    {
        LOGE("Invalid FQDN: (%d) Failed to match \"%s\"\n", rc, fqdn_string);
        goto err;
    }

    regfree(&re);
    return true;

 err:
    regfree(&re);
    return false;
}

