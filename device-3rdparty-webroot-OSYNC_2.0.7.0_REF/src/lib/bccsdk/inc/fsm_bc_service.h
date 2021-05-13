#ifndef _FSM_BC_SERVICE_H_
#define _FSM_BC_SERVICE_H_

#include <limits.h>
#include <stdint.h>

#include "fsm.h"
#include "ds_tree.h"


/**
 * @brief the plugin manager, a singleton tracking instances
 *
 * The cache tracks the global initialization of the plugin
 * and the running sessions.
 */
struct fsm_bc_mgr
{
    bool initialized;
    ds_tree_t fsm_sessions;
};


/**
 * @brief a session, instance of processing state and routines.
 *
 * The session provides an executing instance of the services'
 * provided by the plugin.
 * It embeds:
 * - a fsm session
 * - state information
 * - a packet parser
 * - latency counters
 * - a set of devices presented to the session
 */
struct fsm_bc_session
{
    struct fsm_session *session;
    bool initialized;
    long min_latency;
    long max_latency;
    long avg_latency;
    int lookup_cnt;
    ds_tree_node_t session_node;
};


struct fsm_bc_mgr *
fsm_bc_get_mgr(void);


/**
 * @brief session initialization entry point
 *
 * Initializes the plugin specific fields of the session,
 * like the pcap handler and the periodic routines called
 * by fsm.
 * @param session pointer provided by fsm
 */
int
brightcloud_plugin_init(struct fsm_session *session);


/**
 * @brief session exit point
 *
 * Frees up resources used by the session.
 * @param session pointer provided by fsm
 */
void
fsm_bc_exit(struct fsm_session *session);


/**
 * @brief session packet periodic processing entry point
 *
 * Periodically called by the fsm manager
 * Sends a flow stats report.
 * @param session the fsm session
 */
void
fsm_bc_periodic(struct fsm_session *session);


/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct fsm_bc_session *
fsm_bc_lookup_session(struct fsm_session *session);


/**
 * @brief Frees a fsm bc session
 *
 * @param bc_session the bc session to delete
 */
void
fsm_bc_free_session(struct fsm_bc_session *bc_session);


/**
 * @brief deletes a session
 *
 * @param session the fsm session keying the bc session to delete
 */
void
fsm_bc_delete_session(struct fsm_session *session);


/**
 * @brief initializes the BC service
 *
 * Initializes the BC service once, with the parameters given within the session
 * @param session the fsm session containing the BC service config
 */
bool
fsm_bc_init(struct fsm_session *session);


/**
 * @brief returns a string matching the category
 *
 * @param session the fsm session
 * @param category the category
 */
char *
fsm_bc_report_cat(struct fsm_session *session,
                  int category);

/**
 * fsm_bc_cat_check: check if a fqdn category matches the policy's
 *                     category rule
 * @req: the request being processed
 * @policy: the policy being checked against
 *
 */
bool
fsm_bc_cat_check(struct fsm_session *session,
                 struct fsm_policy_req *req,
                 struct fsm_policy *policy);

void
fsm_bc_get_stats(struct fsm_session *session,
                 struct fsm_url_stats *stats);

#endif /* _BC_SERVICE_H_ */
