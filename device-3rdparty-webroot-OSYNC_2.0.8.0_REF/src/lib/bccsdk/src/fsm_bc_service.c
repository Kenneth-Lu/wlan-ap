#include <stdlib.h>
#include <stddef.h>
#include <time.h>

#include "fsm_bc_service.h"
#include "wc_telemetry.h"
#include "fsm_policy.h"
#include "const.h"
#include "ds_tree.h"
#include "dns_cache.h"
#include "log.h"

static struct fsm_bc_mgr cache_mgr =
{
    .initialized = false,
};


/**
 * @brief returns the plugin's session manager
 *
 * @return the plugin's session manager
 */
struct fsm_bc_mgr *
fsm_bc_get_mgr(void)
{
    return &cache_mgr;
}


/**
 * @brief compare sessions
 *
 * @param a session pointer
 * @param b session pointer
 * @return 0 if sessions matches
 */
static int
fsm_bc_session_cmp(void *a, void *b)
{
    uintptr_t p_a = (uintptr_t)a;
    uintptr_t p_b = (uintptr_t)b;

    if (p_a == p_b) return 0;
    if (p_a < p_b) return -1;
    return 1;
}


/**
 * @brief session initialization entry point
 *
 * Initializes the plugin specific fields of the session,
 * like the pcap handler and the periodic routines called
 * by fsm.
 * @param session pointer provided by fsm
 */
int
brightcloud_plugin_init(struct fsm_session *session)
{
    struct fsm_bc_session *fsm_bc_session;
    struct fsm_web_cat_ops *cat_ops;
    struct fsm_bc_mgr *mgr;
    time_t now;

    if (session == NULL) return -1;

    mgr = fsm_bc_get_mgr();

    /* Initialize the manager on first call */
    if (!mgr->initialized)
    {
        bool ret;

        ret = fsm_bc_init(session);
        if (!ret) return 0;

        ds_tree_init(&mgr->fsm_sessions, fsm_bc_session_cmp,
                     struct fsm_bc_session, session_node);

        mgr->initialized = true;
    }

    /* Look up the fsm bc session */
    fsm_bc_session = fsm_bc_lookup_session(session);
    if (fsm_bc_session == NULL)
    {
        LOGE("%s: could not allocate fsm bc plugin", __func__);
        return -1;
    }

    /* Bail if the session is already initialized */
    if (fsm_bc_session->initialized) return 0;

    /* Set the fsm session */
    session->ops.periodic = fsm_bc_periodic;
    session->ops.update = fsm_bc_update;
    session->ops.exit = fsm_bc_exit;
    session->handler_ctxt = fsm_bc_session;

    fsm_bc_session->session = session;
    /* read other config on startup*/
    fsm_bc_update(session);

    now = time(NULL);
    fsm_bc_session->stat_report_ts = now;

    /* Set the plugin specific ops */
    cat_ops = &session->p_ops->web_cat_ops;
    cat_ops->categories_check = fsm_bc_cat_check;
    cat_ops->cat2str = fsm_bc_report_cat;
    cat_ops->get_stats = fsm_bc_get_stats;

    /* Initialize latency counters */
    fsm_bc_session->min_latency = LONG_MAX;
    fsm_bc_session->initialized = true;

    /* Initialize offline failure counter */
    fsm_bc_session->bc_offline.connection_failures = 0;

    LOGD("%s: added session %s", __func__, session->name);

    return 0;

err_alloc_aggr:
    fsm_bc_free_session(fsm_bc_session);
    return -1;
}

/**
 * @brief session exit point
 *
 * Frees up resources used by the session.
 * @param session pointer provided by fsm
 */
void
fsm_bc_exit(struct fsm_session *session)
{
    struct fsm_bc_mgr *mgr;

    mgr = fsm_bc_get_mgr();
    if (!mgr->initialized) return;

    fsm_bc_delete_session(session);
    return;
}

/**
 * @brief logs the health stats report to log file
 *
 * @param fsm_bc_session pointer to brightcloud session
 *
 * @param hs pointer containing health stats
 */
static void
bc_log_stats(struct fsm_bc_session *fsm_bc_session, struct wc_health_stats *hs)
{
    struct fsm_session *session;

    session = fsm_bc_session->session;
    LOGN("%s(): brightcloud %s activity report", __func__, session->name);
    LOGN("connectivity failures: %u", hs->connectivity_failures);
    LOGN("total lookups: %u", hs->total_lookups);
    LOGN("total cache hits: %u", hs->cache_hits);
    LOGN("total remote lookups: %u", hs->remote_lookups);
    LOGN("cloud uncategorized responses: %u", hs->uncategorized);
    LOGN("cache entries: [%u/%u]", hs->cached_entries, hs->cache_size);
    LOGN("min lookup latency in ms: %u", hs->min_latency);
    LOGN("max lookup latency in ms: %u", hs->max_latency);
    LOGN("avg lookup latency in ms: %u", hs->avg_latency);
}

