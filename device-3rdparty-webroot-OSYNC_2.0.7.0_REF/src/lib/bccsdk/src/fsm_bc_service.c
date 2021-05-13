#include <stdlib.h>
#include <stddef.h>
#include <time.h>

#include "fsm_bc_service.h"
#include "const.h"
#include "ds_tree.h"
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
    session->ops.exit = fsm_bc_exit;
    session->handler_ctxt = fsm_bc_session;

    /* Set the plugin specific ops */
    cat_ops = &session->p_ops->web_cat_ops;
    cat_ops->categories_check = fsm_bc_cat_check;
    cat_ops->cat2str = fsm_bc_report_cat;
    cat_ops->get_stats = fsm_bc_get_stats;

    /* Initialize latency counters */
    fsm_bc_session->min_latency = LONG_MAX;
    fsm_bc_session->initialized = true;
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
 * @brief session packet periodic processing entry point
 *
 * Periodically called by the fsm manager
 * Sends a flow stats report.
 * @param session the fsm session
 */
void
fsm_bc_periodic(struct fsm_session *session)
{
    /* Place holder */
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
