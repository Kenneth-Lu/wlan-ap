#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "fsm_bc_service.h"
#include "fsm_bc_policy.h"
#include "json_util.h"
#include "ovsdb_utils.h"
#include "qm_conn.h"
#include "target.h"
#include "unity.h"

#define OTHER_CONFIG_NELEMS 4
#define OTHER_CONFIG_NELEM_SIZE 32

char g_other_configs[][2][OTHER_CONFIG_NELEMS][OTHER_CONFIG_NELEM_SIZE] =
{
    {
        {
            "mqtt_v",
        },
        {
            "dev-test/brightcloud_ut_topic",
        },
    },
};

struct fsm_session_conf g_confs[2] =
{
    {
        .handler = "brightcloud_test_session_0",
    },
    {
        .handler = "brightcloud_test_session_1",
    }
};


static
void send_report(struct fsm_session *session, char *report)
{
#ifndef ARCH_X86
    qm_response_t res;
    bool ret = false;
#endif

    LOGT("%s: msg len: %zu, msg: %s\n, topic: %s",
         __func__, report ? strlen(report) : 0,
         report ? report : "None", session->topic);
    if (report == NULL) return;

#ifndef ARCH_X86
    ret = qm_conn_send_direct(QM_REQ_COMPRESS_DISABLE, session->topic,
                                       report, strlen(report), &res);
    if (!ret) LOGE("error sending mqtt with topic %s", session->topic);
#endif
    json_free(report);

    return;
}

struct fsm_session_ops g_ops =
{
    .send_report = send_report,
};

union fsm_plugin_ops p_ops;

struct fsm_session g_sessions[2] =
{
    {
        .type = FSM_WEB_CAT,
        .conf = &g_confs[0],
    },
    {
        .type = FSM_WEB_CAT,
        .conf = &g_confs[1],
    }
};


struct fsm_bc_mgr *g_mgr;

const char *test_name = "fsm_brightcloud_tests";

void
global_test_init(void)
{
    size_t n_sessions, i;

    g_mgr = NULL;
    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];
        struct str_pair *pair;

        session->conf = &g_confs[i];
        session->ops  = g_ops;
        session->p_ops = &p_ops;
        session->name = g_confs[i].handler;
        session->conf->other_config = schema2tree(OTHER_CONFIG_NELEM_SIZE,
                                                  OTHER_CONFIG_NELEM_SIZE,
                                                  OTHER_CONFIG_NELEMS,
                                                  g_other_configs[0][0],
                                                  g_other_configs[0][1]);
        pair = ds_tree_find(session->conf->other_config, "mqtt_v");
        session->topic = pair->value;
    }
}

void
global_test_exit(void)
{
    size_t n_sessions, i;

    g_mgr = NULL;
    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        free_str_tree(session->conf->other_config);
    }
}



void
setUp(void)
{
    size_t n_sessions, i;

    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        brightcloud_plugin_init(session);
    }
    g_mgr = fsm_bc_get_mgr();

    return;
}

void tearDown(void)
{
    size_t n_sessions, i;

    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, unregister them */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        fsm_bc_exit(session);
    }
    g_mgr = NULL;

    return;
}


void
test_fqdn_pre_validation(void)
{
    /*
    * char[][] fqdn_samples has 50 strings as FQDNs where 39 are right, 21 aren't
    * Checks for starting '-', valid characters, valid length (4 < x < 255)
    * and proper separation between '.'
    */
    char *fqdn_samples[50] =
    {
        "",
        "unifi",
        "localhost",
        "https://notify.bugsnag.com/",
        "lan",
        "gw",
        "Basement-controller-hc2500hc2500000FFF165103",
        "gsp-ssl.ls.apple.com",
        "iphonesubmissions.apple.com",
        "Seagate-2TB-NAS",
        "valid.origin-apple.com.akadns.net",
        "15-courier.push.apple.com",
        "33-courier.push.apple.com",
        "40-courier.push.apple.com",
        "45-courier.push.apple.com",
        "bag.itunes.apple.com",
        "cl5-cdn.origin-apple.com.akadns.net",
        "clients1.google.com",
        "configuration.apple.com",
        "&eventtype=close&reason=2&duration=5",
        "gdmf.apple.com",
        "google.com",
        "gp.symcd.com",
        "gs-loc-new.ls-apple.com.akadns.net",
        "gsp-ssl-dynamic.ls-apple.com.akadns.net",
        "nakivo",
        "localhost",
        "(none)",
        "dakboard",
        "Seagate-2TB-NAS",
            "https://notify.bugsnag.com/",
        "time-ios.apple.com",
        "iphonesubmissions.apple.com",
        "time-ios.g.aaplimg.com",
        "wpad",
            "ioxmjtcmuo",
        "rxyriagpud",
        "tmvkhefyxp",
        "gsp64-ssl.ls.apple.com",
        "init-p01st.push.apple.co",
            "www.icloud.com",
        "cl5.apple.com",
        "configuration.apple.com",
        "gsp-ssl.ls.apple.com",
        "3-courier.push.apple.com",
        "accounts.google.com",
        "bookmarks.fe.apple-dns.net",
        "dlinkrouter",
        "gateway-carry.icloud.com",
        "gdmf.apple.com"
    };
    bool res = false;
    int m_cnt = 0;
    int w_cnt = 0;
    int i = 0;
    int num_fqdn_samples =  0;

    num_fqdn_samples = (sizeof(fqdn_samples)/sizeof(char *));
    while (i < num_fqdn_samples)
    {
        // replaces last byte of string from \n to \0
        res = fqdn_pre_validation(fqdn_samples[i]);
        // tracks wrong/right counts
        (res == true) ? m_cnt++ : w_cnt++;
        i++;
    }
    LOGI("# matched fqdn: %d\n", m_cnt);
    LOGI("# invalid fqdn: %d\n", w_cnt);
    TEST_ASSERT_EQUAL(30, m_cnt);
    TEST_ASSERT_EQUAL(20, w_cnt);

     //checks that fqdn_string does not exceed 255 character limit
    char test_str[512] =
        "Thisisaverylongstringsetonrepeattotestcharacterexceedinglimit."
        "Thisisaverylongstringsetonrepeattotestcharacterexceedinglimit."
        "Thisisaverylongstringsetonrepeattotestcharacterexceedinglimit."
        "Thisisaverylongstringsetonrepeattotestcharacterexceedinglimit."
        "Thisisaverylongstringsetonrepeattotestcharacterexceedinglimit";
    res = fqdn_pre_validation(test_str);
    TEST_ASSERT_FALSE(res);

     return;
}


/**
 * @brief test plugin init()/exit() sequence
 *
 * Validate plugin reference counts and pointers
 */
void test_load_unload_plugin(void)
{
    /* SetUp() has called init(). Validate settings */
    TEST_ASSERT_NOT_NULL(g_mgr);
}


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_TRACE);

    UnityBegin(test_name);

    global_test_init();

    RUN_TEST(test_load_unload_plugin);
    RUN_TEST(test_fqdn_pre_validation);

    global_test_exit();

    return UNITY_END();
}