/**
 * @brief populate health stats report read from brightcloud plugin
 *
 * @param plugin_context pointer to fsm_bc_session
 * @param stats stats read from brightcloud plugin
 * @param hs pointer containing health stats
 */
static void
bc_report_compute_health_stats(struct fsm_bc_session *fsm_bc_session,
                               struct fsm_url_stats *stats,
                               struct wc_health_stats *hs)
{
    struct fsm_bc_offline *bc_offline;
    struct fsm_url_stats *prev_stats;
    uint32_t dns_cache_hits;
    uint32_t count;

    prev_stats = &fsm_bc_session->health_stats;
    dns_cache_hits = dns_cache_get_hit_count(IP2ACTION_BC_SVC);

    /* Compute total lookups */
    /* In the plugin, every dns transaction is first checked if present in
     * the cache, and hence, every transaction is a cache lookup. Due to this,
     * cache_lookups are not filled in by the plugin. Successful cache lookups
     * result in cache_hits being incremented. Hence, use cache_hits in counting
     * total_lookups */
    count = (uint32_t)(stats->cloud_lookups + stats->cache_hits) + dns_cache_hits;
    count -= (uint32_t)(prev_stats->cloud_lookups + prev_stats->cache_hits);

    hs->total_lookups = count;
    prev_stats->cache_lookups = stats->cache_lookups;

    /* Compute cache hits */
    count = (uint32_t)(stats->cache_hits - prev_stats->cache_hits) + dns_cache_hits;
    hs->cache_hits = count;
    prev_stats->cache_hits = stats->cache_hits + dns_cache_hits;

    /* Compute remote_lookups */
    count = (uint32_t)(stats->cloud_lookups - prev_stats->cloud_lookups);
    hs->remote_lookups = count;
    prev_stats->cloud_lookups = stats->cloud_lookups;

    /* Compute connectivity_failures */
    bc_offline = &fsm_bc_session->bc_offline;
    hs->connectivity_failures = bc_offline->connection_failures;
    bc_offline->connection_failures = 0;

    /* Compute service_failures */
    count = (uint32_t)(stats->categorization_failures
                       - prev_stats->categorization_failures);
    hs->service_failures = count;
    prev_stats->categorization_failures = stats->categorization_failures;

    /* Compute uncategorized requests */
    count = (uint32_t)(stats->uncategorized - prev_stats->uncategorized);
    hs->uncategorized = count;
    prev_stats->uncategorized = stats->uncategorized;

    /* Compute min latency */
    count = (uint32_t)(stats->min_lookup_latency);
    hs->min_latency = count;

    /* Compute max latency */
    count = (uint32_t)(stats->max_lookup_latency);
    hs->max_latency = count;

    /* Compute average latency */
    count = (uint32_t)(stats->avg_lookup_latency);
    hs->avg_latency = count;

    /* Compute cached entries */
    count = (uint32_t)(stats->cache_entries);
    hs->cached_entries = count;

    /* Compute cache size */
    count = (uint32_t)(stats->cache_size);
    hs->cache_size = count;
}

/**
 * @brief computes the health stats and sends the report using MQTT
 *
 * @param plugin_context pointer to symc_fsm_plugin_context
 * @param stats stats read from brightcloud plugin
 * @param now time stats were read
 */
static void
bc_report_health_stats(struct fsm_bc_session *fsm_bc_session,
                       struct fsm_url_stats *stats,
                       time_t now)
{
    struct fsm_url_report_stats report_stats;
    struct wc_packed_buffer *serialized;
    struct wc_observation_window ow;
    struct wc_observation_point op;
    struct wc_stats_report report;
    struct fsm_session *session;
    struct wc_health_stats hs;

    memset(&report, 0, sizeof(report));
    memset(&ow, 0, sizeof(ow));
    memset(&op, 0, sizeof(op));
    memset(&hs, 0, sizeof(hs));
    memset(&report_stats, 0, sizeof(report_stats));

    session = fsm_bc_session->session;

    /* Set observation point */
    op.location_id = session->location_id;
    op.node_id = session->node_id;

    /* set observation window */
    ow.started_at = fsm_bc_session->stat_report_ts;
    ow.ended_at = now;
    fsm_bc_session->stat_report_ts = now;
    bc_report_compute_health_stats(fsm_bc_session, stats, &hs);

    /* Log locally */
    bc_log_stats(fsm_bc_session, &hs);

    /* Prepare report */
    report.provider = session->name;
    report.op = &op;
    report.ow = &ow;
    report.health_stats = &hs;

    /* Serialize report */
    serialized = wc_serialize_wc_stats_report(&report);

    /* Emit report */
    session->ops.send_pb_report(session,
                                fsm_bc_session->health_stats_report_topic,
                                serialized->buf,
                                serialized->len);

    /* Free the serialized protobuf */
    wc_free_packed_buffer(serialized);
}

/**
 * @brief session packet periodic processing entry point
 *
 * Periodically called by the fsm manager
 * Sends a flow stats report.
 * @param session the fsm session
 */
void
fsm_bc_periodic(struct fsm_session *session)
{
    struct fsm_bc_session *fsm_bc_session;
    struct fsm_url_stats stats;
    struct fsm_bc_mgr *mgr;
    double cmp_report;
    bool get_stats;
    time_t now;

    fsm_bc_session = (struct fsm_bc_session *)session->handler_ctxt;
    if (!fsm_bc_session) return;

    mgr = fsm_bc_get_mgr();
    if (!mgr->initialized) return;

    now = time(NULL);

    /* Check if the time has come to report the stats through mqtt */
    cmp_report = now - fsm_bc_session->stat_report_ts;
    get_stats = (cmp_report >= fsm_bc_session->health_stats_report_interval);

    /* No need to gather stats, bail */
    if (get_stats)
    {
        LOGN("%s(): preparing report for webroot", __func__);
        memset(&stats, 0, sizeof(stats));
        fsm_bc_get_stats(session, &stats);
        bc_report_health_stats(fsm_bc_session, &stats, now);
    }
}

#define BC_REPORT_HEALTH_STATS_INTERVAL (60*10)

/**
 * @brief update callback invoked when
 * configuration is changed
 *
 * @param session the fsm session
 */
void
fsm_bc_update(struct fsm_session *session)
{
    struct fsm_bc_session *fsm_bc_session;
    char *hs_report_interval;
    char *hs_report_topic;
    int interval;

    fsm_bc_session = (struct fsm_bc_session *)session->handler_ctxt;
    if (!fsm_bc_session) return;

    fsm_bc_session->health_stats_report_interval = (long)BC_REPORT_HEALTH_STATS_INTERVAL;
    hs_report_interval = session->ops.get_config(session, "wc_health_stats_interval_secs");
    if (hs_report_interval != NULL)
    {
        interval = strtoul(hs_report_interval, NULL, 10);
        fsm_bc_session->health_stats_report_interval = (long)interval;
    }

    hs_report_topic = session->ops.get_config(session, "wc_health_stats_topic");
    fsm_bc_session->health_stats_report_topic = hs_report_topic;
}

/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct fsm_bc_session *
fsm_bc_lookup_session(struct fsm_session *session)
{
    struct fsm_bc_mgr *mgr;
    struct fsm_bc_session *bc_session;
    ds_tree_t *sessions;

    mgr = fsm_bc_get_mgr();
    sessions = &mgr->fsm_sessions;

    bc_session = ds_tree_find(sessions, session);
    if (bc_session != NULL) return bc_session;

    LOGD("%s: Adding new session %s", __func__, session->name);
    bc_session = calloc(1, sizeof(*bc_session));
    if (bc_session == NULL) return NULL;

    ds_tree_insert(sessions, bc_session, session);

    return bc_session;
}


/**
 * @brief Frees a fsm bc session
 *
 * @param bc_session the fsm bc session to delete
 */
void
fsm_bc_free_session(struct fsm_bc_session *bc_session)
{
    free(bc_session);
}


/**
 * @brief deletes a session
 *
 * @param session the fsm session keying the bc session to delete
 */
void
fsm_bc_delete_session(struct fsm_session *session)
{
    struct fsm_bc_mgr *mgr;
    struct fsm_bc_session *bc_session;
    ds_tree_t *sessions;

    mgr = fsm_bc_get_mgr();
    sessions = &mgr->fsm_sessions;

    bc_session = ds_tree_find(sessions, session);
    if (bc_session == NULL) return;

    LOGD("%s: removing session %s", __func__, session->name);
    ds_tree_remove(sessions, bc_session);
    fsm_bc_free_session(bc_session);

    return;
}
